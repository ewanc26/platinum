import { statusCode, wolfram, type WolframManifest } from './wolfram.js'

// update.json: Wolfram's wf_update_parse_manifest (update.h, docs/update.md),
// running in Wolfram's C through the Node addon. The adapter keeps the
// bridge's terms and adds nothing of its own: sizes and prefixes in Policy,
// the parsed manifest with its schema field, and ManifestError.kind --
// 'parse' where Wolfram returns WF_ERR_PARSE and 'validation' where it
// returns WF_ERR_VALIDATION (a policy mismatch). Field caps, the url rules
// and the signature absence rule all live in the C now.
//
// The bytes cap stays here because it is fetch-size policy rather than parse
// logic: Wolfram's parser accepts any body length it is handed.

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

export class ManifestError extends Error {
  constructor(readonly kind: 'parse' | 'validation', message: string) {
    super(message)
  }
}

export function parseManifest(text: string, policy: Policy = {}): Manifest {
  if (Buffer.byteLength(text) > MAX_MANIFEST_BYTES) throw new ManifestError('parse', 'manifest too large')
  let core: WolframManifest
  try {
    core = wolfram.parseManifest(text, policy)
  } catch (err) {
    if (statusCode(err) === 'WF_ERR_VALIDATION') {
      throw new ManifestError('validation', 'manifest is outside the update policy')
    }
    throw new ManifestError('parse', 'manifest is not a valid update manifest')
  }
  return {
    schema: 1,
    app: core.app,
    version: core.version,
    notes: core.notes,
    asset: { ...core.asset },
    hasSignature: core.hasSignature,
  }
}
