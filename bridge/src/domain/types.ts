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
