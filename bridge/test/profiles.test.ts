import assert from 'node:assert/strict'
import test from 'node:test'
import type { Agent } from '@atproto/api'
import { DomainApi, validActor } from '../src/domain/api.js'

const post = { uri: 'at://did:plc:b/app.bsky.feed.post/1', cid: 'cid1', author: { did: 'did:plc:b', handle: 'b.test' }, record: { text: 'pinned!' } }

function agentWith(profile: Record<string, unknown>, extra: Record<string, unknown> = {}) {
  const calls: string[] = []
  const agent = {
    accountDid: 'did:plc:me',
    getProfile: async (a: { actor: string }) => { calls.push(`profile ${a.actor}`); return { data: { did: 'did:plc:b', handle: 'b.test', ...profile } } },
    getPosts: async () => ({ data: { posts: [post] } }),
    follow: async (d: string) => { calls.push(`follow ${d}`); return { uri: 'at://me/follow/1', cid: 'x' } },
    deleteFollow: async (u: string) => { calls.push(`unfollow ${u}`) },
    ...extra,
  }
  return { agent: agent as unknown as Agent, calls }
}

test('actors are handles or DIDs and nothing else', () => {
  assert.equal(validActor('Alice.Example.com'), 'alice.example.com')
  assert.equal(validActor('did:plc:abc123'), 'did:plc:abc123')
  for (const bad of ['', 'localhost', 'https://x.com', 'a b.com', '../x', 'x'.repeat(300), 42, null, 'did:', '.com', 'a..com']) {
    assert.equal(validActor(bad), undefined, String(bad))
  }
})

test("someone else's profile carries follow state and the pinned post", async () => {
  const { agent, calls } = agentWith({ viewer: { following: 'at://me/follow/9' }, pinnedPost: { uri: post.uri, cid: 'cid1' } })
  const p = await new DomainApi().profile(agent, 'b.test')
  assert.equal(p?.following, true)
  assert.equal(p?.followedBy, false)
  assert.equal(p?.pinned?.text, 'pinned!')
  assert.deepEqual(calls, ['profile b.test'])
  assert.ok(!JSON.stringify(p).includes('at://me/follow'), 'record URIs stay on the bridge')
})

test('your own profile has no follow state', async () => {
  const { agent, calls } = agentWith({})
  const p = await new DomainApi().profile(agent)
  assert.equal(p?.following, undefined)
  assert.deepEqual(calls, ['profile did:plc:me'])
})

test('an unknown actor is undefined, not an error', async () => {
  const { agent } = agentWith({}, { getProfile: async () => { throw Object.assign(new Error('x'), { status: 400 }) } })
  assert.equal(await new DomainApi().profile(agent, 'nobody.test'), undefined)
})

test('follow is idempotent in both directions', async () => {
  const a = agentWith({})
  assert.deepEqual(await new DomainApi().follow(a.agent, 'did:plc:b', true), { did: 'did:plc:b', on: true })
  assert.deepEqual(a.calls, ['profile did:plc:b', 'follow did:plc:b'])

  const b = agentWith({ viewer: { following: 'at://me/follow/9' } })
  await new DomainApi().follow(b.agent, 'did:plc:b', true)
  assert.deepEqual(b.calls, ['profile did:plc:b'])

  const c = agentWith({ viewer: { following: 'at://me/follow/9' } })
  await new DomainApi().follow(c.agent, 'did:plc:b', false)
  assert.deepEqual(c.calls, ['profile did:plc:b', 'unfollow at://me/follow/9'])

  const d = agentWith({})
  await new DomainApi().follow(d.agent, 'did:plc:b', false)
  assert.deepEqual(d.calls, ['profile did:plc:b'])
})

test('follow lists and the author feed are reduced to the contract', async () => {
  const agent = {
    getFollows: async () => ({ data: { cursor: 'c1', follows: [{ did: 'did:plc:x', handle: 'x.test', displayName: 'X', description: 'secret', viewer: { following: 'at://leak' } }] } }),
    getFollowers: async () => ({ data: { follows: [], followers: [{ did: 'did:plc:y' }] } }),
    getAuthorFeed: async (a: { filter?: string }) => ({ data: { cursor: 'c2', feed: [{ post: { ...post, filter: a.filter } }] } }),
  } as unknown as Agent
  const api = new DomainApi()
  const f = await api.actors(agent, 'follows', 'b.test', 20)
  assert.deepEqual(f, { actors: [{ did: 'did:plc:x', handle: 'x.test', displayName: 'X' }], cursor: 'c1' })
  assert.deepEqual((await api.actors(agent, 'followers', 'b.test', 20)).actors, [{ did: 'did:plc:y', handle: undefined, displayName: undefined }])
  const feed = await api.authorFeed(agent, 'b.test', 20)
  assert.equal(feed.posts.length, 1)
  assert.equal(feed.cursor, 'c2')
})
