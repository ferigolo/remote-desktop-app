# **Progressive Development and Testing Flow**

This document establishes the step-by-step action plan for building the ultra-low latency screen sharing system. Developing this architecture requires constant validation, ensuring that inter-process communication (IPC) and graphical rendering are perfectly optimized before introducing network transport.

## **Phase 1: The Control Bridge (Frontend ![][image1] Backend)**

The goal here is to establish the communication channel so that the web interface can manage the lifecycle of the native engine without blocking the main thread.**What to build:**

1. Monorepo setup containing the application shell (Tauri \+ statically exported Next.js).
2. A dynamic C++ library (.dylib on macOS, .so on Linux) with simple _startup_ and _shutdown_ functions.
3. Rust bindings (via FFI/C-ABI) to load this library and expose it to Next.js via Tauri commands (invoke).

**Functional Tests:**

- **Integration Test:** When clicking a "Start Engine" button in Next.js, Rust invokes C++, which writes a log in the system terminal confirming execution, returning a boolean success flag for the frontend to change the button state to "Connected".
- **IPC Stress Test:** Send 1,000 _ping/pong_ commands per second from Next.js to C++ and measure the call latency. The latency must consistently remain under 1ms.

## **Phase 2: The Graphics Engine and Independent Window**

Before capturing real screens or transmitting data, we must ensure the program can display frames on the GPU with near-zero overhead.**What to build:**

1. C++ code to instantiate a raw native OS window, entirely detached from the Tauri WebView.
2. Graphics context setup (Metal on macOS, or Vulkan/OpenGL via Wayland/X11 on Linux).
3. Rendering of a dummy pixel buffer (a texture) onto a 2D _quad_ that fills the screen.

**Functional Tests:**

- **Visual Test:** The native window must open instantly when commanded by Next.js.
- **Graphics Performance Test:** The C++ engine should generate a dynamic color pattern (e.g., moving color bars) and update the texture at a steady 60fps.
- **Resource Test:** Monitor the system task manager. CPU usage to maintain this window at 60fps should consistently be under 1-2%, proving that processing is fully isolated on the GPU.

## **Phase 3: Local Capture and Encoder**

Feeding the Phase 2 graphics pipeline with real local screen data using zero-copy native APIs.**What to build:**

1. Implementation of native capture APIs (_ScreenCaptureKit_ on macOS / _PipeWire_ on Linux).
2. Direct coupling of the captured buffer to the texture rendered in Phase 2\.
3. Initialization of native hardware encoders (VideoToolbox / VA-API).

**Functional Tests:**

- **Graphics Loopback Test:** The program captures the current screen and renders it within its own native window. Visual latency (e.g., dragging a real OS window vs. the response in the native mirror) should be imperceptible to the naked eye.

## **Phase 4: Transport and Signaling**

Separating the "mirror effect" between two distinct machines over the network. **What to build:**

1. Signaling Server in Python (_asyncio_ \+ _websockets_).
2. Integration of libwebrtc into the C++ code.
3. Packaging of captured frames (H.264/H.265/AV1) and transmission via RTP/UDP tracks.
4. Implementation of the WebRTC connection manager on both Host and Client.

**Functional Tests:**

- **Handshake Test (Mock):** Two clients run locally, exchange SDP (Session Description Protocol) files and ICE candidates through the Python server, and establish the P2P tunnel.
- **P2P Streaming Test (Local Network):** Run the Host and Client on two physical computers on the same LAN. Measure the frame rate (FPS), packet loss, and glass-to-glass latency.

## **Phase 5: Client Decoding**

Receiving the transmitted bitstream and efficiently decoding it on the client's GPU before passing it to the rendering pipeline. **What to build:**

1. Extraction of the encoded frames from the WebRTC video track.
2. Hardware decoding logic on the client side.

### Decoders:

1. Software decoder (Any)
2. CUDA decoder (Nvidia)
3. VideoToolbox decoder (Apple Silicon)
4. QVC decoder (Intel)

**Functional Tests:**

- **Decoding Performance Test:** The client receives a high-bitrate stream and decodes it. CPU usage should remain minimal while maintaining a consistent frame rate, proving that hardware acceleration is active.
- **Visual Artifact Test:** Validate that the decoded frames do not suffer from tearing, smearing, or color shifts compared to the host's original screen.

## **Phase 6: Input Injection (Remote Control)**

The final stage: transforming the video _streamer_ into a full bidirectional remote control tool.**What to build:**

1. Capture of native events (mouse, keyboard) within the C++ Client window.
2. Opening the RTCDataChannel (SCTP/UDP) over the existing WebRTC connection.
3. Transmission, reception, and injection of simulated inputs into the Host operating system using _CoreGraphics_ (macOS) and _uinput / XDG Remote Desktop Portal_ (Linux).

**Functional Tests:**

- **Precision and Resolution Test:** Click a target on the Client screen (e.g., natively at 1080p) and ensure the simulated cursor on the Host (e.g., natively at 4K) hits exactly the corresponding pixel after geometric normalization.
- **OS Security Test:** Validate the proper functioning of Accessibility permissions (macOS) and Wayland security barriers (Linux), ensuring artificial commands are not dropped by the kernel.
