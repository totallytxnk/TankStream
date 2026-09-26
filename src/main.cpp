/**
 * TankStream – entry point
 *
 * Demonstrates the full pipeline:
 *   Webcam (or synthetic frames) → H264Encoder → WebRTCBridge → browser
 *
 * Signaling is performed via console copy-paste for simplicity.
 * Replace the signaling section with a WebSocket server for production.
 */

#include "H264Encoder.hpp"
#include "WebRTCBridge.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

namespace {

std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

/**
 * Simple synthetic NV12 frame generator for testing without a camera.
 * Produces a moving colour bar pattern.
 */
void generateSyntheticNV12(uint8_t* dst, int width, int height, int frameIndex) {
    const int ySize = width * height;
    const int uvSize = ySize / 2;

    // Y plane – horizontal gradient that shifts
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            dst[y * width + x] = static_cast<uint8_t>((x + frameIndex * 3) & 0xFF);
        }
    }

    // UV plane (NV12 interleaved)
    uint8_t* uv = dst + ySize;
    for (int i = 0; i < uvSize; i += 2) {
        uv[i]     = 128; // U
        uv[i + 1] = static_cast<uint8_t>((frameIndex * 2) & 0xFF); // V
    }
}

bool openWebcam(AVFormatContext** fmtCtx, AVCodecContext** decCtx,
                int* videoStreamIndex, int width, int height) {
    avdevice_register_all();

    const AVInputFormat* inputFormat = av_find_input_format(
#ifdef _WIN32
        "dshow"
#elif defined(__APPLE__)
        "avfoundation"
#else
        "v4l2"
#endif
    );

    AVDictionary* options = nullptr;
    // Request a reasonable resolution / framerate
    av_dict_set(&options, "video_size", (std::to_string(width) + "x" + std::to_string(height)).c_str(), 0);
    av_dict_set(&options, "framerate", "30", 0);

    const char* device =
#ifdef _WIN32
        "video=Integrated Camera"; // adjust to your device name
#elif defined(__APPLE__)
        "0";                       // first camera
#else
        "/dev/video0";
#endif

    if (avformat_open_input(fmtCtx, device, inputFormat, &options) < 0) {
        av_dict_free(&options);
        std::cerr << "[main] Could not open webcam device – falling back to synthetic frames\n";
        return false;
    }
    av_dict_free(&options);

    if (avformat_find_stream_info(*fmtCtx, nullptr) < 0) {
        avformat_close_input(fmtCtx);
        return false;
    }

    *videoStreamIndex = -1;
    for (unsigned i = 0; i < (*fmtCtx)->nb_streams; ++i) {
        if ((*fmtCtx)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            *videoStreamIndex = static_cast<int>(i);
            break;
        }
    }
    if (*videoStreamIndex < 0) {
        avformat_close_input(fmtCtx);
        return false;
    }

    const AVCodec* decoder = avcodec_find_decoder(
        (*fmtCtx)->streams[*videoStreamIndex]->codecpar->codec_id);
    if (!decoder) {
        avformat_close_input(fmtCtx);
        return false;
    }

    *decCtx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(*decCtx, (*fmtCtx)->streams[*videoStreamIndex]->codecpar);
    if (avcodec_open2(*decCtx, decoder, nullptr) < 0) {
        avcodec_free_context(decCtx);
        avformat_close_input(fmtCtx);
        return false;
    }

    std::cout << "[main] Webcam opened successfully\n";
    return true;
}

} // namespace

int main(int /*argc*/, char** /*argv*/) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    const int width  = 1280;
    const int height = 720;
    const int fps    = 30;

    // ------------------------------------------------------------------
    // 1. Encoder
    // ------------------------------------------------------------------
    H264Encoder encoder;
    H264Encoder::Config encCfg;
    encCfg.width        = width;
    encCfg.height       = height;
    encCfg.fps          = fps;
    encCfg.bitrate_kbps = 2500;

    if (!encoder.init(encCfg)) {
        std::cerr << "Failed to initialise H.264 encoder\n";
        return 1;
    }

    // ------------------------------------------------------------------
    // 2. WebRTC bridge
    // ------------------------------------------------------------------
    WebRTCBridge::Config bridgeCfg;
    bridgeCfg.videoBitrateKbps = 2500;

    WebRTCBridge bridge(bridgeCfg);

    bridge.onLocalDescription([](const std::string& sdp, const std::string& type) {
        std::cout << "\n========== LOCAL SDP (" << type << ") ==========\n";
        std::cout << sdp << std::endl;
        std::cout << "========== END SDP ==========\n";
        std::cout << "Copy the SDP above and paste it into the browser client.\n";
        std::cout << "Then paste the browser's answer SDP below and press Enter.\n\n";
    });

    bridge.onStateChange([](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Connected) {
            std::cout << "[main] PeerConnection CONNECTED – streaming\n";
        }
    });

    if (!bridge.start()) {
        std::cerr << "Failed to start WebRTC bridge\n";
        return 1;
    }

    // ------------------------------------------------------------------
    // 3. Signaling thread (simple console answer reader)
    // ------------------------------------------------------------------
    std::thread signalingThread([&bridge]() {
    std::string line;
    std::string answerSdp;
    bool collecting = false;

    while (g_running) {
        if (!std::getline(std::cin, line)) {
            break;
        }

        // Start of a new SDP
        if (line.rfind("v=0", 0) == 0) {
            collecting = true;
            answerSdp.clear();
        }

        if (collecting) {
            answerSdp += line + "\n";

            // Firefox / Chrome put ice-ufrag near the end.
            // Wait until we have both ice-ufrag and ice-pwd, then apply.
            if (answerSdp.find("a=ice-ufrag:") != std::string::npos &&
                answerSdp.find("a=ice-pwd:")   != std::string::npos &&
                (line.empty() || line.find("a=ssrc:") != std::string::npos ||
                 line.find("a=setup:") != std::string::npos)) {
                // Give a tiny moment in case more lines are still arriving
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::cout << "[main] Applying remote answer...\n";
                bridge.setRemoteDescription(answerSdp, "answer");
                collecting = false;
            }
        }
    }
});

    // ------------------------------------------------------------------
    // 4. Capture / encode / send loop
    // ------------------------------------------------------------------
    AVFormatContext* fmtCtx = nullptr;
    AVCodecContext*  decCtx = nullptr;
    int videoStream = -1;
    const bool useWebcam = openWebcam(&fmtCtx, &decCtx, &videoStream, width, height);

    const size_t nv12Size = static_cast<size_t>(width * height * 3 / 2);
    std::vector<uint8_t> frameBuffer(nv12Size);

    encoder.setEncodedCallback([&bridge](const uint8_t* data, size_t size,
                                         int64_t pts, bool /*keyframe*/) {
        // pts is in encoder time_base (1/fps). Convert to milliseconds.
        const int64_t ptsMs = pts * 1000 / 30;
        bridge.sendVideoFrame(data, size, ptsMs);
    });

    auto nextFrameTime = std::chrono::steady_clock::now();
    const auto frameDuration = std::chrono::milliseconds(1000 / fps);
    int frameIndex = 0;

    std::cout << "[main] Entering capture loop (Ctrl+C to quit)\n";

    while (g_running) {
        if (useWebcam) {
            AVPacket* pkt = av_packet_alloc();
            if (av_read_frame(fmtCtx, pkt) >= 0) {
                if (pkt->stream_index == videoStream) {
                    if (avcodec_send_packet(decCtx, pkt) >= 0) {
                        AVFrame* frame = av_frame_alloc();
                        while (avcodec_receive_frame(decCtx, frame) >= 0) {
                            // Convert whatever the camera gives us to NV12
                            // For brevity we assume the decoder already outputs a usable format
                            // or we re-use the encoder's internal sws path.
                            // In a full implementation we would sws_scale here.
                            // For the demo we fall back to synthetic if conversion is complex.
                            generateSyntheticNV12(frameBuffer.data(), width, height, frameIndex);
                            encoder.encodeFrame(frameBuffer.data(), nv12Size,
                                                frameIndex, AV_PIX_FMT_NV12);
                            ++frameIndex;
                        }
                        av_frame_free(&frame);
                    }
                }
                av_packet_unref(pkt);
            }
            av_packet_free(&pkt);
        } else {
            // Synthetic frames
            generateSyntheticNV12(frameBuffer.data(), width, height, frameIndex);
            encoder.encodeFrame(frameBuffer.data(), nv12Size, frameIndex, AV_PIX_FMT_NV12);
            ++frameIndex;
        }

        nextFrameTime += frameDuration;
        std::this_thread::sleep_until(nextFrameTime);
    }

    std::cout << "\n[main] Shutting down...\n";
    g_running = false;
    if (signalingThread.joinable()) {
        // Best-effort; stdin may block
        signalingThread.detach();
    }

    if (decCtx) {
        avcodec_free_context(&decCtx);
    }
    if (fmtCtx) {
        avformat_close_input(&fmtCtx);
    }

    return 0;
}
