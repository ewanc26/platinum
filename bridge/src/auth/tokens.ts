import { createHash, randomBytes, randomUUID } from 'node:crypto'
import type { InstallationRecord, InstallationStore } from '../storage/interfaces.js'

export interface IssuedToken {
  token: string
  record: InstallationRecord
}

export interface InstallationOptions {
  clientVersion?: string
  installationLabel?: string
}

export class TokenService {
  constructor(private readonly store: InstallationStore) {}

  async issue(did: string, token = generateToken(), options: InstallationOptions = {}): Promise<IssuedToken> {
    const record: InstallationRecord = {
      id: randomUUID(),
      did,
      tokenHash: hashToken(token),
      createdAt: new Date().toISOString(),
      clientVersion: options.clientVersion,
      installationLabel: options.installationLabel,
    }
    await this.store.create(record)
    return { token, record }
  }

  async authenticate(token: string): Promise<InstallationRecord | undefined> {
    if (!token) return undefined
    const record = await this.store.getByTokenHash(hashToken(token))
    if (!record || record.revokedAt) return undefined
    const updated = { ...record, lastUsedAt: new Date().toISOString() }
    await this.store.update(updated)
    return updated
  }

  async revoke(token: string): Promise<boolean> {
    const record = await this.store.getByTokenHash(hashToken(token))
    if (!record || record.revokedAt) return false
    return this.store.revoke(record.id)
  }

  async rotate(token: string, options: InstallationOptions = {}): Promise<IssuedToken | undefined> {
    const record = await this.authenticate(token)
    if (!record) return undefined
    await this.store.revoke(record.id)
    return this.issue(record.did, generateToken(), {
      clientVersion: options.clientVersion ?? record.clientVersion,
      installationLabel: options.installationLabel ?? record.installationLabel,
    })
  }
}

export function hashToken(token: string): string {
  return createHash('sha256').update(token, 'utf8').digest('hex')
}

function generateToken(): string {
  return randomBytes(32).toString('base64url')
}
