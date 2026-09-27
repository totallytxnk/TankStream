#include "tankstream/WebRTCReceiver.hpp"

#include <iostream>
#include <regex>

WebRTCReceiver::WebRTCReceiver() : WebRTCReceiver(Config{}) {}

WebRTCReceiver::WebRTCReceiver(const Config& cfg) : m_cfg(cfg) {}

WebRTCReceiver::~WebRTCReceiver() {
    stop();
}

void WebRTCReceiver::stop() {
    m_trackOpen = false;
    m_videoTrack.reset();
    if (m_pc) {
        m_pc->close();
        m_pc.reset();
    }
}

static std::string forceAnswerSetup(std::string sdp) {
    return std::regex_replace(sdp, std::regex(R"(a=setup:actpass)"), "a=setup:active");
}

bool WebRTCReceiver::acceptOffer(const std::string& offerSdp) {
    stop();
    rtc::InitLogger(rtc::LogLevel::Warning);

    rtc::Configuration config;
    if (!m_cfg.stunServer.empty()) {
        config.iceServers.emplace_back(m_cfg.stunServer);
    }

    m_pc = std::make_shared<rtc::PeerConnection>(config);

    m_pc->onStateChange([this](rtc::PeerConnection::State state) {
        std::cout << "[WebRTCReceiver] State: " << state << std::endl;
        if (m_onState) {
            m_onState(state);
        }
    });

    m_pc->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
        std::cout << "[WebRTCReceiver] Gathering: " << state << std::endl;
        if (state == rtc::PeerConnection::GatheringState::Complete) {
            auto desc = m_pc->localDescription();
            if (desc && m_onLocalDesc) {
                std::string sdp = forceAnswerSetup(std::string(desc.value()));
                m_onLocalDesc(sdp, "answer");
            }
        }
    });

    m_pc->onTrack([this](std::shared_ptr<rtc::Track> track) {
        std::cout << "[WebRTCReceiver] Track received: " << track->mid() << std::endl;
        m_videoTrack = track;

        auto depacketizer = std::make_shared<rtc::H264RtpDepacketizer>();
        track->setMediaHandler(depacketizer);

        track->onFrame([this](rtc::binary data, rtc::FrameInfo info) {
            if (!m_onVideo || data.empty()) {
                return;
            }
            const int64_t ptsMs = static_cast<int64_t>(info.timestamp) / 90;
            m_onVideo(reinterpret_cast<const uint8_t*>(data.data()), data.size(), ptsMs);
        });

        track->onOpen([this]() {
            std::cout << "[WebRTCReceiver] Video track open\n";
            m_trackOpen = true;
        });

        track->onClosed([this]() {
            std::cout << "[WebRTCReceiver] Video track closed\n";
            m_trackOpen = false;
        });
    });

    try {
        m_pc->setRemoteDescription(rtc::Description(offerSdp, "offer"));
        m_pc->setLocalDescription();
    } catch (const std::exception& e) {
        std::cerr << "[WebRTCReceiver] acceptOffer error: " << e.what() << std::endl;
        if (m_pc) {
            auto desc = m_pc->localDescription();
            if (desc && m_onLocalDesc) {
                std::string sdp = forceAnswerSetup(std::string(desc.value()));
                m_onLocalDesc(sdp, "answer");
                return true;
            }
        }
        stop();
        return false;
    }

    return true;
}
