#pragma once

#include <cstdint>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

/**
 * H.264 decoder → RGB24 frames for Qt display.
 * Accepts Annex-B access units (with start codes).
 */
class H264Decoder {
public:
    struct Frame {
        std::vector<uint8_t> rgb;  // RGB24 packed
        int width = 0;
        int height = 0;
        int64_t ptsMs = 0;
    };

    H264Decoder() = default;
    ~H264Decoder();

    H264Decoder(const H264Decoder&) = delete;
    H264Decoder& operator=(const H264Decoder&) = delete;

    bool init();
    void shutdown();

    /** Decode one Annex-B access unit. May return empty if more data needed. */
    Frame decode(const uint8_t* data, size_t size, int64_t ptsMs = 0);

    bool isOpen() const { return m_ctx != nullptr; }

private:
    AVCodecContext* m_ctx = nullptr;
    AVFrame* m_frame = nullptr;
    AVFrame* m_rgbFrame = nullptr;
    AVPacket* m_packet = nullptr;
    SwsContext* m_sws = nullptr;
    int m_swsW = 0;
    int m_swsH = 0;
};
