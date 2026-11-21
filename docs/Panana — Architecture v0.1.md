# Panana — Architecture v0.1

Draft, 21 November 2026. Written from the design discussion so far. Items marked **Proposed** are not final.

## 1. What Panana is

Panana is a direct file transfer program between two users, written in C over TCP for Linux. The server is not cloud storage. It connects the two users and relays the bytes live, and it never stores the file. It stores only metadata that describes the transfer.

It is built as an independent product that can work with the backend of different websites. The first consumer is the anoyt app, where storing user files would break the core philosophy.

## 2. Requirements

- Both users are online at the same time (synchronous transfer).
- A transfer must resume after a connection break instead of restarting.
- Written in C, over TCP, on Linux servers.
- Works with a Node.js backend through an API.
- A direct connection between users is not guaranteed, so the server relays the data without storing it.
- Goals: learn networking and Linux, and ship a reusable product, not just an anoyt feature.

## 3. Decisions made

| Decision | Choice |
| --- | --- |
| Overall model | Hybrid: Node controls metadata, the C server handles the data |
| Browser connection | Browsers connect directly to the C server. No proxy in between. |
| TLS | An embedded library inside the C program, not nginx or Caddy |
| Secure and non-secure | Both supported (`wss://` and `ws://`) |
| Project layout | One `src/` folder with a few `.c` files and a main file, built with CMake |

## 4. Two planes

The design separates two kinds of traffic.

**Control plane (Node API).** Small messages that describe a transfer: create it, say who may take part, record progress and state. This is the metadata.

**Data plane (C server).** The file bytes, which are heavy and continuous. They travel from the sender's browser to the C server and on to the receiver's browser. Node is never in this path.

```
            control (HTTP API)
  Node API <------------------> C server (Panana)
     ^                             ^        ^
     | control                     | data   | data
     v                             v        v
  browsers  -----------------> sender    receiver
                          (WebSocket)  (WebSocket)
```

This keeps Panana reusable: any backend that can call the control API can use it.

## 5. Browser protocol

A browser cannot open a raw TCP socket, so the data plane uses **WebSocket**. It gives one two-way connection per user and delivers whole messages, so the stream-boundary problem is handled by the protocol for the browser side.

The C server implements a **minimal** WebSocket server:

1. The opening handshake: read the HTTP upgrade request and answer with `101 Switching Protocols` using the SHA-1 and base64 accept value.
2. Reading frames, including unmasking (browser frames are always masked).
3. Writing frames (server frames are not masked).
4. Opcodes: binary data, close, ping and pong.

Skipped at first: fragmented messages, compression and other extensions. A maximum frame size is enforced from the start.

WebRTC and WebTransport were considered and set aside for now. WebRTC runs on UDP and is too large to implement in C at this stage.

## 6. TLS and dependencies

- **Proposed:** OpenSSL (`libssl` for TLS, `libcrypto` for SHA-1). One dependency covers both jobs. mbedTLS is the fallback if something smaller is wanted later.
- Each connection is wrapped in a small `conn_t` that holds the file descriptor and an optional TLS object. If the TLS object is null the connection is plain TCP. All code above it, including WebSocket, uses only `conn_read` and `conn_write`.
- The library does not issue certificates. For production a real certificate (for example from Let's Encrypt, obtained with a separate tool) is loaded from two files. Browsers do not accept self-signed certificates for `wss://`, so development uses plain `ws://`.
- TLS is added last, after the relay works over plain TCP, because nonblocking TLS needs retry handling (`WANT_READ` and `WANT_WRITE`).

## 7. Transfer lifecycle (Proposed)

1. A user asks the anoyt backend (Node) to send a file to another user.
2. Node creates a transfer record and issues a token to each user.
3. Both browsers open a WebSocket to the C server and present their token.
4. The C server validates the token and pairs the two users on the same transfer.
5. The sender reads the file in chunks and sends them. The server forwards each chunk to the receiver without storing it.
6. The receiver acknowledges how many bytes it has safely received. Progress is reported to the control plane.
7. If the connection breaks, both sides reconnect, agree on the last acknowledged offset, and the sender continues from there.

Open point: whether the C server checks a token by calling Node, or verifies a signed token itself.

## 8. What the server stores (Proposed)

Only data describing the transfer, never the file:

- Transfer ID
- File name and total size
- Sender and receiver identities
- Bytes acknowledged by the receiver (the resume offset)
- State (waiting, active, paused, done, failed)
- Timestamps

This list is not final and is the first open question below.

## 9. Data-plane messages (Proposed outline)

Binary WebSocket messages with a type and fields. Candidate types:

- **HELLO:** present the token and join the transfer
- **OFFER / ACCEPT:** file details and the receiver's consent
- **DATA:** a chunk with its offset and length
- **ACK:** the receiver confirms bytes up to an offset
- **RESUME:** reconnect and state the offset to continue from
- **DONE:** transfer complete
- **ERROR:** failure with a reason

The exact layout is to be designed once the core transfer works.

## 10. Code layout

```
panana/
├── CMakeLists.txt
└── src/
    ├── main.c
    ├── net.c
    └── net.h
```

This will grow as needed, with likely additions for the WebSocket layer, the connection wrapper, and the transfer logic. The layout will be restructured when it becomes necessary.

## 11. Milestones

| Milestone | Goal |
| --- | --- |
| M1 | One program with listen and connect modes; two TCP sockets exchange one message |
| M2 | Echo back and forth, and handle the peer disconnecting |
| M3 | Send a whole file in chunks and save it |
| M4 | Break the connection mid-transfer and resume |
| M5 | WebSocket handshake and frames over plain TCP, tested from a browser |
| M6 | Token check and the control API with Node |
| M7 | Relay between two peers at once (`poll` or `epoll`) |
| M8 | TLS through the embedded library |

The core protocol from M1 to M4 does not depend on the browser layer, so it can be built first.

## 12. Open questions

1. Exactly what the server stores so that resume works.
2. The shape of the control API between the C server and Node.
3. How tokens are verified.
4. How the receiving browser saves a large file to disk without holding it all in memory.
5. Whether a native client (for example the Android app) will later use the raw TCP protocol directly.

## 13. Out of scope for now

- Cloud or temporary file storage
- WebRTC and direct browser-to-browser connections
- WebSocket compression and extensions
- Multiple files per transfer
