#include "tankstream/WebRTCBridge.hpp"

#include <cstring>
#include <iostream>

WebRTCBridge::WebRTCBridge() : WebRTCBridge(Config{}) {}

WebRTCBridge::WebRTCBridge(const Config& cfg) : m_cfg(cfg) {}

WebRTCBridge::~WebRTCBridge() {
    stop();
}

void WebRTCBridge::stop() {
    m_trackOpen = false;
    m_videoTrack.reset();
    m_rtpConfig.reset();
    if (m_pc) {
        m_pc->close();
        m_pc.reset();
    }
}

bool WebRTCBridge::start() {
    stop();
    rtc::InitLogger(rtc::LogLevel::Warning);

    rtc::Configuration config;
    // LAN-only: only add STUN if explicitly configured
    if (!m_cfg.stunServer.empty()) {
        config.iceServers.emplace_back(m_cfg.stunServer);
    }

    m_pc = std::make_shared<rtc::PeerConnection>(config);

    m_pc->onStateChange([this](rtc::PeerConnection::State state) {
        std::cout << "[WebRTCBridge] State: " << state << std::endl;
        if (m_onState) {
            m_onState(state);
        }
    });

    m_pc->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
        std::cout << "[WebRTCBridge] Gathering: " << state << std::endl;
        if (state == rtc::PeerConnection::GatheringState::Complete) {
            auto desc = m_pc->localDescription();
            if (desc && m_onLocalDesc) {
                m_onLocalDesc(std::string(desc.value()), desc->typeString());
            }
        }
    });

    m_pc->onLocalCandidate([this](rtc::Candidate candidate) {
        if (m_onLocalCand) {
            m_onLocalCand(candidate.candidate(), candidate.mid());
        }
    });

    setupTrack();
    m_pc->setLocalDescription();
    return true;
}

void WebRTCBridge::setupTrack() {
    rtc::Description::Video media("video", rtc::Description::Direction::SendOnly);
    media.addH264Codec(m_cfg.videoPayloadType);
    media.setBitrate(m_cfg.videoBitrateKbps);
    media.addSSRC(m_cfg.videoSsrc, m_cfg.videoCname, "tankstream-stream", m_cfg.videoCname);

    m_videoTrack = m_pc->addTrack(media);

    m_rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(
        m_cfg.videoSsrc,
        m_cfg.videoCname,
        m_cfg.videoPayloadType,
        rtc::H264RtpPacketizer::ClockRate);

    auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(
        rtc::NalUnit::Separator::LongStartSequence,
        m_rtpConfig);

    auto srReporter = std::make_shared<rtc::RtcpSrReporter>(m_rtpConfig);
    packetizer->addToChain(srReporter);

    auto nackResponder = std::make_shared<rtc::RtcpNackResponder>();
    packetizer->addToChain(nackResponder);

    m_videoTrack->setMediaHandler(packetizer);

    m_videoTrack->onOpen([this]() {
        std::cout << "[WebRTCBridge] Video track open\n";
        m_trackOpen = true;
    });

    m_videoTrack->onClosed([this]() {
        std::cout << "[WebRTCBridge] Video track closed\n";
        m_trackOpen = false;
    });
}

void WebRTCBridge::sendVideoFrame(const uint8_t* data, size_t size, int64_t ptsMs) {
    if (!m_trackOpen || !m_videoTrack || !m_videoTrack->isOpen()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_sendMutex);
    const uint32_t rtpTs = static_cast<uint32_t>(ptsMs * 90);

    rtc::binary frame(size);
    std::memcpy(frame.data(), data, size);

    rtc::FrameInfo info(rtpTs);
    m_videoTrack->sendFrame(std::move(frame), info);
}

void WebRTCBridge::setRemoteDescription(const std::string& sdp, const std::string& type) {
    if (!m_pc) return;
    try {
        rtc::Description desc(sdp, type);
        m_pc->setRemoteDescription(std::move(desc));
    } catch (const std::exception& e) {
        std::cerr << "[WebRTCBridge] setRemoteDescription error: " << e.what() << std::endl;
    }
}

void WebRTCBridge::addRemoteCandidate(const std::string& candidate, const std::string& mid) {
    if (!m_pc) return;
    try {
        m_pc->addRemoteCandidate(rtc::Candidate(candidate, mid));
    } catch (const std::exception& e) {
        std::cerr << "[WebRTCBridge] addRemoteCandidate error: " << e.what() << std::endl;
    }
}
