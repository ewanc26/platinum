import assert from 'node:assert/strict'
import { spawn, type ChildProcess } from 'node:child_process'
import { mkdtemp, rm } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import test from 'node:test'

// Starts the real server in a child process so the test exercises the
// routing order, not a copy of it.
async function startServer(): Promise<{ baseUrl: string; child: ChildProcess; dataDir: string }> {
  const dataDir = await mkdtemp(join(tmpdir(), 'platinum-revoke-'))
  const port = 20000 + Math.floor(Math.random() * 20000)
  const baseUrl = `http://127.0.0.1:${port}`
  const child = spawn(process.execPath, ['--import', 'tsx', 'src/server.ts'], {
    env: {
      ...process.env,
      PLATINUM_BRIDGE_PORT: String(port),
      PLATINUM_BRIDGE_URL: baseUrl,
      // The OAuth client metadata needs an https client id that is not an IP.
      PLATINUM_BRIDGE_PUBLIC_URL: 'https://platinum-bridge.example',
      PLATINUM_BRIDGE_DATA_DIR: dataDir,
    },
    stdio: ['ignore', 'pipe', 'pipe'],
  })

  await new Promise<void>((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('server did not start')), 20000)
    child.stdout?.on('data', chunk => {
      if (String(chunk).includes('listening')) {
        clearTimeout(timer)
        resolve()
      }
    })
    child.on('exit', code => reject(new Error(`server exited with ${code}`)))
  })

  return { baseUrl, child, dataDir }
}

test('POST /v1/revoke is refused without a valid bearer, like every other /v1 route', async () => {
  const { baseUrl, child, dataDir } = await startServer()
  try {
    const none = await fetch(`${baseUrl}/v1/revoke`, { method: 'POST' })
    assert.equal(none.status, 401)
    assert.equal((await none.json() as { error: string }).error, 'missing_bearer_token')

    const bogus = await fetch(`${baseUrl}/v1/revoke`, {
      method: 'POST',
      headers: { authorization: 'Bearer not-a-real-token' },
    })
    assert.equal(bogus.status, 401)
    assert.equal((await bogus.json() as { error: string }).error, 'invalid_token')

    const profile = await fetch(`${baseUrl}/v1/profile`)
    assert.equal(profile.status, 401)
  } finally {
    child.kill()
    await rm(dataDir, { recursive: true, force: true })
  }
})
