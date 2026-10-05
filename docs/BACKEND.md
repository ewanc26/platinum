# Platinum Backend Design

This document defines the backend direction for Platinum beyond the initial bridge prototype.

The backend is the modern service boundary between the Classic Mac OS 9 client and the AT Protocol ecosystem. It should keep the Mac client deliberately simple while providing a stable, versioned API that can evolve independently of Bluesky's implementation details.

## Goals

The backend should:

- provide a stable HTTP/JSON API for Platinum;
- own AT Protocol OAuth and session lifecycle;
- keep OAuth refresh credentials off Classic Mac OS;
- issue and revoke per-installation bridge credentials;
- translate modern AT Protocol APIs into compact client-oriented responses;
- provide predictable pagination and error handling;
- isolate external PDS/AppView failures from the native application;
- support multiple Platinum installations for one account;
- be deployable as a small self-hosted service;
- remain independent of any particular frontend or Mac OS 9 build.

The backend is not intended to become a generic Bluesky proxy.

## Non-goals

The backend should not:

- implement an AT Protocol server;
- store users' Bluesky passwords;
- move OAuth refresh credentials to the Mac client;
- require a database before one is justified;
- reproduce every Bluesky API endpoint;
- expose raw OAuth, DPoP or PDS session objects to Platinum;
- become the native application's UI layer.

## Logical architecture

    Modern Internet
           |
    +------v-----------+
    | Platinum Backend|
    |                 |
    | OAuth           |
    | Identity        |
    | API             |
    | Token registry  |
    | Persistence     |
    +----+--------+---+
         |        |
    AT Protocol   |
         |        |
    +----v---+    |
    | PDS /  |    |
    | AppView|    |
    +--------+    |
                  |
          +-------v-------+
          | Platinum Mac  |
          | OS 9 Client   |
          +---------------+

The current repository calls this service the **Platinum Bridge**. The bridge should remain the deployment/runtime name even if the internal code is eventually reorganised as a broader backend.

## API layers

The backend should have four logical layers.

### Transport

Responsible for:

- HTTP request parsing;
- routing;
- authentication headers;
- content types;
- request-size limits;
- response serialization;
- HTTP status codes.

The transport layer should not contain AT Protocol business logic.

### Authentication

Responsible for:

- browser OAuth initiation;
- OAuth callback handling;
- OAuth state validation;
- OAuth session persistence;
- pairing-code creation and exchange;
- bridge-token validation and revocation.

The Mac client should see only pairing and bridge-token semantics.

### Domain API

Responsible for client-facing operations:

- account/profile;
- home timeline;
- notifications;
- posts;
- threads/replies;
- profile lookup;
- future media and interaction features.

Domain handlers should call service abstractions rather than constructing raw AT Protocol requests throughout the HTTP router.

### AT Protocol adapter

Responsible for:

- constructing AT Protocol client operations;
- PDS/AppView communication;
- session restoration;
- translating AT Protocol responses into internal domain objects;
- handling protocol-specific errors.

This is the boundary that protects the Platinum API from upstream API churn.

## Proposed module layout

The existing prototype is intentionally small. As the backend grows, migrate toward:

    bridge/
    ├── src/
    │   ├── server.ts
    │   ├── config.ts
    │   ├── http/
    │   │   ├── json.ts
    │   │   ├── errors.ts
    │   │   └── router.ts
    │   ├── auth/
    │   │   ├── oauth.ts
    │   │   ├── pairing.ts
    │   │   └── tokens.ts
    │   ├── domain/
    │   │   ├── profile.ts
    │   │   ├── timeline.ts
    │   │   ├── notifications.ts
    │   │   └── posts.ts
    │   ├── atproto/
    │   │   └── client.ts
    │   └── storage/
    │       ├── interfaces.ts
    │       └── file.ts
    ├── test/
    └── package.json

Do not perform this reorganisation mechanically. Introduce modules when there is enough behaviour to justify the boundary.

The first backend refactor now establishes configuration, HTTP helpers, authentication services, storage interfaces, a file-backed installation store, an AT Protocol adapter and a domain API. The HTTP server remains the composition root rather than becoming a second framework.

Legacy `tokens.json` bridge credentials are migrated once at startup into hashed installation records. New installation credentials are never written to disk in plaintext.

## Authentication model

The backend has two distinct authentication lifecycles.

### Browser OAuth

The user authenticates with their PDS/identity provider through the browser.

The backend stores the OAuth session and refresh information.

The Mac client is not involved in this lifecycle.

### Platinum installation

After OAuth succeeds:

1. the backend creates a short-lived pairing code;
2. the user enters the code into Platinum;
3. Platinum exchanges it for a bridge token;
4. the backend associates that token with the OAuth account;
5. Platinum uses the bridge token for API requests.

Tokens should eventually become explicit records rather than an unstructured token-to-DID map.

A future token record should contain at least:

    token id
    token secret/hash
    account DID
    created at
    last used at
    revoked at
    client version
    installation label

The plaintext token should only be available at issuance time. Where practical, persistent storage should contain a hash rather than the raw bearer credential.

## Account model

The backend should treat the AT Protocol DID as the canonical account identity.

Do not use a handle as the permanent account key. Handles can change.

Conceptually:

    Account
      did
      current handle
      OAuth session reference

A bridge token references the account rather than directly embedding account identity into the token itself.

## Installation model

A single account may use multiple Platinum installations.

Therefore:

    Account 1 --- * Installation

An installation represents one Platinum client instance and its credential.

This allows future features such as:

- listing active installations;
- revoking one installation without revoking OAuth;
- showing last-used information;
- identifying old Classic Mac hardware;
- rotating a token.

These features should not be exposed to the Mac client until there is a user-facing reason for them.

## Storage

The current implementation uses JSON files because the prototype needs minimal operational dependencies.

The storage boundary should become an interface before introducing a database.

Required stores are conceptually:

- OAuth state store;
- OAuth session store;
- account store;
- installation/token store;
- pairing-code store.

The first production-capable implementation can continue using files if the deployment is single-instance and the operational requirements are modest.

SQLite is the likely next persistence step when transactional updates, concurrent requests, token hashing, installation management or reliable cleanup become necessary.

Do not introduce PostgreSQL, Redis or another external service merely for architectural fashion.

## Pairing lifecycle

Pairing codes are ephemeral:

    OAuth success
         |
     create code
         |
      10 minute TTL
         |
         +---- exchange ----> installation/token
         |
         +---- expiry ------> discard

Requirements:

- six-character human-enterable code;
- safe alphabet without ambiguous characters;
- single-use;
- ten-minute expiry;
- no token or OAuth credential in the code;
- no sensitive logging.

The pairing store should eventually support atomic exchange so two simultaneous requests cannot redeem the same code.

## API versioning

The existing protocol: 1 response establishes the client protocol version.

Keep protocol versioning independent from the backend package version.

A backend release can change without requiring a Mac client update as long as the wire protocol remains compatible.

Breaking API changes should introduce a new protocol version rather than silently changing existing response shapes.

Prefer additive changes within protocol version 1.

## Response design

Responses should be client-oriented rather than raw @atproto/api objects.

For example, the timeline API should eventually expose only the fields Platinum needs for rendering:

    {
      "feed": [
        {
          "uri": "...",
          "cid": "...",
          "author": {
            "did": "...",
            "handle": "...",
            "displayName": "...",
            "avatar": "..."
          },
          "text": "...",
          "createdAt": "...",
          "replyCount": 0,
          "repostCount": 0,
          "likeCount": 0
        }
      ],
      "cursor": "..."
    }

The exact schema should be established before the native UI depends heavily on it.

Do not expose internal AT Protocol object graphs merely because they are convenient for the bridge implementation.

## Error model

Errors should have stable machine-readable codes:

    {
      "error": "invalid_token",
      "message": "The Platinum installation token is no longer valid."
    }

The error value is part of the protocol contract.

The message is for diagnostics and may change.

Backend errors should distinguish:

- invalid client request;
- authentication failure;
- expired/revoked installation;
- upstream AT Protocol failure;
- temporary backend failure;
- unsupported protocol version.

Do not leak upstream stack traces, credentials, filesystem paths or internal exception messages.

## Upstream failure handling

The backend should shield the Mac client from transient upstream failures.

At minimum:

- preserve meaningful HTTP status information;
- map known AT Protocol errors to stable Platinum errors;
- avoid retrying non-idempotent operations blindly;
- allow safe GET operations to be retried where appropriate;
- distinguish authentication/session failure from PDS downtime.

Posting must never be automatically retried unless idempotency semantics are explicitly designed.

## Caching

Caching should be introduced selectively.

Suitable candidates may include:

- public profile metadata;
- DID/document resolution;
- avatar URLs or metadata;
- short-lived AppView responses where safe.

Do not cache authenticated timeline or notification state without understanding freshness and account-specific semantics.

Classic Mac OS 9 can benefit from backend-side work that reduces payload size, but correctness comes first.

## Media

Media should not initially be proxied through the backend.

When media support is added, decide explicitly whether Platinum receives:

- direct remote URLs;
- resized backend-generated assets;
- metadata only;
- or a dedicated media endpoint.

Classic Mac OS 9 constraints mean payload size and image format support need to be treated as first-class design constraints.

## Observability

The backend should eventually provide structured operational information without logging credentials.

Useful fields include:

- request ID;
- route;
- HTTP status;
- duration;
- backend version;
- upstream service;
- upstream status.

Never log:

- Authorization headers;
- bridge tokens;
- OAuth codes;
- refresh credentials;
- DPoP private material;
- full request bodies containing user content unless explicitly required for debugging.

## Deployment model

The first supported deployment should remain simple:

    Internet
       |
    HTTPS reverse proxy
       |
    Node.js Platinum Bridge
       |
    local persistent storage

The backend should work behind a reverse proxy and should derive OAuth metadata from its externally reachable public URL.

A future deployment can use managed storage without changing the Mac client protocol.

## Development sequence

Backend work should proceed in this order:

1. establish internal service boundaries;
2. extract configuration and HTTP helpers;
3. formalise authentication/token records;
4. formalise domain response types;
5. add tests around pairing, token validation and protocol responses;
6. introduce storage interfaces;
7. add SQLite when concurrent persistence requirements justify it;
8. add domain endpoints incrementally;
9. add observability and deployment hardening;
10. keep the native Mac client consuming only the stable protocol.

The immediate goal is not a large framework. It is a backend whose boundaries are clear enough that the native client can be built against it without coupling itself to @atproto/api.

## Compatibility rule

The Platinum backend is an implementation detail behind the Platinum protocol.

When possible:

    AT Protocol change
           |
           v
    backend adapter change
           |
           v
    same Platinum response
           |
           v
    no Mac client update

That is the core architectural advantage of the bridge.
