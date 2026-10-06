// Client side of the hosted OAuth node's pairing contract: a port of Wolfram's
// oauth_pairing.h (docs/oauth-pairing.md in ewanc26/wolfram), tested against
// Wolfram's shared vectors in test/vectors/oauth_pairing.json.
//
// This is NOT Platinum's Mac-to-bridge pairing (POST /v1/pair): the Mac client
// cannot do TLS, so that stays Platinum-specific and is outside Wolfram's
// contract. This is how the bridge itself would sign an account in through a
// Wolfram OAuth node instead of running its own OAuth client.
//
// The poll `token` is a bearer credential. Nothing here logs it, the pair code,
// a poll URL or a raw reply, and errors never carry reply text.

export const POLL_INTERVAL_MS = 1500
export const MAX_POLLS = 360

const CODE_MAX = 64
const URL_MAX = 512
const TOKEN_MAX = 256
const HANDLE_MAX = 256
const DID_MAX = 256
const MESSAGE_MAX = 256

export interface PairBegin {
  pairCode: string
  pairUrl: string
  /** Unix seconds; 0 if the node gave none. */
  expiresAt: number
}

export type PairPoll =
  | { state: 'pending' }
  | { state: 'complete'; token: string; handle: string; did: string; service: string }
  | { state: 'error'; message: string }

export class PairingParseError extends Error {}
export class PairingError extends Error {
  constructor(readonly kind: 'auth' | 'timeout' | 'cancelled' | 'parse' | 'begin', message: string) {
    super(message)
  }
}

const fits = (v: unknown, cap: number): v is string =>
  typeof v === 'string' && Buffer.byteLength(v) < cap
const isHttp = (s: string) => s.startsWith('https://') || s.startsWith('http://')

function object(text: string): Record<string, unknown> {
  let v: unknown
  try {
    v = JSON.parse(text)
  } catch {
    throw new PairingParseError('reply is not JSON')
  }
  if (typeof v !== 'object' || v === null || Array.isArray(v)) throw new PairingParseError('reply is not an object')
  return v as Record<string, unknown>
}

/** The `begin` request body. An empty handle, or one with control characters, is refused. */
export function buildBegin(handle: string): string {
  if (!handle || /[\u0000-\u001f\u007f]/.test(handle)) throw new PairingParseError('handle is empty or has control characters')
  return JSON.stringify({ handle })
}

export function parseBegin(text: string): PairBegin {
  const o = object(text)
  if (!fits(o.pair_code, CODE_MAX) || o.pair_code.length === 0) throw new PairingParseError('pair_code missing or too long')
  if (!fits(o.pair_url, URL_MAX) || !isHttp(o.pair_url)) throw new PairingParseError('pair_url missing, too long or not http(s)')
  let expiresAt = 0
  if (o.expires_at !== undefined) {
    if (typeof o.expires_at !== 'number' || !Number.isFinite(o.expires_at)) throw new PairingParseError('expires_at is not a number')
    expiresAt = Math.trunc(o.expires_at)
  }
  return { pairCode: o.pair_code, pairUrl: o.pair_url, expiresAt }
}

export function parsePoll(text: string): PairPoll {
  const o = object(text)
  switch (o.status) {
    case 'pending':
      return { state: 'pending' }
    case 'complete': {
      const { token, handle, did, service } = o
      if (!fits(token, TOKEN_MAX) || token.length === 0 ||
          !fits(handle, HANDLE_MAX) || handle.length === 0 ||
          !fits(did, DID_MAX) || !did.startsWith('did:') ||
          !fits(service, URL_MAX) || !isHttp(service)) {
        throw new PairingParseError('complete reply is missing a field or one is invalid')
      }
      return { state: 'complete', token, handle, did, service }
    }
    case 'error': {
      // Only display text, so it is truncated rather than refused.
      const m = typeof o.message === 'string' ? o.message : ''
      return { state: 'error', message: Buffer.from(m).subarray(0, MESSAGE_MAX - 1).toString('utf8').replace(/�+$/, '') }
    }
    default:
      throw new PairingParseError('unknown status')
  }
}

export interface Reply {
  http: number
  text: string
}

export interface Transport {
  /** POST uk.ewancroft.oauth.begin. Throws on a network failure. */
  begin(body: string): Promise<Reply>
  /** GET uk.ewancroft.oauth.poll?code=. Throws on a network failure. */
  poll(code: string): Promise<Reply>
}

export interface Hooks {
  onCode?(begin: PairBegin): void
  cancel?(): boolean
  sleep(ms: number): Promise<void>
  /** Unix seconds; defaults to the system clock. */
  now?(): number
}

export type Completed = Extract<PairPoll, { state: 'complete' }>

/**
 * The whole begin/poll loop. Returns the completed sign-in or throws a
 * PairingError: 'auth' for a terminal error reply (including a 404 for an
 * unknown code, polled exactly once), 'timeout', 'cancelled', 'parse' when the
 * node breaks the contract on a 2xx, 'begin' when begin fails. Network failures
 * and 5xx are retried.
 */
export async function runPairing(transport: Transport, handle: string, hooks: Hooks): Promise<Completed & { pollsMade: number }> {
  let begin: PairBegin
  try {
    const reply = await transport.begin(buildBegin(handle))
    if (reply.http < 200 || reply.http > 299) throw new PairingError('begin', `begin failed with HTTP ${reply.http}`)
    begin = parseBegin(reply.text)
  } catch (error) {
    if (error instanceof PairingError) throw error
    throw new PairingError('begin', 'begin failed')
  }
  hooks.onCode?.(begin)

  const now = hooks.now ?? (() => Math.floor(Date.now() / 1000))
  for (let polls = 0; ; polls++) {
    if (hooks.cancel?.()) throw new PairingError('cancelled', 'cancelled')
    if (polls >= MAX_POLLS && begin.expiresAt === 0) throw new PairingError('timeout', 'gave up waiting')
    if (polls > 0) await hooks.sleep(POLL_INTERVAL_MS)
    if (begin.expiresAt > 0 && now() > begin.expiresAt) throw new PairingError('timeout', 'the pairing expired')

    let reply: Reply
    try {
      reply = await transport.poll(begin.pairCode)
    } catch {
      continue // transport failure: transient
    }
    if (reply.http === 404) {
      let message = 'Unknown pairing code.'
      try {
        const m = object(reply.text).message
        if (typeof m === 'string' && m) message = m
      } catch { /* keep the default */ }
      throw Object.assign(new PairingError('auth', message), { pollsMade: polls + 1 })
    }
    if (reply.http >= 500) continue
    if (reply.http < 200 || reply.http > 299) throw Object.assign(new PairingError('parse', `unexpected HTTP ${reply.http}`), { pollsMade: polls + 1 })
    let parsed: PairPoll
    try {
      parsed = parsePoll(reply.text)
    } catch {
      throw Object.assign(new PairingError('parse', 'the node broke the pairing contract'), { pollsMade: polls + 1 })
    }
    if (parsed.state === 'complete') return { ...parsed, pollsMade: polls + 1 }
    if (parsed.state === 'error') throw Object.assign(new PairingError('auth', parsed.message), { pollsMade: polls + 1 })
  }
}

/** Real transport over fetch. Sends nothing but the handle and the pair code. */
export function httpTransport(baseUrl: string, fetchImpl: typeof fetch = fetch): Transport {
  const base = baseUrl.replace(/\/+$/, '')
  const read = async (res: Response): Promise<Reply> => {
    const text = await res.text()
    return { http: res.status, text: text.slice(0, 64 * 1024) }
  }
  return {
    begin: async body => read(await fetchImpl(`${base}/xrpc/uk.ewancroft.oauth.begin`, {
      method: 'POST', headers: { 'content-type': 'application/json' }, body, redirect: 'error',
    })),
    poll: async code => read(await fetchImpl(`${base}/xrpc/uk.ewancroft.oauth.poll?code=${encodeURIComponent(code)}`, { redirect: 'error' })),
  }
}
