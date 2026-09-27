/**
 * Console test harness (optional). Prefer tankstream-ui.exe for normal use.
 */
#include "tankstream/H264Encoder.hpp"
#include "tankstream/WebRTCBridge.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static std::atomic<bool> g_running{true};

static void onSignal(int) {
    g_running = false;
}

static void generateSyntheticNV12(std::vector<uint8_t>& buf, int w, int h, int frameIdx) {
    buf.resize(static_cast<size_t>(w * h * 3 / 2));
    uint8_t* y = buf.data();
    uint8_t* uv = y + w * h;
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            y[row * w + col] = static_cast<uint8_t>((col + frameIdx * 3) & 0xFF);
        }
    }
    for (int row = 0; row < h / 2; ++row) {
        for (int col = 0; col < w; col += 2) {
            uv[row * w + col]     = static_cast<uint8_t>(128 + (frameIdx * 2 + col) % 64);
            uv[row * w + col + 1] = static_cast<uint8_t>(128 + (frameIdx + row) % 64);
        }
    }
}

int main() {
    std::signal(SIGINT, onSignal);

    std::cout << "=== TankStream console harness ===\n";
    std::cout << "Prefer tankstream-ui.exe for normal use.\n\n";

    H264Encoder::Config ecfg;
    ecfg.width = 1280;
    ecfg.height = 720;
    ecfg.fps = 30;
    ecfg.bitrateKbps = 2500;

    H264Encoder encoder;
    if (!encoder.init(ecfg)) {
        std::cerr << "Encoder init failed\n";
        return 1;
    }

    WebRTCBridge bridge;
    bridge.onLocalDescription([](const std::string& sdp, const std::string& type) {
        std::cout << "\n========== LOCAL SDP (" << type << ") ==========\n"
                  << sdp
                  << "\n========== END SDP ==========\n"
                  << "Paste answer SDP below and press Enter.\n";
    });
    bridge.onStateChange([](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Connected) {
            std::cout << "[main] PeerConnection CONNECTED – streaming\n";
        }
    });

    if (!bridge.start()) {
        std::cerr << "WebRTC start failed\n";
        return 1;
    }

    std::thread signaling([&bridge]() {
        std::string line;
        std::string answer;
        bool collecting = false;
        while (g_running) {
            if (!std::getline(std::cin, line)) break;
            if (line.rfind("v=0", 0) == 0) {
                collecting = true;
                answer.clear();
            }
            if (collecting) {
                answer += line + "\n";
                if (answer.find("a=ice-ufrag:") != std::string::npos &&
                    answer.find("a=ice-pwd:") != std::string::npos &&
                    (line.find("a=ssrc:") != std::string::npos ||
                     line.find("a=setup:") != std::string::npos ||
                     line.empty())) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    std::cout << "[main] Applying remote answer...\n";
                    bridge.setRemoteDescription(answer, "answer");
                    collecting = false;
                }
            }
        }
    });

    std::cout << "[main] Entering capture loop (Ctrl+C to quit)\n";
    std::vector<uint8_t> nv12;
    int frameIdx = 0;
    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    const auto interval = std::chrono::milliseconds(1000 / ecfg.fps);

    while (g_running) {
        generateSyntheticNV12(nv12, ecfg.width, ecfg.height, frameIdx);
        const int64_t ptsMs = frameIdx * (1000 / ecfg.fps);
        auto encoded = encoder.encodeNV12(nv12.data(), ptsMs);
        if (!encoded.empty()) {
            bridge.sendVideoFrame(encoded.data(), encoded.size(), ptsMs);
        }
        ++frameIdx;
        next += interval;
        std::this_thread::sleep_until(next);
    }

    g_running = false;
    if (signaling.joinable()) signaling.join();
    return 0;
}
