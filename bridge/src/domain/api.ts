import type { Agent } from '@atproto/api'
import type { Notifications, PostResult, Profile, Timeline } from './types.js'

export class DomainApi {
  async profile(agent: Agent): Promise<Profile> {
    const result = await agent.getProfile({ actor: agent.accountDid })
    const profile = result.data

    return {
      did: profile.did,
      handle: profile.handle,
      displayName: profile.displayName,
      description: profile.description,
      avatar: profile.avatar,
      banner: profile.banner,
      followersCount: profile.followersCount,
      followsCount: profile.followsCount,
      postsCount: profile.postsCount,
    }
  }

  async timeline(agent: Agent, limit: number, cursor?: string): Promise<Timeline> {
    const result = await agent.getTimeline({ limit, cursor })
    return { feed: result.data.feed, cursor: result.data.cursor }
  }

  async notifications(agent: Agent, limit: number, cursor?: string): Promise<Notifications> {
    const result = await agent.listNotifications({ limit, cursor })
    return { notifications: result.data.notifications, cursor: result.data.cursor }
  }

  async post(agent: Agent, text: string): Promise<PostResult> {
    const result = await agent.post({ text })
    return { uri: result.uri, cid: result.cid }
  }
}
