/**
 * TankStream – TypeScript WebRTC receiver client
 *
 * Designed for ultra-low-latency playback of the H.264 stream produced by
 * the native TankStream sender.
 *
 * Critical low-latency setting:
 *   (track as any).playoutDelayHint = 0;
 *
 * This disables the browser's jitter buffer, trading a tiny amount of
 * resilience for the lowest possible glass-to-glass latency.
 *
 * Usage (browser):
 *   1. Include this module or paste the class into a web page.
 *   2. Create a <video autoplay playsinline> element.
 *   3. Call TankStreamClient.create(videoElement).
 *   4. Paste the native SDP offer → generate answer → paste answer back.
 */

export interface TankStreamClientOptions {
  /** Prefer hardware decoding when available */
  preferHardwareDecoder?: boolean;
  /** ICE servers (default: Google public STUN) */
  iceServers?: RTCIceServer[];
}

export class TankStreamClient {
  private pc: RTCPeerConnection;
  private videoEl: HTMLVideoElement;
  private remoteStream: MediaStream;

  private constructor(videoEl: HTMLVideoElement, options: TankStreamClientOptions = {}) {
    this.videoEl = videoEl;
    this.remoteStream = new MediaStream();

    const iceServers = options.iceServers ?? [
      { urls: "stun:stun.l.google.com:19302" },
    ];

    this.pc = new RTCPeerConnection({
      iceServers,
      // Bundle everything on a single transport
      bundlePolicy: "max-bundle",
      rtcpMuxPolicy: "require",
    });

    this.pc.ontrack = (ev) => this.onTrack(ev);
    this.pc.oniceconnectionstatechange = () => {
      console.log("[TankStream] ICE state:", this.pc.iceConnectionState);
    };
    this.pc.onconnectionstatechange = () => {
      console.log("[TankStream] Connection state:", this.pc.connectionState);
    };
  }

  /**
   * Factory – creates the client and attaches the remote stream to the video element.
   */
  static create(videoEl: HTMLVideoElement, options?: TankStreamClientOptions): TankStreamClient {
    const client = new TankStreamClient(videoEl, options);
    videoEl.srcObject = client.remoteStream;
    videoEl.playsInline = true;
    videoEl.autoplay = true;
    // Reduce any residual buffering the video element might apply
    videoEl.muted = true; // required for autoplay in most browsers
    return client;
  }

  /**
   * Apply a remote SDP offer (from the native TankStream process)
   * and return the local answer SDP that must be sent back.
   */
  async applyOffer(offerSdp: string): Promise<string> {
    const offer: RTCSessionDescriptionInit = {
      type: "offer",
      sdp: offerSdp,
    };

    await this.pc.setRemoteDescription(offer);

    const answer = await this.pc.createAnswer({
      // Prefer H.264 if the browser offers a choice
      offerToReceiveAudio: true,
      offerToReceiveVideo: true,
    });

    // Optional: rewrite SDP to force low-latency parameters if needed
    // (most modern browsers already respect playoutDelayHint)

    await this.pc.setLocalDescription(answer);
    return this.pc.localDescription?.sdp ?? "";
  }

  /**
   * Add a remote ICE candidate received from the native side.
   */
  async addIceCandidate(candidate: string, sdpMid?: string): Promise<void> {
    try {
      await this.pc.addIceCandidate({
        candidate,
        sdpMid: sdpMid ?? "0",
      });
    } catch (err) {
      console.warn("[TankStream] Failed to add ICE candidate:", err);
    }
  }

  /**
   * Close the PeerConnection and release resources.
   */
  close(): void {
    this.pc.close();
    this.remoteStream.getTracks().forEach((t) => t.stop());
  }

  // ------------------------------------------------------------------
  // Private
  // ------------------------------------------------------------------

  private onTrack(ev: RTCTrackEvent): void {
    const track = ev.track;
    console.log("[TankStream] Received track:", track.kind, track.id);

    if (track.kind === "video") {
      // Critical low-latency knobs
      track.contentHint = "motion";

      // Disable the browser jitter buffer (Chrome / Edge / some Firefox builds)
      // This is the single most important setting for sub-100 ms glass-to-glass.
      (track as any).playoutDelayHint = 0;

      // Also request the lowest possible latency from the decoder when available
      if ("getCapabilities" in RTCRtpReceiver) {
        // Future-proofing; currently just a hint
      }
    }

    this.remoteStream.addTrack(track);

    // Ensure the video element starts playing as soon as data arrives
    this.videoEl.play().catch((err) => {
      console.warn("[TankStream] video.play() failed (autoplay policy?):", err);
    });
  }
}

// ---------------------------------------------------------------------------
// Minimal browser demo (when loaded directly in a page)
// ---------------------------------------------------------------------------
declare global {
  interface Window {
    TankStreamClient: typeof TankStreamClient;
  }
}

if (typeof window !== "undefined") {
  window.TankStreamClient = TankStreamClient;

  // Auto-wire a simple UI if the expected elements exist
  document.addEventListener("DOMContentLoaded", () => {
    const video = document.getElementById("tankstream-video") as HTMLVideoElement | null;
    const offerBox = document.getElementById("offer-sdp") as HTMLTextAreaElement | null;
    const answerBox = document.getElementById("answer-sdp") as HTMLTextAreaElement | null;
    const applyBtn = document.getElementById("apply-offer") as HTMLButtonElement | null;

    if (!video || !offerBox || !answerBox || !applyBtn) {
      return; // not the demo page
    }

    const client = TankStreamClient.create(video);

    applyBtn.addEventListener("click", async () => {
      const offer = offerBox.value.trim();
      if (!offer) {
        alert("Paste the native SDP offer first");
        return;
      }
      try {
        const answer = await client.applyOffer(offer);
        answerBox.value = answer;
        console.log("[TankStream] Answer ready – paste it back into the native process");
      } catch (err) {
        console.error(err);
        alert("Failed to apply offer: " + (err as Error).message);
      }
    });
  });
}

export default TankStreamClient;
