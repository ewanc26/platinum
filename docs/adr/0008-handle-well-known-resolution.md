# 0008: Handle resolution over HTTPS well-known

- Status: accepted; revises ADR 0003
- Date: 2026-10-31

## Context

ADR 0003 treated `com.atproto.identity.resolveHandle` as the way to resolve a
handle and explicitly rejected DNS TXT. The AT Protocol handle spec supports
two methods: a DNS TXT record at `_atproto.<handle>`, and an HTTPS document at
`https://<handle>/.well-known/atproto-did`. `resolveHandle` is a service
convenience, not the protocol's only method.

## Decision

1. Resolve a handle first by fetching
   `https://<handle>/.well-known/atproto-did` over verified TLS and reading the
   DID text from the body.
2. If that returns 404 or is unavailable, call
   `com.atproto.identity.resolveHandle` on a PDS/AppView the client already
   trusts.
3. Do not implement DNS TXT. It adds an Open Transport DNS path and a second
   resolver to test without improving the common case.
4. Validate bidirectionally once the DID document is fetched: its `alsoKnownAs`
   must contain `at://<handle>` for the handle that was resolved. A document
   that merely claims a handle is not proof.

## Consequences

- Sign-in can start from a handle without a preconfigured PDS: the well-known
  document gives the DID, the DID document gives the PDS.
- A `did:web` host and a handle's well-known host are dialled directly, so
  their certificates must be valid for those names. That is the method's trust
  model.
- The well-known body is a DID, not JSON. It needs its own strict reader (ASCII,
  bounded, `did:` prefix, no whitespace except a trailing newline).
- A handle that serves no well-known document and whose resolveHandle service is
  down is unresolved; the UI must say so rather than guessing a PDS.
