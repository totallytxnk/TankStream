# TankStream

**Peer-to-peer, ultra-low-latency local media bridge** for streaming webcam video and microphone audio across local networks using WebRTC and hardware-accelerated H.264.

Designed for local LAN / Wi-Fi scenarios where end-to-end latency must stay in the low tens of milliseconds.  
Built as a lightweight, modular, cross-platform open-source project.

```
Tags: webrtc, libdatachannel, ffmpeg, h264, low-latency, p2p, c++, typescript, media-streaming
```

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            TankStream Native Core (C++20)                   │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────────┐  │
│  │  Webcam Capture  │───▶│   H264Encoder    │───▶│   WebRTCBridge       │  │
│  │  (FFmpeg avdev)  │    │  (libavcodec +   │    │  (libdatachannel)    │  │
│  │                  │    │   HW accel)      │    │                      │  │
│  │  NV12 / YUV420P  │    │  zerolatency,    │    │  ICE / DTLS / SRTP   │  │
│  │                  │    │  GOP=1, B=0      │    │  H.264 RTP packetizer│  │
│  └──────────────────┘    └──────────────────┘    │  + Opus audio track  │  │
│                                                   └──────────┬───────────┘  │
└──────────────────────────────────────────────────────────────┼──────────────┘
                                                               │ RTP / SRTP
                                                               ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                     Browser / TypeScript Receiver                           │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │  RTCPeerConnection  →  videoTrack  (playoutDelayHint = 0)            │  │
│  │  Minimal jitter buffer → HTML5 <video> element                       │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Key Design Decisions

| Component              | Choice                          | Rationale                                      |
|------------------------|---------------------------------|------------------------------------------------|
| WebRTC stack           | `paullouisageneau/libdatachannel` | Lightweight, no Google libwebrtc dependency   |
| Video codec            | Hardware H.264 (NVENC / VAAPI / VideoToolbox / MediaFoundation) | Lowest encode latency on modern GPUs          |
| Encoder tuning         | `tune=zerolatency`, `gop_size=1`, `b_frames=0` | Forces IDR every frame, zero B-frame delay    |
| Transport              | WebRTC media tracks + SRTP      | Firewall-friendly, encrypted, standard        |
| Receiver buffering     | `playoutDelayHint = 0`          | Disables browser jitter buffer for min latency|

---

## Repository Layout

```
tankstream/
├── CMakeLists.txt
├── .gitignore
├── README.md
├── src/
│   ├── H264Encoder.hpp
│   ├── H264Encoder.cpp
│   ├── WebRTCBridge.hpp
│   ├── WebRTCBridge.cpp
│   └── main.cpp
└── frontend/
    ├── package.json
    └── tankstream-client.ts
```

---

## Prerequisites

### Native (C++)

- CMake ≥ 3.16
- C++20 compiler (GCC 10+, Clang 12+, MSVC 2019+)
- FFmpeg development libraries (`libavcodec`, `libavformat`, `libavutil`, `libswscale`, `libavdevice`)
- `libdatachannel` (pulled automatically via CMake FetchContent, or system install)
- Threads (usually provided by the platform)

**Linux (Ubuntu/Debian example):**
```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev \
  libssl-dev
```

**macOS (Homebrew):**
```bash
brew install cmake ffmpeg pkg-config
```

**Windows:**  
Use vcpkg or pre-built FFmpeg + OpenSSL. CMake will fetch libdatachannel.

### Frontend

- Node.js ≥ 18
- Modern browser with WebRTC support (Chrome, Firefox, Edge, Safari)

---

## Building the Native Core

```bash
git clone https://github.com/your-org/tankstream.git
cd tankstream
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

The binary `tankstream` will appear in the build directory.

### Optional CMake options

```bash
cmake .. -DTANKSTREAM_USE_SYSTEM_LIBDATACHANNEL=ON   # use system libdatachannel
```

---

## Running (Demo Mode)

The current `main.cpp` implements a **copy-paste signaling** demo (no external signaling server required for LAN testing):

1. Start the native sender:
   ```bash
   ./tankstream
   ```
2. It prints a local SDP offer. Copy the entire SDP.
3. Open the frontend (see below), paste the offer, and generate an answer.
4. Paste the answer back into the native process.
5. Video appears in the browser with minimal buffering.

For production use, replace the console signaling with a lightweight WebSocket or HTTP signaling server (examples exist in the libdatachannel repository).

---

## Frontend Client

```bash
cd frontend
npm install
npx ts-node tankstream-client.ts   # or integrate into your web app
```

Key low-latency setting applied automatically:

```typescript
track.contentHint = "motion";
(track as any).playoutDelayHint = 0;   // disable jitter buffer
```

---

## H.264 Encoder Configuration (Critical for Latency)

The `H264Encoder` class forces:

- `tune = zerolatency`
- `gop_size = 1` (every frame is an IDR)
- `max_b_frames = 0`
- Prefer hardware encoder (`h264_nvenc`, `h264_vaapi`, `h264_videotoolbox`, `h264_mf`) when available, falling back to `libx264` with ultrafast + zerolatency.

Input frames are expected in NV12 or YUV420P. The encoder converts if necessary via `sws_scale`.

---

## WebRTC Media Path

- Video: H.264 NAL units (Annex-B start codes) → `rtc::H264RtpPacketizer` (LongStartSequence) → RTP → SRTP
- Audio: Opus (planned / stub ready)
- SSRC, CNAME, and payload type are negotiated via SDP
- RTCP SR + NACK responder are chained for basic recovery

---

## License

MIT License – see LICENSE file (add one when publishing).

---

## Contributing

Contributions are welcome! Please keep the core lightweight:

- Prefer standard C++20 + well-maintained libraries
- Avoid heavy frameworks
- Document any new hardware acceleration path
- Add unit tests where practical

Open issues for feature requests (multi-viewer, TURN support, audio prioritization, etc.).

---

## Acknowledgments

- [libdatachannel](https://github.com/paullouisageneau/libdatachannel) by Paul-Louis Ageneau
- FFmpeg project
- WebRTC community

---

**TankStream** – Local media, global speed.  
Built for makers who care about every millisecond.
