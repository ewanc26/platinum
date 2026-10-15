import { createHash, randomBytes, randomUUID } from 'node:crypto'
import type { InstallationRecord, InstallationStore } from '../storage/interfaces.js'

export interface IssuedToken {
  token: string
  record: InstallationRecord
}

export interface InstallationOptions {
  clientVersion?: string
  installationLabel?: string
  authKind?: 'oauth' | 'app-password'
}

/* lastUsedAt is written at most this often. It is only shown to the operator,
 * so a write on every request would rewrite the installations file for nothing. */
const LAST_USED_WRITE_INTERVAL_MS = 60 * 60 * 1000

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
      authKind: options.authKind,
    }
    await this.store.create(record)
    return { token, record }
  }

  async authenticate(token: string): Promise<InstallationRecord | undefined> {
    if (!token) return undefined
    const record = await this.store.getByTokenHash(hashToken(token))
    if (!record || record.revokedAt) return undefined
    const now = Date.now()
    const lastUsed = record.lastUsedAt ? Date.parse(record.lastUsedAt) : NaN
    if (Number.isFinite(lastUsed) && Math.abs(now - lastUsed) < LAST_USED_WRITE_INTERVAL_MS) return record
    const updated = { ...record, lastUsedAt: new Date(now).toISOString() }
    await this.store.update(updated)
    return updated
  }

  async revoke(token: string): Promise<boolean> {
    return (await this.revokeRecord(token)) !== undefined
  }

  /** Revoke and return the record that was revoked, or undefined if the token was not valid. */
  async revokeRecord(token: string): Promise<InstallationRecord | undefined> {
    const record = await this.store.getByTokenHash(hashToken(token))
    if (!record || record.revokedAt) return undefined
    return (await this.store.revoke(record.id)) ? record : undefined
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
