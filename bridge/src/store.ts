import { readFile, writeFile, mkdir } from 'node:fs/promises'
import { join } from 'node:path'
import type { NodeSavedSession, NodeSavedSessionStore, NodeSavedState, NodeSavedStateStore } from '@atproto/oauth-client-node'

export class FileStore {
  constructor(private readonly root: string) {}

  async init(): Promise<void> {
    await mkdir(this.root, { recursive: true })
  }

  private async load<T>(name: string): Promise<Record<string, T>> {
    try {
      return JSON.parse(await readFile(join(this.root, name), 'utf8')) as Record<string, T>
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code === 'ENOENT') return {}
      throw error
    }
  }

  private async save<T>(name: string, value: Record<string, T>): Promise<void> {
    await writeFile(join(this.root, name), JSON.stringify(value, null, 2), { mode: 0o600 })
  }

  sessionStore(): NodeSavedSessionStore {
    return {
      set: async (key, value) => {
        const data = await this.load<NodeSavedSession>('sessions.json')
        data[key] = value
        await this.save('sessions.json', data)
      },
      get: async (key) => (await this.load<NodeSavedSession>('sessions.json'))[key],
      del: async (key) => {
        const data = await this.load<NodeSavedSession>('sessions.json')
        delete data[key]
        await this.save('sessions.json', data)
      },
    }
  }

  stateStore(): NodeSavedStateStore {
    return {
      set: async (key, value) => {
        const data = await this.load<NodeSavedState>('states.json')
        data[key] = value
        await this.save('states.json', data)
      },
      get: async (key) => (await this.load<NodeSavedState>('states.json'))[key],
      del: async (key) => {
        const data = await this.load<NodeSavedState>('states.json')
        delete data[key]
        await this.save('states.json', data)
      },
    }
  }
}

export class PairingStore {
  private readonly records = new Map<string, { did: string; token: string; expiresAt: number }>()

  create(did: string): string {
    const code = randomString(6)
    this.records.set(code, { did, token: randomString(32), expiresAt: Date.now() + 10 * 60_000 })
    return code
  }

  exchange(code: string) {
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
  const alphabet = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'
  const bytes = new Uint8Array(length)
  crypto.getRandomValues(bytes)
  return Array.from(bytes, byte => alphabet[byte % alphabet.length]).join('')
}
