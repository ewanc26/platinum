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

test('you can delete your own post, and only yours', async () => {
  const calls: string[] = []
  const agent = { accountDid: 'did:plc:me', deletePost: async (u: string) => { calls.push(u) } } as unknown as Agent
  const api = new DomainApi()
  assert.equal(await api.deletePost(agent, 'at://did:plc:me/app.bsky.feed.post/3k'), 'deleted')
  assert.equal(await api.deletePost(agent, 'at://did:plc:other/app.bsky.feed.post/3k'), 'not_yours')
  assert.equal(await api.deletePost(agent, 'at://did:plc:me.evil/app.bsky.feed.post/3k'), 'not_yours')
  assert.equal(await api.deletePost(agent, 'not a uri'), 'not_yours')
  assert.deepEqual(calls, ['at://did:plc:me/app.bsky.feed.post/3k'], 'nothing else reaches the PDS')
  assert.equal(await api.deletePost({ deletePost: async () => { throw new Error('no') } } as unknown as Agent, 'at://did:plc:me/app.bsky.feed.post/3k'), 'not_yours', 'no account means no delete')
})
