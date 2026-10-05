// Checks what scripts/release.sh --dry-run wrote to release/, using the
// updater's own parser, so a release the updater would refuse cannot be cut.
//   node --import tsx scripts/verify-release.mts <version>
import { createHash } from 'node:crypto'
import { readFileSync } from 'node:fs'
import { parseManifest } from '../bridge/src/update/manifest.ts'
import { APP, RELEASE_PREFIX } from '../bridge/src/update/install.ts'

const dir = new URL('../release/', import.meta.url)
const version = process.argv[2]
const fail = (m: string): never => { console.error(`verify-release: ${m}`); process.exit(1) }
if (!version) fail('usage: verify-release.mts <version>')
const manifest = parseManifest(readFileSync(new URL('update.json', dir), 'utf8'), { app: APP, urlPrefix: RELEASE_PREFIX })
if (manifest.version !== version) fail(`manifest version ${manifest.version} != ${version}`)
const archive = readFileSync(new URL(manifest.asset.name, dir))
if (archive.length !== manifest.asset.size) fail('archive size does not match the manifest')
const sha = createHash('sha256').update(archive).digest('hex')
if (sha !== manifest.asset.sha256) fail('archive SHA-256 does not match the manifest')
const sidecar = readFileSync(new URL(`${manifest.asset.name}.sha256`, dir), 'utf8').split(/\s+/)[0]
if (sidecar !== sha) fail('.sha256 sidecar does not match the archive')
if (!manifest.asset.url.endsWith(`/download/v${version}/${manifest.asset.name}`)) fail('asset URL does not point at this tag')
console.log(`ok: release ${version} (${manifest.asset.name}, ${archive.length} bytes)`)
