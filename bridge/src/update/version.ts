// Version comparison for MAJOR.MINOR.PATCH[-prerelease], following semver
// precedence. Anything else is an error rather than "equal": an updater that
// treats garbage as equal would never update, and one that treats it as newer
// would update to anything.

export interface ParsedVersion {
  major: number
  minor: number
  patch: number
  pre: string[]
}

const VERSION_RE = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$/

export function parseVersion(text: string): ParsedVersion {
  const m = VERSION_RE.exec(text)
  if (!m) throw new Error(`not a version: ${JSON.stringify(text.slice(0, 40))}`)
  const [, major, minor, patch, pre] = m
  const parts = [Number(major), Number(minor), Number(patch)]
  if (!parts.every(Number.isSafeInteger)) throw new Error('version component too large')
  return { major: parts[0]!, minor: parts[1]!, patch: parts[2]!, pre: pre ? pre.split('.') : [] }
}

function comparePre(a: string[], b: string[]): number {
  // A version without a prerelease outranks one with it.
  if (a.length === 0 && b.length === 0) return 0
  if (a.length === 0) return 1
  if (b.length === 0) return -1
  const n = Math.max(a.length, b.length)
  for (let i = 0; i < n; i++) {
    const x = a[i]
    const y = b[i]
    if (x === undefined) return -1
    if (y === undefined) return 1
    const xn = /^\d+$/.test(x)
    const yn = /^\d+$/.test(y)
    if (xn && yn) {
      const d = Number(x) - Number(y)
      if (d !== 0) return d < 0 ? -1 : 1
    } else if (xn) {
      return -1
    } else if (yn) {
      return 1
    } else if (x !== y) {
      return x < y ? -1 : 1
    }
  }
  return 0
}

/** Negative when a < b, zero when equal, positive when a > b. */
export function compareVersions(a: string, b: string): number {
  const x = parseVersion(a)
  const y = parseVersion(b)
  for (const k of ['major', 'minor', 'patch'] as const) {
    if (x[k] !== y[k]) return x[k] < y[k] ? -1 : 1
  }
  return comparePre(x.pre, y.pre)
}
