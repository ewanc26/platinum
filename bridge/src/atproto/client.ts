import { Agent } from '@atproto/api'
import { NodeOAuthClient } from '@atproto/oauth-client-node'
import type { InstallationRecord } from '../storage/interfaces.js'

export class AtprotoClient {
  constructor(private readonly oauth: NodeOAuthClient) {}

  async forInstallation(record: InstallationRecord): Promise<Agent> {
    const session = await this.oauth.restore(record.did)
    return new Agent(session)
  }
}
