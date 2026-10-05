import { createInterface } from 'node:readline/promises'
import { fileURLToPath } from 'node:url'
import { applyUpdate, checkForUpdate, readPackageVersion, rollback } from './install.js'

// platinum-bridge update <check|apply|rollback> [--yes]
//
// Never runs on its own. `apply` and `rollback` change which version starts on
// the next restart, so they ask for confirmation unless --yes is given, and
// they never restart the bridge. No token or credential is read or printed:
// release assets are public.

const usage = 'usage: update <check|apply|rollback> [--yes]   (apply and rollback need PLATINUM_BRIDGE_INSTALL_DIR)'

async function confirm(question: string, yes: boolean): Promise<boolean> {
  if (yes) return true
  if (!process.stdin.isTTY) return false
  const rl = createInterface({ input: process.stdin, output: process.stdout })
  const answer = await rl.question(`${question} [y/N] `)
  rl.close()
  return answer.trim().toLowerCase() === 'y'
}

async function main(): Promise<number> {
  const [cmd, ...flags] = process.argv.slice(2)
  const yes = flags.includes('--yes')
  const current = await readPackageVersion(fileURLToPath(new URL('../../package.json', import.meta.url)))
  const root = process.env.PLATINUM_BRIDGE_INSTALL_DIR

  if (cmd === 'check') {
    const check = await checkForUpdate(current)
    console.log(check.available ? `update available: ${current} -> ${check.latest}` : `up to date (${current})`)
    return 0
  }
  if (cmd === 'apply') {
    if (!root) { console.error(usage); return 2 }
    const check = await checkForUpdate(current)
    if (!check.available) { console.log(`up to date (${current})`); return 0 }
    if (!(await confirm(`Install ${check.latest} (SHA-256 will be verified) into ${root}?`, yes))) {
      console.log('not installed: confirmation required (pass --yes in a script)')
      return 1
    }
    const dir = await applyUpdate(check, root)
    console.log(`installed ${check.latest} at ${dir}; the previous version is kept. Restart the bridge to use it.`)
    return 0
  }
  if (cmd === 'rollback') {
    if (!root) { console.error(usage); return 2 }
    if (!(await confirm('Switch back to the previous version?', yes))) return 1
    console.log(`rolled back to ${await rollback(root)}. Restart the bridge to use it.`)
    return 0
  }
  console.error(usage)
  return 2
}

main().then(code => process.exit(code), err => {
  console.error(`update failed: ${err instanceof Error ? err.message : 'unknown error'}`)
  process.exit(1)
})
