import { statusCode, wolfram } from '../wolfram.js'
import { UPDATE_PUBLIC_KEY_HEX } from './update_key.js'

// Verification of update.json.sig: a detached Ed25519 (RFC 8032) signature over
// the exact bytes of update.json, 128 hex characters and an optional newline.
// This is Wolfram's wf_update_verify_signature (update.h, docs/update.md),
// running in Wolfram's C through the Node addon -- the Ed25519 check is no
// longer Node's. Call it on the downloaded bytes BEFORE parsing them and
// refuse on any error; an unsigned release is not a valid one.

export class SignatureError extends Error {
  constructor(readonly kind: 'parse' | 'validation', message: string) {
    super(message)
  }
}

/** Throws SignatureError unless `sigText` is a valid signature of `manifest` under `publicKeyHex`. */
export function verifyManifestSignature(manifest: Buffer, sigText: string, publicKeyHex: string = UPDATE_PUBLIC_KEY_HEX): void {
  try {
    wolfram.verifySignature(manifest, sigText, publicKeyHex)
  } catch (err) {
    if (statusCode(err) === 'WF_ERR_VALIDATION') {
      throw new SignatureError('validation', 'update.json is not signed by the release key')
    }
    throw new SignatureError('parse', 'signature text or public key is malformed')
  }
}
