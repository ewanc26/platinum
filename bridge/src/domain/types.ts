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
  /** You have muted / blocked this account. Absent for your own profile. */
  muted?: boolean
  blocking?: boolean
  pinned?: TimelinePost
}

export interface ActorList {
  actors: Author[]
  cursor?: string
}

export interface Named {
  uri: string
  name: string
}

export interface NamedList {
  items: Named[]
}

export interface MuteBlockResult {
  did: string
  on: boolean
}

export interface FollowResult {
  did: string
  on: boolean
}

export interface Author {
  did: string
  handle?: string
  displayName?: string
  /** An image reference for GET /v1/image (the 128 px avatar), when the account has one. */
  avatar?: string
}

/** A picture on a post: the reference to ask GET /v1/image for, and the author's description of it. */
export interface PostImage {
  ref: string
  alt: string
}

/** A link card: where it goes, what it is called, and the host to show under it. */
export interface PostCard {
  uri: string
  title: string
  domain: string
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
  /** Up to four pictures, in order. Absent when the post has none. */
  images?: PostImage[]
  /** The link card, when the post has one. */
  card?: PostCard
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
  /** Present, and false, only when a reply gate was asked for and could not be set. */
  replyGateApplied?: boolean
}
