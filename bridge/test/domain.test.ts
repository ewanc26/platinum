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
              post: {
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
            },
            {
              post: {
                uri: 'at://did:plc:author/app.bsky.feed.post/2',
                cid: 'bafyexample2',
                author: {
                  did: 'did:plc:author',
                },
                record: {},
              },
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
  assert.equal(result.posts[0].author.handle, 'author.example')
  assert.equal(result.posts[0].text, 'hello from Bluesky')
  assert.equal(result.posts[0].likeCount, 4)
  assert.equal(result.posts[0].quoteCount, 3)
  assert.equal(result.posts[1].text, '')
  assert.equal(result.posts[1].likeCount, 0)
  assert.equal('embed' in result.posts[0], false)
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


test('normalizer clips Unicode text by code point', async () => {
  const agent = {
    accountDid: 'did:plc:example',
    getTimeline: async () => ({
      data: {
        cursor: undefined,
        feed: [
          {
            post: {
              uri: 'at://did:plc:author/app.bsky.feed.post/3',
              cid: 'bafyexample3',
              author: {
                did: 'did:plc:author',
                handle: 'author.example',
              },
              record: {
                text: '😀'.repeat(301),
                createdAt: '2026-10-05T00:00:00.000Z',
              },
            },
          },
        ],
      },
    }),
  } as unknown as Agent

  const result = await new DomainApi().timeline(agent, 20)
  assert.equal(Array.from(result.posts[0].text).length, 300)
  assert.equal(result.posts[0].text, '😀'.repeat(300))
})

test('profile description is clipped by Unicode code point', async () => {
  const agent = {
    accountDid: 'did:plc:example',
    getProfile: async () => ({
      data: {
        did: 'did:plc:example',
        handle: 'example.test',
        displayName: 'Example',
        description: 'é'.repeat(600),
        followersCount: 0,
        followsCount: 0,
        postsCount: 0,
      },
    }),
  } as unknown as Agent

  const result = await new DomainApi().profile(agent)
  assert.equal(Array.from(result?.description ?? "").length, 511)
  assert.equal(result?.description, "é".repeat(511))
})
