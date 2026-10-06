export interface Profile {
  did: string
  handle?: string
  displayName?: string
  description?: string
  avatar?: string
  banner?: string
  followersCount?: number
  followsCount?: number
  postsCount?: number
  /** The signed-in account follows / is followed by this one. Absent for your own profile. */
  following?: boolean
  followedBy?: boolean
  pinned?: TimelinePost
}

export interface ActorList {
  actors: Author[]
  cursor?: string
}

export interface FollowResult {
  did: string
  on: boolean
}

export interface Author {
  did: string
  handle?: string
  displayName?: string
}

export interface TimelinePost {
  uri: string
  cid: string
  author: Author
  text: string
  createdAt: string
  likeCount: number
  repostCount: number
  replyCount: number
  quoteCount: number
  /** Whether the signed-in account has liked / reposted this post. */
  liked: boolean
  reposted: boolean
}

/** A post in a thread, flattened. depth < 0 is an ancestor, 0 the post asked for, > 0 a reply. */
export interface ThreadPost extends TimelinePost {
  depth: number
}

export interface Thread {
  posts: ThreadPost[]
  /** True when replies were left out to stay within the bound. */
  truncated: boolean
}

export interface Timeline {
  posts: TimelinePost[]
  cursor?: string
}

export interface Notification {
  uri: string
  cid: string
  author: Author
  reason: string
  indexedAt: string
  isRead: boolean
}

export interface Notifications {
  notifications: Notification[]
  cursor?: string
}

/** Result of POST /v1/like and /v1/repost: the state now, and the count to show. */
export interface ToggleResult {
  uri: string
  on: boolean
  count: number
}

export interface PostResult {
  uri: string
  cid: string
}
