import { test } from 'node:test'
import assert from 'node:assert/strict'
import type { Agent } from '@atproto/api'
import {
  DomainApi,
  isMessageView,
  normalizeConversation,
  normalizeMessage,
  validConvoId,
} from '../src/domain/api.js'

const viewer = 'did:plc:viewer'
const bob = { did: 'did:plc:bob', handle: 'bob.example', displayName: 'Bob', avatar: 'https://example.invalid/x' }

test('a conversation names the other person, with their handle and a preview', () => {
  const convo = normalizeConversation({
    id: 'convo-1',
    members: [{ did: viewer, handle: 'me.example' }, bob],
    lastMessage: { text: 'See you Saturday.' },
    unreadCount: 2,
  }, viewer)

  assert.ok(convo)
  assert.equal(convo.name, 'Bob')
  assert.equal(convo.handle, '@bob.example')
  assert.equal(convo.group, false)
  assert.equal(convo.preview, 'See you Saturday.')
  assert.equal(convo.unread, 2)
  assert.equal('extra' in convo, false)
})

test('a group is named as one, without a handle, and a missing message gives no preview', () => {
  const convo = normalizeConversation({
    id: 'convo-2',
    members: [{ did: viewer }, bob, { did: 'did:plc:cat', handle: 'cat.example' }],
  }, viewer)

  assert.ok(convo)
  assert.equal(convo.name, 'Group')
  assert.equal(convo.group, true)
  assert.equal(convo.handle, undefined)
  assert.equal(convo.preview, '')
  assert.equal(convo.unread, 0)
})

test('a conversation with nobody but the viewer in it is left out', () => {
  assert.equal(normalizeConversation({ id: 'solo', members: [{ did: viewer }] }, viewer), undefined)
})

test('the conversation list goes through the chat proxy, and skips empty conversations', async () => {
  const proxied: Array<[string, string]> = []
  const chat = {
    chat: {
      bsky: {
        convo: {
          listConvos: async (params: { limit: number; cursor?: string }) => {
            assert.equal(params.limit, 20)
            return {
              data: {
                cursor: 'next-page',
                convos: [
                  { id: 'convo-1', members: [{ did: viewer }, bob], unreadCount: 1 },
                  { id: 'solo', members: [{ did: viewer }] },
                ],
              },
            }
          },
        },
      },
    },
  }
  const agent = {
    accountDid: viewer,
    withProxy: (service: string, did: string) => {
      proxied.push([service, did])
      return chat
    },
  } as unknown as Agent

  const result = await new DomainApi().conversations(agent, 20)

  assert.deepEqual(proxied, [['bsky_chat', 'did:web:api.bsky.chat']])
  assert.equal(result.conversations.length, 1)
  assert.equal(result.conversations[0].id, 'convo-1')
  assert.equal(result.cursor, 'next-page')
})

test('messages come back in the server order, with the viewer\'s own marked and deleted ones left out', async () => {
  const chat = {
    chat: {
      bsky: {
        convo: {
          getMessages: async (params: { convoId: string }) => {
            assert.equal(params.convoId, 'convo-1')
            return {
              data: {
                messages: [
                  { id: 'm3', text: 'See you', sender: { did: bob.did }, sentAt: '2026-10-01T10:00:00.000Z' },
                  { $type: 'chat.bsky.convo.defs#deletedMessageView', id: 'm2', sender: { did: viewer } },
                  { id: 'm1', text: 'Hello', sender: { did: viewer }, sentAt: '2026-10-01T09:00:00.000Z' },
                ],
              },
            }
          },
        },
      },
    },
  }
  const agent = { accountDid: viewer, withProxy: () => chat } as unknown as Agent

  const result = await new DomainApi().conversationMessages(agent, 'convo-1', 20)

  assert.equal(result.messages.length, 2)
  assert.equal(result.messages[0].mine, false)
  assert.equal(result.messages[1].mine, true)
  assert.equal(result.messages[1].text, 'Hello')
})

test('a deleted message is not a message view, and a live one is', () => {
  assert.equal(isMessageView({ $type: 'chat.bsky.convo.defs#deletedMessageView', id: 'x' }), false)
  assert.equal(isMessageView({ id: 'x', text: 'hi', sender: { did: viewer }, sentAt: 'now' }), true)
})

test('text is clipped by code point, not by UTF-16 unit', () => {
  const emoji = '🙂'.repeat(1200)
  const message = normalizeMessage({ id: 'm', text: emoji, sender: { did: viewer }, sentAt: 'now' }, viewer)

  assert.equal(Array.from(message.text).length, 1000)
  assert.equal(Array.from(message.text).every(c => c === '🙂'), true)
})

test('a conversation id is bounded and limited to the characters the chat service uses', () => {
  assert.equal(validConvoId('3kbmkjmx7ua2e'), '3kbmkjmx7ua2e')
  assert.equal(validConvoId('convo.one:two-3_4'), 'convo.one:two-3_4')
  assert.equal(validConvoId(''), undefined)
  assert.equal(validConvoId('a'.repeat(129)), undefined)
  assert.equal(validConvoId('../../etc'), undefined)
  assert.equal(validConvoId(undefined), undefined)
})
