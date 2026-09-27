# TankStream

**Peer-to-peer low-latency video bridge for Windows (same Wi-Fi).**

One-click connect on the local network — no SDP copy-paste.

## Features (v0.5)

- LAN discovery (UDP beacons on port 47829)
- Auto TCP signalling (offer/answer)
- Qt 6 Sender + Receiver UI
- Hardware H.264 when available
- Native video display in Receiver

## Two-PC test

1. Same Wi-Fi
2. Firewall: allow tankstream-ui.exe (Private) on both PCs
3. Sender: Start Streaming
4. Receiver: Start Listening → select sender → Connect
5. Status: CONNECTED; video on Receiver

## Build

See previous setup (MSYS2 UCRT64 + local libdatachannel).
Needs Qt6 Widgets + Network.

```bash
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTANKSTREAM_LIBDATACHANNEL_ROOT="C:/Users/YOU/libdatachannel"
cmake --build .
cp ./libdatachannel-build/libdatachannel.dll .
./tankstream-ui.exe
```
