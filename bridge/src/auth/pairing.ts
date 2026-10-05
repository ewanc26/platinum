import { randomBytes } from 'node:crypto'

export interface PairingRecord {
  did: string
  token: string
  expiresAt: number
}

const alphabet = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'

export class PairingService {
  private readonly records = new Map<string, PairingRecord>()

  create(did: string, ttlMs = 10 * 60_000): string {
    const code = randomString(6)
    this.records.set(code, {
      did,
      token: randomString(32),
      expiresAt: Date.now() + ttlMs,
    })
    return code
  }

  exchange(code: string): PairingRecord | undefined {
    const key = code.toUpperCase()
    const record = this.records.get(key)

    if (!record || record.expiresAt < Date.now()) {
      this.records.delete(key)
      return undefined
    }

    this.records.delete(key)
    return record
  }
}

function randomString(length: number): string {
  const bytes = randomBytes(length)
  return Array.from(bytes, byte => alphabet[byte % alphabet.length]).join('')
}
