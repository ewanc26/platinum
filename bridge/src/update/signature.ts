import { createPublicKey, verify } from 'node:crypto'
import { UPDATE_PUBLIC_KEY_HEX } from './update_key.js'

// Verification of update.json.sig: a detached Ed25519 (RFC 8032) signature over
// the exact bytes of update.json, 128 hex characters and an optional newline.
// This is the contract of Wolfram's wf_update_verify_signature (update.h,
// docs/update.md), run against Wolfram's vectors in test/vectors/ed25519.json.
// Node has Ed25519 built in, so unlike the manifest parser nothing is ported:
// the check is crypto.verify. Call it on the downloaded bytes BEFORE parsing
// them and refuse on any error; an unsigned release is not a valid one.

export class SignatureError extends Error {
  constructor(readonly kind: 'parse' | 'validation', message: string) {
    super(message)
  }
}

// An Ed25519 SubjectPublicKeyInfo is this fixed prefix and the 32 key bytes.
const SPKI_PREFIX = Buffer.from('302a300506032b6570032100', 'hex')

/** The signature text is exactly 128 hex characters, either case, then at most one newline (LF or CRLF). */
function decodeSignature(text: string): Buffer {
  let t = text
  if (t.endsWith('\n')) t = t.slice(0, -1)
  if (t.endsWith('\r')) t = t.slice(0, -1)
  if (!/^[0-9a-fA-F]{128}$/.test(t)) throw new SignatureError('parse', 'signature is not 128 hex characters')
  return Buffer.from(t, 'hex')
}

/** Throws SignatureError unless `sigText` is a valid signature of `manifest` under `publicKeyHex`. */
export function verifyManifestSignature(manifest: Buffer, sigText: string, publicKeyHex: string = UPDATE_PUBLIC_KEY_HEX): void {
  if (!/^[0-9a-fA-F]{64}$/.test(publicKeyHex)) throw new SignatureError('parse', 'public key is not 64 hex characters')
  const sig = decodeSignature(sigText)
  let ok = false
  try {
    const key = createPublicKey({ key: Buffer.concat([SPKI_PREFIX, Buffer.from(publicKeyHex, 'hex')]), format: 'der', type: 'spki' })
    ok = verify(null, manifest, key, sig)
  } catch {
    ok = false
  }
  if (!ok) throw new SignatureError('validation', 'update.json is not signed by the release key')
}
