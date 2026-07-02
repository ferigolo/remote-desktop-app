# **High-Performance Screen Sharing Architecture**

This document outlines the architectural blueprint for a cross-platform (macOS & Linux), ultra-low latency screen sharing and remote control application. The system prioritizes maximum performance by bypassing traditional web technologies for media rendering and leveraging native OS APIs coupled with WebRTC for peer-to-peer transport.

## **Core Architectural Principles**

1. **Strict Layer Separation:** The application is divided into a Management Shell (UI) and a Core Media Engine. The UI handles state and signaling, while the native engine exclusively handles the media and data pipelines.
2. **Native Graphical Processing:** Video frames are captured and rendered using low-level, zero-copy native APIs (ScreenCaptureKit on macOS, PipeWire on Linux) directly mapped to the GPU.
3. **Hardware Acceleration:** Video encoding and decoding are strictly hardware-accelerated to prevent CPU bottlenecking.
4. **Multiplexed WebRTC:** All real-time data—video, system audio, microphone audio, and remote input commands—are multiplexed over a single WebRTC PeerConnection.\]

## **Component Layers**

### **1\. The Management Shell (Next.js \+ Tauri)**

This layer acts as the application wrapper and orchestrates the user experience (frontend).

- **Technology:** Tauri (Rust backend) paired with a statically exported Next.js frontend.
- **Responsibilities:**
  - **UI/UX:** Renders the main dashboard, settings, device list, and floating control overlays.
  - **Session Persistence:** Securely stores cryptographic keys and IDs of previously connected devices (using Tauri's secure storage/SQLite) to enable rapid "one-click" reconnections.
  - **Signaling Orchestration:** Communicates with the external Signaling Server via WebSockets to initiate the connection handshake.
  - **Process Management & Bridge (IPC):** Spawns and manages the underlying Core Media Engine as a standalone process (Tauri Sidecar), using standard IPC (stdio/sockets) to instruct it (e.g., "Start Capture", "Accept Connection", "Mute Mic").

### **2\. The Core Media Engine (Standalone C++ Sidecar)**

This is the heart of the application, designed for absolute performance and minimal latency. It runs as a completely separate executable process to ensure maximum stability (crash isolation) and strict compliance with windowing systems' main-thread rendering requirements (such as Wayland and macOS).

- **Technology:** Pure C++ utilizing OS-specific graphics and input APIs, linked with libwebrtc. Compiled as a standalone executable.
- **Responsibilities:**
  - **Peer-to-Peer Transport:** Manages the WebRTC connection directly via UDP.
  - **Host Mode (Transmitter):**
    - **Video Capture:** Intercepts the display buffer using **ScreenCaptureKit** (macOS) or **PipeWire** (Linux/Wayland).
    - **Audio Capture:** Captures system audio alongside the display stream.
    - **Hardware Encoding:** Pushes raw frames to hardware encoders (VideoToolbox on macOS, VA-API/NVENC on Linux).
    - **Input Injection:** Receives control packets via RTCDataChannel and translates them into physical OS inputs using **CoreGraphics** (macOS) or **uinput / XDG Remote Desktop Portal** (Linux).
  - **Client Mode (Receiver):**
    - **Independent Rendering Window:** Opens a dedicated, native graphical window (outside the Tauri WebView) to display the incoming video stream.
    - **Hardware Decoding & Rendering:** Decodes incoming frames and maps them directly as GPU textures for zero-copy rendering.
    - **Input Capture:** Listens to mouse and keyboard events on this native window, normalizes the coordinates, and dispatches them via the RTCDataChannel to the Host.
    - **Microphone Capture:** Captures local microphone audio (CoreAudio/PipeWire) and streams it back to the Host.

### **3\. The Signaling Server (Python)**

A lightweight backend service used only during the initial connection phase.

- **Technology:** Python using asynchronous WebSockets (e.g., asyncio \+ websockets).
- **Responsibilities:**
  - Acts as a rendezvous point for peers to exchange WebRTC metadata.
  - Relays Session Description Protocol (SDP) offers/answers (including intents for Video, Audio, and DataChannels).
  - Relays ICE Candidates (Interactive Connectivity Establishment) for NAT traversal (using external STUN/TURN servers).
  - _Note: Once the P2P connection is established, the signaling server is bypassed for that session._

## **The WebRTC Topology**

The system multiplexes four distinct data streams over a single WebRTC connection:

1. **Video Track (RTP/UDP):** High-definition display stream (Host ![][image1] Client).
2. **System Audio Track (RTP/UDP):** The audio playing on the Host machine (Host ![][image1] Client).
3. **Microphone Audio Track (RTP/UDP):** Voice communication (Client ![][image1] Host).
4. **Data Channel (SCTP/UDP):** An ultra-low latency, ordered binary channel dedicated exclusively to serialized mouse coordinates and keyboard keycodes (Client ![][image1] Host).

## **Complete Data Flow: Establishing a Remote Control Session**

### **Phase 1: Initiation and Signaling**

1. **User Action:** The user on the Client PC opens the Next.js UI and selects a saved Host PC from the connection list.
2. **Local Check:** The Next.js app requests the Host's stored credentials from the Tauri Rust backend.
3. **Signaling Request:** Tauri sends a connection intent via WebSocket to the Python Signaling Server.
4. **Handshake:** The Host PC (listening via its own UI/Signaling connection) receives the intent. Both machines' C++ engines generate SDP offers/answers specifying the need for video, audio, and a DataChannel. They exchange these and their ICE candidates through the Python server.

### **Phase 2: Engine Activation & Media Flow**

1. **P2P Tunnel Established:** The C++ engines successfully negotiate a direct UDP connection.
2. **Host Capture:** The Host's C++ engine initializes ScreenCaptureKit/PipeWire, capturing the screen and system audio, encoding them via hardware, and transmitting them over the RTP tracks.
3. **Client Render:** The Client's C++ engine opens a borderless native window, decodes the incoming stream via hardware, and maps the frames directly to GPU textures. System audio is routed to the Client's speakers.

### **Phase 3: Bidirectional Interaction (Remote Control)**

1. **Input Event:** The user clicks or types inside the Client's native video window.
2. **Capture & Normalize:** The Client's C++ engine hooks these events. It normalizes mouse coordinates (e.g., calculating the click position as a percentage of the video dimensions to account for differing screen resolutions).
3. **Data Transmission:** The normalized events are packed into a minimal binary structure and sent instantly via the RTCDataChannel.
4. **Input Injection:** The Host's C++ engine receives the binary packet and calls CGEventCreateMouseEvent (macOS) or interacts with /dev/uinput (Linux) to physically move the cursor and execute the click on the Host OS.
5. **Voice Feedback:** Simultaneously, the Client's microphone audio is transmitted via a separate RTP track, allowing the Client to talk to someone near the Host PC.

## **Key Performance Considerations**

- **Avoiding the DOM:** By keeping video rendering and input capture in a dedicated C++ window, we prevent the UI framework (DOM manipulation, React state updates) from interfering with the real-time media loop.
- **Zero-Copy:** The architecture strives to keep pixel data on the GPU. On Linux, PipeWire manages buffers without copying them to main memory. During rendering, frames are directly uploaded as textures.
- **Predictable Input Latency:** Utilizing the SCTP DataChannel ensures that control inputs are transmitted over the same optimized UDP path as the media, avoiding the overhead and potential head-of-line blocking of separate TCP connections.
