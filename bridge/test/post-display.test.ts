import assert from 'node:assert/strict'
import test from 'node:test'
import { readFileSync } from 'node:fs'
import { wolfram } from '../src/wolfram.js'

// Wolfram's post_display vectors (test/vectors/post_display.json), verbatim;
// CI diffs them against Wolfram's at the commit pinned in the CI drift job.
// The same vectors run through Wolfram's own binding tests and its C suite.
const v = JSON.parse(readFileSync(new URL('./vectors/post_display.json', import.meta.url), 'utf8')) as {
  display: { name: string; record_text?: string; embed_text?: string; expect: unknown }[]
}

test('Wolfram fills the display model exactly as its vectors say', () => {
  for (const c of v.display) {
    if (c.expect === 'parse_error') {
      try {
        wolfram.postDisplay(c.record_text, c.embed_text)
        assert.fail(`${c.name}: expected a throw`)
      } catch (e) {
        assert.equal((e as { code?: string }).code, 'WF_ERR_PARSE', c.name)
      }
    } else {
      assert.deepEqual(wolfram.postDisplay(c.record_text, c.embed_text), c.expect, c.name)
    }
  }
})
