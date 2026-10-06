import assert from 'node:assert/strict'
import test from 'node:test'
import { execFileSync } from 'node:child_process'
import { generateKeyPairSync, sign as edSign, type KeyObject } from 'node:crypto'
import { mkdtempSync, mkdirSync, readFileSync, readlinkSync, writeFileSync, existsSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { compareVersions } from '../src/update/version.js'
import { ManifestError, parseManifest } from '../src/update/manifest.js'
import { applyUpdate, assertSafeMembers, checkForUpdate, rollback, sha256Hex, APP, RELEASE_PREFIX, SIGNATURE_URL, type Fetcher } from '../src/update/install.js'
import { SignatureError, verifyManifestSignature } from '../src/update/signature.js'
import { UPDATE_PUBLIC_KEY_HEX } from '../src/update/update_key.js'

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

// A throwaway key made in-process for these tests: nothing is signed with
// Platinum's real key and no private key is stored anywhere.
function testKey(): { privateKey: KeyObject; publicHex: string } {
  const { publicKey, privateKey } = generateKeyPairSync('ed25519')
  const der = publicKey.export({ format: 'der', type: 'spki' })
  return { privateKey, publicHex: der.subarray(der.length - 32).toString('hex') }
}
const KEY = testKey()

function fetcherFor(manifest: string, archive: Buffer, opts: { key?: KeyObject; noSig?: boolean; sigText?: string } = {}): Fetcher {
  return async url => {
    if (url === SIGNATURE_URL) {
      if (opts.noSig) throw new Error('download failed: HTTP 404')
      return Buffer.from(opts.sigText ?? `${edSign(null, Buffer.from(manifest), opts.key ?? KEY.privateKey).toString('hex')}\n`)
    }
    return url.endsWith('update.json') ? Buffer.from(manifest) : archive
  }
}

// checkForUpdate with the throwaway key standing in for the release key.
const check = (current: string, fetcher: Fetcher) => checkForUpdate(current, fetcher, undefined, undefined, KEY.publicHex)

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

  const c1 = await check('0.3.1', fetcherFor(manifestFor('0.4.0', v1.data), v1.data))
  assert.equal(c1.available, true)
  const dir1 = await applyUpdate(c1, root, fetcherFor('', v1.data))
  assert.equal(readlinkSync(join(root, 'current')), dir1)
  assert.equal(existsSync(join(root, 'previous')), false)

  const c2 = await check('0.4.0', fetcherFor(manifestFor('0.5.0', v2.data), v2.data))
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
  const found = await check('0.3.1', fetcherFor(manifestFor('0.4.0', v.data), v.data))
  const tampered = Buffer.from(v.data)
  tampered[tampered.length - 1] = tampered[tampered.length - 1]! ^ 0xff
  await assert.rejects(applyUpdate(found, root, fetcherFor('', tampered)), /SHA-256 does not match|archive size/)
  assert.equal(existsSync(join(root, 'current')), false)
  assert.equal(existsSync(join(root, 'releases', '0.4.0')), false)
})

test('no update is applied when the manifest is not newer', async () => {
  const v = makeArchive('0.3.1')
  const found = await check('0.3.1', fetcherFor(manifestFor('0.3.1', v.data), v.data))
  assert.equal(found.available, false)
  await assert.rejects(applyUpdate(found, mkdtempSync(join(tmpdir(), 'plat-root-')), fetcherFor('', v.data)), /no newer/)
})

test('unsafe archive members are rejected', () => {
  assert.throws(() => assertSafeMembers('./ok\n../escape\n'))
  assert.throws(() => assertSafeMembers('/etc/passwd\n'))
  assert.doesNotThrow(() => assertSafeMembers('./dist/server.js\npackage.json\n'))
})

const edvec = JSON.parse(readFileSync(new URL('./vectors/ed25519.json', import.meta.url), 'utf8')) as {
  valid: { note: string; public: string; message_hex: string; signature: string }[]
  invalid: { note: string; public: string; message_hex: string; signature: string }[]
}

test('signature verification matches Wolfram\'s Ed25519 vectors', () => {
  assert.ok(edvec.valid.length >= 18 && edvec.invalid.length >= 8)
  for (const v of edvec.valid) {
    assert.doesNotThrow(() => verifyManifestSignature(Buffer.from(v.message_hex, 'hex'), v.signature, v.public), v.note)
    assert.doesNotThrow(() => verifyManifestSignature(Buffer.from(v.message_hex, 'hex'), `${v.signature}\n`, v.public), v.note)
  }
  // Node's own verifier agrees with Wolfram's on the non-canonical S (S + L) and S = L.
  for (const v of edvec.invalid) {
    assert.throws(() => verifyManifestSignature(Buffer.from(v.message_hex, 'hex'), v.signature, v.public), SignatureError, v.note)
  }
})

test('the signature text must be exactly 128 hex characters', () => {
  const v = edvec.valid[0]!
  const m = Buffer.from(v.message_hex, 'hex')
  for (const bad of ['', v.signature.slice(2), `${v.signature}00`, `${v.signature}\n\n`, ` ${v.signature}`, `${'g'}${v.signature.slice(1)}`]) {
    assert.throws(() => verifyManifestSignature(m, bad, v.public), (e: unknown) => e instanceof SignatureError && e.kind === 'parse', JSON.stringify(bad.slice(0, 12)))
  }
  assert.doesNotThrow(() => verifyManifestSignature(m, `${v.signature}\r\n`, v.public))
})

test('an unsigned or wrongly signed release is refused before its manifest is read', async () => {
  const v = makeArchive('0.4.0')
  const manifest = manifestFor('0.4.0', v.data)
  const other = testKey()
  await assert.rejects(check('0.3.1', fetcherFor(manifest, v.data, { noSig: true })), /not signed/)
  await assert.rejects(check('0.3.1', fetcherFor(manifest, v.data, { key: other.privateKey })), SignatureError)
  await assert.rejects(check('0.3.1', fetcherFor(manifest, v.data, { sigText: '0'.repeat(128) })), SignatureError)
  // A manifest changed after it was signed.
  const signed = fetcherFor(manifest, v.data)
  const swapped: Fetcher = async (url, max) => (url === SIGNATURE_URL ? signed(url, max) : Buffer.from(manifest.replace('"notes":"test"', '"notes":"evil"')))
  await assert.rejects(check('0.3.1', swapped), SignatureError)
  // Platinum's real key does not accept a manifest signed by the throwaway key, and the built-in key is well formed.
  await assert.rejects(checkForUpdate('0.3.1', fetcherFor(manifest, v.data)), SignatureError)
  assert.match(UPDATE_PUBLIC_KEY_HEX, /^[0-9a-f]{64}$/)
  // With the right key it is accepted, so the refusals above were the signature.
  assert.equal((await check('0.3.1', fetcherFor(manifest, v.data))).available, true)
})
