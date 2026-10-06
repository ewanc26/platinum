import { convertImage, ImageError, type Plim } from './convert.js'

/**
 * Where images come from. The Mac names an image by an opaque `ref`, never a
 * URL, and the bridge only ever fetches from the Bluesky image CDN, so the
 * route cannot be used to make the bridge request an arbitrary address.
 */
export const CDN_HOST = 'cdn.bsky.app'
export const MAX_SOURCE_BYTES = 5_000_000

const REF = /^(avatar|avatar_thumbnail|feed_thumbnail|feed_fullsize)\/plain\/did:[a-z0-9]+:[A-Za-z0-9._:%-]+\/[A-Za-z0-9]+@(jpeg|png)$/

export function validImageRef(value: unknown): string | undefined {
  return typeof value === 'string' && value.length <= 300 && REF.test(value) ? value : undefined
}

/** The ref for a CDN URL the AppView gave us, or undefined if it isn't one we would serve. */
export function refFromCdnUrl(url: unknown): string | undefined {
  if (typeof url !== 'string') return undefined
  const prefix = `https://${CDN_HOST}/img/`
  return url.startsWith(prefix) ? validImageRef(url.slice(prefix.length)) : undefined
}

export type Fetcher = (url: string) => Promise<Uint8Array>

export const cdnFetch: Fetcher = async url => {
  const res = await fetch(url, { redirect: 'error', signal: AbortSignal.timeout(10_000), headers: { accept: 'image/jpeg,image/png' } })
  if (!res.ok) throw new ImageError('corrupt', `The image host answered ${res.status}.`)
  const declared = Number(res.headers.get('content-length') ?? '0')
  if (declared > MAX_SOURCE_BYTES) throw new ImageError('too_large', 'The source image is too large.')
  const reader = res.body?.getReader()
  if (!reader) throw new ImageError('corrupt', 'The image host sent nothing.')
  const chunks: Uint8Array[] = []
  let total = 0
  for (;;) {
    const { done, value } = await reader.read()
    if (done) break
    total += value.length
    if (total > MAX_SOURCE_BYTES) {
      await reader.cancel()
      throw new ImageError('too_large', 'The source image is too large.')
    }
    chunks.push(value)
  }
  return Buffer.concat(chunks)
}

export interface ImageRequest {
  ref: string
  width: number
  depth: 4 | 8
}

/** `w` and `depth` from a query string: width 16 to 320 (default 320), depth 4 or 8 (default 4). */
export function parseImageParams(w: string | null, depth: string | null): { width: number; depth: 4 | 8 } | undefined {
  const width = w === null ? 320 : /^\d{1,3}$/.test(w) ? Number(w) : NaN
  if (!(width >= 16 && width <= 320)) return undefined
  if (depth !== null && depth !== '4' && depth !== '8') return undefined
  return { width, depth: depth === '8' ? 8 : 4 }
}

/** Small least-recently-used cache of converted images, bounded by total bytes. */
export class ImageService {
  private cache = new Map<string, Plim>()
  private bytes = 0

  constructor(private readonly fetcher: Fetcher = cdnFetch, private readonly limitBytes = 4_000_000) {}

  async get(req: ImageRequest): Promise<Plim> {
    const key = `${req.ref}|${req.width}|${req.depth}`
    const hit = this.cache.get(key)
    if (hit) {
      this.cache.delete(key)
      this.cache.set(key, hit)
      return hit
    }
    const source = await this.fetcher(`https://${CDN_HOST}/img/${req.ref}`)
    const out = convertImage(source, { maxWidth: req.width, maxHeight: 320, depth: req.depth })
    this.cache.set(key, out)
    this.bytes += out.blob.length
    for (const [k, v] of this.cache) {
      if (this.bytes <= this.limitBytes) break
      this.cache.delete(k)
      this.bytes -= v.blob.length
    }
    return out
  }
}
