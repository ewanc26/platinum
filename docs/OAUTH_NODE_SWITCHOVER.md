# Switching the bridge's upstream sign-in to a Wolfram OAuth node

Status: planning. Tracked in [#75](https://github.com/ewanc26/platinum/issues/75).
The client side of Wolfram's pairing contract is ported and tested
(`bridge/src/auth/oauth-node-pairing.ts`). The bridge's sign-in still runs its
own OAuth client (`@atproto/oauth-client-node`). Nothing here changes the
Mac-to-bridge protocol in [BRIDGE_PROTOCOL.md](BRIDGE_PROTOCOL.md).

## References and pins

- Contract: Wolfram `docs/oauth-pairing.md`, `docs/oauth-node.md`, and
  `include/wolfram/oauth_pairing.h`.
- Shared vectors: `bridge/test/vectors/oauth_pairing.json`. CI diffs them
  against Wolfram commit `df6ee1f4bc5b5f59760ec1a88ecb86ea59c8a048`
  (`.github/workflows/ci.yml`). They are byte-identical to that commit.
- C reference read for this port: `src/session/oauth/pairing.c` at Wolfram
  `5edc0d9`. It matches the pinned commit. `src/node/oauth_node.c` changed
  between the pin and `5edc0d9`, so the node side must be re-read before the
  switch-over.

## Behaviour aligned to the C driver

These follow `pairing.c`, because the contract doc is silent on them:

- A 404 is terminal with the fixed message `Unknown pairing code.`, whatever
  the body says.
- Any non-2xx reply other than 404 is transient and is polled again, up to the
  poll budget. The C driver does this for every non-404 HTTP error.
- A zero or negative `expires_at` means no expiry.
- Field limits are "fits in the buffer", never truncated. The error `message`
  is cut at 255 bytes on a whole character.

## Open questions for Wolfram (decide there first)

1. Should a 4xx other than 404 (for example 403 or 400) be transient? The C
   driver retries it for the full nine-minute budget. For a client error that
   retry is probably wrong. Document the answer in `oauth-pairing.md` and
   update the vectors and the pin here together.
2. Does the transport follow redirects? The pairing code does not say. The
   port refuses them, on both begin and poll, so the request stays on the
   configured node.
3. The contract's "reject, do not truncate" rule is not enforced by the
   transport. `httpTransport` cuts a response body at 64 KiB. A 2xx body that
   long becomes invalid JSON and fails as a contract violation, which is
   terminal. That is safe but should be an explicit rejection.
4. Tokens cannot be wiped in JavaScript. The poll `token` is a string on the
   heap until it is collected. The C side zeroes it with
   `wf_oauth_pair_poll_wipe`. Record this as a known difference.

## Steps

1. **Wolfram: persistence.** Node sessions, the DPoP key and the refresh token
   are in memory (`oauth-node.md`). A restart breaks every installation. Settle
   storage in Wolfram before the bridge depends on it. Until then the bridge
   would turn every node restart into a forced re-pair.
2. **Wolfram: revocation.** The node has no logout or revoke operation.
   Revoking a bridge token only invalidates it locally. The node session stays
   valid for `NODE_SESSION_TTL`, which is 30 days, so the node needs a revoke
   operation before this switch is safe.
3. **Bridge: config.** Add an explicit node URL setting, for example
   `PLATINUM_BRIDGE_OAUTH_NODE_URL`, and document it in `.env.example`. Pin the
   bearer's audience to that origin. Refuse a `service` from `complete` that
   does not match it. Never send the bearer to any other host.
4. **Bridge: installations.** Add an auth-mode value, `node`, beside
   `oauth` and `app-password` (`bridge/src/auth/tokens.ts`). Existing OAuth
   installations cannot be migrated, because the node owns the DPoP key. They
   must re-pair. Keep the current OAuth path for them until the re-pair window
   closes, and run both modes side by side until then.
5. **Bridge: atproto client.** Build the `Agent` against the node's proxy URL
   with the node bearer, rather than restoring an OAuth session by DID
   (`bridge/src/atproto/client.ts`).
6. **Bridge: sign-in route.** Replace the browser OAuth callback with
   begin, then a background poll loop of up to nine minutes. Cap the number of
   concurrent pending sign-ins, rate-limit by client, and stop the loop when the
   pairing is abandoned.
7. **Verify the proxy covers the bridge.** Check every XRPC call the bridge
   makes, including blob upload and `getBlob`. Verify against the node, not
   just the docs.
8. **Docs.** Update `AGENTS.md` sections 6 and 18, `README.md`,
   `docs/BACKEND.md`, `docs/parity.tsv` and `CHANGELOG.md`. Update
   `docs/BRIDGE_PROTOCOL.md` only if the Mac-facing contract changes, which the
   plan does not expect.
9. **Remove the old client.** After the re-pair window: delete
   `@atproto/oauth-client-node`, the OAuth state and session stores, the DPoP
   code and the file-backed OAuth session state, and update the ownership table
   in `tools/ownership.txt`.

## Risks

- **Trust boundary.** The bridge now sends all account traffic through the node
  operator's service. Today the bridge holds the user's refresh token itself.
  Moving it to the node is a change in who is trusted, and the README should say
  so.
- **Single point of failure.** Every timeline, post and notification call goes
  through one node. Its availability becomes the bridge's availability.
- **Serialised proxy.** `proxy_handler` in `oauth_node.c` holds the node's
  global mutex across the whole upstream PDS call (lines 674 to 760 at
  `5edc0d9`). Every proxied request for every session waits behind the slowest
  one. This must be fixed or measured before the bridge depends on it.
- **Content type.** The proxy labels every upstream body `application/json`,
  including `getBlob` image bytes. Check this before serving images through
  the node.
- **Bearer exposure.** The node token is a bearer credential held by the bridge.
  Keep it out of logs and error text, which the port already does.
- **Concurrency and abuse.** Each sign-in holds a poll loop for up to nine
  minutes. Without caps, one client could hold many pending pairings open.
- **Migration.** Existing OAuth users re-pair. Anyone running both modes
  side by side needs the per-installation auth mode to route correctly.

## Not verified

No real Wolfram OAuth node was run. Everything above is checked on the host
against the vectors and against a local stub server. Nothing here has been run
on Mac OS 9 hardware, and nothing here is a hardware result.
