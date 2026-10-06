import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import test from 'node:test'
import jpeg from 'jpeg-js'
import { PNG } from 'pngjs'
import { buildPalette, convertImage, convertPixels, decodeImage, ImageError, rowBytes, shrink } from '../src/image/convert.js'

const hex = (u: Uint8Array): string => Buffer.from(u).toString('hex')
const vectors = JSON.parse(readFileSync(new URL('./vectors/image/convert.json', import.meta.url), 'utf8')) as {
  vectors: { name: string; width: number; height: number; rgba: string; maxWidth: number; maxHeight: number; depth: 4 | 8; blob: string }[]
}

for (const v of vectors.vectors) {
  test(`vector: ${v.name}`, () => {
    const out = convertPixels(
      { width: v.width, height: v.height, data: new Uint8Array(Buffer.from(v.rgba, 'hex')) },
      { maxWidth: v.maxWidth, maxHeight: v.maxHeight, depth: v.depth },
    )
    assert.equal(hex(out.blob), v.blob)
  })
}

test('row bytes are whole bytes rounded up to even', () => {
  assert.deepEqual([1, 2, 3, 4, 5].map(w => rowBytes(w, 4)), [2, 2, 2, 2, 4])
  assert.deepEqual([1, 2, 3, 4].map(w => rowBytes(w, 8)), [2, 2, 4, 4])
})

test('an image is never enlarged and keeps its proportions when shrunk', () => {
  const rgb = new Uint8Array(8 * 4 * 3)
  assert.deepEqual({ ...shrink(8, 4, rgb, 320, 320), data: undefined }, { width: 8, height: 4, data: undefined })
  const s = shrink(8, 4, rgb, 4, 320)
  assert.deepEqual([s.width, s.height], [4, 2])
  const t = shrink(2, 100, new Uint8Array(2 * 100 * 3), 320, 10)
  assert.deepEqual([t.width, t.height], [1, 10])
})

test('a photo-like image gets a palette within the limit and every index is valid', () => {
  const w = 40, h = 30
  const data = new Uint8Array(w * h * 4)
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    const i = (y * w + x) * 4
    data[i] = (x * 255) / (w - 1); data[i + 1] = (y * 255) / (h - 1); data[i + 2] = ((x + y) * 255) / (w + h); data[i + 3] = 255
  }
  for (const depth of [4, 8] as const) {
    const out = convertPixels({ width: w, height: h, data }, { maxWidth: 320, maxHeight: 320, depth })
    assert.ok(out.colours <= 2 ** depth && out.colours > 2)
    const view = new DataView(out.blob.buffer)
    assert.equal(out.blob.length, 16 + out.colours * 3 + h * view.getUint16(10))
    const rb = view.getUint16(10)
    const palStart = 16
    const pixStart = 16 + out.colours * 3
    let err = 0
    for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
      const b = out.blob[pixStart + y * rb + (depth === 8 ? x : x >> 1)]!
      const idx = depth === 8 ? b : x % 2 === 0 ? b >> 4 : b & 15
      assert.ok(idx < out.colours, 'index in range')
      const i = (y * w + x) * 4
      err += Math.abs(out.blob[palStart + idx * 3]! - data[i]!) + Math.abs(out.blob[palStart + idx * 3 + 1]! - data[i + 1]!) + Math.abs(out.blob[palStart + idx * 3 + 2]! - data[i + 2]!)
    }
    assert.ok(err / (w * h * 3) < (depth === 4 ? 40 : 12), `mean error ${err / (w * h * 3)} at depth ${depth}`)
  }
})

test('the palette is sorted and has no duplicates', () => {
  const rgb = new Uint8Array(300)
  for (let i = 0; i < 100; i++) { rgb[i * 3] = (i * 37) % 256; rgb[i * 3 + 1] = (i * 91) % 256; rgb[i * 3 + 2] = (i * 17) % 256 }
  const p = buildPalette(rgb, 16)
  const keys = p.map(c => (c[0] << 16) | (c[1] << 8) | c[2])
  assert.deepEqual(keys, [...new Set(keys)].sort((a, b) => a - b))
  assert.ok(p.length <= 16)
})

test('PNG and JPEG are decoded, anything else is refused', () => {
  const png = new PNG({ width: 2, height: 1 })
  png.data = Buffer.from([255, 0, 0, 255, 0, 0, 255, 255])
  const decoded = decodeImage(PNG.sync.write(png))
  assert.deepEqual([decoded.width, decoded.height, hex(decoded.data)], [2, 1, 'ff0000ff0000ffff'])

  const j = jpeg.encode({ width: 8, height: 8, data: Buffer.alloc(8 * 8 * 4, 200) }, 90)
  const jd = decodeImage(j.data)
  assert.deepEqual([jd.width, jd.height], [8, 8])
  const out = convertImage(j.data, { maxWidth: 320, maxHeight: 320, depth: 4 })
  assert.equal(out.colours, 1)

  assert.throws(() => decodeImage(new Uint8Array([0x47, 0x49, 0x46, 0x38, 0x39, 0x61])), (e: unknown) => e instanceof ImageError && e.code === 'unsupported')
  assert.throws(() => decodeImage(new Uint8Array([0xff, 0xd8, 0xff, 0x00])), (e: unknown) => e instanceof ImageError && e.code === 'corrupt')
  assert.throws(() => decodeImage(new Uint8Array(0)), (e: unknown) => e instanceof ImageError && e.code === 'unsupported')
})

test('an oversize or inconsistent source is refused', () => {
  assert.throws(() => convertPixels({ width: 4000, height: 4000, data: new Uint8Array(4) }, { maxWidth: 320, maxHeight: 320, depth: 4 }), (e: unknown) => e instanceof ImageError)
  assert.throws(() => convertPixels({ width: 2, height: 2, data: new Uint8Array(4) }, { maxWidth: 320, maxHeight: 320, depth: 4 }), (e: unknown) => e instanceof ImageError)
})

test('the biggest allowed image fits the declared limit', () => {
  const w = 320, h = 320
  const data = new Uint8Array(w * h * 4)
  for (let i = 0; i < data.length; i += 4) { data[i] = i % 251; data[i + 1] = (i >> 3) % 241; data[i + 2] = (i >> 5) % 239; data[i + 3] = 255 }
  const out = convertPixels({ width: w, height: h, data }, { maxWidth: 320, maxHeight: 320, depth: 4 })
  assert.ok(out.blob.length <= 80_000, String(out.blob.length))
})
