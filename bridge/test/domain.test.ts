import assert from 'node:assert/strict'
import test from 'node:test'
import type { Agent } from '@atproto/api'
import { DomainApi } from '../src/domain/api.js'

test('timeline responses are reduced to the bridge feed contract', async () => {
  const calls: unknown[] = []
  const agent = {
    accountDid: 'did:plc:example',
    getTimeline: async (input: unknown) => {
      calls.push(input)
      return {
        data: {
          cursor: 'next-cursor',
          feed: [
            {
              post: undefined,
            },
            {
              uri: 'at://did:plc:author/app.bsky.feed.post/1',
              cid: 'bafyexample',
              author: {
                did: 'did:plc:author',
                handle: 'author.example',
                displayName: 'An Author',
              },
              record: {
                text: 'hello from Bluesky',
                createdAt: '2026-10-05T00:00:00.000Z',
                extra: 'not exposed',
              },
              likeCount: 4,
              repostCount: 2,
              replyCount: 1,
              quoteCount: 3,
              embed: { huge: true },
            },
          ],
        },
      }
    },
  } as unknown as Agent

  const result = await new DomainApi().timeline(agent, 20, 'old-cursor')

  assert.deepEqual(calls, [{ limit: 20, cursor: 'old-cursor' }])
  assert.equal(result.cursor, 'next-cursor')
  assert.equal(result.posts.length, 2)
  assert.equal(result.posts[0].text, '')
  assert.equal(result.posts[1].author.handle, 'author.example')
  assert.equal(result.posts[1].text, 'hello from Bluesky')
  assert.equal(result.posts[1].likeCount, 4)
  assert.equal(result.posts[1].quoteCount, 3)
  assert.equal('embed' in result.posts[1], false)
})

test('notification responses are reduced to stable fields', async () => {
  const agent = {
    accountDid: 'did:plc:example',
    listNotifications: async () => ({
      data: {
        cursor: undefined,
        notifications: [
          {
            uri: 'at://did:plc:author/app.bsky.feed.post/2',
            cid: 'bafyexample2',
            author: {
              did: 'did:plc:author',
              handle: 'author.example',
              displayName: 'An Author',
              avatar: 'https://example.invalid/avatar',
            },
            reason: 'like',
            indexedAt: '2026-10-05T00:01:00.000Z',
            isRead: true,
            extra: 'not exposed',
          },
        ],
      },
    }),
  } as unknown as Agent

  const result = await new DomainApi().notifications(agent, 20)

  assert.equal(result.notifications.length, 1)
  assert.equal(result.notifications[0].reason, 'like')
  assert.equal(result.notifications[0].isRead, true)
  assert.equal('extra' in result.notifications[0], false)
})
