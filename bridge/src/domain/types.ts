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

export interface Timeline {
  feed: unknown[]
  cursor?: string
}

export interface Notifications {
  notifications: unknown[]
  cursor?: string
}

export interface PostResult {
  uri: string
  cid: string
}
