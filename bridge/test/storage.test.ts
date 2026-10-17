import assert from 'node:assert/strict'
import { mkdtemp, rm } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import test from 'node:test'
import { FileStorage } from '../src/storage/file.js'
import type { InstallationRecord } from '../src/storage/interfaces.js'

function record(id: string, tokenHash: string): InstallationRecord {
  return { id, did: `did:plc:${id}`, tokenHash, createdAt: '2026-10-17T00:00:00.000Z' }
}

test('installations are found by token hash, survive a reopen, and track revocation', async () => {
  const root = await mkdtemp(join(tmpdir(), 'platinum-storage-'))
  try {
    const first = new FileStorage(root)
    await first.init()
    await first.installations().create(record('a', 'hash-a'))
    await first.installations().create(record('b', 'hash-b'))
    assert.equal((await first.installations().getByTokenHash('hash-b'))?.id, 'b')
    assert.equal(await first.installations().getByTokenHash('missing'), undefined)

    assert.equal(await first.installations().revoke('a'), true)
    assert.equal(await first.installations().revoke('a'), false)

    const reopened = new FileStorage(root)
    await reopened.init()
    assert.equal((await reopened.installations().getByTokenHash('hash-a'))?.revokedAt !== undefined, true)
    assert.equal((await reopened.installations().get('b'))?.tokenHash, 'hash-b')
  } finally {
    await rm(root, { recursive: true, force: true })
  }
})

test('creating a duplicate installation id is refused', async () => {
  const root = await mkdtemp(join(tmpdir(), 'platinum-storage-'))
  try {
    const storage = new FileStorage(root)
    await storage.init()
    await storage.installations().create(record('a', 'hash-a'))
    await assert.rejects(storage.installations().create(record('a', 'hash-other')))
  } finally {
    await rm(root, { recursive: true, force: true })
  }
})

test('concurrent writes to one state file all land', async () => {
  const root = await mkdtemp(join(tmpdir(), 'platinum-storage-'))
  try {
    const storage = new FileStorage(root)
    await storage.init()
    const sessions = storage.sessionStore()
    await Promise.all(Array.from({ length: 50 }, (_, i) => sessions.set(`key-${i}`, { value: i } as never)))
    for (let i = 0; i < 50; i++) assert.deepEqual(await sessions.get(`key-${i}`), { value: i })
  } finally {
    await rm(root, { recursive: true, force: true })
  }
})
