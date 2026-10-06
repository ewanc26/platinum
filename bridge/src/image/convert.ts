import jpeg from 'jpeg-js'
import { PNG } from 'pngjs'

/**
 * The image pipeline: decode, flatten, shrink, quantise, pack. The Mac never
 * decodes an image. It receives the "PLIM" blob this module builds and wraps it
 * in a PixMap; the format is docs/IMAGES.md and the vectors under
 * bridge/test/vectors/image/ are its test cases. Everything here is integer
 * arithmetic with a fixed order, so the same input always gives the same bytes.
 */

export const MAGIC = 0x504c494d // 'PLIM'
export const VERSION = 1
export const HEADER_BYTES = 16
export const MAX_BLOB_BYTES = 80_000
export const MAX_SOURCE_PIXELS = 12_000_000
export const MAX_WIDTH = 320
export const MIN_WIDTH = 16

export interface Rgba {
  width: number
  height: number
  /** width*height*4 bytes, R G B A. */
  data: Uint8Array
}

export class ImageError extends Error {
  constructor(public readonly code: 'unsupported' | 'too_large' | 'corrupt', message: string) {
    super(message)
  }
}

export function decodeImage(bytes: Uint8Array): Rgba {
  if (bytes.length >= 3 && bytes[0] === 0xff && bytes[1] === 0xd8) {
    try {
      const d = jpeg.decode(bytes, { useTArray: true, formatAsRGBA: true, maxResolutionInMP: 12, maxMemoryUsageInMB: 256 })
      return { width: d.width, height: d.height, data: d.data }
    } catch (error) {
      throw new ImageError('corrupt', `The JPEG could not be decoded: ${(error as Error).message}`)
    }
  }
  if (bytes.length >= 8 && bytes[0] === 0x89 && bytes[1] === 0x50 && bytes[2] === 0x4e && bytes[3] === 0x47) {
    try {
      const p = PNG.sync.read(Buffer.from(bytes), { skipRescale: false })
      if (p.width * p.height > MAX_SOURCE_PIXELS) throw new ImageError('too_large', 'The image is too large.')
      return { width: p.width, height: p.height, data: new Uint8Array(p.data) }
    } catch (error) {
      if (error instanceof ImageError) throw error
      throw new ImageError('corrupt', `The PNG could not be decoded: ${(error as Error).message}`)
    }
  }
  throw new ImageError('unsupported', 'Only JPEG and PNG images are supported.')
}

/** Alpha onto white: (c*a + 255*(255-a) + 127) / 255, integer. Returns packed RGB, 3 bytes per pixel. */
export function flatten(img: Rgba): Uint8Array {
  const out = new Uint8Array(img.width * img.height * 3)
  for (let i = 0, o = 0; i < img.data.length; i += 4) {
    const a = img.data[i + 3]!
    for (let c = 0; c < 3; c++) {
      out[o++] = a === 255 ? img.data[i + c]! : Math.floor((img.data[i + c]! * a + 255 * (255 - a) + 127) / 255)
    }
  }
  return out
}

export interface Rgb {
  width: number
  height: number
  data: Uint8Array
}

/**
 * Area-average shrink to fit `maxWidth` x `maxHeight`, never enlarging, never
 * below 1x1. Destination pixel (dx,dy) averages the source pixels in
 * [floor(dx*sw/dw), floor((dx+1)*sw/dw)) by the same for rows, each channel
 * rounded half up: (sum + n/2) / n, integer.
 */
export function shrink(width: number, height: number, rgb: Uint8Array, maxWidth: number, maxHeight: number): Rgb {
  let dw = width
  let dh = height
  if (width > maxWidth || height > maxHeight) {
    // scale = min(maxWidth/width, maxHeight/height), kept in integers.
    if (maxWidth * height <= maxHeight * width) {
      dw = maxWidth
      dh = Math.max(1, Math.floor((height * maxWidth) / width))
    } else {
      dh = maxHeight
      dw = Math.max(1, Math.floor((width * maxHeight) / height))
    }
  }
  if (dw === width && dh === height) return { width, height, data: rgb }
  const out = new Uint8Array(dw * dh * 3)
  for (let dy = 0; dy < dh; dy++) {
    const y0 = Math.floor((dy * height) / dh)
    const y1 = Math.max(y0 + 1, Math.floor(((dy + 1) * height) / dh))
    for (let dx = 0; dx < dw; dx++) {
      const x0 = Math.floor((dx * width) / dw)
      const x1 = Math.max(x0 + 1, Math.floor(((dx + 1) * width) / dw))
      const n = (y1 - y0) * (x1 - x0)
      for (let c = 0; c < 3; c++) {
        let sum = 0
        for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) sum += rgb[(y * width + x) * 3 + c]!
        out[(dy * dw + dx) * 3 + c] = Math.floor((sum + Math.floor(n / 2)) / n)
      }
    }
  }
  return { width: dw, height: dh, data: out }
}

type Colour = [number, number, number]

/**
 * A palette of at most `maxColours` entries, sorted ascending by (r,g,b).
 * An image with no more distinct colours than that gets exactly those colours.
 * Otherwise median cut over a 5-bit-per-channel histogram: split the box with the
 * widest channel range at the count-weighted median until there are enough boxes,
 * each box's entry being its count-weighted mean (rounded half up).
 */
export function buildPalette(rgb: Uint8Array, maxColours: number): Colour[] {
  const counts = new Map<number, number>()
  for (let i = 0; i < rgb.length; i += 3) {
    const key = (rgb[i]! << 16) | (rgb[i + 1]! << 8) | rgb[i + 2]!
    counts.set(key, (counts.get(key) ?? 0) + 1)
  }
  if (counts.size <= maxColours) {
    return [...counts.keys()].sort((a, b) => a - b).map(k => [(k >> 16) & 255, (k >> 8) & 255, k & 255] as Colour)
  }

  // Reduce to the histogram of 5-bit colours so the work is bounded and ties are stable.
  const bins = new Map<number, { n: number; r: number; g: number; b: number }>()
  for (const [key, n] of counts) {
    const r = (key >> 16) & 255, g = (key >> 8) & 255, b = key & 255
    const bk = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
    const e = bins.get(bk) ?? { n: 0, r: 0, g: 0, b: 0 }
    e.n += n; e.r += r * n; e.g += g * n; e.b += b * n
    bins.set(bk, e)
  }
  type Entry = { key: number; n: number; r: number; g: number; b: number }
  const entries: Entry[] = [...bins.entries()].sort((a, b) => a[0] - b[0]).map(([key, e]) => ({ key, ...e }))

  const boxes: Entry[][] = [entries]
  const range = (box: Entry[]): { ch: 0 | 1 | 2; span: number } => {
    let best: { ch: 0 | 1 | 2; span: number } = { ch: 0, span: -1 }
    for (const ch of [0, 1, 2] as const) {
      let lo = 31, hi = 0
      for (const e of box) {
        const v = ch === 0 ? e.key >> 10 : ch === 1 ? (e.key >> 5) & 31 : e.key & 31
        if (v < lo) lo = v
        if (v > hi) hi = v
      }
      if (hi - lo > best.span) best = { ch, span: hi - lo }
    }
    return best
  }
  while (boxes.length < maxColours) {
    let pick = -1
    let pickSpan = 0
    boxes.forEach((box, i) => {
      if (box.length < 2) return
      const { span } = range(box)
      if (span > pickSpan) { pick = i; pickSpan = span }
    })
    if (pick < 0) break
    const box = boxes[pick]!
    const { ch } = range(box)
    const val = (e: Entry) => (ch === 0 ? e.key >> 10 : ch === 1 ? (e.key >> 5) & 31 : e.key & 31)
    const sorted = [...box].sort((a, b) => val(a) - val(b) || a.key - b.key)
    const total = sorted.reduce((s, e) => s + e.n, 0)
    let acc = 0
    let cut = 1
    for (let i = 0; i < sorted.length - 1; i++) {
      acc += sorted[i]!.n
      cut = i + 1
      if (acc * 2 >= total) break
    }
    boxes.splice(pick, 1, sorted.slice(0, cut), sorted.slice(cut))
  }
  const palette = boxes.map(box => {
    const n = box.reduce((s, e) => s + e.n, 0)
    const mean = (f: (e: Entry) => number): number => Math.floor((box.reduce((s, e) => s + f(e), 0) + Math.floor(n / 2)) / n)
    return [mean(e => e.r), mean(e => e.g), mean(e => e.b)] as Colour
  })
  // Merge duplicates the rounding may have produced, then sort.
  const seen = new Set<number>()
  return palette
    .filter(c => { const k = (c[0] << 16) | (c[1] << 8) | c[2]; if (seen.has(k)) return false; seen.add(k); return true })
    .sort((a, b) => ((a[0] << 16) | (a[1] << 8) | a[2]) - ((b[0] << 16) | (b[1] << 8) | b[2]))
}

/** Index of the nearest palette entry by squared RGB distance; the lowest index wins a tie. */
export function nearest(palette: Colour[], r: number, g: number, b: number): number {
  let best = 0
  let bestD = Infinity
  for (let i = 0; i < palette.length; i++) {
    const p = palette[i]!
    const d = (p[0] - r) ** 2 + (p[1] - g) ** 2 + (p[2] - b) ** 2
    if (d < bestD) { best = i; bestD = d }
  }
  return best
}

export interface Plim {
  blob: Uint8Array
  width: number
  height: number
  depth: 4 | 8
  colours: number
}

/** The PixMap row size for `width` pixels at `depth` bits: whole bytes, rounded up to even. */
export function rowBytes(width: number, depth: number): number {
  const bytes = Math.ceil((width * depth) / 8)
  return bytes + (bytes % 2)
}

export function pack(rgb: Rgb, palette: Colour[], depth: 4 | 8): Plim {
  const rb = rowBytes(rgb.width, depth)
  const total = HEADER_BYTES + palette.length * 3 + rgb.height * rb
  const blob = new Uint8Array(total)
  const view = new DataView(blob.buffer)
  view.setUint32(0, MAGIC)
  blob[4] = VERSION
  blob[5] = depth
  view.setUint16(6, rgb.width)
  view.setUint16(8, rgb.height)
  view.setUint16(10, rb)
  view.setUint16(12, palette.length)
  view.setUint16(14, 0)
  let o = HEADER_BYTES
  for (const [r, g, b] of palette) { blob[o++] = r; blob[o++] = g; blob[o++] = b }
  const cache = new Map<number, number>()
  for (let y = 0; y < rgb.height; y++) {
    for (let x = 0; x < rgb.width; x++) {
      const i = (y * rgb.width + x) * 3
      const key = (rgb.data[i]! << 16) | (rgb.data[i + 1]! << 8) | rgb.data[i + 2]!
      let idx = cache.get(key)
      if (idx === undefined) { idx = nearest(palette, rgb.data[i]!, rgb.data[i + 1]!, rgb.data[i + 2]!); cache.set(key, idx) }
      const base = o + y * rb
      if (depth === 8) blob[base + x] = idx
      else blob[base + (x >> 1)]! |= x % 2 === 0 ? idx << 4 : idx
    }
  }
  return { blob, width: rgb.width, height: rgb.height, depth, colours: palette.length }
}

export interface ConvertOptions {
  maxWidth: number
  maxHeight: number
  depth: 4 | 8
}

/** Everything from decoded pixels to a blob. Throws ImageError('too_large') if no fit under MAX_BLOB_BYTES. */
export function convertPixels(img: Rgba, options: ConvertOptions): Plim {
  if (img.width < 1 || img.height < 1 || img.width * img.height > MAX_SOURCE_PIXELS || img.data.length !== img.width * img.height * 4) {
    throw new ImageError('corrupt', 'The image has an impossible size.')
  }
  const flat = flatten(img)
  const small = shrink(img.width, img.height, flat, options.maxWidth, options.maxHeight)
  const palette = buildPalette(small.data, 2 ** options.depth)
  const out = pack(small, palette, options.depth)
  if (out.blob.length > MAX_BLOB_BYTES) throw new ImageError('too_large', 'The converted image is still too large.')
  return out
}

export function convertImage(bytes: Uint8Array, options: ConvertOptions): Plim {
  return convertPixels(decodeImage(bytes), options)
}
