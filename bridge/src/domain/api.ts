import type { Agent } from '@atproto/api'
import type {
  Author,
  Notification,
  Notifications,
  PostResult,
  ToggleResult,
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
  viewer?: { like?: string; repost?: string }
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
    liked: typeof post.viewer?.like === 'string',
    reposted: typeof post.viewer?.repost === 'string',
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

export type ToggleKind = 'like' | 'repost'

/** A post reference from a client. Bounded and shaped, never trusted further. */
export function validPostRef(uri: unknown, cid: unknown): { uri: string; cid: string } | undefined {
  if (typeof uri !== 'string' || typeof cid !== 'string') return undefined
  if (uri.length > 512 || cid.length > 255 || cid.length === 0) return undefined
  if (!/^at:\/\/did:[a-z]+:[A-Za-z0-9._:%-]+\/app\.bsky\.feed\.post\/[A-Za-z0-9._~:-]+$/.test(uri)) return undefined
  if (!/^[A-Za-z0-9]+$/.test(cid)) return undefined
  return { uri, cid }
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

  /**
   * Set like or repost to `on`, idempotently. The current viewer state is read
   * from the AppView first, so the client sends only the state it wants and
   * never needs the like or repost record URI; a repeated request is a no-op.
   */
  async toggle(agent: Agent, kind: ToggleKind, ref: { uri: string; cid: string }, on: boolean): Promise<ToggleResult> {
    const result = await agent.getPosts({ uris: [ref.uri] })
    const post = result.data.posts[0]
    if (!post) return { uri: ref.uri, on: false, count: 0 }
    const existing = kind === 'like' ? post.viewer?.like : post.viewer?.repost
    let count = (kind === 'like' ? post.likeCount : post.repostCount) ?? 0

    if (on && !existing) {
      if (kind === 'like') await agent.like(ref.uri, ref.cid)
      else await agent.repost(ref.uri, ref.cid)
      count += 1
    } else if (!on && existing) {
      if (kind === 'like') await agent.deleteLike(existing)
      else await agent.deleteRepost(existing)
      count = Math.max(0, count - 1)
    }
    return { uri: ref.uri, on, count }
  }

  async post(agent: Agent, text: string): Promise<PostResult> {
    const result = await agent.post({ text })
    return { uri: result.uri, cid: result.cid }
  }
}
