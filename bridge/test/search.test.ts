import assert from 'node:assert/strict'
import test from 'node:test'
import type { Agent } from '@atproto/api'
import { DomainApi, validCollectionUri, validQuery } from '../src/domain/api.js'

const post = { uri: 'at://did:plc:a/app.bsky.feed.post/1', cid: 'c1', author: { did: 'did:plc:a' }, record: { text: 'hit' } }

test('queries are bounded and clean', () => {
  assert.equal(validQuery('  hello  '), 'hello')
  for (const bad of ['', '   ', 'x'.repeat(101), 'a\nb', 'a\u0000b', 5, null, undefined]) assert.equal(validQuery(bad), undefined, String(bad))
  assert.equal(validQuery('😀'.repeat(100)), '😀'.repeat(100))
})

test('feed and list URIs must be of their own collection', () => {
  const feed = 'at://did:plc:a/app.bsky.feed.generator/whats-hot'
  const list = 'at://did:plc:a/app.bsky.graph.list/3k'
  assert.equal(validCollectionUri(feed, 'app.bsky.feed.generator'), feed)
  assert.equal(validCollectionUri(list, 'app.bsky.graph.list'), list)
  assert.equal(validCollectionUri(list, 'app.bsky.feed.generator'), undefined)
  assert.equal(validCollectionUri('https://x.test/y', 'app.bsky.graph.list'), undefined)
  assert.equal(validCollectionUri(feed + '/x', 'app.bsky.feed.generator'), undefined)
  assert.equal(validCollectionUri(feed + 'x'.repeat(600), 'app.bsky.feed.generator'), undefined)
})

test('actor and post search are reduced to the contracts', async () => {
  const agent = {
    searchActors: async (a: unknown) => ({ data: { cursor: 'c', actors: [{ did: 'did:plc:x', handle: 'x.test', description: 'secret', viewer: { muted: true } }] }, _a: a }),
    app: { bsky: { feed: { searchPosts: async () => ({ data: { cursor: 'p', posts: [post] } }) } } },
  } as unknown as Agent
  const api = new DomainApi()
  assert.deepEqual(await api.searchActors(agent, 'x', 20, 'cur'), { actors: [{ did: 'did:plc:x', handle: 'x.test', displayName: undefined }], cursor: 'c' })
  const posts = await api.searchPosts(agent, 'hit', 20)
  assert.equal(posts.posts[0]?.text, 'hit')
  assert.equal(posts.cursor, 'p')
})

test('saved feeds are the custom ones, named; none means no request', async () => {
  const asked: unknown[] = []
  const agent = {
    getPreferences: async () => ({ savedFeeds: [{ type: 'timeline', value: 'following' }, { type: 'feed', value: 'at://did:plc:a/app.bsky.feed.generator/g' }, { type: 'list', value: 'at://l' }] }),
    app: { bsky: { feed: { getFeedGenerators: async (a: unknown) => { asked.push(a); return { data: { feeds: [{ uri: 'at://did:plc:a/app.bsky.feed.generator/g', displayName: 'Great', creator: { did: 'x' } }] } } } } } },
  } as unknown as Agent
  assert.deepEqual(await new DomainApi().savedFeeds(agent), { items: [{ uri: 'at://did:plc:a/app.bsky.feed.generator/g', name: 'Great' }] })
  assert.deepEqual(asked, [{ feeds: ['at://did:plc:a/app.bsky.feed.generator/g'] }])
  const none = { getPreferences: async () => ({ savedFeeds: [] }) } as unknown as Agent
  assert.deepEqual(await new DomainApi().savedFeeds(none), { items: [] })
})

test('a feed returns timeline posts and a missing one is undefined', async () => {
  const ok = { app: { bsky: { feed: { getFeed: async () => ({ data: { cursor: 'f', feed: [{ post }] } }) } } } } as unknown as Agent
  const r = await new DomainApi().feed(ok, 'at://did:plc:a/app.bsky.feed.generator/g', 20)
  assert.equal(r?.posts.length, 1)
  const gone = { app: { bsky: { feed: { getFeed: async () => { throw Object.assign(new Error('x'), { status: 400 }) } } } } } as unknown as Agent
  assert.equal(await new DomainApi().feed(gone, 'at://did:plc:a/app.bsky.feed.generator/g', 20), undefined)
})

test('lists and their members are reduced to names and accounts', async () => {
  const agent = {
    accountDid: 'did:plc:me',
    app: { bsky: { graph: {
      getLists: async () => ({ data: { lists: [{ uri: 'at://did:plc:me/app.bsky.graph.list/1', name: 'Friends', purpose: 'x', creator: { did: 'y' } }] } }),
      getList: async () => ({ data: { cursor: 'm', items: [{ subject: { did: 'did:plc:b', handle: 'b.test', description: 'secret' } }] } }),
    } } },
  } as unknown as Agent
  const api = new DomainApi()
  assert.deepEqual(await api.lists(agent), { items: [{ uri: 'at://did:plc:me/app.bsky.graph.list/1', name: 'Friends' }] })
  assert.deepEqual(await api.listMembers(agent, 'at://did:plc:me/app.bsky.graph.list/1', 20), { actors: [{ did: 'did:plc:b', handle: 'b.test', displayName: undefined }], cursor: 'm' })
})
