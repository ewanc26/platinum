import assert from 'node:assert/strict'
import { test } from 'node:test'
import { mkdir, mkdtemp, readFile, rm } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import type { NodeSavedState } from '@atproto/oauth-client-node'
import type { InstallationRecord } from '../src/storage/interfaces.js'
import { FileStorage } from '../src/storage/file.js'

// A directory where the temporary file should go makes the next write fail
// before anything is renamed over the real file.
async function withStorage(run: (root: string, storage: FileStorage) => Promise<void>): Promise<void> {
  const root = await mkdtemp(join(tmpdir(), 'platinum-storage-'))
  try {
    const storage = new FileStorage(root)
    await storage.init()
    await run(root, storage)
  } finally {
    await rm(root, { recursive: true, force: true })
  }
}

test('a failed key-value write leaves the previous file intact', async () => {
  await withStorage(async (root, storage) => {
    const states = storage.stateStore()
    await states.set('first', { nonce: 'one' } as unknown as NodeSavedState)

    await mkdir(join(root, 'states.json.tmp'))
    await assert.rejects(states.set('second', { nonce: 'two' } as unknown as NodeSavedState))

    const kept = JSON.parse(await readFile(join(root, 'states.json'), 'utf8')) as Record<string, unknown>
    assert.deepEqual(Object.keys(kept), ['first'])
  })
})

test('a failed installation write leaves the previous file intact', async () => {
  await withStorage(async (root, storage) => {
    const installations = storage.installations()
    await installations.create({ id: 'one', did: 'did:plc:one' } as unknown as InstallationRecord)

    await mkdir(join(root, 'installations.json.tmp'))
    await assert.rejects(installations.create({ id: 'two', did: 'did:plc:two' } as unknown as InstallationRecord))

    const kept = JSON.parse(await readFile(join(root, 'installations.json'), 'utf8')) as Record<string, unknown>
    assert.deepEqual(Object.keys(kept), ['one'])
  })
})

test('a successful key-value write replaces the file and leaves no temporary file behind', async () => {
  await withStorage(async (root, storage) => {
    const sessions = storage.sessionStore()
    await sessions.set('did:plc:one', { tokenSet: {} } as never)
    await sessions.set('did:plc:two', { tokenSet: {} } as never)

    const kept = JSON.parse(await readFile(join(root, 'sessions.json'), 'utf8')) as Record<string, unknown>
    assert.deepEqual(Object.keys(kept).sort(), ['did:plc:one', 'did:plc:two'])
    await assert.rejects(readFile(join(root, 'sessions.json.tmp'), 'utf8'), { code: 'ENOENT' })
  })
})
