import { refFromCdnUrl } from '../image/fetch.js'
import { wolfram } from '../wolfram.js'
import type { PostCard, PostImage } from './types.js'

// The record/embed half of post shaping runs in Wolfram's C, through the
// Node addon (wolfram's post_display.h): the post text, createdAt, the
// validated rich-text facets, the image list, the embed kind, the reply flag
// and the external/video/quote fields. Those are Wolfram's model (Platinum
// #39); what the bridge's wire contract adds on top — the text and field
// bounds, the image references for GET /v1/image, the card's host, the
// four-image cap — is protocol policy and lives here, thin, over the filled
// model. The vectors of the contract are Wolfram's
// test/vectors/post_display.json, copied byte-identical and CI-diffed.

export const MAX_POST_TEXT_LENGTH = 300
export const MAX_ALT_LENGTH = 300
export const MAX_CARD_TITLE_LENGTH = 100
export const MAX_CARD_URI_LENGTH = 300
export const MAX_POST_IMAGES = 4
export const MAX_DISPLAY_NAME_LENGTH = 64
export const MAX_HANDLE_LENGTH = 255
export const MAX_PROFILE_DESCRIPTION_LENGTH = 511

function stringValue(value: unknown): string {
  return typeof value === 'string' ? value : ''
}

/** Text clipped by Unicode code point, the way the Mac counts it. */
export function clipped(value: unknown, maximum: number): string {
  return Array.from(stringValue(value)).slice(0, maximum).join('')
}

/** The reference for the small avatar the AppView linked, or undefined if there is none we would serve. */
export function avatarRef(url: unknown): string | undefined {
  const ref = refFromCdnUrl(url)
  return ref?.startsWith('avatar/') ? `avatar_thumbnail/${ref.slice('avatar/'.length)}` : undefined
}

/** Wire fields of one post that come from its record and embed. */
export interface PostDisplayFields {
  text: string
  createdAt: string
  images?: PostImage[]
  card?: PostCard
}

/**
 * Fill the display part of a post the wire contract wants, from the record
 * and embed the AppView posted. Everything the C model rejects or leaves out
 * (odd shapes, mid-codepoint facets, images the bridge could not serve) yields
 * the empty wire result, never a throw.
 */
export function postDisplayFields(record: unknown, embed: unknown): PostDisplayFields {
  // JSON.parse produced these values, so they re-serialize losslessly and
  // without cycles; members they omit stay omitted for C's "absent".
  const display = wolfram.postDisplay(JSON.stringify(record) ?? null, embed === undefined ? null : JSON.stringify(embed) ?? null)
  const out: PostDisplayFields = {
    text: clipped(display.text, MAX_POST_TEXT_LENGTH),
    createdAt: clipped(display.createdAt ?? '', 64),
  }

  const images: PostImage[] = []
  for (const image of display.images.slice(0, MAX_POST_IMAGES)) {
    const ref = refFromCdnUrl(image.thumb)
    if (ref) images.push({ ref, alt: clipped(image.alt, MAX_ALT_LENGTH) })
  }
  if (images.length) out.images = images

  const ext = display.external
  if (ext?.uri) {
    let domain = ''
    try {
      const url = new URL(ext.uri)
      if (url.protocol !== 'http:' && url.protocol !== 'https:') domain = ''
      else domain = url.hostname
    } catch {
      domain = ''
    }
    if (domain) out.card = { uri: clipped(ext.uri, MAX_CARD_URI_LENGTH), title: clipped(ext.title, MAX_CARD_TITLE_LENGTH), domain: clipped(domain, 100) }
  }
  return out
}
