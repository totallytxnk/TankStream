#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include <rtc/rtc.hpp>

/**
 * Sender-side WebRTC bridge (H.264 send-only track).
 * LAN-only by default (no STUN — host candidates only).
 */
class WebRTCBridge {
public:
    using LocalDescriptionCallback = std::function<void(const std::string& sdp, const std::string& type)>;
    using LocalCandidateCallback   = std::function<void(const std::string& candidate, const std::string& mid)>;
    using StateChangeCallback      = std::function<void(rtc::PeerConnection::State state)>;

    struct Config {
        std::string stunServer       = "";  // empty = LAN only
        uint32_t    videoSsrc        = 42;
        uint8_t     videoPayloadType = 96;
        std::string videoCname       = "tankstream-video";
        int         videoBitrateKbps = 2500;
    };

    WebRTCBridge();
    explicit WebRTCBridge(const Config& cfg);
    ~WebRTCBridge();

    WebRTCBridge(const WebRTCBridge&) = delete;
    WebRTCBridge& operator=(const WebRTCBridge&) = delete;

    bool start();
    void stop();

    void sendVideoFrame(const uint8_t* data, size_t size, int64_t ptsMs);

    void setRemoteDescription(const std::string& sdp, const std::string& type);
    void addRemoteCandidate(const std::string& candidate, const std::string& mid);

    void onLocalDescription(LocalDescriptionCallback cb) { m_onLocalDesc = std::move(cb); }
    void onLocalCandidate(LocalCandidateCallback cb)     { m_onLocalCand = std::move(cb); }
    void onStateChange(StateChangeCallback cb)           { m_onState = std::move(cb); }

    bool isConnected() const {
        return m_pc && m_pc->state() == rtc::PeerConnection::State::Connected;
    }

private:
    void setupTrack();

    Config m_cfg;
    std::shared_ptr<rtc::PeerConnection> m_pc;
    std::shared_ptr<rtc::Track> m_videoTrack;
    std::shared_ptr<rtc::RtpPacketizationConfig> m_rtpConfig;

    LocalDescriptionCallback m_onLocalDesc;
    LocalCandidateCallback   m_onLocalCand;
    StateChangeCallback      m_onState;

    std::mutex m_sendMutex;
    std::atomic<bool> m_trackOpen{false};
};
