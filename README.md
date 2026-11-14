# project-name

Direct file transfer between two users. The server never stores your files.

> **Status:** early idea. Nothing works yet. This README will change as I learn more.

## The problem

Services like WhatsApp and Telegram work like this:

1. The sender uploads the file to the company's server.
2. The receiver downloads it from that server later.

This means the server keeps a copy of every file, and the file travels twice (up, then down).

## The idea

- The server is only a **matchmaker**. It helps two users find and connect to each other.
- The file bytes go **directly from sender to receiver**, in real time (both users are online).
- The server stores only **metadata** about a transfer (information that describes it), never the file contents.
- If the connection breaks, the transfer **resumes from where it stopped**. It does not start again from zero.

## Goals

- Written in **C**, on top of **TCP**, running on **Linux** servers.
- Use protocols that work with the web, like the ones traditional services use (exact choice still open, see below).
- Easy to use from a **Node.js** backend.
- Learn **networking** and **Linux** properly along the way.

## Open questions

- What metadata must the server store so a broken transfer can resume?
- How do two users behind home routers / firewalls (NAT) connect to each other? What if a direct connection is impossible?
- Which protocol: raw TCP, WebSocket, WebRTC, or a mix?
- How does it connect to Node.js: a native addon (N-API) or a separate process?
- Security: encryption, user authentication, checking that the file arrived intact.

## Roadmap

1. Learn TCP basics: write a simple echo server and client in C.
2. Send a file over localhost, with proper message framing.
3. Resume a transfer from an offset after a dropped connection.
4. Add the server as a matchmaker between two clients.
5. Make it work across a real network (NAT, firewalls).
6. Node.js integration.

## Build

TBD

## License

TBD
