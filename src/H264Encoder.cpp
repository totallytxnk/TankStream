#include "H264Encoder.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {

const char* kHardwareEncoders[] = {
    "h264_nvenc",        // NVIDIA
    "h264_vaapi",        // Intel/AMD on Linux
    "h264_videotoolbox", // Apple
    "h264_mf",           // Windows Media Foundation
    "h264_qsv",          // Intel Quick Sync
    nullptr
};

void logError(const char* what, int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errnum, buf, sizeof(buf));
    std::cerr << "[H264Encoder] " << what << ": " << buf << std::endl;
}

} // namespace

H264Encoder::~H264Encoder() {
    if (m_sws) {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    if (m_ctx) {
        avcodec_free_context(&m_ctx);
    }
}

bool H264Encoder::openCodec(const char* name) {
    const AVCodec* codec = avcodec_find_encoder_by_name(name);
    if (!codec) {
        return false;
    }

    m_ctx = avcodec_alloc_context3(codec);
    if (!m_ctx) {
        return false;
    }

    m_ctx->width         = m_cfg.width;
    m_ctx->height        = m_cfg.height;
    m_ctx->time_base     = AVRational{1, m_cfg.fps};
    m_ctx->framerate     = AVRational{m_cfg.fps, 1};
    m_ctx->bit_rate      = static_cast<int64_t>(m_cfg.bitrate_kbps) * 1000;
    m_ctx->gop_size      = 1;          // IDR every frame
    m_ctx->max_b_frames  = 0;          // no B-frames
    m_ctx->pix_fmt       = AV_PIX_FMT_YUV420P;
    m_ctx->flags        |= AV_CODEC_FLAG_LOW_DELAY;
    m_ctx->flags2       |= AV_CODEC_FLAG2_FAST;

    // Ultra-low-latency private options (libx264 and many HW encoders honour these)
    av_opt_set(m_ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(m_ctx->priv_data, "preset", "ultrafast", 0);

    // NVENC specific low-latency knobs
    if (std::strcmp(name, "h264_nvenc") == 0) {
        av_opt_set(m_ctx->priv_data, "zerolatency", "1", 0);
        av_opt_set(m_ctx->priv_data, "delay", "0", 0);
        av_opt_set(m_ctx->priv_data, "rc", "cbr", 0);
        av_opt_set(m_ctx->priv_data, "preset", "p1", 0); // fastest
        av_opt_set(m_ctx->priv_data, "tune", "ll", 0);   // low latency
    }

    // VAAPI often needs explicit surface format; we stay software path for simplicity
    // and let FFmpeg handle conversion. Real production code would use hwframes.

    int ret = avcodec_open2(m_ctx, codec, nullptr);
    if (ret < 0) {
        logError("avcodec_open2 failed", ret);
        avcodec_free_context(&m_ctx);
        return false;
    }

    std::cout << "[H264Encoder] Opened encoder: " << name << std::endl;
    return true;
}

bool H264Encoder::init(const Config& cfg) {
    m_cfg = cfg;

    // Prefer user-specified encoder, otherwise try hardware list, finally libx264
    if (!m_cfg.preferred_encoder.empty()) {
        if (openCodec(m_cfg.preferred_encoder.c_str())) {
            // success
        }
    }

    if (!m_ctx) {
        for (const char** p = kHardwareEncoders; *p; ++p) {
            if (openCodec(*p)) {
                break;
            }
        }
    }

    if (!m_ctx) {
        if (!openCodec("libx264")) {
            std::cerr << "[H264Encoder] No suitable H.264 encoder found" << std::endl;
            return false;
        }
        // Force zerolatency again for software path
        av_opt_set(m_ctx->priv_data, "tune", "zerolatency", 0);
        av_opt_set(m_ctx->priv_data, "preset", "ultrafast", 0);
    }

    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();
    if (!m_frame || !m_packet) {
        std::cerr << "[H264Encoder] Failed to allocate frame/packet" << std::endl;
        return false;
    }

    m_frame->format = m_ctx->pix_fmt;
    m_frame->width  = m_ctx->width;
    m_frame->height = m_ctx->height;

    int ret = av_frame_get_buffer(m_frame, 32);
    if (ret < 0) {
        logError("av_frame_get_buffer", ret);
        return false;
    }

    m_frameIndex = 0;
    return true;
}

bool H264Encoder::prepareFrame(const uint8_t* data, size_t size, AVPixelFormat format) {
    if (format != m_srcFormat || !m_sws) {
        if (m_sws) {
            sws_freeContext(m_sws);
            m_sws = nullptr;
        }
        m_sws = sws_getContext(
            m_cfg.width, m_cfg.height, format,
            m_cfg.width, m_cfg.height, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!m_sws) {
            std::cerr << "[H264Encoder] sws_getContext failed" << std::endl;
            return false;
        }
        m_srcFormat = format;
    }

    // Build source planes
    const int srcSize = av_image_get_buffer_size(format, m_cfg.width, m_cfg.height, 1);
    if (static_cast<int>(size) < srcSize) {
        std::cerr << "[H264Encoder] Input buffer too small" << std::endl;
        return false;
    }

    uint8_t* srcData[4] = {};
    int srcLinesize[4] = {};
    av_image_fill_arrays(srcData, srcLinesize,
                         const_cast<uint8_t*>(data),
                         format, m_cfg.width, m_cfg.height, 1);

    av_frame_make_writable(m_frame);
    sws_scale(m_sws,
              srcData, srcLinesize, 0, m_cfg.height,
              m_frame->data, m_frame->linesize);

    m_frame->pts = m_frameIndex++;
    return true;
}

void H264Encoder::drainPackets() {
    while (true) {
        int ret = avcodec_receive_packet(m_ctx, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            logError("avcodec_receive_packet", ret);
            break;
        }

        if (m_callback) {
            const bool keyframe = (m_packet->flags & AV_PKT_FLAG_KEY) != 0;
            m_callback(m_packet->data, static_cast<size_t>(m_packet->size),
                       m_packet->pts, keyframe);
        }
        av_packet_unref(m_packet);
    }
}

bool H264Encoder::encodeFrame(const uint8_t* data, size_t size, int64_t /*pts*/,
                              AVPixelFormat format) {
    if (!m_ctx) {
        return false;
    }

    if (!prepareFrame(data, size, format)) {
        return false;
    }

    int ret = avcodec_send_frame(m_ctx, m_frame);
    if (ret < 0) {
        logError("avcodec_send_frame", ret);
        return false;
    }

    drainPackets();
    return true;
}
