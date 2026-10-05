import { parseVersion } from './version.js'

// update.json, written by scripts/release.sh and attached to each GitHub
// release. Shape follows ewanc26/wolfram#106 (the canonical shared contract):
// { schema, app, version, notes, asset: { name, url, size, sha256 }, signature }.
// This is the Node port; vectors are in test/update-vectors.json and should be
// replaced by Wolfram's test/vectors/update/ once published.

export interface Asset {
  name: string
  url: string
  size: number
  sha256: string
}

export interface Manifest {
  schema: 1
  app: string
  version: string
  notes: string
  asset: Asset
  signature: null
}

export const MAX_MANIFEST_BYTES = 64 * 1024
export const MAX_ARTIFACT_BYTES = 128 * 1024 * 1024

export class ManifestError extends Error {}

function fail(msg: string): never {
  throw new ManifestError(msg)
}

export function parseManifest(text: string, opts: { app: string; urlPrefix: string }): Manifest {
  if (Buffer.byteLength(text) > MAX_MANIFEST_BYTES) fail('manifest too large')
  let raw: unknown
  try {
    raw = JSON.parse(text)
  } catch {
    fail('manifest is not JSON')
  }
  if (typeof raw !== 'object' || raw === null) fail('manifest is not an object')
  const o = raw as Record<string, unknown>
  if (o.schema !== 1) fail('unsupported manifest schema')
  if (o.app !== opts.app) fail('manifest is for a different app')
  if (typeof o.version !== 'string') fail('manifest has no version')
  try {
    parseVersion(o.version)
  } catch {
    fail('manifest version is not a version')
  }
  if (typeof o.notes !== 'string' || o.notes.length > 1024) fail('manifest notes missing or too long')
  // A signature is reserved. Until a key exists, anything but null is refused
  // so a manifest cannot claim a signature this code does not check.
  if (o.signature !== null) fail('manifest carries a signature this updater cannot verify')
  const a = o.asset
  if (typeof a !== 'object' || a === null) fail('manifest has no asset')
  const r = a as Record<string, unknown>
  if (typeof r.name !== 'string' || !/^[A-Za-z0-9._-]{1,96}$/.test(r.name)) fail('asset name is not a plain file name')
  if (typeof r.url !== 'string' || r.url.length > 512 || !r.url.startsWith('https://') || !r.url.startsWith(opts.urlPrefix)) {
    fail('asset url is not an https URL under the release prefix')
  }
  const size = r.size
  if (typeof size !== 'number' || !Number.isSafeInteger(size) || size <= 0 || size > MAX_ARTIFACT_BYTES) fail('asset size is out of range')
  if (typeof r.sha256 !== 'string' || !/^[0-9a-f]{64}$/.test(r.sha256)) fail('asset sha256 is not 64 lowercase hex digits')
  return {
    schema: 1,
    app: o.app,
    version: o.version,
    notes: o.notes,
    asset: { name: r.name, url: r.url, size, sha256: r.sha256 },
    signature: null,
  }
}
