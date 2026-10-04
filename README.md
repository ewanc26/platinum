# Platinum

A Mac OS 9 Bluesky client.

Platinum is designed around the limitations of classic Mac OS. The Mac client
speaks a small HTTP/JSON protocol to the Platinum Bridge; the bridge handles
modern TLS, AT Protocol and OAuth.

## Architecture

    Platinum (Mac OS 9)
            |
        HTTP + JSON
            |
      Platinum Bridge
            |
       AT Protocol
            |
        PDS / AppView

This keeps the classic Mac application genuinely classic while allowing it to
communicate with modern Bluesky infrastructure.

## Bridge

The bridge lives in bridge/ and targets Node.js 22 or newer.

    cd bridge
    cp .env.example .env
    npm install
    npm run check
    npm run dev

Open the bridge's /login?handle=your-handle.bsky.social endpoint in a modern
browser. After OAuth completes, the browser displays a single-use pairing code
for Platinum.

The bridge stores OAuth sessions under .platinum-bridge/. That directory is
ignored by Git and must not be committed.

See docs/BRIDGE_PROTOCOL.md for the protocol used by the Mac client.

## Version

Platinum and its bridge protocol currently start at 0.1.0.
