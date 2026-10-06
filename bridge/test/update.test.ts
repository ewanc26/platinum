import assert from 'node:assert/strict'
import test from 'node:test'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, mkdirSync, readFileSync, readlinkSync, writeFileSync, existsSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { compareVersions } from '../src/update/version.js'
import { ManifestError, parseManifest } from '../src/update/manifest.js'
import { applyUpdate, assertSafeMembers, checkForUpdate, rollback, sha256Hex, APP, RELEASE_PREFIX, type Fetcher } from '../src/update/install.js'

// Wolfram's own vectors (test/vectors/update/), copied verbatim at the commit
// pinned in ci.yml; CI fails if the copies drift.
const vec = (name: string) => JSON.parse(readFileSync(new URL(`./vectors/update/${name}.json`, import.meta.url), 'utf8'))
const versions = vec('versions') as { compare: { a: string; b: string; sign: number }[]; invalid: string[] }
const sha256s = vec('sha256') as { vectors: { name: string; input_hex?: string; repeat?: { byte_hex: string; count: number }; sha256: string }[] }
const manifests = vec('manifest') as { vectors: { name: string; manifest: unknown; expect: string; has_signature?: boolean; policy?: { max_size?: number; app?: string; url_prefix?: string } }[] }

test('version comparison matches Wolfram\'s vectors', () => {
  for (const { a, b, sign } of versions.compare) assert.equal(Math.sign(compareVersions(a, b)), sign, `${a} vs ${b}`)
  for (const bad of versions.invalid) {
    assert.throws(() => compareVersions(bad, '1.0.0'), undefined, JSON.stringify(bad))
    assert.throws(() => compareVersions('1.0.0', bad), undefined, JSON.stringify(bad))
  }
})

test('SHA-256 matches Wolfram\'s known answers', () => {
  for (const v of sha256s.vectors) {
    const input = v.repeat ? Buffer.alloc(v.repeat.count, Number.parseInt(v.repeat.byte_hex, 16)) : Buffer.from(v.input_hex ?? '', 'hex')
    assert.equal(sha256Hex(input), v.sha256, v.name)
  }
})

test('manifest parsing matches Wolfram\'s vectors', () => {
  for (const v of manifests.vectors) {
    const policy = { maxSize: v.policy?.max_size, app: v.policy?.app, urlPrefix: v.policy?.url_prefix }
    const run = () => parseManifest(JSON.stringify(v.manifest), policy)
    if (v.expect === 'ok') {
      const m = run()
      assert.equal(m.hasSignature, v.has_signature ?? false, v.name)
    } else {
      assert.throws(run, (e: unknown) => e instanceof ManifestError && e.kind === (v.expect === 'parse_error' ? 'parse' : 'validation'), v.name)
    }
  }
})

function makeArchive(version: string, extra?: string): { data: Buffer; dir: string } {
  const dir = mkdtempSync(join(tmpdir(), 'plat-arc-'))
  const src = join(dir, 'src')
  mkdirSync(join(src, 'dist'), { recursive: true })
  writeFileSync(join(src, 'package.json'), JSON.stringify({ version }))
  writeFileSync(join(src, 'dist', 'server.js'), `// ${version}\n`)
  const out = join(dir, 'a.tgz')
  execFileSync('tar', ['-czf', out, '-C', src, ...(extra ? [extra] : ['.'])])
  return { data: readFileSync(out), dir }
}

function manifestFor(version: string, data: Buffer): string {
  return JSON.stringify({
    schema: 1, app: APP, version, notes: 'test',
    asset: { name: `platinum-bridge-${version}.tar.gz`, url: `${RELEASE_PREFIX}download/v${version}/platinum-bridge-${version}.tar.gz`, size: data.length, sha256: sha256Hex(data) },
    signature: null,
  })
}

function fetcherFor(manifest: string, archive: Buffer): Fetcher {
  return async url => (url.endsWith('update.json') ? Buffer.from(manifest) : archive)
}

test('manifest rejects wrong product, schema, host and hashes', () => {
  const opts = { app: APP, urlPrefix: RELEASE_PREFIX }
  const good = JSON.parse(manifestFor('0.4.0', Buffer.from('x')))
  assert.doesNotThrow(() => parseManifest(JSON.stringify(good), opts))
  const mutate = (f: (m: any) => void) => { const m = structuredClone(good); f(m); return JSON.stringify(m) }
  assert.throws(() => parseManifest(mutate(m => { m.app = 'other' }), opts))
  assert.throws(() => parseManifest(mutate(m => { m.schema = 2 }), opts))
  assert.throws(() => parseManifest(mutate(m => { m.asset.url = 'https://evil.example/a.tgz' }), opts))
  assert.throws(() => parseManifest(mutate(m => { m.asset.url = RELEASE_PREFIX.replace('https', 'http') + 'x' }), opts))
  assert.throws(() => parseManifest(mutate(m => { m.asset.sha256 = 'ABC' }), opts))
  assert.throws(() => parseManifest(mutate(m => { m.asset.size = 0 }), opts))
  assert.throws(() => parseManifest('not json', opts))
})

test('update installs a verified archive, keeps the previous version, and rolls back', async () => {
  const root = mkdtempSync(join(tmpdir(), 'plat-root-'))
  const v1 = makeArchive('0.4.0')
  const v2 = makeArchive('0.5.0')

  const c1 = await checkForUpdate('0.3.1', fetcherFor(manifestFor('0.4.0', v1.data), v1.data))
  assert.equal(c1.available, true)
  const dir1 = await applyUpdate(c1, root, fetcherFor('', v1.data))
  assert.equal(readlinkSync(join(root, 'current')), dir1)
  assert.equal(existsSync(join(root, 'previous')), false)

  const c2 = await checkForUpdate('0.4.0', fetcherFor(manifestFor('0.5.0', v2.data), v2.data))
  const dir2 = await applyUpdate(c2, root, fetcherFor('', v2.data))
  assert.equal(readlinkSync(join(root, 'current')), dir2)
  assert.equal(readlinkSync(join(root, 'previous')), dir1)
  assert.equal(readFileSync(join(dir1, 'dist', 'server.js'), 'utf8'), '// 0.4.0\n')

  assert.equal(await rollback(root), dir1)
  assert.equal(readlinkSync(join(root, 'current')), dir1)
})

test('a tampered archive is refused and nothing is installed', async () => {
  const root = mkdtempSync(join(tmpdir(), 'plat-root-'))
  const v = makeArchive('0.4.0')
  const check = await checkForUpdate('0.3.1', fetcherFor(manifestFor('0.4.0', v.data), v.data))
  const tampered = Buffer.from(v.data)
  tampered[tampered.length - 1] = tampered[tampered.length - 1]! ^ 0xff
  await assert.rejects(applyUpdate(check, root, fetcherFor('', tampered)), /SHA-256 does not match|archive size/)
  assert.equal(existsSync(join(root, 'current')), false)
  assert.equal(existsSync(join(root, 'releases', '0.4.0')), false)
})

test('no update is applied when the manifest is not newer', async () => {
  const v = makeArchive('0.3.1')
  const check = await checkForUpdate('0.3.1', fetcherFor(manifestFor('0.3.1', v.data), v.data))
  assert.equal(check.available, false)
  await assert.rejects(applyUpdate(check, mkdtempSync(join(tmpdir(), 'plat-root-')), fetcherFor('', v.data)), /no newer/)
})

test('unsafe archive members are rejected', () => {
  assert.throws(() => assertSafeMembers('./ok\n../escape\n'))
  assert.throws(() => assertSafeMembers('/etc/passwd\n'))
  assert.doesNotThrow(() => assertSafeMembers('./dist/server.js\npackage.json\n'))
})
