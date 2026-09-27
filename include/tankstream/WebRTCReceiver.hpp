#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <rtc/rtc.hpp>

/**
 * Receiver-side WebRTC bridge.
 * Accepts a remote offer, produces an answer, depacketizes H.264 RTP
 * and delivers Annex-B access units via callback.
 */
class WebRTCReceiver {
public:
    using LocalDescriptionCallback = std::function<void(const std::string& sdp, const std::string& type)>;
    using StateChangeCallback      = std::function<void(rtc::PeerConnection::State state)>;
    using VideoFrameCallback       = std::function<void(const uint8_t* data, size_t size, int64_t ptsMs)>;

    struct Config {
        std::string stunServer = "";  // empty = LAN only
    };

    WebRTCReceiver();
    explicit WebRTCReceiver(const Config& cfg);
    ~WebRTCReceiver();

    WebRTCReceiver(const WebRTCReceiver&) = delete;
    WebRTCReceiver& operator=(const WebRTCReceiver&) = delete;

    /**
     * Apply remote offer and generate local answer.
     * onLocalDescription will fire with the answer SDP.
     */
    bool acceptOffer(const std::string& offerSdp);

    void stop();

    void onLocalDescription(LocalDescriptionCallback cb) { m_onLocalDesc = std::move(cb); }
    void onStateChange(StateChangeCallback cb)           { m_onState = std::move(cb); }
    void onVideoFrame(VideoFrameCallback cb)             { m_onVideo = std::move(cb); }

    bool isConnected() const {
        return m_pc && m_pc->state() == rtc::PeerConnection::State::Connected;
    }

private:
    Config m_cfg;
    std::shared_ptr<rtc::PeerConnection> m_pc;
    std::shared_ptr<rtc::Track> m_videoTrack;

    LocalDescriptionCallback m_onLocalDesc;
    StateChangeCallback      m_onState;
    VideoFrameCallback       m_onVideo;

    std::atomic<bool> m_trackOpen{false};
};
