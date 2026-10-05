import assert from 'node:assert/strict'
import test from 'node:test'
import { TokenService, hashToken } from '../src/auth/tokens.js'
import type { InstallationRecord, InstallationStore } from '../src/storage/interfaces.js'

class MemoryInstallationStore implements InstallationStore {
  private readonly records = new Map<string, InstallationRecord>()

  async create(record: InstallationRecord): Promise<void> { this.records.set(record.id, record) }
  async get(id: string): Promise<InstallationRecord | undefined> { return this.records.get(id) }
  async getByTokenHash(tokenHash: string): Promise<InstallationRecord | undefined> {
    return [...this.records.values()].find(record => record.tokenHash === tokenHash)
  }
  async update(record: InstallationRecord): Promise<void> { this.records.set(record.id, record) }
  async revoke(id: string, revokedAt = new Date().toISOString()): Promise<boolean> {
    const record = this.records.get(id)
    if (!record || record.revokedAt) return false
    this.records.set(id, { ...record, revokedAt })
    return true
  }
}

test('token service stores only a token hash', async () => {
  const store = new MemoryInstallationStore()
  const service = new TokenService(store)
  const token = 'test-token'

  const issued = await service.issue('did:plc:test', token)
  const record = await store.getByTokenHash(hashToken(token))

  assert.equal(record?.did, 'did:plc:test')
  assert.equal(record?.id, issued.record.id)
  assert.equal(record?.tokenHash, hashToken(token))
  assert.notEqual(record?.tokenHash, token)
})

test('revoked tokens no longer authenticate and retain revocation metadata', async () => {
  const store = new MemoryInstallationStore()
  const service = new TokenService(store)
  const token = 'test-token'

  const issued = await service.issue('did:plc:test', token)
  assert.ok(await service.authenticate(token))
  assert.equal(await service.revoke(token), true)
  assert.equal((await store.get(issued.record.id))?.revokedAt !== undefined, true)
  assert.equal(await service.authenticate(token), undefined)
})

test('token rotation revokes the old installation and issues a new one', async () => {
  const store = new MemoryInstallationStore()
  const service = new TokenService(store)
  const token = 'test-token'

  const issued = await service.issue('did:plc:test', token, {
    clientVersion: '1.2.3',
    installationLabel: 'Quadra',
  })
  const rotated = await service.rotate(token)

  assert.ok(rotated)
  assert.notEqual(rotated?.token, token)
  assert.notEqual(rotated?.record.id, issued.record.id)
  assert.equal(rotated?.record.did, issued.record.did)
  assert.equal(rotated?.record.clientVersion, '1.2.3')
  assert.equal(rotated?.record.installationLabel, 'Quadra')
  assert.equal(await service.authenticate(token), undefined)
  assert.ok(await service.authenticate(rotated!.token))
})
