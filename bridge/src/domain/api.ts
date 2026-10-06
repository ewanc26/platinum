import type { Agent } from '@atproto/api'
import { MutedWords, isMutedPost } from './muted.js'
import type {
  Author,
  Notification,
  Notifications,
  PostResult,
  ActorList,
  NamedList,
  FollowResult,
  MuteBlockResult,
  Thread,
  ThreadPost,
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
    viewer?: { following?: string }
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

export const MAX_THREAD_POSTS = 40
export const MAX_THREAD_DEPTH = 6
const MAX_ANCESTORS = 10

type ThreadNode = { post?: TimelinePostView; parent?: unknown; replies?: unknown[] }

function isThreadNode(value: unknown): value is ThreadNode {
  if (value === null || typeof value !== 'object') return false
  const post = (value as ThreadNode).post
  return post !== undefined && post !== null && typeof post === 'object' && typeof post.uri === 'string'
}

/**
 * Flatten a getPostThread tree into one bounded list the Mac can draw as rows:
 * ancestors first (oldest at the top, negative depth), the post asked for at
 * depth 0, then replies depth-first. Blocked and not-found nodes are skipped.
 */
export function flattenThread(root: unknown): Thread {
  const out: ThreadPost[] = []
  let truncated = false
  if (!isThreadNode(root)) return { posts: out, truncated }

  const ancestors: ThreadNode[] = []
  let node: unknown = root.parent
  while (isThreadNode(node) && ancestors.length < MAX_ANCESTORS) {
    ancestors.unshift(node)
    node = node.parent
  }
  if (isThreadNode(node)) truncated = true
  ancestors.forEach((a, i) => out.push({ ...normalizeTimelinePost({ post: a.post! }), depth: i - ancestors.length }))
  out.push({ ...normalizeTimelinePost({ post: root.post! }), depth: 0 })

  const walk = (n: ThreadNode, depth: number) => {
    for (const r of n.replies ?? []) {
      if (!isThreadNode(r)) continue
      if (out.length >= MAX_THREAD_POSTS || depth > MAX_THREAD_DEPTH) { truncated = true; return }
      out.push({ ...normalizeTimelinePost({ post: r.post! }), depth })
      walk(r, depth + 1)
    }
  }
  walk(root, 1)
  return { posts: out, truncated }
}

/** A handle or DID a client may ask about. Bounded and shaped; never a URL. */
export function validActor(value: unknown): string | undefined {
  if (typeof value !== 'string' || value.length === 0 || value.length > 253) return undefined
  if (/^did:[a-z]+:[A-Za-z0-9._:%-]+$/.test(value)) return value
  if (/^([a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?$/.test(value)) return value.toLowerCase()
  return undefined
}

/**
 * A search query from a client: 1 to 100 characters, no control characters.
 * Trimmed; an empty query is refused rather than searched.
 */
export function validQuery(value: unknown): string | undefined {
  if (typeof value !== 'string') return undefined
  const q = value.trim()
  if (q.length === 0 || Array.from(q).length > 100 || /[\u0000-\u001f\u007f]/.test(q)) return undefined
  return q
}

/** An at:// URI of one collection, for feeds and lists. */
export function validCollectionUri(value: unknown, collection: 'app.bsky.feed.generator' | 'app.bsky.graph.list'): string | undefined {
  if (typeof value !== 'string' || value.length > 512) return undefined
  const prefix = /^at:\/\/did:[a-z]+:[A-Za-z0-9._:%-]+\//
  if (!prefix.test(value)) return undefined
  const rest = value.replace(prefix, '')
  return new RegExp(`^${collection.replace(/\./g, '\\.')}/[A-Za-z0-9._~:-]+$`).test(rest) ? value : undefined
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

/**
 * The time a client says it has read up to. It must be an ISO 8601 timestamp
 * the client was given (an indexedAt); a time in the future is clamped to now so
 * a wrong Mac clock cannot mark notifications that have not arrived yet.
 */
export function validSeenAt(value: unknown, now = new Date()): string | undefined {
  if (typeof value !== 'string' || value.length > 64) return undefined
  if (!/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})$/.test(value)) return undefined
  const t = Date.parse(value)
  if (Number.isNaN(t)) return undefined
  return t > now.getTime() ? now.toISOString() : new Date(t).toISOString()
}

export class DomainApi {
  readonly mutedWords = new MutedWords()

  /** Drop posts matching the account's muted words. A page can come back short; the cursor still moves on. */
  private async unmuted<T extends { post: TimelinePostView }>(agent: Agent, items: T[]): Promise<T[]> {
    const words = await this.mutedWords.load(agent)
    return words.length === 0 ? items : items.filter(i => !isMutedPost(words, i.post))
  }

  async profile(agent: Agent, actor?: string): Promise<Profile | undefined> {
    const mine = actor === undefined
    let profile
    try {
      profile = (await agent.getProfile({ actor: actor ?? agent.accountDid })).data
    } catch (error) {
      if (!mine && (error as { status?: number }).status === 400) return undefined
      throw error
    }

    let pinned: TimelinePost | undefined
    const ref = profile.pinnedPost
    if (ref && typeof ref.uri === 'string') {
      const found = await agent.getPosts({ uris: [ref.uri] })
      const post = found.data.posts[0]
      if (post) pinned = normalizeTimelinePost({ post })
    }

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
      following: mine ? undefined : typeof profile.viewer?.following === 'string',
      followedBy: mine ? undefined : typeof profile.viewer?.followedBy === 'string',
      muted: mine ? undefined : profile.viewer?.muted === true,
      blocking: mine ? undefined : typeof profile.viewer?.blocking === 'string',
      pinned,
    }
  }

  async actors(agent: Agent, kind: 'follows' | 'followers', actor: string, limit: number, cursor?: string): Promise<ActorList> {
    if (kind === 'follows') {
      const r = await agent.getFollows({ actor, limit, cursor })
      return { actors: r.data.follows.map(authorFrom), cursor: r.data.cursor }
    }
    const r = await agent.getFollowers({ actor, limit, cursor })
    return { actors: r.data.followers.map(authorFrom), cursor: r.data.cursor }
  }

  /** Who liked or reposted a post: the same bounded author shape as follow lists. */
  async engagement(agent: Agent, kind: 'likes' | 'reposts', uri: string, limit: number, cursor?: string): Promise<ActorList | undefined> {
    try {
      if (kind === 'likes') {
        const r = await agent.getLikes({ uri, limit, cursor })
        return { actors: r.data.likes.map(l => authorFrom(l.actor)), cursor: r.data.cursor }
      }
      const r = await agent.getRepostedBy({ uri, limit, cursor })
      return { actors: r.data.repostedBy.map(authorFrom), cursor: r.data.cursor }
    } catch (error) {
      if ((error as { status?: number }).status === 400) return undefined
      throw error
    }
  }

  async searchActors(agent: Agent, q: string, limit: number, cursor?: string): Promise<ActorList> {
    const r = await agent.searchActors({ q, limit, cursor })
    return { actors: r.data.actors.map(authorFrom), cursor: r.data.cursor }
  }

  async searchPosts(agent: Agent, q: string, limit: number, cursor?: string): Promise<Timeline> {
    const r = await agent.app.bsky.feed.searchPosts({ q, limit, cursor })
    return { posts: (await this.unmuted(agent, r.data.posts.map(post => ({ post })))).map(normalizeTimelinePost), cursor: r.data.cursor }
  }

  /** The account's saved custom feeds, named. Lists and the home timeline are left out. */
  async savedFeeds(agent: Agent): Promise<NamedList> {
    const prefs = await agent.getPreferences()
    const uris = prefs.savedFeeds.filter(f => f.type === 'feed').map(f => f.value).slice(0, 25)
    if (uris.length === 0) return { items: [] }
    const r = await agent.app.bsky.feed.getFeedGenerators({ feeds: uris })
    return { items: r.data.feeds.map(f => ({ uri: clipped(f.uri, 512), name: clipped(f.displayName, MAX_DISPLAY_NAME_LENGTH) })) }
  }

  async feed(agent: Agent, uri: string, limit: number, cursor?: string): Promise<Timeline | undefined> {
    try {
      const r = await agent.app.bsky.feed.getFeed({ feed: uri, limit, cursor })
      return { posts: (await this.unmuted(agent, r.data.feed)).map(normalizeTimelinePost), cursor: r.data.cursor }
    } catch (error) {
      if ((error as { status?: number }).status === 400) return undefined
      throw error
    }
  }

  async lists(agent: Agent): Promise<NamedList> {
    const r = await agent.app.bsky.graph.getLists({ actor: agent.accountDid!, limit: 50 })
    return { items: r.data.lists.map(l => ({ uri: clipped(l.uri, 512), name: clipped(l.name, MAX_DISPLAY_NAME_LENGTH) })) }
  }

  async listMembers(agent: Agent, uri: string, limit: number, cursor?: string): Promise<ActorList | undefined> {
    try {
      const r = await agent.app.bsky.graph.getList({ list: uri, limit, cursor })
      return { actors: r.data.items.map(i => authorFrom(i.subject)), cursor: r.data.cursor }
    } catch (error) {
      if ((error as { status?: number }).status === 400) return undefined
      throw error
    }
  }

  async authorFeed(agent: Agent, actor: string, limit: number, cursor?: string): Promise<Timeline> {
    const r = await agent.getAuthorFeed({ actor, limit, cursor, filter: 'posts_no_replies' })
    return { posts: r.data.feed.map(normalizeTimelinePost), cursor: r.data.cursor }
  }

  /** Set following to `on`, idempotently, from the viewer state the AppView reports. */
  async follow(agent: Agent, did: string, on: boolean): Promise<FollowResult | undefined> {
    let existing: string | undefined
    try {
      existing = (await agent.getProfile({ actor: did })).data.viewer?.following
    } catch (error) {
      if ((error as { status?: number }).status === 400) return undefined
      throw error
    }
    if (on && !existing) await agent.follow(did)
    else if (!on && existing) await agent.deleteFollow(existing)
    return { did, on }
  }

  /** Set muted to `on`, idempotently. Muting is private to the account and leaves no record. */
  async mute(agent: Agent, did: string, on: boolean): Promise<MuteBlockResult | undefined> {
    let muted: boolean
    try {
      muted = (await agent.getProfile({ actor: did })).data.viewer?.muted === true
    } catch (error) {
      if ((error as { status?: number }).status === 400) return undefined
      throw error
    }
    if (on && !muted) await agent.mute(did)
    else if (!on && muted) await agent.unmute(did)
    return { did, on }
  }

  /** Set blocking to `on`, idempotently. A block is a public record; the Mac never holds its URI. */
  async block(agent: Agent, did: string, on: boolean): Promise<MuteBlockResult | undefined> {
    let existing: string | undefined
    try {
      existing = (await agent.getProfile({ actor: did })).data.viewer?.blocking
    } catch (error) {
      if ((error as { status?: number }).status === 400) return undefined
      throw error
    }
    const repo = (agent as unknown as { accountDid: string }).accountDid
    if (on && !existing) {
      await agent.app.bsky.graph.block.create({ repo }, { subject: did, createdAt: new Date().toISOString() })
    } else if (!on && existing) {
      const rkey = existing.split('/').pop() ?? ''
      if (rkey) await agent.app.bsky.graph.block.delete({ repo, rkey })
    }
    return { did, on }
  }

  async timeline(agent: Agent, limit: number, cursor?: string): Promise<Timeline> {
    const result = await agent.getTimeline({ limit, cursor })

    return {
      posts: (await this.unmuted(agent, result.data.feed)).map(normalizeTimelinePost),
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

  async markSeen(agent: Agent, seenAt: string): Promise<{ seenAt: string }> {
    await agent.updateSeenNotifications(seenAt)
    return { seenAt }
  }

  async thread(agent: Agent, ref: { uri: string }): Promise<Thread | undefined> {
    const result = await agent.getPostThread({ uri: ref.uri, depth: MAX_THREAD_DEPTH, parentHeight: MAX_ANCESTORS })
    const thread = flattenThread(result.data.thread)
    return thread.posts.length > 0 ? thread : undefined
  }

  /**
   * Post, optionally as a reply, a quote, or with a reply gate. The reply root
   * is worked out here from the parent's own record, so the Mac only ever names
   * the post it is replying to or quoting. Returns undefined when the parent or
   * quoted post no longer exists, and writes nothing then. The gate is a
   * separate record: if the post lands and the gate does not, the post stands
   * and `replyGateApplied` is false so the Mac can say so.
   */
  async post(
    agent: Agent,
    text: string,
    replyTo?: { uri: string; cid: string },
    options: { quote?: { uri: string; cid: string }; replyGate?: ReplyGate } = {},
  ): Promise<PostResult | undefined> {
    const record: { text: string; reply?: unknown; embed?: unknown } = { text }
    if (replyTo) {
      const found = await agent.getPosts({ uris: [replyTo.uri] })
      const parent = found.data.posts[0]
      if (!parent) return undefined
      const pr = parent.record as { reply?: { root?: { uri?: unknown; cid?: unknown } } } | undefined
      const r = pr?.reply?.root
      const root = r && typeof r.uri === 'string' && typeof r.cid === 'string'
        ? { uri: r.uri, cid: r.cid }
        : { uri: parent.uri, cid: parent.cid }
      record.reply = { root, parent: { uri: parent.uri, cid: parent.cid } }
    }
    if (options.quote) {
      // The AppView's own cid, not the Mac's, so a stale copy cannot be quoted.
      const found = await agent.getPosts({ uris: [options.quote.uri] })
      const quoted = found.data.posts[0]
      if (!quoted) return undefined
      record.embed = { $type: 'app.bsky.embed.record', record: { uri: quoted.uri, cid: quoted.cid } }
    }
    const result = await agent.post(record as never)
    const out: PostResult = { uri: result.uri, cid: result.cid }

    const gate = options.replyGate
    if (gate && gate !== 'everyone') {
      const rule = GATE_RULES[gate]
      try {
        await agent.app.bsky.feed.threadgate.create(
          { repo: (agent as unknown as { accountDid: string }).accountDid, rkey: result.uri.split('/').pop() ?? '' },
          { post: result.uri, allow: rule as never, createdAt: new Date().toISOString() },
        )
      } catch {
        out.replyGateApplied = false
      }
    }
    return out
  }
}

export type ReplyGate = 'everyone' | 'nobody' | 'mentioned' | 'following' | 'followers'

const GATE_RULES: Record<Exclude<ReplyGate, 'everyone'>, unknown[]> = {
  nobody: [],
  mentioned: [{ $type: 'app.bsky.feed.threadgate#mentionRule' }],
  following: [{ $type: 'app.bsky.feed.threadgate#followingRule' }],
  followers: [{ $type: 'app.bsky.feed.threadgate#followerRule' }],
}

export function validReplyGate(value: unknown): ReplyGate | undefined {
  return value === 'everyone' || value === 'nobody' || value === 'mentioned' || value === 'following' || value === 'followers'
    ? value
    : undefined
}
