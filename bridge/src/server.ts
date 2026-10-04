import { createServer, type IncomingMessage, type ServerResponse } from 'node:http'
import { readFile, writeFile, mkdir } from 'node:fs/promises'
import { resolve } from 'node:path'
import { Agent } from '@atproto/api'
import { NodeOAuthClient } from '@atproto/oauth-client-node'
import { FileStore, PairingStore } from './store.js'

const port = Number(process.env.PLATINUM_BRIDGE_PORT ?? 8787)
const host = process.env.PLATINUM_BRIDGE_HOST ?? '127.0.0.1'
const baseUrl = process.env.PLATINUM_BRIDGE_URL ?? 'http://' + host + ':' + port
const publicUrl = process.env.PLATINUM_BRIDGE_PUBLIC_URL ?? baseUrl
const dataDir = resolve(process.env.PLATINUM_BRIDGE_DATA_DIR ?? '.platinum-bridge')

await mkdir(dataDir, { recursive: true })
const store = new FileStore(dataDir)
await store.init()
const pairing = new PairingStore()

const oauth = new NodeOAuthClient({
  clientMetadata: {
    client_id: new URL('/client-metadata.json', publicUrl).href,
    client_name: 'Platinum Bridge',
    client_uri: publicUrl,
    redirect_uris: [new URL('/atproto-oauth-callback', publicUrl).href],
    grant_types: ['authorization_code', 'refresh_token'],
    response_types: ['code'],
    application_type: 'web',
    token_endpoint_auth_method: 'none',
    dpop_bound_access_tokens: true,
    scope: 'atproto',
  },
  stateStore: store.stateStore(),
  sessionStore: store.sessionStore(),
})

type TokenRecord = { did: string }
const tokenPath = resolve(dataDir, 'tokens.json')

async function tokens(): Promise<Record<string, TokenRecord>> {
  try {
    return JSON.parse(await readFile(tokenPath, 'utf8')) as Record<string, TokenRecord>
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === 'ENOENT') return {}
    throw error
  }
}

async function tokenAgent(token: string): Promise<Agent | undefined> {
  const record = (await tokens())[token]
  if (!record) return undefined
  const session = await oauth.restore(record.did)
  return new Agent(session)
}

function json(res: ServerResponse, status: number, value: unknown): void {
  res.writeHead(status, {
    'content-type': 'application/json; charset=utf-8',
    'cache-control': 'no-store',
  })
  res.end(JSON.stringify(value))
}

function html(res: ServerResponse, status: number, value: string): void {
  res.writeHead(status, {
    'content-type': 'text/html; charset=utf-8',
    'cache-control': 'no-store',
  })
  res.end(value)
}

function redirect(res: ServerResponse, location: string): void {
  res.writeHead(302, { location, 'cache-control': 'no-store' })
  res.end()
}

async function readBody(req: IncomingMessage): Promise<string> {
  const chunks: Buffer[] = []
  for await (const chunk of req) chunks.push(Buffer.from(chunk))
  return Buffer.concat(chunks).toString('utf8')
}

async function route(req: IncomingMessage, res: ServerResponse): Promise<void> {
  const url = new URL(req.url ?? '/', baseUrl)

  if (req.method === 'GET' && url.pathname === '/health') {
    return json(res, 200, { ok: true, service: 'platinum-bridge', version: '0.1.0' })
  }

  if (req.method === 'GET' && url.pathname === '/client-metadata.json') {
    return json(res, 200, oauth.clientMetadata)
  }

  if (req.method === 'POST' && url.pathname === '/v1/revoke') {
    const authorization = req.headers.authorization
    const token = authorization?.startsWith('Bearer ') ? authorization.slice(7) : undefined
    if (!token) return json(res, 401, { error: 'missing_bearer_token' })
    const data = await tokens()
    if (!data[token]) return json(res, 404, { error: 'invalid_token' })
    delete data[token]
    await writeFile(tokenPath, JSON.stringify(data, null, 2), { mode: 0o600 })
    return json(res, 200, { ok: true })
  }

  if (req.method === 'GET' && url.pathname === '/login') {
    const handle = url.searchParams.get('handle')
    if (!handle) return html(res, 400, '<h1>Platinum</h1><p>A Bluesky handle is required.</p>')
    const target = await oauth.authorize(handle)
    return redirect(res, target)
  }

  if (req.method === 'GET' && url.pathname === '/atproto-oauth-callback') {
    const { session } = await oauth.callback(url.searchParams)
    const code = pairing.create(session.did)
    return html(
      res,
      200,
      '<!doctype html><meta charset="utf-8"><title>Platinum pairing</title>' +
      '<style>body{font:20px system-ui;max-width:36rem;margin:4rem auto;padding:1rem}code{font-size:2rem;letter-spacing:.25rem}</style>' +
      '<h1>Platinum</h1><p>OAuth succeeded. Enter this pairing code in Platinum:</p>' +
      '<p><code>' + code + '</code></p><p>The code expires in ten minutes and can only be used once.</p>',
    )
  }

  if (req.method === 'POST' && url.pathname === '/v1/pair') {
    let input: { code?: string }
    try {
      input = JSON.parse(await readBody(req)) as { code?: string }
    } catch {
      return json(res, 400, { error: 'invalid_json' })
    }
    if (!input.code) return json(res, 400, { error: 'missing_code' })
    const record = pairing.exchange(input.code)
    if (!record) return json(res, 401, { error: 'invalid_or_expired_code' })

    const data = await tokens()
    data[record.token] = { did: record.did }
    await writeFile(tokenPath, JSON.stringify(data, null, 2), { mode: 0o600 })

    return json(res, 200, { protocol: 1, token: record.token, did: record.did })
  }

  const authorization = req.headers.authorization
  const token = authorization?.startsWith('Bearer ') ? authorization.slice(7) : undefined
  if (!token) return json(res, 401, { error: 'missing_bearer_token' })

  const agent = await tokenAgent(token)
  if (!agent) return json(res, 401, { error: 'invalid_token' })

  if (req.method === 'GET' && url.pathname === '/v1/profile') {
    const result = await agent.getProfile({ actor: agent.did })
    return json(res, 200, result.data)
  }

  if (req.method === 'GET' && url.pathname === '/v1/timeline') {
    const limit = Math.min(Math.max(Number(url.searchParams.get('limit') ?? 20), 1), 50)
    const cursor = url.searchParams.get('cursor') ?? undefined
    const result = await agent.getTimeline({ limit, cursor })
    return json(res, 200, result.data)
  }

  if (req.method === 'GET' && url.pathname === '/v1/notifications') {
    const limit = Math.min(Math.max(Number(url.searchParams.get('limit') ?? 20), 1), 50)
    const cursor = url.searchParams.get('cursor') ?? undefined
    const result = await agent.listNotifications({ limit, cursor })
    return json(res, 200, result.data)
  }

  if (req.method === 'POST' && url.pathname === '/v1/post') {
    let input: { text?: string }
    try {
      input = JSON.parse(await readBody(req)) as { text?: string }
    } catch {
      return json(res, 400, { error: 'invalid_json' })
    }
    if (!input.text?.trim()) return json(res, 400, { error: 'missing_text' })
    const result = await agent.post({ text: input.text })
    return json(res, 200, result)
  }

  return json(res, 404, { error: 'not_found' })
}

createServer((req, res) => {
  route(req, res).catch(error => {
    console.error(error)
    json(res, 500, { error: 'bridge_error' })
  })
}).listen(port, host, () => {
  console.log('Platinum Bridge listening on ' + baseUrl)
})
