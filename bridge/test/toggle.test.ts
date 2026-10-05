import assert from 'node:assert/strict'
import test from 'node:test'
import type { Agent } from '@atproto/api'
import { DomainApi, normalizeTimelinePost, validPostRef } from '../src/domain/api.js'

const URI = 'at://did:plc:author/app.bsky.feed.post/3k2a'
const CID = 'bafyreiexample'

function fakeAgent(viewer: { like?: string; repost?: string }, counts = { likeCount: 4, repostCount: 2 }, exists = true) {
  const calls: string[] = []
  const agent = {
    getPosts: async () => ({ data: { posts: exists ? [{ uri: URI, cid: CID, viewer, ...counts }] : [] } }),
    like: async (u: string, c: string) => { calls.push(`like ${u} ${c}`); return { uri: 'at://me/like/1', cid: 'x' } },
    deleteLike: async (u: string) => { calls.push(`unlike ${u}`) },
    repost: async (u: string, c: string) => { calls.push(`repost ${u} ${c}`); return { uri: 'at://me/repost/1', cid: 'x' } },
    deleteRepost: async (u: string) => { calls.push(`unrepost ${u}`) },
  }
  return { agent: agent as unknown as Agent, calls }
}

test('like when not liked creates one like and counts it', async () => {
  const { agent, calls } = fakeAgent({})
  const r = await new DomainApi().toggle(agent, 'like', { uri: URI, cid: CID }, true)
  assert.deepEqual(r, { uri: URI, on: true, count: 5 })
  assert.deepEqual(calls, [`like ${URI} ${CID}`])
})

test('like when already liked is a no-op', async () => {
  const { agent, calls } = fakeAgent({ like: 'at://me/like/9' })
  const r = await new DomainApi().toggle(agent, 'like', { uri: URI, cid: CID }, true)
  assert.deepEqual(r, { uri: URI, on: true, count: 4 })
  assert.deepEqual(calls, [])
})

test('unlike deletes the existing like record the AppView reported', async () => {
  const { agent, calls } = fakeAgent({ like: 'at://me/like/9' })
  const r = await new DomainApi().toggle(agent, 'like', { uri: URI, cid: CID }, false)
  assert.deepEqual(r, { uri: URI, on: false, count: 3 })
  assert.deepEqual(calls, ['unlike at://me/like/9'])
})

test('unrepost when not reposted is a no-op, and the count never goes negative', async () => {
  const { agent, calls } = fakeAgent({}, { likeCount: 0, repostCount: 0 })
  const r = await new DomainApi().toggle(agent, 'repost', { uri: URI, cid: CID }, false)
  assert.deepEqual(r, { uri: URI, on: false, count: 0 })
  assert.deepEqual(calls, [])
})

test('repost and undo use the repost calls', async () => {
  const a = fakeAgent({})
  await new DomainApi().toggle(a.agent, 'repost', { uri: URI, cid: CID }, true)
  const b = fakeAgent({ repost: 'at://me/repost/2' })
  await new DomainApi().toggle(b.agent, 'repost', { uri: URI, cid: CID }, false)
  assert.deepEqual([...a.calls, ...b.calls], [`repost ${URI} ${CID}`, 'unrepost at://me/repost/2'])
})

test('a vanished post reports off and writes nothing', async () => {
  const { agent, calls } = fakeAgent({}, undefined, false)
  const r = await new DomainApi().toggle(agent, 'like', { uri: URI, cid: CID }, true)
  assert.equal(r.on, false)
  assert.deepEqual(calls, [])
})

test('post references are validated', () => {
  assert.deepEqual(validPostRef(URI, CID), { uri: URI, cid: CID })
  for (const [u, c] of [
    ['https://bsky.app/x', CID],
    ['at://did:plc:a/app.bsky.feed.like/1', CID],
    [URI, ''],
    [URI, 'not a cid!'],
    [URI + 'x'.repeat(600), CID],
    [42, CID],
    [URI, null],
  ]) assert.equal(validPostRef(u, c), undefined, `${u} ${c}`)
})

test('timeline items carry viewer state as booleans only', () => {
  const p = normalizeTimelinePost({ post: { uri: URI, cid: CID, author: { did: 'did:plc:a' }, record: {}, viewer: { like: 'at://me/like/1' } } })
  assert.equal(p.liked, true)
  assert.equal(p.reposted, false)
  assert.ok(!JSON.stringify(p).includes('at://me/like/1'), 'record URIs stay on the bridge')
})
