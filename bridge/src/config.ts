import { resolve } from 'node:path'

export interface BridgeConfig {
  host: string
  port: number
  baseUrl: string
  publicUrl: string
  dataDir: string
  version: string
  maxBodyBytes: number
  /** Off unless the operator opts in: it makes the bridge handle account passwords. */
  allowAppPassword: boolean
}

export function loadConfig(env: NodeJS.ProcessEnv = process.env): BridgeConfig {
  const port = Number(env.PLATINUM_BRIDGE_PORT ?? 8787)
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    throw new Error('PLATINUM_BRIDGE_PORT must be a valid TCP port')
  }

  const host = env.PLATINUM_BRIDGE_HOST ?? '127.0.0.1'
  const baseUrl = env.PLATINUM_BRIDGE_URL ?? `http://${host}:${port}`
  const publicUrl = env.PLATINUM_BRIDGE_PUBLIC_URL ?? baseUrl
  const dataDir = resolve(env.PLATINUM_BRIDGE_DATA_DIR ?? '.platinum-bridge')

  new URL(baseUrl)
  new URL(publicUrl)

  return {
    host,
    port,
    baseUrl,
    publicUrl,
    dataDir,
    version: '0.2.0',
    maxBodyBytes: 64 * 1024,
    allowAppPassword: env.PLATINUM_BRIDGE_ALLOW_APP_PASSWORD === '1',
  }
}
