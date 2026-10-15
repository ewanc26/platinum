import { readFile, writeFile, mkdir, rename } from 'node:fs/promises'
import { join } from 'node:path'
import type { NodeSavedSession, NodeSavedSessionStore, NodeSavedState, NodeSavedStateStore } from '@atproto/oauth-client-node'
import type { StoredAppSession } from '../auth/app-password.js'
import type { BridgeStorage, InstallationRecord, InstallationStore } from './interfaces.js'

/* Runs read-modify-write cycles on one file one at a time. Without it, two
 * saves that both load before either writes lose one another's change. */
class FileQueue {
  private readonly tails = new Map<string, Promise<unknown>>()
  run<T>(name: string, task: () => Promise<T>): Promise<T> {
    const previous = this.tails.get(name) ?? Promise.resolve()
    const result = previous.then(task, task)
    this.tails.set(name, result.catch(() => undefined))
    return result
  }
}

export class FileStorage implements BridgeStorage {
  private readonly queue = new FileQueue()
  private readonly installationStore: FileInstallationStore
  constructor(private readonly root: string) { this.installationStore = new FileInstallationStore(root, this.queue) }
  async init(): Promise<void> { await mkdir(this.root, { recursive: true }) }
  sessionStore(): NodeSavedSessionStore { return this.keyValueStore<NodeSavedSession>('sessions.json') }
  stateStore(): NodeSavedStateStore { return this.keyValueStore<NodeSavedState>('states.json') }
  installations(): InstallationStore { return this.installationStore }
  appPasswordSessions() { return this.keyValueStore<StoredAppSession>('app-password-sessions.json') }
  private keyValueStore<T>(name: string): { set: (key: string, value: T) => Promise<void>; get: (key: string) => Promise<T | undefined>; del: (key: string) => Promise<void> } {
    return {
      set: (key, value) => this.queue.run(name, async () => { const data = await this.load<T>(name); data[key] = value; await this.save(name, data) }),
      get: async key => (await this.load<T>(name))[key],
      del: key => this.queue.run(name, async () => { const data = await this.load<T>(name); delete data[key]; await this.save(name, data) }),
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

/* Write to a temporary file and rename it over `path`, so a failed or
 * interrupted write leaves the previous file intact rather than truncated. */
async function writeFileAtomic(path: string, data: string): Promise<void> {
  const temporary = path + '.tmp'
  await writeFile(temporary, data, { mode: 0o600 })
  await rename(temporary, path)
}

class FileInstallationStore implements InstallationStore {
  /* The parsed file, kept after each successful write so a request does not
   * re-read and re-parse it. The bridge is the only writer, so this stays in
   * step with the file. */
  private cache: Record<string, InstallationRecord> | undefined
  constructor(private readonly root: string, private readonly queue: FileQueue) {}
  create(record: InstallationRecord): Promise<void> {
    return this.queue.run('installations.json', async () => {
      const data = await this.load()
      if (data[record.id]) throw new Error('installation id already exists')
      data[record.id] = record
      await this.save(data)
    })
  }
  async get(id: string): Promise<InstallationRecord | undefined> { return (await this.load())[id] }
  async getByTokenHash(tokenHash: string): Promise<InstallationRecord | undefined> {
    return Object.values(await this.load()).find(record => record.tokenHash === tokenHash)
  }
  update(record: InstallationRecord): Promise<void> {
    return this.queue.run('installations.json', async () => {
      const data = await this.load()
      if (!data[record.id]) return
      data[record.id] = record
      await this.save(data)
    })
  }
  revoke(id: string, revokedAt = new Date().toISOString()): Promise<boolean> {
    return this.queue.run('installations.json', async () => {
      const data = await this.load()
      const record = data[id]
      if (!record || record.revokedAt) return false
      data[id] = { ...record, revokedAt }
      await this.save(data)
      return true
    })
  }
  /* Returns a copy, so a write that fails before saving leaves the cache as it was. */
  private async load(): Promise<Record<string, InstallationRecord>> {
    if (this.cache) return { ...this.cache }
    let data: Record<string, InstallationRecord>
    try { data = JSON.parse(await readFile(join(this.root, 'installations.json'), 'utf8')) as Record<string, InstallationRecord> }
    catch (error) { if ((error as NodeJS.ErrnoException).code === 'ENOENT') data = {}; else throw error }
    this.cache = data
    return { ...data }
  }
  private async save(value: Record<string, InstallationRecord>): Promise<void> {
    await writeFileAtomic(join(this.root, 'installations.json'), JSON.stringify(value, null, 2))
    this.cache = value
  }
}
