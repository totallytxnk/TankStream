#include "tankstream/H264Decoder.hpp"

#include <cstring>
#include <iostream>

H264Decoder::~H264Decoder() {
    shutdown();
}

void H264Decoder::shutdown() {
    if (m_sws) {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    if (m_rgbFrame) {
        av_frame_free(&m_rgbFrame);
    }
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    if (m_ctx) {
        avcodec_free_context(&m_ctx);
    }
    m_swsW = m_swsH = 0;
}

bool H264Decoder::init() {
    shutdown();

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "[H264Decoder] H.264 decoder not found\n";
        return false;
    }

    m_ctx = avcodec_alloc_context3(codec);
    if (!m_ctx) {
        return false;
    }

    m_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_ctx->flags2 |= AV_CODEC_FLAG2_FAST;

    if (avcodec_open2(m_ctx, codec, nullptr) < 0) {
        avcodec_free_context(&m_ctx);
        return false;
    }

    m_frame = av_frame_alloc();
    m_rgbFrame = av_frame_alloc();
    m_packet = av_packet_alloc();
    if (!m_frame || !m_rgbFrame || !m_packet) {
        shutdown();
        return false;
    }

    std::cout << "[H264Decoder] Opened software H.264 decoder\n";
    return true;
}

H264Decoder::Frame H264Decoder::decode(const uint8_t* data, size_t size, int64_t ptsMs) {
    Frame result;
    if (!m_ctx || !data || size == 0) {
        return result;
    }

    if (av_new_packet(m_packet, static_cast<int>(size)) < 0) {
        return result;
    }
    std::memcpy(m_packet->data, data, size);

    if (avcodec_send_packet(m_ctx, m_packet) < 0) {
        av_packet_unref(m_packet);
        return result;
    }
    av_packet_unref(m_packet);

    if (avcodec_receive_frame(m_ctx, m_frame) != 0) {
        return result;
    }

    const int w = m_frame->width;
    const int h = m_frame->height;

    if (!m_sws || m_swsW != w || m_swsH != h) {
        if (m_sws) {
            sws_freeContext(m_sws);
        }
        m_sws = sws_getContext(w, h, static_cast<AVPixelFormat>(m_frame->format),
                               w, h, AV_PIX_FMT_RGB24,
                               SWS_BILINEAR, nullptr, nullptr, nullptr);
        m_swsW = w;
        m_swsH = h;
    }

    if (!m_sws) {
        return result;
    }

    const int rgbSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, w, h, 1);
    result.rgb.resize(static_cast<size_t>(rgbSize));
    av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize,
                         result.rgb.data(), AV_PIX_FMT_RGB24, w, h, 1);

    sws_scale(m_sws,
              m_frame->data, m_frame->linesize, 0, h,
              m_rgbFrame->data, m_rgbFrame->linesize);

    result.width = w;
    result.height = h;
    result.ptsMs = ptsMs;
    return result;
}
