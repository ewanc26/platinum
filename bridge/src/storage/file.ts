import { readFile, writeFile, mkdir, rename } from 'node:fs/promises'
import { join } from 'node:path'
import type { NodeSavedSession, NodeSavedSessionStore, NodeSavedState, NodeSavedStateStore } from '@atproto/oauth-client-node'
import type { BridgeStorage, InstallationRecord, InstallationStore } from './interfaces.js'

export class FileStorage implements BridgeStorage {
  private readonly installationStore: FileInstallationStore
  constructor(private readonly root: string) { this.installationStore = new FileInstallationStore(root) }
  async init(): Promise<void> { await mkdir(this.root, { recursive: true }) }
  sessionStore(): NodeSavedSessionStore { return this.keyValueStore<NodeSavedSession>('sessions.json') }
  stateStore(): NodeSavedStateStore { return this.keyValueStore<NodeSavedState>('states.json') }
  installations(): InstallationStore { return this.installationStore }
  private keyValueStore<T>(name: string): { set: (key: string, value: T) => Promise<void>; get: (key: string) => Promise<T | undefined>; del: (key: string) => Promise<void> } {
    return {
      set: async (key, value) => { const data = await this.load<T>(name); data[key] = value; await this.save(name, data) },
      get: async key => (await this.load<T>(name))[key],
      del: async key => { const data = await this.load<T>(name); delete data[key]; await this.save(name, data) },
    }
  }
  private async load<T>(name: string): Promise<Record<string, T>> {
    try { return JSON.parse(await readFile(join(this.root, name), 'utf8')) as Record<string, T> }
    catch (error) { if ((error as NodeJS.ErrnoException).code === 'ENOENT') return {}; throw error }
  }
  private async save<T>(name: string, value: Record<string, T>): Promise<void> {
    await writeFile(join(this.root, name), JSON.stringify(value, null, 2), { mode: 0o600 })
  }
}

class FileInstallationStore implements InstallationStore {
  constructor(private readonly root: string) {}
  async create(record: InstallationRecord): Promise<void> {
    const data = await this.load()
    if (data[record.id]) throw new Error('installation id already exists')
    data[record.id] = record
    await this.save(data)
  }
  async get(id: string): Promise<InstallationRecord | undefined> { return (await this.load())[id] }
  async getByTokenHash(tokenHash: string): Promise<InstallationRecord | undefined> {
    return Object.values(await this.load()).find(record => record.tokenHash === tokenHash)
  }
  async update(record: InstallationRecord): Promise<void> {
    const data = await this.load()
    if (!data[record.id]) return
    data[record.id] = record
    await this.save(data)
  }
  async revoke(id: string, revokedAt = new Date().toISOString()): Promise<boolean> {
    const data = await this.load()
    const record = data[id]
    if (!record || record.revokedAt) return false
    data[id] = { ...record, revokedAt }
    await this.save(data)
    return true
  }
  private async load(): Promise<Record<string, InstallationRecord>> {
    try { return JSON.parse(await readFile(join(this.root, 'installations.json'), 'utf8')) as Record<string, InstallationRecord> }
    catch (error) { if ((error as NodeJS.ErrnoException).code === 'ENOENT') return {}; throw error }
  }
  private async save(value: Record<string, InstallationRecord>): Promise<void> {
    const path = join(this.root, 'installations.json')
    const temporary = path + '.tmp'
    await writeFile(temporary, JSON.stringify(value, null, 2), { mode: 0o600 })
    await rename(temporary, path)
  }
}
