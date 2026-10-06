// Semantic version comparison: a port of Wolfram's wf_update_compare_versions
// (update.h, ewanc26/wolfram#106), tested against Wolfram's own vectors in
// test/vectors/update/versions.json. Strict semver 2.0.0: no leading `v`, no
// leading zeros, build metadata allowed and ignored. A malformed version is an
// error, never "equal".

export interface ParsedVersion {
  core: [string, string, string]
  pre: string[]
}

const NUM = '(0|[1-9][0-9]*)'
const PRE_ID = '(?:0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*)'
const BUILD_ID = '[0-9A-Za-z-]+'
const VERSION_RE = new RegExp(
  `^${NUM}\\.${NUM}\\.${NUM}(?:-(${PRE_ID}(?:\\.${PRE_ID})*))?(?:\\+${BUILD_ID}(?:\\.${BUILD_ID})*)?$`,
)

export function parseVersion(text: string): ParsedVersion {
  const m = typeof text === 'string' ? VERSION_RE.exec(text) : null
  if (!m) throw new Error(`not a version: ${JSON.stringify(String(text).slice(0, 40))}`)
  return { core: [m[1]!, m[2]!, m[3]!], pre: m[4] ? m[4].split('.') : [] }
}

/** Compare digit strings without leading zeros as numbers, with no overflow. */
function cmpNum(a: string, b: string): number {
  if (a.length !== b.length) return a.length < b.length ? -1 : 1
  return a < b ? -1 : a > b ? 1 : 0
}

function comparePre(a: string[], b: string[]): number {
  if (a.length === 0 || b.length === 0) return a.length === b.length ? 0 : a.length === 0 ? 1 : -1
  for (let i = 0; i < Math.min(a.length, b.length); i++) {
    const x = a[i]!
    const y = b[i]!
    const xn = /^[0-9]+$/.test(x)
    const yn = /^[0-9]+$/.test(y)
    let c: number
    if (xn && yn) c = cmpNum(x, y)
    else if (xn !== yn) c = xn ? -1 : 1
    else c = x < y ? -1 : x > y ? 1 : 0
    if (c !== 0) return c
  }
  return a.length === b.length ? 0 : a.length < b.length ? -1 : 1
}

/** Negative when a < b, zero when equal, positive when a > b. Throws on a malformed version. */
export function compareVersions(a: string, b: string): number {
  const x = parseVersion(a)
  const y = parseVersion(b)
  for (let i = 0; i < 3; i++) {
    const c = cmpNum(x.core[i]!, y.core[i]!)
    if (c !== 0) return c
  }
  return comparePre(x.pre, y.pre)
}
