import assert from 'node:assert/strict'
import test from 'node:test'
import { readFileSync } from 'node:fs'
import {
  buildBegin, parseBegin, parsePoll, runPairing, PairingError, PairingParseError,
  type Reply, type Transport,
} from '../src/auth/oauth-node-pairing.js'

// Wolfram's vectors (test/vectors/oauth_pairing.json), verbatim; CI diffs them.
const v = JSON.parse(readFileSync(new URL('./vectors/oauth_pairing.json', import.meta.url), 'utf8')) as {
  build_begin: { name: string; handle: string; body?: string; invalid?: boolean }[]
  begin: { name: string; body?: unknown; body_text?: string; expect: unknown }[]
  poll: { name: string; body?: unknown; body_text?: string; expect: unknown }[]
  driver: { name: string; polls: { http: number; body: unknown }[]; expect: { status: string; polls_made: number; message?: string } }[]
}
const text = (c: { body?: unknown; body_text?: string }) => (c.body_text !== undefined ? c.body_text : JSON.stringify(c.body))

test('begin request bodies', () => {
  for (const c of v.build_begin) {
    if (c.invalid) assert.throws(() => buildBegin(c.handle), PairingParseError, c.name)
    else assert.equal(buildBegin(c.handle), c.body, c.name)
  }
})

test('begin replies', () => {
  for (const c of v.begin) {
    if (c.expect === 'parse_error') assert.throws(() => parseBegin(text(c)), PairingParseError, c.name)
    else {
      const e = c.expect as { pair_code: string; pair_url: string; expires_at: number }
      assert.deepEqual(parseBegin(text(c)), { pairCode: e.pair_code, pairUrl: e.pair_url, expiresAt: e.expires_at }, c.name)
    }
  }
})

test('poll replies', () => {
  for (const c of v.poll) {
    if (c.expect === 'parse_error') assert.throws(() => parsePoll(text(c)), PairingParseError, c.name)
    else {
      const { state, ...rest } = c.expect as { state: string } & Record<string, string>
      assert.deepEqual(parsePoll(text(c)), { state, ...rest }, c.name)
    }
  }
})

function scripted(polls: { http: number; body: unknown }[]) {
  let made = 0
  const t: Transport = {
    begin: async () => ({ http: 200, text: JSON.stringify({ pair_code: 'abc123', pair_url: 'https://auth.example.com/pair/abc123', expires_at: 0 }) }),
    poll: async () => {
      const p = polls[Math.min(made, polls.length - 1)]!
      made++
      return { http: p.http, text: JSON.stringify(p.body) } as Reply
    },
  }
  return { t, made: () => made }
}

test('driver sessions', async () => {
  for (const c of v.driver) {
    const s = scripted(c.polls)
    const logs: string[] = []
    const orig = { log: console.log, error: console.error, warn: console.warn }
    console.log = console.error = console.warn = (...a: unknown[]) => { logs.push(a.join(' ')) }
    let outcome: { status: string; message?: string }
    try {
      const done = await runPairing(s.t, 'a.example', { sleep: async () => {} })
      outcome = { status: 'ok' }
      assert.ok(done.token.length > 0)
    } catch (e) {
      assert.ok(e instanceof PairingError, c.name)
      outcome = { status: e.kind, message: e.message }
    } finally { Object.assign(console, orig) }
    assert.equal(outcome.status, c.expect.status, c.name)
    assert.equal(s.made(), c.expect.polls_made, `${c.name}: polls made`)
    if (c.expect.message !== undefined) assert.equal(outcome.message, c.expect.message, c.name)
    assert.equal(logs.join(''), '', 'nothing is logged')
  }
})

test('the token and pair code never appear in an error', async () => {
  const s = scripted([{ http: 200, body: { status: 'weird', token: 'SECRETTOKEN-0123' } }])
  await assert.rejects(runPairing(s.t, 'a.example', { sleep: async () => {} }), (e: Error) => {
    assert.ok(!e.message.includes('SECRETTOKEN') && !e.message.includes('abc123'))
    return true
  })
})

test('an expired pairing gives up, and cancel stops before polling', async () => {
  const s = scripted([{ http: 200, body: { status: 'pending' } }])
  s.t.begin = async () => ({ http: 200, text: JSON.stringify({ pair_code: 'c', pair_url: 'https://a.example/p', expires_at: 100 }) })
  await assert.rejects(runPairing(s.t, 'a.example', { sleep: async () => {}, now: () => 101 }), (e: PairingError) => e.kind === 'timeout')
  assert.equal(s.made(), 0)
  await assert.rejects(runPairing(s.t, 'a.example', { sleep: async () => {}, cancel: () => true }), (e: PairingError) => e.kind === 'cancelled')
})

test('without an expiry it stops after 360 pending polls', async () => {
  const s = scripted([{ http: 200, body: { status: 'pending' } }])
  await assert.rejects(runPairing(s.t, 'a.example', { sleep: async () => {} }), (e: PairingError) => e.kind === 'timeout')
  assert.equal(s.made(), 360)
})

test('network failures are retried and begin failure is reported', async () => {
  let n = 0
  const t: Transport = {
    begin: async () => ({ http: 200, text: JSON.stringify({ pair_code: 'c', pair_url: 'https://a.example/p' }) }),
    poll: async () => {
      if (n++ < 2) throw new Error('boom')
      return { http: 200, text: JSON.stringify({ status: 'complete', token: 'T', handle: 'a.b', did: 'did:plc:x', service: 'https://a.example' }) }
    },
  }
  const done = await runPairing(t, 'a.b', { sleep: async () => {} })
  assert.equal(done.did, 'did:plc:x')
  await assert.rejects(runPairing({ ...t, begin: async () => ({ http: 500, text: '' }) }, 'a.b', { sleep: async () => {} }), (e: PairingError) => e.kind === 'begin')
})
