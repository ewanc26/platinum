import { createRequire } from 'node:module'
import { fileURLToPath } from 'node:url'

// The bridge runs Wolfram's update logic in Wolfram's own C, through the Node
// addon built from wolfram's node/wolfram_core.c at the commit pinned in
// .github/workflows/ci.yml. This file is glue, not logic: the module's Type-
// Script-facing surface and its loader. The rules it calls live in Wolfram and
// run on its vectors (test/update.test.ts); a status that changes meaning
// there breaks Wolfram's binding tests first, per wolfram's own AGENTS.md.

// Built by tools/build-addon.sh into bridge/build/wolfram_core.node before the
// tests run. WOLFRAM_NODE_ADDON points at another build of it, as in Wolfram's
// own addon tests.
const preset = fileURLToPath(new URL('../../build/wolfram_core.node', import.meta.url))

export interface WolframManifest {
  app: string
  version: string
  notes: string
  asset: { name: string; url: string; size: number; sha256: string }
  hasSignature: boolean
}

export interface UpdatePolicy {
  /** Reject assets larger than this; 0 and absent mean Wolfram's 64 MiB default. */
  maxSize?: number
  app?: string
  urlPrefix?: string
}

// The module throws Error values whose `code` is the wf_status name
// (WF_ERR_PARSE, WF_ERR_VALIDATION, ...); read it, never its message, which is
// Wolfram's call site and does not read as prose.
export interface Wolfram {
  compareVersions(a: string, b: string): number
  sha256Hex(data: Buffer | string): string
  parseManifest(body: Buffer | string, policy?: UpdatePolicy): WolframManifest
  verifySignature(manifest: Buffer | string, sigText: string, pkHex: string): boolean
}

export function loadWolfram(): Wolfram {
  const path = process.env.WOLFRAM_NODE_ADDON ?? preset
  return createRequire(import.meta.url)(path) as Wolfram
}

/** The wf_status name the module threw with, or undefined for a native type error. */
export function statusCode(err: unknown): string | undefined {
  return err instanceof Error ? (err as { code?: string }).code : undefined
}

export const wolfram: Wolfram = loadWolfram()
