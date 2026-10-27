import { createRequire } from 'node:module'
import { fileURLToPath } from 'node:url'

// The bridge runs Wolfram's contracts in Wolfram's own C, through the Node
// addon built from wolfram's node/wolfram_core.c at the commit pinned in
// .github/workflows/ci.yml. This file is glue, not logic: the addon's Type-
// Script-facing surface and its loader. The rules it calls live in Wolfram;
// each consumer runs the vectors of its contract (update and oauth pairing
// today), and a status that changes meaning there breaks Wolfram's binding
// tests first, per wolfram's own AGENTS.md.

// Built by tools/build-addon.sh into bridge/build/wolfram_core.node before the
// tests run. WOLFRAM_NODE_ADDON points at another build of it, as in Wolfram's
// own addon tests.
const preset = fileURLToPath(new URL('../build/wolfram_core.node', import.meta.url))

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

export interface PairBeginResult {
  pairCode: string
  pairUrl: string
  /** Unix seconds; 0 if the node gave none. */
  expiresAt: number
}

export type PairPollResult =
  | { state: 'pending' }
  | { state: 'complete'; token: string; handle: string; did: string; service: string }
  | { state: 'error'; message: string }

/** One validated richtext facet over the post's text. */
export interface WolframFacet {
  kind: 'link' | 'mention' | 'tag'
  byteStart: number
  byteEnd: number
  target: string
}

/** One picture the author attached, in the author's order. */
export interface WolframDisplayImage {
  /** CDN thumbnail URL, or null: an image with none is still counted. */
  thumb: string | null
  alt: string | null
  width: number
  height: number
}

/** The display model wolfram/post_display.h fills from a post's record and embed. */
export interface WolframPostDisplay {
  text: string
  createdAt: string | null
  facets: WolframFacet[]
  images: WolframDisplayImage[]
  embedKind: 'none' | 'images' | 'video' | 'external' | 'record' | 'recordWithMedia' | 'unknown'
  isReply: boolean
  external: { title: string | null; uri: string | null; description: string | null; thumb: string | null }
  video: { thumb: string | null; alt: string | null; width: number; height: number }
  quote: { uri: string | null; authorHandle: string | null; text: string | null }
}

// The module throws Error values whose `code` is the wf_status name
// (WF_ERR_PARSE, WF_ERR_VALIDATION, ...); read it, never its message, which is
// Wolfram's call site and does not read as prose.
export interface Wolfram {
  // wolfram/update.h
  compareVersions(a: string, b: string): number
  sha256Hex(data: Buffer | string): string
  parseManifest(body: Buffer | string, policy?: UpdatePolicy): WolframManifest
  verifySignature(manifest: Buffer | string, sigText: string, pkHex: string): boolean
  // wolfram/oauth_pairing.h: the parsers only; the begin/poll loop stays with
  // the consumer, whose transport is its own.
  buildPairBegin(handle: string): string
  parsePairBegin(body: Buffer | string): PairBeginResult
  parsePairPoll(body: Buffer | string): PairPollResult
  // wolfram/post_display.h: the display model of a post from its raw
  // record/embed JSON text. undefined or null arguments mean the member is
  // absent; intake text that does not parse throws WF_ERR_PARSE before
  // Wolfram's C ever sees it. Hostile data yields empty members, never a
  // throw. Fetching (network) stays in the consumer.
  postDisplay(record?: Buffer | string | null, embed?: Buffer | string | null): WolframPostDisplay
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
