# 0003: DID and handle resolution

- Status: accepted
- Date: 2026-10-31

## Context

Every session starts with a handle and a password. The client needs the
account's DID, and then the PDS that hosts it, before it can call
`createSession`. The bridge used `@atproto/api`, which hides this. The Mac must
do it with HTTPS requests and no DNS TXT dependency.

## Decision

1. Resolve a handle with `com.atproto.identity.resolveHandle` on a service the
   client already trusts (a PDS or the public AppView), over verified TLS.
2. Resolve `did:plc` documents at `https://plc.directory/<did>`.
3. Resolve `did:web` documents using the method rules: the first
   colon-separated segment is the host (a port is `%3A`-encoded), later
   segments are the path, and no path means `/.well-known/did.json`.
4. Read the `#atproto_pds` service whose type is
   `AtprotoPersonalDataServer`; require an https endpoint and a document `id`
   matching the DID asked for.
5. Refuse any other DID method with `WF_ERR_UNSUPPORTED`.

DNS TXT resolution for handles is deliberately not implemented. Open Transport
DNS is not a reliable place to build identity on, it would add a second
resolution path to test, and an HTTPS resolver gives the same answer through
the transport already in use.

## Consequences

- The caller must supply a resolver service for the first step. The UI will
  default to the public AppView and show which service it used; that choice is
  not made in this change.
- A `did:web` host is dialled directly, so its certificate must be valid for
  that host. That is the method's own trust model.
- A DID document that names an http PDS is rejected rather than downgraded.
- The DID document's claimed handle (`alsoKnownAs`) is not used as proof of a
  handle. Handle and DID are cross-checked by the resolver's answer, not by the
  document's self-assertion.
