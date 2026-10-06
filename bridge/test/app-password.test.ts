import assert from 'node:assert/strict'
import test from 'node:test'
import { spawn } from 'node:child_process'
import { mkdtempSync, readdirSync, readFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { AtpAgent, type AtpSessionData } from '@atproto/api'
import {
  AppPasswordService, FailureLimiter, InvalidCredentialsError, InvalidServiceError, validateService,
  type KeyValue, type StoredAppSession,
} from '../src/auth/app-password.js'

class MemoryKv<T> implements KeyValue<T> {
  readonly data = new Map<string, T>()
  async get(k: string) { return this.data.get(k) }
  async set(k: string, v: T) { this.data.set(k, v) }
  async del(k: string) { this.data.delete(k) }
}

const SECRET = 'aaaa-bbbb-cccc-dddd'
const session: AtpSessionData = { accessJwt: 'a.j.t', refreshJwt: 'r.j.t', handle: 'x.test', did: 'did:plc:x', active: true }

class FakeAgent extends AtpAgent {
  seen: { identifier?: string; password?: string } = {}
  accept = true
  constructor() { super({ service: 'https://pds.example.com' }) }
  override async login(o: { identifier: string; password: string }): Promise<never> {
    this.seen = o
    if (!this.accept) throw new Error(`rejected ${o.identifier} ${o.password}`)
    Object.defineProperty(this, 'session', { value: session, configurable: true })
    return {} as never
  }
  override async resumeSession(s: AtpSessionData): Promise<never> {
    Object.defineProperty(this, 'session', { value: s, configurable: true })
    return {} as never
  }
}

test('login returns the session and the password is stored nowhere', async () => {
  const store = new MemoryKv<StoredAppSession>()
  const agent = new FakeAgent()
  const service = new AppPasswordService(store, () => agent)
  const logs: string[] = []
  const orig = { log: console.log, error: console.error, warn: console.warn }
  console.log = console.error = console.warn = (...a: unknown[]) => { logs.push(a.join(' ')) }
  try {
    const out = await service.login('x.test', SECRET, 'https://pds.example.com')
    await service.save('inst-1', out.service, out.session)
  } finally { Object.assign(console, orig) }
  assert.equal(agent.seen.password, SECRET)
  assert.ok(!JSON.stringify([...store.data.values()]).includes(SECRET))
  assert.ok(!logs.join('\n').includes(SECRET))
  const restored = await service.restore('inst-1')
  assert.equal(restored?.session?.did, 'did:plc:x')
  await service.forget('inst-1')
  assert.equal(await service.restore('inst-1'), undefined)
})

test('rejected credentials give a fixed error that does not echo upstream text or the password', async () => {
  const agent = new FakeAgent()
  agent.accept = false
  const service = new AppPasswordService(new MemoryKv<StoredAppSession>(), () => agent)
  await assert.rejects(service.login('x.test', SECRET, 'https://pds.example.com'), (e: Error) => {
    assert.ok(e instanceof InvalidCredentialsError)
    assert.ok(!e.message.includes(SECRET) && !e.message.includes('x.test'))
    return true
  })
})

test('service URLs are restricted to public https hosts', () => {
  assert.equal(validateService(undefined), 'https://bsky.social')
  assert.equal(validateService('https://eurosky.social/'), 'https://eurosky.social')
  for (const bad of ['http://bsky.social', 'https://127.0.0.1', 'https://[::1]', 'https://localhost', 'https://x.local',
    'https://user:pw@bsky.social', 'https://bsky.social:8443', 'https://intranet', 'ftp://x.com', 'not a url']) {
    assert.throws(() => validateService(bad), InvalidServiceError, bad)
  }
})

test('failure limiter blocks after the limit and recovers after the window', () => {
  let now = 0
  const l = new FailureLimiter(3, 1000, () => now)
  for (let i = 0; i < 3; i++) { assert.equal(l.blocked('ip'), false); l.fail('ip') }
  assert.equal(l.blocked('ip'), true)
  assert.equal(l.blocked('other'), false)
  now = 1001
  assert.equal(l.blocked('ip'), false)
})

async function freePort(): Promise<number> {
  const { createServer } = await import('node:net')
  return new Promise((resolve, reject) => {
    const s = createServer().listen(0, '127.0.0.1', () => {
      const port = (s.address() as { port: number }).port
      s.close(() => resolve(port))
    }).on('error', reject)
  })
}

async function runServer(env: Record<string, string>): Promise<{ port: number; stop: () => string }> {
  // An OS-assigned port, not a random guess, and a generous start-up wait:
  // under a loaded runner tsx can take several seconds to compile the server.
  const port = await freePort()
  const dir = mkdtempSync(join(tmpdir(), 'plat-srv-'))
  const child = spawn(process.execPath, ['--import', 'tsx', 'src/server.ts'], {
    cwd: new URL('..', import.meta.url).pathname,
    env: { ...process.env, PLATINUM_BRIDGE_PORT: String(port), PLATINUM_BRIDGE_DATA_DIR: dir, PLATINUM_BRIDGE_PUBLIC_URL: 'https://bridge.example.com', ...env },
  })
  let out = ''
  child.stdout.on('data', d => { out += d })
  child.stderr.on('data', d => { out += d })
  for (let i = 0; i < 300 && !out.includes('listening'); i++) await new Promise(r => setTimeout(r, 100))
  assert.ok(out.includes('listening'), `server did not start: ${out}`)
  return { port, stop: () => { child.kill(); return out + readdirSync(dir).map(f => readFileSync(join(dir, f), 'utf8')).join('') } }
}

async function post(port: number, body: unknown): Promise<{ status: number; json: any }> {
  const res = await fetch(`http://127.0.0.1:${port}/v1/login/app-password`, { method: 'POST', body: JSON.stringify(body) })
  return { status: res.status, json: await res.json() }
}

test('endpoint is off by default', async () => {
  const s = await runServer({})
  try {
    const r = await post(s.port, { identifier: 'x.test', password: SECRET })
    assert.equal(r.status, 403)
    assert.equal(r.json.error, 'app_password_disabled')
  } finally { assert.ok(!s.stop().includes(SECRET)) }
})

test('enabled endpoint validates input and never leaks the password', async () => {
  const s = await runServer({ PLATINUM_BRIDGE_ALLOW_APP_PASSWORD: '1' })
  try {
    assert.equal((await post(s.port, { identifier: 'x.test' })).status, 400)
    assert.equal((await post(s.port, { identifier: 'x.test', password: SECRET, service: 'http://bsky.social' })).json.error, 'invalid_service')
    assert.equal((await post(s.port, { identifier: 'x.test', password: SECRET, service: 'https://127.0.0.1' })).json.error, 'invalid_service')
  } finally { assert.ok(!s.stop().includes(SECRET)) }
})
