import { Agent } from '@atproto/api'
import { NodeOAuthClient } from '@atproto/oauth-client-node'
import type { AppPasswordService } from '../auth/app-password.js'
import type { InstallationRecord } from '../storage/interfaces.js'

export class AtprotoClient {
  constructor(
    private readonly oauth: NodeOAuthClient,
    private readonly appPassword?: AppPasswordService,
  ) {}

  async forInstallation(record: InstallationRecord): Promise<Agent | undefined> {
    if (record.authKind === 'app-password') return this.appPassword?.restore(record.id)
    const session = await this.oauth.restore(record.did)
    return new Agent(session)
  }
}
