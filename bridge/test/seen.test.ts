import assert from 'node:assert/strict'
import test from 'node:test'
import type { Agent } from '@atproto/api'
import { DomainApi, validSeenAt } from '../src/domain/api.js'

const now = new Date('2026-10-05T12:00:00.000Z')

test('seenAt accepts the timestamps the bridge hands out', () => {
  assert.equal(validSeenAt('2026-10-05T00:01:00.000Z', now), '2026-10-05T00:01:00.000Z')
  assert.equal(validSeenAt('2026-10-05T01:01:00+01:00', now), '2026-10-05T00:01:00.000Z')
})

test('seenAt in the future is clamped to now', () => {
  assert.equal(validSeenAt('2030-01-01T00:00:00Z', now), now.toISOString())
})

test('seenAt rejects anything that is not an ISO timestamp', () => {
  for (const bad of [undefined, 42, '', 'yesterday', '2026-10-05', '2026-10-05 00:00:00Z', '2026-13-45T99:99:99Z', 'x'.repeat(65)]) {
    assert.equal(validSeenAt(bad, now), undefined, String(bad))
  }
})

test('markSeen passes the time through to updateSeen', async () => {
  const calls: unknown[] = []
  const agent = { updateSeenNotifications: async (t?: string) => { calls.push(t); return {} } } as unknown as Agent
  assert.deepEqual(await new DomainApi().markSeen(agent, '2026-10-05T00:01:00.000Z'), { seenAt: '2026-10-05T00:01:00.000Z' })
  assert.deepEqual(calls, ['2026-10-05T00:01:00.000Z'])
})
