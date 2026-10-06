import assert from 'node:assert/strict'
import test from 'node:test'
import type { Agent } from '@atproto/api'
import { DomainApi } from '../src/domain/api.js'

const URI = 'at://did:plc:a/app.bsky.feed.post/1'

test('likes are reduced to author fields and keep the cursor', async () => {
  const seen: unknown[] = []
  const agent = {
    getLikes: async (a: unknown) => { seen.push(a); return { data: { cursor: 'c', likes: [{ createdAt: 'x', actor: { did: 'did:plc:x', handle: 'x.test', displayName: 'X', viewer: { following: 'at://leak' }, description: 'private' } }] } } },
  } as unknown as Agent
  const r = await new DomainApi().engagement(agent, 'likes', URI, 20, 'cur')
  assert.deepEqual(r, { actors: [{ did: 'did:plc:x', handle: 'x.test', displayName: 'X' }], cursor: 'c' })
  assert.deepEqual(seen, [{ uri: URI, limit: 20, cursor: 'cur' }])
})

test('reposts use the reposted-by list', async () => {
  const agent = { getRepostedBy: async () => ({ data: { repostedBy: [{ did: 'did:plc:y' }] } }) } as unknown as Agent
  const r = await new DomainApi().engagement(agent, 'reposts', URI, 20)
  assert.deepEqual(r?.actors.map(a => a.did), ['did:plc:y'])
  assert.equal(r?.cursor, undefined)
})

test('a vanished post is undefined, and other failures propagate', async () => {
  const gone = { getLikes: async () => { throw Object.assign(new Error('x'), { status: 400 }) } } as unknown as Agent
  assert.equal(await new DomainApi().engagement(gone, 'likes', URI, 20), undefined)
  const down = { getLikes: async () => { throw Object.assign(new Error('x'), { status: 500 }) } } as unknown as Agent
  await assert.rejects(new DomainApi().engagement(down, 'likes', URI, 20))
})
