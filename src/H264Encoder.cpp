#include "tankstream/H264Encoder.hpp"

#include <cstring>
#include <iostream>

H264Encoder::~H264Encoder() {
    shutdown();
}

void H264Encoder::shutdown() {
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
    m_codecName.clear();
    m_frameIndex = 0;
}

bool H264Encoder::tryOpenCodec(const char* name) {
    const AVCodec* codec = avcodec_find_encoder_by_name(name);
    if (!codec) {
        return false;
    }

    m_ctx = avcodec_alloc_context3(codec);
    if (!m_ctx) {
        return false;
    }

    m_ctx->width = m_cfg.width;
    m_ctx->height = m_cfg.height;
    m_ctx->time_base = AVRational{1, m_cfg.fps};
    m_ctx->framerate = AVRational{m_cfg.fps, 1};
    m_ctx->bit_rate = static_cast<int64_t>(m_cfg.bitrateKbps) * 1000;
    m_ctx->gop_size = 1;
    m_ctx->max_b_frames = 0;
    m_ctx->pix_fmt = AV_PIX_FMT_NV12;
    m_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    // Software x264 low-latency
    if (std::string(name) == "libx264") {
        av_opt_set(m_ctx->priv_data, "preset", "ultrafast", 0);
        av_opt_set(m_ctx->priv_data, "tune", "zerolatency", 0);
        m_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    }

    // NVENC low-latency (correct option names)
    if (std::string(name).find("nvenc") != std::string::npos) {
        av_opt_set(m_ctx->priv_data, "preset", "p1", 0);       // fastest
        av_opt_set(m_ctx->priv_data, "tune", "ll", 0);         // low latency
        av_opt_set(m_ctx->priv_data, "zerolatency", "1", 0);
        av_opt_set(m_ctx->priv_data, "delay", "0", 0);
        av_opt_set(m_ctx->priv_data, "rc", "cbr", 0);
    }

    // AMF
    if (std::string(name).find("amf") != std::string::npos) {
        av_opt_set(m_ctx->priv_data, "usage", "ultralowlatency", 0);
        av_opt_set(m_ctx->priv_data, "quality", "speed", 0);
    }

    // QSV
    if (std::string(name).find("qsv") != std::string::npos) {
        av_opt_set(m_ctx->priv_data, "preset", "veryfast", 0);
        av_opt_set(m_ctx->priv_data, "async_depth", "1", 0);
    }

    if (avcodec_open2(m_ctx, codec, nullptr) < 0) {
        avcodec_free_context(&m_ctx);
        return false;
    }

    m_codecName = name;
    return true;
}

bool H264Encoder::init(const Config& cfg) {
    shutdown();
    m_cfg = cfg;

    const char* candidates[] = {
        "h264_nvenc",
        "h264_amf",
        "h264_qsv",
        "h264_mf",
        "libx264",
        nullptr
    };

    bool opened = false;
    for (int i = 0; candidates[i]; ++i) {
        if (tryOpenCodec(candidates[i])) {
            opened = true;
            break;
        }
    }

    if (!opened) {
        std::cerr << "[H264Encoder] No H.264 encoder available\n";
        return false;
    }

    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();
    if (!m_frame || !m_packet) {
        shutdown();
        return false;
    }

    m_frame->format = m_ctx->pix_fmt;
    m_frame->width = m_cfg.width;
    m_frame->height = m_cfg.height;
    if (av_frame_get_buffer(m_frame, 32) < 0) {
        shutdown();
        return false;
    }

    std::cout << "[H264Encoder] Opened encoder: " << m_codecName << std::endl;
    return true;
}

std::vector<uint8_t> H264Encoder::drainPackets() {
    std::vector<uint8_t> out;
    while (avcodec_receive_packet(m_ctx, m_packet) == 0) {
        out.insert(out.end(), m_packet->data, m_packet->data + m_packet->size);
        av_packet_unref(m_packet);
    }
    return out;
}

std::vector<uint8_t> H264Encoder::encodeFrame(const uint8_t* yPlane, int yStride,
                                              const uint8_t* uvPlane, int uvStride,
                                              int64_t ptsMs) {
    if (!m_ctx || !m_frame) {
        return {};
    }

    if (av_frame_make_writable(m_frame) < 0) {
        return {};
    }

    // Copy Y
    for (int y = 0; y < m_cfg.height; ++y) {
        std::memcpy(m_frame->data[0] + y * m_frame->linesize[0],
                    yPlane + y * yStride,
                    static_cast<size_t>(m_cfg.width));
    }

    if (m_ctx->pix_fmt == AV_PIX_FMT_NV12) {
        for (int y = 0; y < m_cfg.height / 2; ++y) {
            std::memcpy(m_frame->data[1] + y * m_frame->linesize[1],
                        uvPlane + y * uvStride,
                        static_cast<size_t>(m_cfg.width));
        }
    } else {
        // YUV420P planar — treat uvPlane as interleaved NV12 and split
        for (int y = 0; y < m_cfg.height / 2; ++y) {
            const uint8_t* src = uvPlane + y * uvStride;
            uint8_t* u = m_frame->data[1] + y * m_frame->linesize[1];
            uint8_t* v = m_frame->data[2] + y * m_frame->linesize[2];
            for (int x = 0; x < m_cfg.width / 2; ++x) {
                u[x] = src[x * 2];
                v[x] = src[x * 2 + 1];
            }
        }
    }

    m_frame->pts = m_frameIndex++;
    (void)ptsMs;

    if (avcodec_send_frame(m_ctx, m_frame) < 0) {
        return {};
    }
    return drainPackets();
}

std::vector<uint8_t> H264Encoder::encodeNV12(const uint8_t* nv12, int64_t ptsMs) {
    const int ySize = m_cfg.width * m_cfg.height;
    return encodeFrame(nv12, m_cfg.width,
                       nv12 + ySize, m_cfg.width,
                       ptsMs);
}
