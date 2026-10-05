import type { NodeSavedSessionStore, NodeSavedStateStore } from '@atproto/oauth-client-node'

export interface InstallationRecord {
  id: string
  did: string
  tokenHash: string
  createdAt: string
  lastUsedAt?: string
  revokedAt?: string
  clientVersion?: string
  installationLabel?: string
  /** How the account signed in. Absent means OAuth (records written before app-password existed). */
  authKind?: 'oauth' | 'app-password'
}

export interface InstallationStore {
  create(record: InstallationRecord): Promise<void>
  get(id: string): Promise<InstallationRecord | undefined>
  getByTokenHash(tokenHash: string): Promise<InstallationRecord | undefined>
  update(record: InstallationRecord): Promise<void>
  revoke(id: string, revokedAt?: string): Promise<boolean>
}

export interface BridgeStorage {
  init(): Promise<void>
  sessionStore(): NodeSavedSessionStore
  stateStore(): NodeSavedStateStore
  installations(): InstallationStore
  appPasswordSessions(): import('../auth/app-password.js').KeyValue<import('../auth/app-password.js').StoredAppSession>
}
