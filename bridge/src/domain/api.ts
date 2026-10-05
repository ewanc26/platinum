import type { Agent } from '@atproto/api'
import type {
  Author,
  Notification,
  Notifications,
  PostResult,
  Profile,
  Timeline,
  TimelinePost,
} from './types.js'

const MAX_HANDLE_LENGTH = 255
const MAX_DISPLAY_NAME_LENGTH = 64
const MAX_POST_TEXT_LENGTH = 300
const MAX_PROFILE_DESCRIPTION_LENGTH = 511

function stringValue(value: unknown): string {
  return typeof value === 'string' ? value : ''
}

function clipped(value: unknown, maximum: number): string {
  return Array.from(stringValue(value)).slice(0, maximum).join('')
}

function authorFrom(value: {
  did: string
  handle?: string
  displayName?: string
}): Author {
  return {
    did: clipped(value.did, MAX_HANDLE_LENGTH),
    handle: value.handle ? clipped(value.handle, MAX_HANDLE_LENGTH) : undefined,
    displayName: value.displayName
      ? clipped(value.displayName, MAX_DISPLAY_NAME_LENGTH)
      : undefined,
  }
}

type TimelinePostView = {
  uri: string
  cid: string
  author: {
    did: string
    handle?: string
    displayName?: string
  }
  record: unknown
  likeCount?: number
  repostCount?: number
  replyCount?: number
  quoteCount?: number
}

type TimelineFeedItem = {
  post: TimelinePostView
}

export function normalizeTimelinePost(feedItem: TimelineFeedItem): TimelinePost {
  const post = feedItem.post
  const record =
    post.record !== null && typeof post.record === 'object'
      ? (post.record as { text?: unknown; createdAt?: unknown })
      : {}

  return {
    uri: clipped(post.uri, 512),
    cid: clipped(post.cid, 255),
    author: authorFrom(post.author),
    text: clipped(record.text, MAX_POST_TEXT_LENGTH),
    createdAt: clipped(record.createdAt, 64),
    likeCount: post.likeCount ?? 0,
    repostCount: post.repostCount ?? 0,
    replyCount: post.replyCount ?? 0,
    quoteCount: post.quoteCount ?? 0,
  }
}

export function normalizeNotification(notification: {
  uri: string
  cid: string
  author: {
    did: string
    handle?: string
    displayName?: string
  }
  reason: string
  indexedAt: string
  isRead?: boolean
}): Notification {
  return {
    uri: clipped(notification.uri, 512),
    cid: clipped(notification.cid, 255),
    author: authorFrom(notification.author),
    reason: clipped(notification.reason, 32),
    indexedAt: clipped(notification.indexedAt, 64),
    isRead: notification.isRead ?? false,
  }
}

export class DomainApi {
  async profile(agent: Agent): Promise<Profile> {
    const result = await agent.getProfile({ actor: agent.accountDid })
    const profile = result.data

    return {
      did: profile.did,
      handle: profile.handle,
      displayName: profile.displayName,
      description: profile.description
        ? clipped(profile.description, MAX_PROFILE_DESCRIPTION_LENGTH)
        : undefined,
      avatar: profile.avatar,
      banner: profile.banner,
      followersCount: profile.followersCount,
      followsCount: profile.followsCount,
      postsCount: profile.postsCount,
    }
  }

  async timeline(agent: Agent, limit: number, cursor?: string): Promise<Timeline> {
    const result = await agent.getTimeline({ limit, cursor })

    return {
      posts: result.data.feed.map(normalizeTimelinePost),
      cursor: result.data.cursor,
    }
  }

  async notifications(agent: Agent, limit: number, cursor?: string): Promise<Notifications> {
    const result = await agent.listNotifications({ limit, cursor })

    return {
      notifications: result.data.notifications.map(normalizeNotification),
      cursor: result.data.cursor,
    }
  }

  async post(agent: Agent, text: string): Promise<PostResult> {
    const result = await agent.post({ text })
    return { uri: result.uri, cid: result.cid }
  }
}
