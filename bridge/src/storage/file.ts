import { randomBytes } from 'node:crypto'
import { readFile, writeFile, mkdir, rename, rm } from 'node:fs/promises'
import { join } from 'node:path'
import type { NodeSavedSession, NodeSavedSessionStore, NodeSavedState, NodeSavedStateStore } from '@atproto/oauth-client-node'
import type { StoredAppSession } from '../auth/app-password.js'
import type { BridgeStorage, InstallationRecord, InstallationStore } from './interfaces.js'

/**
 * Runs writes to one file one at a time. A read-modify-write that awaits
 * between its read and its save would otherwise lose the updates of a write
 * that overlapped it.
 */
const writes = new Map<string, Promise<unknown>>()
function exclusive<R>(path: string, work: () => Promise<R>): Promise<R> {
  const previous = writes.get(path) ?? Promise.resolve()
  const result = previous.then(work)
  writes.set(path, result.then(() => undefined, () => undefined))
  return result
}

/**
 * Writes the whole file or leaves the old one: the data goes to a temporary
 * name unique to this write, then renamed over the target. A shared temporary
 * name would let two overlapping saves rename each other's file.
 */
async function writeFileAtomic(path: string, data: string): Promise<void> {
  const temporary = `${path}.${randomBytes(8).toString('hex')}.tmp`
  try {
    await writeFile(temporary, data, { mode: 0o600 })
    await rename(temporary, path)
  } catch (error) {
    await rm(temporary, { force: true })
    throw error
  }
}

export class FileStorage implements BridgeStorage {
  private readonly installationStore: FileInstallationStore
  constructor(private readonly root: string) { this.installationStore = new FileInstallationStore(root) }
  async init(): Promise<void> { await mkdir(this.root, { recursive: true }) }
  sessionStore(): NodeSavedSessionStore { return this.keyValueStore<NodeSavedSession>('sessions.json') }
  stateStore(): NodeSavedStateStore { return this.keyValueStore<NodeSavedState>('states.json') }
  installations(): InstallationStore { return this.installationStore }
  appPasswordSessions() { return this.keyValueStore<StoredAppSession>('app-password-sessions.json') }
  private keyValueStore<T>(name: string): { set: (key: string, value: T) => Promise<void>; get: (key: string) => Promise<T | undefined>; del: (key: string) => Promise<void> } {
    const path = join(this.root, name)
    return {
      set: (key, value) => exclusive(path, async () => { const data = await this.load<T>(name); data[key] = value; await this.save(name, data) }),
      get: async key => (await this.load<T>(name))[key],
      del: key => exclusive(path, async () => { const data = await this.load<T>(name); delete data[key]; await this.save(name, data) }),
    }
  }
  private async load<T>(name: string): Promise<Record<string, T>> {
    try { return JSON.parse(await readFile(join(this.root, name), 'utf8')) as Record<string, T> }
    catch (error) { if ((error as NodeJS.ErrnoException).code === 'ENOENT') return {}; throw error }
  }
  private async save<T>(name: string, value: Record<string, T>): Promise<void> {
    await writeFileAtomic(join(this.root, name), JSON.stringify(value, null, 2))
  }
}

/**
 * Installations live in one JSON file. The store keeps them in memory, with an
 * index by token hash, so an authenticated request is a map lookup rather than
 * a scan of the file. Every write goes through this instance and updates both
 * the map and the file, so the cache cannot drift while the bridge is the only
 * writer, which is how it is deployed.
 */
class FileInstallationStore implements InstallationStore {
  private records?: Map<string, InstallationRecord>
  private byTokenHash?: Map<string, string>
  private readonly path: string

  constructor(private readonly root: string) { this.path = join(root, 'installations.json') }

  create(record: InstallationRecord): Promise<void> {
    return exclusive(this.path, async () => {
      const records = await this.loaded()
      if (records.has(record.id)) throw new Error('installation id already exists')
      records.set(record.id, record)
      await this.save()
    })
  }
  async get(id: string): Promise<InstallationRecord | undefined> { return (await this.loaded()).get(id) }
  async getByTokenHash(tokenHash: string): Promise<InstallationRecord | undefined> {
    await this.loaded()
    const id = this.byTokenHash!.get(tokenHash)
    return id === undefined ? undefined : this.records!.get(id)
  }
  update(record: InstallationRecord): Promise<void> {
    return exclusive(this.path, async () => {
      const records = await this.loaded()
      if (!records.has(record.id)) return
      records.set(record.id, record)
      await this.save()
    })
  }
  revoke(id: string, revokedAt = new Date().toISOString()): Promise<boolean> {
    return exclusive(this.path, async () => {
      const records = await this.loaded()
      const record = records.get(id)
      if (!record || record.revokedAt) return false
      records.set(id, { ...record, revokedAt })
      await this.save()
      return true
    })
  }
  private async loaded(): Promise<Map<string, InstallationRecord>> {
    if (!this.records) {
      const data = await this.read()
      this.records = new Map(Object.entries(data))
      this.byTokenHash = new Map([...this.records.values()].map(record => [record.tokenHash, record.id]))
    }
    return this.records
  }
  private async read(): Promise<Record<string, InstallationRecord>> {
    try { return JSON.parse(await readFile(this.path, 'utf8')) as Record<string, InstallationRecord> }
    catch (error) { if ((error as NodeJS.ErrnoException).code === 'ENOENT') return {}; throw error }
  }
  private async save(): Promise<void> {
    const data = Object.fromEntries(this.records!)
    await writeFileAtomic(this.path, JSON.stringify(data, null, 2))
    this.byTokenHash = new Map([...this.records!.values()].map(record => [record.tokenHash, record.id]))
  }
}
