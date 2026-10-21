import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import test from 'node:test'
import { convertPixels } from '../src/image/convert.js'

/*
 * The Mac-side encoder (macos9/image_encode.c) is checked against the blobs in
 * the TS_SNAPSHOT block of macos9/test/test_image_encode.c. This test checks the
 * other direction: the converter still produces those blobs. A change to either
 * side that alters the bytes fails one of the two suites. The format is in the
 * comment above the block: "SNAP w h maxW maxH depth", RGBA hex chunks, "=", blob
 * hex chunks, "END".
 */

const source = readFileSync(new URL('../../macos9/test/test_image_encode.c', import.meta.url), 'utf8')
const block = source.split('/* TS_SNAPSHOT_BEGIN */')[1]?.split('/* TS_SNAPSHOT_END */')[0] ?? ''
const parts = [...block.matchAll(/"([^"]*)"/g)].map(m => m[1]!)

type Snapshot = { width: number; height: number; maxWidth: number; maxHeight: number; depth: 4 | 8; rgba: string; blob: string }

function parseSnapshots(entries: string[]): Snapshot[] {
  const out: Snapshot[] = []
  let i = 0
  while (i < entries.length) {
    const header = /^SNAP (\d+) (\d+) (\d+) (\d+) (\d+)$/.exec(entries[i]!)
    assert.ok(header, `snapshot header at entry ${i}: ${entries[i]}`)
    i++
    let rgba = ''
    while (i < entries.length && entries[i] !== '=') rgba += entries[i++]
    i++
    let blob = ''
    while (i < entries.length && entries[i] !== 'END') blob += entries[i++]
    i++
    const depth = Number(header[5])
    assert.ok(depth === 4 || depth === 8, 'depth is 4 or 8')
    out.push({
      width: Number(header[1]), height: Number(header[2]),
      maxWidth: Number(header[3]), maxHeight: Number(header[4]),
      depth, rgba, blob,
    })
  }
  return out
}

const snapshots = parseSnapshots(parts)

test('the C test carries TypeScript snapshots', () => {
  assert.ok(snapshots.length >= 1, 'at least one snapshot is present')
})

for (const [index, s] of snapshots.entries()) {
  test(`snapshot ${index}: ${s.width}x${s.height} to ${s.maxWidth}x${s.maxHeight} at depth ${s.depth} matches the C fixture`, () => {
    const out = convertPixels(
      { width: s.width, height: s.height, data: new Uint8Array(Buffer.from(s.rgba, 'hex')) },
      { maxWidth: s.maxWidth, maxHeight: s.maxHeight, depth: s.depth },
    )
    assert.equal(Buffer.from(out.blob).toString('hex'), s.blob)
  })
}
