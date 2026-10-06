import assert from 'node:assert/strict'
import test from 'node:test'
import { PNG } from 'pngjs'
import { ImageError } from '../src/image/convert.js'
import { ImageService, parseImageParams, refFromCdnUrl, validImageRef } from '../src/image/fetch.js'

const png = (): Uint8Array => {
  const p = new PNG({ width: 4, height: 4 })
  for (let i = 0; i < p.data.length; i += 4) { p.data[i] = 10; p.data[i + 1] = 20; p.data[i + 2] = 30; p.data[i + 3] = 255 }
  return PNG.sync.write(p)
}

test('only image references of the known shape are accepted', () => {
  const good = 'feed_thumbnail/plain/did:plc:abc123/bafkreiabc@jpeg'
  assert.equal(validImageRef(good), good)
  for (const bad of ['', 'https://evil.example/x', '../etc/passwd', 'feed_thumbnail/plain/did:plc:abc/bafy@gif',
    'other/plain/did:plc:abc/bafy@jpeg', 'feed_thumbnail/plain/did:plc:abc/bafy@jpeg?x=1', 'feed_thumbnail/plain/did:plc:abc/../bafy@jpeg',
    'x'.repeat(400), 12, null]) {
    assert.equal(validImageRef(bad), undefined, String(bad))
  }
})

test('a ref is taken from a CDN url and from nothing else', () => {
  assert.equal(refFromCdnUrl('https://cdn.bsky.app/img/avatar/plain/did:plc:abc/bafy@jpeg'), 'avatar/plain/did:plc:abc/bafy@jpeg')
  assert.equal(refFromCdnUrl('https://evil.example/img/avatar/plain/did:plc:abc/bafy@jpeg'), undefined)
  assert.equal(refFromCdnUrl('http://cdn.bsky.app/img/avatar/plain/did:plc:abc/bafy@jpeg'), undefined)
  assert.equal(refFromCdnUrl(undefined), undefined)
})

test('width and depth are bounded', () => {
  assert.deepEqual(parseImageParams(null, null), { width: 320, depth: 4 })
  assert.deepEqual(parseImageParams('32', '8'), { width: 32, depth: 8 })
  for (const [w, d] of [['15', null], ['321', null], ['abc', null], ['-5', null], ['32', '5'], ['32', '16'], ['0032x', null]] as const) {
    assert.equal(parseImageParams(w, d), undefined, `${w} ${d}`)
  }
})

test('the service fetches only from the CDN, converts, and caches', async () => {
  const urls: string[] = []
  const svc = new ImageService(async u => { urls.push(u); return png() })
  const req = { ref: 'avatar/plain/did:plc:abc/bafy@png', width: 32, depth: 8 as const }
  const a = await svc.get(req)
  const b = await svc.get(req)
  assert.equal(a, b)
  assert.deepEqual(urls, ['https://cdn.bsky.app/img/avatar/plain/did:plc:abc/bafy@png'])
  assert.equal(a.width, 4)
  await svc.get({ ...req, depth: 4 })
  assert.equal(urls.length, 2, 'depth is part of the cache key')
})

test('the cache is bounded by bytes', async () => {
  let fetches = 0
  const svc = new ImageService(async () => { fetches++; return png() }, 40)
  const one = { ref: 'avatar/plain/did:plc:a/b1@png', width: 32, depth: 8 as const }
  const two = { ref: 'avatar/plain/did:plc:a/b2@png', width: 32, depth: 8 as const }
  await svc.get(one)
  await svc.get(two)
  await svc.get(one)
  assert.equal(fetches, 3, 'the oldest entry was evicted to stay under the limit')
})

test('a fetch or decode failure is an error, not a blob', async () => {
  await assert.rejects(new ImageService(async () => { throw new ImageError('corrupt', 'x') }).get({ ref: 'avatar/plain/did:plc:a/b@png', width: 32, depth: 8 }))
  await assert.rejects(new ImageService(async () => new Uint8Array([1, 2, 3])).get({ ref: 'avatar/plain/did:plc:a/b@png', width: 32, depth: 8 }))
})
