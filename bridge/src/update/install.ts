import { createHash } from 'node:crypto'
import { execFile } from 'node:child_process'
import { promisify } from 'node:util'
import { mkdir, readlink, rename, rm, symlink, writeFile, readFile, lstat } from 'node:fs/promises'
import { join } from 'node:path'
import { DEFAULT_MAX_SIZE, parseManifest, type Asset, type Manifest } from './manifest.js'
import { compareVersions } from './version.js'
import { verifyManifestSignature } from './signature.js'
import { UPDATE_PUBLIC_KEY_HEX } from './update_key.js'

const execFileAsync = promisify(execFile)

export const APP = 'platinum-bridge'
export const RELEASE_PREFIX = 'https://github.com/ewanc26/platinum/releases/'
export const MANIFEST_URL = `${RELEASE_PREFIX}latest/download/update.json`
export const SIGNATURE_URL = `${RELEASE_PREFIX}latest/download/update.json.sig`

export type Fetcher = (url: string, maxBytes: number) => Promise<Buffer>

/** Fetch with a hard size cap. Sends no credentials of any kind. */
export const httpsFetcher: Fetcher = async (url, maxBytes) => {
  const res = await fetch(url, { redirect: 'follow', headers: { 'user-agent': 'platinum-bridge-updater' } })
  if (!res.ok) throw new Error(`download failed: HTTP ${res.status}`)
  const declared = Number(res.headers.get('content-length') ?? 0)
  if (declared > maxBytes) throw new Error('download exceeds size limit')
  const chunks: Buffer[] = []
  let total = 0
  for await (const chunk of res.body as unknown as AsyncIterable<Uint8Array>) {
    total += chunk.length
    if (total > maxBytes) throw new Error('download exceeds size limit')
    chunks.push(Buffer.from(chunk))
  }
  return Buffer.concat(chunks)
}

export interface UpdateCheck {
  current: string
  latest: string
  available: boolean
  manifest: Manifest
}

export async function checkForUpdate(
  current: string,
  fetcher: Fetcher = httpsFetcher,
  manifestUrl = MANIFEST_URL,
  signatureUrl = SIGNATURE_URL,
  publicKeyHex = UPDATE_PUBLIC_KEY_HEX,
): Promise<UpdateCheck> {
  const body = await fetcher(manifestUrl, 64 * 1024)
  // Authenticity first, on the bytes as downloaded and before anything is
  // parsed: a release with no signature, or one signed by another key, is
  // refused here, not trusted on its SHA-256.
  let sig: Buffer
  try {
    sig = await fetcher(signatureUrl, 256)
  } catch {
    throw new Error('the latest release is not signed, so it was refused')
  }
  verifyManifestSignature(body, sig.toString('utf8'), publicKeyHex)
  const manifest = parseManifest(body.toString('utf8'), { app: APP, urlPrefix: `${RELEASE_PREFIX}download/` })
  // The manifest's own `signature` member stays reserved (Wolfram's docs/update.md):
  // the signature that counts is the detached one above, so a manifest that
  // claims another is refused rather than trusted unchecked.
  if (manifest.hasSignature) throw new Error('manifest carries an inline signature, which is not part of the contract')
  return { current, latest: manifest.version, available: compareVersions(manifest.version, current) > 0, manifest }
}

export function sha256Hex(data: Buffer): string {
  return createHash('sha256').update(data).digest('hex')
}

function pickArtifact(manifest: Manifest): Asset {
  if (manifest.asset.name !== `platinum-bridge-${manifest.version}.tar.gz`) {
    throw new Error('manifest asset is not the bridge archive for its own version')
  }
  return manifest.asset
}

/** Reject archive members that could write outside the release directory. */
export function assertSafeMembers(listing: string): void {
  for (const line of listing.split('\n')) {
    const name = line.trim()
    if (!name) continue
    if (name.startsWith('/') || name.split('/').includes('..') || name.includes('\0')) {
      throw new Error(`unsafe archive member: ${JSON.stringify(name.slice(0, 80))}`)
    }
  }
}

export interface Layout {
  root: string
  releases: string
  current: string
  previous: string
}

export function layout(root: string): Layout {
  return { root, releases: join(root, 'releases'), current: join(root, 'current'), previous: join(root, 'previous') }
}

async function pointTo(link: string, target: string): Promise<void> {
  const tmp = `${link}.new-${process.pid}`
  await rm(tmp, { force: true })
  await symlink(target, tmp, 'dir')
  await rename(tmp, link) // atomic replace
}

async function readLink(link: string): Promise<string | undefined> {
  try {
    return await readlink(link)
  } catch {
    return undefined
  }
}

/**
 * Download, verify and unpack a release into releases/<version>, then repoint
 * `current`. The previous target is kept as `previous` for rollback. Nothing is
 * executed from the archive and nothing is unpacked before the SHA-256 matches.
 * The running process is not touched: the new version takes effect when the
 * operator restarts the bridge.
 */
export async function applyUpdate(check: UpdateCheck, root: string, fetcher: Fetcher = httpsFetcher): Promise<string> {
  if (!check.available) throw new Error('no newer version to install')
  const artifact = pickArtifact(check.manifest)
  const lay = layout(root)
  await mkdir(lay.releases, { recursive: true })

  const archive = await fetcher(artifact.url, Math.min(artifact.size, DEFAULT_MAX_SIZE))
  if (archive.length !== artifact.size) throw new Error('archive size does not match the manifest')
  if (sha256Hex(archive) !== artifact.sha256) throw new Error('archive SHA-256 does not match the manifest; nothing was installed')

  const dest = join(lay.releases, check.latest)
  const staging = `${dest}.staging-${process.pid}`
  await rm(staging, { recursive: true, force: true })
  await mkdir(staging, { recursive: true })
  const file = join(staging, '.archive.tgz')
  await writeFile(file, archive)
  try {
    const { stdout } = await execFileAsync('tar', ['-tzf', file], { maxBuffer: 16 * 1024 * 1024 })
    assertSafeMembers(stdout)
    await execFileAsync('tar', ['-xzf', file, '-C', staging, '--no-same-owner', '--no-same-permissions'])
    await rm(file)
    await rm(dest, { recursive: true, force: true })
    await rename(staging, dest)
  } catch (err) {
    await rm(staging, { recursive: true, force: true })
    throw err
  }

  const before = await readLink(lay.current)
  if (before) await pointTo(lay.previous, before)
  await pointTo(lay.current, dest)
  return dest
}

/** Swap `current` back to the version it replaced. */
export async function rollback(root: string): Promise<string> {
  const lay = layout(root)
  const prev = await readLink(lay.previous)
  if (!prev) throw new Error('there is no previous version to roll back to')
  const cur = await readLink(lay.current)
  await pointTo(lay.current, prev)
  if (cur) await pointTo(lay.previous, cur)
  return prev
}

export async function installedVersionDir(root: string): Promise<string | undefined> {
  const lay = layout(root)
  try {
    await lstat(lay.current)
    return await readLink(lay.current)
  } catch {
    return undefined
  }
}

export async function readPackageVersion(packageJsonPath: string): Promise<string> {
  const pkg = JSON.parse(await readFile(packageJsonPath, 'utf8')) as { version?: string }
  if (typeof pkg.version !== 'string') throw new Error('package.json has no version')
  return pkg.version
}
