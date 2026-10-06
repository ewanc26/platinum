import assert from 'node:assert/strict'
import test from 'node:test'
import type { Agent } from '@atproto/api'
import { DomainApi, flattenThread, MAX_THREAD_POSTS } from '../src/domain/api.js'

const pv = (id: string) => ({
  uri: `at://did:plc:a/app.bsky.feed.post/${id}`,
  cid: `cid${id}`,
  author: { did: 'did:plc:a', handle: 'a.test' },
  record: { text: `post ${id}`, createdAt: '2026-10-05T00:00:00Z' },
})
const node = (id: string, extra: Record<string, unknown> = {}) => ({ $type: 'app.bsky.feed.defs#threadViewPost', post: pv(id), ...extra })

test('a thread flattens to ancestors, the post, then replies depth-first', () => {
  const tree = node('c', {
    parent: node('b', { parent: node('a') }),
    replies: [node('d', { replies: [node('e')] }), { $type: 'app.bsky.feed.defs#notFoundPost', uri: 'x', notFound: true }, node('f')],
  })
  const t = flattenThread(tree)
  assert.deepEqual(t.posts.map(p => [p.text, p.depth]), [
    ['post a', -2], ['post b', -1], ['post c', 0], ['post d', 1], ['post e', 2], ['post f', 1],
  ])
  assert.equal(t.truncated, false)
  assert.ok(!JSON.stringify(t).includes('notFound'), 'missing posts are skipped')
})

test('a large thread is bounded and says so', () => {
  const replies = Array.from({ length: 100 }, (_, i) => node(`r${i}`))
  const t = flattenThread(node('root', { replies }))
  assert.equal(t.posts.length, MAX_THREAD_POSTS)
  assert.equal(t.truncated, true)
})

test('a deep chain stops at the depth limit', () => {
  let n: Record<string, unknown> = node('leaf')
  for (let i = 0; i < 12; i++) n = node(`n${i}`, { replies: [n] })
  const t = flattenThread(node('top', { replies: [n] }))
  assert.ok(t.posts.every(p => p.depth <= 6))
  assert.equal(t.truncated, true)
})

test('nothing usable gives an empty thread', () => {
  assert.deepEqual(flattenThread({ $type: 'app.bsky.feed.defs#blockedPost' }), { posts: [], truncated: false })
  assert.deepEqual(flattenThread(null), { posts: [], truncated: false })
})

function postingAgent(parentRecord: unknown, exists = true) {
  const calls: unknown[] = []
  const agent = {
    getPosts: async () => ({ data: { posts: exists ? [{ uri: 'at://p/parent', cid: 'cp', record: parentRecord }] : [] } }),
    post: async (r: unknown) => { calls.push(r); return { uri: 'at://p/new', cid: 'cn' } },
  }
  return { agent: agent as unknown as Agent, calls }
}

test('a reply to a top-level post uses it as both root and parent', async () => {
  const { agent, calls } = postingAgent({ text: 'hi' })
  await new DomainApi().post(agent, 'yes', { uri: 'at://p/parent', cid: 'cp' })
  assert.deepEqual(calls, [{ text: 'yes', reply: { root: { uri: 'at://p/parent', cid: 'cp' }, parent: { uri: 'at://p/parent', cid: 'cp' } } }])
})

test('a reply to a reply keeps the thread root', async () => {
  const { agent, calls } = postingAgent({ text: 'hi', reply: { root: { uri: 'at://p/root', cid: 'cr' }, parent: { uri: 'x', cid: 'y' } } })
  await new DomainApi().post(agent, 'yes', { uri: 'at://p/parent', cid: 'cp' })
  assert.deepEqual((calls[0] as { reply: unknown }).reply, { root: { uri: 'at://p/root', cid: 'cr' }, parent: { uri: 'at://p/parent', cid: 'cp' } })
})

test('a reply to a vanished post writes nothing', async () => {
  const { agent, calls } = postingAgent({}, false)
  assert.equal(await new DomainApi().post(agent, 'yes', { uri: 'at://p/parent', cid: 'cp' }), undefined)
  assert.deepEqual(calls, [])
})

test('a plain post is not a reply', async () => {
  const { agent, calls } = postingAgent({})
  await new DomainApi().post(agent, 'hello')
  assert.deepEqual(calls, [{ text: 'hello' }])
})
