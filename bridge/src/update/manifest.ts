import { parseVersion } from './version.js'

// update.json: a port of Wolfram's wf_update_parse_manifest (update.h,
// docs/update.md, ewanc26/wolfram#106), tested against Wolfram's vectors in
// test/vectors/update/manifest.json. The bridge is Node and cannot call
// Wolfram's C, so this copy exists; it is deleted if a Node binding appears.
//
// ManifestError.kind is 'parse' where Wolfram returns WF_ERR_PARSE and
// 'validation' where it returns WF_ERR_VALIDATION (a policy mismatch).

export interface Asset {
  name: string
  url: string
  size: number
  sha256: string // lowercase hex
}

export interface Manifest {
  schema: 1
  app: string
  version: string
  notes: string
  asset: Asset
  /** A signature was present. It is reserved and NOT verified (#49). */
  hasSignature: boolean
}

export interface Policy {
  /** Reject assets larger than this; default 64 MiB, as in Wolfram. */
  maxSize?: number
  app?: string
  urlPrefix?: string
}

export const DEFAULT_MAX_SIZE = 64 * 1024 * 1024
export const MAX_MANIFEST_BYTES = 64 * 1024
const APP_MAX = 32
const VERSION_MAX = 32
const NOTES_MAX = 1024
const NAME_MAX = 96
const URL_MAX = 512

export class ManifestError extends Error {
  constructor(readonly kind: 'parse' | 'validation', message: string) {
    super(message)
  }
}

const parseFail = (m: string): never => { throw new ManifestError('parse', m) }

/** Wolfram's buffers hold cap-1 bytes plus a NUL; a longer value is refused, never cut. */
function fits(o: Record<string, unknown>, key: string, cap: number): string | undefined {
  const v = o[key]
  if (typeof v !== 'string' || Buffer.byteLength(v) >= cap) return undefined
  return v
}

/** https://host[:port]/..., no userinfo, no whitespace, control bytes or backslash. */
function urlOk(u: string): boolean {
  if (!u.startsWith('https://')) return false
  for (const ch of Buffer.from(u)) if (ch <= 0x20 || ch === 0x7f || ch === 0x5c) return false
  const rest = u.slice(8)
  const end = rest.search(/[/?#]/)
  const host = end < 0 ? rest : rest.slice(0, end)
  return host.length > 0 && !host.includes('@')
}

export function parseManifest(text: string, policy: Policy = {}): Manifest {
  if (Buffer.byteLength(text) > MAX_MANIFEST_BYTES) parseFail('manifest too large')
  let raw: unknown
  try {
    raw = JSON.parse(text)
  } catch {
    parseFail('manifest is not JSON')
  }
  if (typeof raw !== 'object' || raw === null || Array.isArray(raw)) parseFail('manifest is not an object')
  const o = raw as Record<string, unknown>
  if (o.schema !== 1) parseFail('unsupported manifest schema')
  const app = fits(o, 'app', APP_MAX)
  const version = fits(o, 'version', VERSION_MAX)
  if (!app || !version) parseFail('manifest app or version missing or too long')
  try {
    parseVersion(version!)
  } catch {
    parseFail('manifest version is not a version')
  }
  const asset = o.asset
  if (typeof asset !== 'object' || asset === null || Array.isArray(asset)) parseFail('manifest has no asset')
  const a = asset as Record<string, unknown>
  const name = fits(a, 'name', NAME_MAX)
  const url = fits(a, 'url', URL_MAX)
  const sha = fits(a, 'sha256', 65)
  if (!name || !url || !sha || !urlOk(url)) parseFail('asset name, url or sha256 missing, too long or unsafe')
  const size = a.size
  if (typeof size !== 'number' || !Number.isInteger(size) || size < 1 || size > 4294967295) parseFail('asset size out of range')
  if (!/^[0-9A-Fa-f]{64}$/.test(sha!)) parseFail('asset sha256 is not 64 hex digits')
  let notes = ''
  if (o.notes !== undefined && o.notes !== null) {
    if (typeof o.notes !== 'string' || Buffer.byteLength(o.notes) >= NOTES_MAX) parseFail('notes too long')
    notes = o.notes as string
  }
  const manifest: Manifest = {
    schema: 1,
    app: app!,
    version: version!,
    notes,
    asset: { name: name!, url: url!, size: size as number, sha256: sha!.toLowerCase() },
    hasSignature: o.signature !== undefined && o.signature !== null,
  }

  const max = policy.maxSize || DEFAULT_MAX_SIZE
  if (manifest.asset.size > max) throw new ManifestError('validation', 'asset larger than the policy allows')
  if (policy.app !== undefined && policy.app !== manifest.app) throw new ManifestError('validation', 'manifest is for a different app')
  if (policy.urlPrefix !== undefined) {
    if (!policy.urlPrefix.startsWith('https://') || !manifest.asset.url.startsWith(policy.urlPrefix)) {
      throw new ManifestError('validation', 'asset url is outside the release prefix')
    }
  }
  return manifest
}
