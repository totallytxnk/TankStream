#pragma once

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

/**
 * Ultra-low-latency H.264 encoder (hardware preferred).
 * GOP=1, no B-frames. Output is Annex-B NAL units.
 */
class H264Encoder {
public:
    struct Config {
        int width = 1280;
        int height = 720;
        int fps = 30;
        int bitrateKbps = 2500;
        AVPixelFormat inputFormat = AV_PIX_FMT_NV12;
    };

    H264Encoder() = default;
    ~H264Encoder();

    H264Encoder(const H264Encoder&) = delete;
    H264Encoder& operator=(const H264Encoder&) = delete;

    bool init(const Config& cfg);
    void shutdown();

    /** Encode one frame. Returns Annex-B access unit, empty on failure. */
    std::vector<uint8_t> encodeFrame(const uint8_t* yPlane, int yStride,
                                     const uint8_t* uvPlane, int uvStride,
                                     int64_t ptsMs);

    /** Encode from packed NV12 buffer (y then interleaved UV). */
    std::vector<uint8_t> encodeNV12(const uint8_t* nv12, int64_t ptsMs);

    bool isOpen() const { return m_ctx != nullptr; }
    const std::string& codecName() const { return m_codecName; }

private:
    bool tryOpenCodec(const char* name);
    std::vector<uint8_t> drainPackets();

    Config m_cfg{};
    AVCodecContext* m_ctx = nullptr;
    AVFrame* m_frame = nullptr;
    AVPacket* m_packet = nullptr;
    SwsContext* m_sws = nullptr;
    std::string m_codecName;
    int64_t m_frameIndex = 0;
};
