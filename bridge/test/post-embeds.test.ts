import assert from 'node:assert/strict'
import test from 'node:test'
import { normalizeTimelinePost } from '../src/domain/api.js'

const CDN = 'https://cdn.bsky.app/img'
const DID = 'did:plc:abc123'

function post(extra: Record<string, unknown>, authorExtra: Record<string, unknown> = {}) {
  return normalizeTimelinePost({
    post: { uri: 'at://x/app.bsky.feed.post/1', cid: 'bafy', author: { did: DID, ...authorExtra }, record: { text: 'hi' }, ...extra },
  })
}

test('a plain post carries no images, card or avatar', () => {
  const p = post({})
  assert.equal('images' in p, false)
  assert.equal('card' in p, false)
  assert.equal(p.author.avatar, undefined)
})

test('the avatar becomes a small-avatar reference, never a URL', () => {
  const p = post({}, { avatar: `${CDN}/avatar/plain/${DID}/bafkreiavatar@jpeg` })
  assert.equal(p.author.avatar, `avatar_thumbnail/plain/${DID}/bafkreiavatar@jpeg`)
  assert.equal(post({}, { avatar: 'https://evil.example/a.jpg' }).author.avatar, undefined)
  assert.equal(post({}, { avatar: `${CDN}/feed_thumbnail/plain/${DID}/bafkreiavatar@jpeg` }).author.avatar, undefined)
})

test('post images keep their references and alt text, at most four', () => {
  const images = Array.from({ length: 6 }, (_, i) => ({
    thumb: `${CDN}/feed_thumbnail/plain/${DID}/bafkreiimg${i}@jpeg`,
    fullsize: `${CDN}/feed_fullsize/plain/${DID}/bafkreiimg${i}@jpeg`,
    alt: i === 0 ? 'A heron' : '',
  }))
  const p = post({ embed: { $type: 'app.bsky.embed.images#view', images } })
  assert.equal(p.images?.length, 4)
  assert.deepEqual(p.images?.[0], { ref: `feed_thumbnail/plain/${DID}/bafkreiimg0@jpeg`, alt: 'A heron' })
  assert.equal(p.images?.[3].alt, '')
})

test('an image the bridge would not serve is left out', () => {
  const p = post({ embed: { images: [{ thumb: 'https://evil.example/x.jpg', alt: 'no' }, { thumb: `${CDN}/feed_thumbnail/plain/${DID}/bafkreiok@png`, alt: 'ok' }] } })
  assert.deepEqual(p.images, [{ ref: `feed_thumbnail/plain/${DID}/bafkreiok@png`, alt: 'ok' }])
})

test('alt text is clipped to what the Mac can hold', () => {
  const p = post({ embed: { images: [{ thumb: `${CDN}/feed_thumbnail/plain/${DID}/bafkreiok@jpeg`, alt: 'a'.repeat(1000) }] } })
  assert.equal(Array.from(p.images?.[0].alt ?? '').length, 300)
})

test('a link card gives title, address and host', () => {
  const p = post({ embed: { $type: 'app.bsky.embed.external#view', external: { uri: 'https://example.com/a/b?c=1', title: 'An article', description: 'ignored', thumb: 'x' } } })
  assert.deepEqual(p.card, { uri: 'https://example.com/a/b?c=1', title: 'An article', domain: 'example.com' })
})

test('a card with a non-web address is dropped', () => {
  assert.equal(post({ embed: { external: { uri: 'javascript:alert(1)', title: 't' } } }).card, undefined)
  assert.equal(post({ embed: { external: { uri: 'not a url', title: 't' } } }).card, undefined)
})

test('images and the card are found inside a record with media', () => {
  const p = post({
    embed: {
      $type: 'app.bsky.embed.recordWithMedia#view',
      record: { record: {} },
      media: { images: [{ thumb: `${CDN}/feed_thumbnail/plain/${DID}/bafkreiok@jpeg`, alt: 'inside' }] },
    },
  })
  assert.equal(p.images?.[0].alt, 'inside')
})
