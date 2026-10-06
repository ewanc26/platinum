import assert from 'node:assert/strict'
import test from 'node:test'
import type { Agent } from '@atproto/api'
import { DomainApi } from '../src/domain/api.js'
import { MutedWords, validMutedWord } from '../src/domain/muted.js'

const mk = (text: string, did = 'did:plc:b', following?: string) => ({
  post: { uri: `at://${did}/app.bsky.feed.post/${text.length}`, cid: 'c', author: { did, handle: 'b.test', viewer: { following } }, record: { text } },
})

function agentWith(mutedWords: unknown[], feed: unknown[]) {
  const calls: string[] = []
  const agent = {
    accountDid: 'did:plc:me',
    getPreferences: async () => { calls.push('prefs'); return { moderationPrefs: { mutedWords } } },
    getTimeline: async () => ({ data: { feed, cursor: 'next' } }),
    addMutedWord: async (w: { value: string }) => { calls.push(`add ${w.value}`) },
    removeMutedWords: async (w: { value: string }[]) => { calls.push(`remove ${w.map(x => x.value).join(',')}`) },
  }
  return { agent: agent as unknown as Agent, calls }
}

test('a muted word hides the post, and the cursor still moves on', async () => {
  const { agent } = agentWith([{ value: 'spoilers', targets: ['content'], actorTarget: 'all' }], [mk('no Spoilers here'), mk('fine')])
  const t = await new DomainApi().timeline(agent, 20)
  assert.deepEqual(t.posts.map(p => p.text), ['fine'])
  assert.equal(t.cursor, 'next')
})

test('exclude-following leaves followed accounts alone', async () => {
  const w = [{ value: 'spoilers', targets: ['content'], actorTarget: 'exclude-following' }]
  const { agent } = agentWith(w, [mk('spoilers', 'did:plc:b', 'at://f/1'), mk('spoilers!', 'did:plc:c')])
  const t = await new DomainApi().timeline(agent, 20)
  assert.equal(t.posts.length, 1)
  assert.equal(t.posts[0]?.author.did, 'did:plc:b')
})

test('preferences are cached for a minute and read again after a change', async () => {
  const { agent, calls } = agentWith([], [mk('x')])
  const api = new DomainApi()
  await api.timeline(agent, 20)
  await api.timeline(agent, 20)
  assert.equal(calls.filter(c => c === 'prefs').length, 1)
  await api.mutedWords.set(agent, 'x', true)
  await api.timeline(agent, 20)
  assert.ok(calls.filter(c => c === 'prefs').length >= 3)
})

test('adding and removing a word is idempotent and case-insensitive', async () => {
  const none = agentWith([], [])
  const mw = new MutedWords()
  assert.deepEqual(await mw.set(none.agent, 'Foo', true), { value: 'Foo', on: true })
  assert.deepEqual(none.calls.filter(c => c !== 'prefs'), ['add Foo'])
  await mw.set(none.agent, 'Foo', false)
  assert.deepEqual(none.calls.filter(c => c !== 'prefs'), ['add Foo'], 'removing what is not there does nothing')

  const has = agentWith([{ value: 'foo', targets: ['content'], actorTarget: 'all' }], [])
  await mw.set(has.agent, 'FOO', true)
  assert.deepEqual(has.calls.filter(c => c !== 'prefs'), [], 'adding what is there does nothing')
  await mw.set(has.agent, 'FOO', false)
  assert.deepEqual(has.calls.filter(c => c !== 'prefs'), ['remove foo'])
})

test('a preferences failure shows posts rather than none', async () => {
  const agent = { accountDid: 'did:plc:me', getPreferences: async () => { throw new Error('down') }, getTimeline: async () => ({ data: { feed: [mk('hi')] } }) } as unknown as Agent
  assert.equal((await new DomainApi().timeline(agent, 20)).posts.length, 1)
})

test('the list is bounded and only names the targets the Mac understands', async () => {
  const { agent } = agentWith([{ value: 'a', targets: ['content', 'weird'], actorTarget: 'all' }], [])
  assert.deepEqual(await new MutedWords().list(agent), [{ value: 'a', targets: ['content'] }])
})

test('a word is 1 to 100 printable characters', () => {
  assert.equal(validMutedWord('  hi '), 'hi')
  for (const bad of ['', '   ', 'x'.repeat(101), 'a\nb', 7, null]) assert.equal(validMutedWord(bad), undefined, String(bad))
})
