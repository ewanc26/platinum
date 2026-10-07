import { AtpAgent, type AtpSessionData } from '@atproto/api'
import { isIP } from 'node:net'

// App-password sign-in, a separate path from OAuth.
//
// The password exists in this process only for the duration of one login()
// call. It is never stored, logged or echoed. What is kept, per installation,
// is the PDS session (access and refresh JWT) in a 0600 file under the data
// directory: the same trust level as the OAuth session store. The Mac client
// receives only a bridge token, exactly as it does after OAuth pairing.

export interface KeyValue<T> {
  get(key: string): Promise<T | undefined>
  set(key: string, value: T): Promise<void>
  del(key: string): Promise<void>
}

export interface StoredAppSession {
  service: string
  session: AtpSessionData
}

export type AgentFactory = (service: string, persist: (session: AtpSessionData | undefined) => void) => AtpAgent

export const DEFAULT_SERVICE = 'https://bsky.social'

export class InvalidServiceError extends Error {}
export class InvalidCredentialsError extends Error {
  constructor() {
    super('invalid credentials')
  }
}

/**
 * The service URL comes from a client, and the bridge will connect to it, so it
 * is restricted: https only, no credentials or non-default port in the URL, no
 * IP literals, no localhost-style names. That blocks the obvious SSRF targets;
 * it is not a complete defence against a hostname that resolves privately, and
 * operators who need that closed should front the bridge with an egress policy.
 */
export function validateService(input: string | undefined): string {
  const text = input?.trim() || DEFAULT_SERVICE
  let url: URL
  try {
    url = new URL(text)
  } catch {
    throw new InvalidServiceError('service is not a URL')
  }
  if (url.protocol !== 'https:') throw new InvalidServiceError('service must be https')
  if (url.username || url.password) throw new InvalidServiceError('service must not carry credentials')
  if (url.port && url.port !== '443') throw new InvalidServiceError('service must use the default port')
  const host = url.hostname.replace(/^\[|\]$/g, '')
  if (isIP(host) !== 0) throw new InvalidServiceError('service must be a hostname, not an IP address')
  if (host === 'localhost' || host.endsWith('.localhost') || host.endsWith('.local') || host.endsWith('.internal') || !host.includes('.')) {
    throw new InvalidServiceError('service must be a public hostname')
  }
  return url.origin
}

/** Allows `max` failures per key per window; success does not reset it. */
export class FailureLimiter {
  private readonly failures = new Map<string, number[]>()
  constructor(private readonly max = 5, private readonly windowMs = 10 * 60 * 1000, private readonly now = () => Date.now()) {}

  blocked(key: string): boolean {
    const recent = this.recent(key)
    return recent.length >= this.max
  }

  fail(key: string): void {
    const recent = this.recent(key)
    recent.push(this.now())
    this.failures.set(key, recent)
  }

  private recent(key: string): number[] {
    const cutoff = this.now() - this.windowMs
    const recent = (this.failures.get(key) ?? []).filter(t => t > cutoff)
    if (recent.length === 0) this.failures.delete(key)
    return recent
  }
}

/**
 * The PDS an account lives on, from its DID document: the `#atproto_pds`
 * endpoint. A handle is resolved first through the entry host, the document is
 * read from the PLC directory (did:plc) or the account's own host (did:web), and
 * the endpoint goes through validateService like any client-supplied service.
 * Any failure returns undefined, so the caller keeps the entry host.
 */
export async function discoverPds(
  identifier: string,
  entry: string,
  fetchJson: (url: string) => Promise<unknown> = defaultFetchJson,
  resolveHandle: (service: string, handle: string) => Promise<string | undefined> = defaultResolveHandle,
): Promise<string | undefined> {
  try {
    const did = identifier.startsWith('did:') ? identifier : await resolveHandle(entry, identifier)
    if (!did) return undefined
    const docUrl = did.startsWith('did:plc:')
      ? `https://plc.directory/${did}`
      : did.startsWith('did:web:') ? `https://${did.slice('did:web:'.length).split(':')[0]}/.well-known/did.json` : undefined
    if (!docUrl) return undefined
    const doc = (await fetchJson(docUrl)) as { service?: Array<{ id?: string; type?: string; serviceEndpoint?: unknown }> }
    const pds = doc.service?.find(svc => svc.id?.endsWith('#atproto_pds'))
    if (typeof pds?.serviceEndpoint !== 'string') return undefined
    return validateService(pds.serviceEndpoint)
  } catch {
    return undefined
  }
}

async function defaultFetchJson(url: string): Promise<unknown> {
  const res = await fetch(url, { redirect: 'error', signal: AbortSignal.timeout(10_000) })
  if (!res.ok) throw new Error(`DID document answered ${res.status}`)
  return res.json()
}

async function defaultResolveHandle(service: string, handle: string): Promise<string | undefined> {
  const agent = new AtpAgent({ service })
  const res = await agent.com.atproto.identity.resolveHandle({ handle })
  return res.data.did
}

export class AppPasswordService {
  constructor(
    private readonly store: KeyValue<StoredAppSession>,
    private readonly makeAgent: AgentFactory = (service, persist) =>
      new AtpAgent({ service, persistSession: (_evt, session) => persist(session) }),
  ) {}

  /** Exchange credentials for a PDS session. The password is not retained. */
  async login(identifier: string, password: string, entry: string): Promise<{ did: string; session: AtpSessionData; service: string }> {
    // The account's own PDS, when its DID document names one; otherwise the entry host.
    const service = (await discoverPds(identifier, entry)) ?? entry
    const agent = this.makeAgent(service, () => undefined)
    try {
      await agent.login({ identifier, password })
    } catch {
      // The upstream message can name the account; callers get a fixed error.
      throw new InvalidCredentialsError()
    }
    if (!agent.session) throw new InvalidCredentialsError()
    return { did: agent.session.did, session: agent.session, service }
  }

  async save(installationId: string, service: string, session: AtpSessionData): Promise<void> {
    await this.store.set(installationId, { service, session })
  }

  async forget(installationId: string): Promise<void> {
    await this.store.del(installationId)
  }

  /** An agent for a stored session; refreshed tokens are written back. */
  async restore(installationId: string): Promise<AtpAgent | undefined> {
    const stored = await this.store.get(installationId)
    if (!stored) return undefined
    const agent = this.makeAgent(stored.service, session => {
      if (session) void this.store.set(installationId, { service: stored.service, session })
    })
    await agent.resumeSession(stored.session)
    return agent
  }
}
