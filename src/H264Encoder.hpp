#pragma once

#include <cstdint>
#include <functional>
#include <memory>
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
 * @brief Hardware-accelerated (or software fallback) H.264 encoder
 *        tuned for ultra-low-latency WebRTC streaming.
 *
 * Configuration forced by design:
 *   - tune        = zerolatency
 *   - gop_size    = 1          (every frame is IDR)
 *   - max_b_frames = 0
 *   - Prefer NVENC / VAAPI / VideoToolbox / MediaFoundation when present
 *
 * Input frames are accepted as raw NV12 or YUV420P.
 * Encoded Annex-B NAL units (with start codes) are delivered via callback.
 */
class H264Encoder {
public:
    using EncodedCallback = std::function<void(const uint8_t* data, size_t size,
                                               int64_t pts, bool keyframe)>;

    struct Config {
        int width          = 1280;
        int height         = 720;
        int fps            = 30;
        int bitrate_kbps   = 2500;
        std::string preferred_encoder; // empty = auto-detect HW first
    };

    H264Encoder() = default;
    ~H264Encoder();

    H264Encoder(const H264Encoder&) = delete;
    H264Encoder& operator=(const H264Encoder&) = delete;

    /**
     * @brief Initialise the encoder with the given configuration.
     * @return true on success
     */
    bool init(const Config& cfg);

    /**
     * @brief Encode one raw frame.
     * @param data   Pointer to planar/semi-planar YUV data
     * @param size   Total buffer size in bytes
     * @param pts    Presentation timestamp (in 1/fps units or AV_TIME_BASE)
     * @param format Pixel format of the input (AV_PIX_FMT_NV12 or AV_PIX_FMT_YUV420P)
     * @return true if the frame was accepted by the encoder
     */
    bool encodeFrame(const uint8_t* data, size_t size, int64_t pts,
                     AVPixelFormat format = AV_PIX_FMT_NV12);

    /**
     * @brief Register the callback that receives encoded Annex-B NAL units.
     */
    void setEncodedCallback(EncodedCallback cb) { m_callback = std::move(cb); }

    bool isInitialised() const { return m_ctx != nullptr; }
    int width() const { return m_cfg.width; }
    int height() const { return m_cfg.height; }
    int fps() const { return m_cfg.fps; }

private:
    bool openCodec(const char* name);
    bool prepareFrame(const uint8_t* data, size_t size, AVPixelFormat format);
    void drainPackets();

    Config               m_cfg;
    AVCodecContext*      m_ctx       = nullptr;
    AVFrame*             m_frame     = nullptr;
    AVPacket*            m_packet    = nullptr;
    SwsContext*          m_sws       = nullptr;
    AVPixelFormat        m_srcFormat = AV_PIX_FMT_NONE;
    EncodedCallback      m_callback;
    int64_t              m_frameIndex = 0;
};
