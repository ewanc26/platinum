import { wolfram } from '../wolfram.js'

// Semantic version order: Wolfram's wf_update_compare_versions (update.h),
// running in Wolfram's C through the Node addon. Nothing is ported any more:
// the strict-semver rule, malformed-version refusals and precedence order are
// Wolfram's, and a malformed version throws with the WF_ERR_PARSE status name
// as `code`.
//
// Negative when a < b, zero when equal, positive when a > b.
// Throws on a malformed version.
export function compareVersions(a: string, b: string): number {
  return wolfram.compareVersions(a, b)
}
