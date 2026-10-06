import { createServer, type IncomingMessage, type ServerResponse } from 'node:http'
import { readFile, unlink } from 'node:fs/promises'
import { join } from 'node:path'
import { Agent } from '@atproto/api'
import { NodeOAuthClient } from '@atproto/oauth-client-node'
import { loadConfig } from './config.js'
import { AtprotoClient } from './atproto/client.js'
import { AppPasswordService, FailureLimiter, InvalidCredentialsError, InvalidServiceError, validateService } from './auth/app-password.js'
import { PairingService } from './auth/pairing.js'
import { TokenService } from './auth/tokens.js'
import { DomainApi, validReplyGate, validActor, validCollectionUri, validPostRef, validQuery, validSeenAt } from './domain/api.js'
import { ImageService, parseImageParams, validImageRef } from './image/fetch.js'
import { ImageError } from './image/convert.js'
import { validMutedWord } from './domain/muted.js'
import { upstreamError } from './atproto/errors.js'
import { BridgeError, errorBody } from './http/errors.js'
import { binary, html, json, readBody, redirect, RequestBodyTooLargeError } from './http/json.js'
import { FileStorage } from './storage/file.js'

const config = loadConfig()
const storage = new FileStorage(config.dataDir)
await storage.init()

const oauth = new NodeOAuthClient({
  clientMetadata: {
    client_id: new URL('/client-metadata.json', config.publicUrl).href,
    client_name: 'Platinum Bridge',
    client_uri: config.publicUrl,
    redirect_uris: [new URL('/atproto-oauth-callback', config.publicUrl).href],
    grant_types: ['authorization_code', 'refresh_token'],
    response_types: ['code'],
    application_type: 'web',
    token_endpoint_auth_method: 'none',
    dpop_bound_access_tokens: true,
    scope: 'atproto',
  },
  stateStore: storage.stateStore(),
  sessionStore: storage.sessionStore(),
})

const pairing = new PairingService()
const tokens = new TokenService(storage.installations())
const appPassword = new AppPasswordService(storage.appPasswordSessions())
const loginFailures = new FailureLimiter()
const atproto = new AtprotoClient(oauth, appPassword)
const domain = new DomainApi()
const images = new ImageService()

await migrateLegacyTokens()

async function migrateLegacyTokens(): Promise<void> {
  try {
    const content = await readFile(join(config.dataDir, 'tokens.json'), 'utf8')
    const legacy = JSON.parse(content) as Record<string, { did: string }>

    for (const [token, record] of Object.entries(legacy)) {
      if (record?.did) {
        await tokens.issue(record.did, token)
      }
    }

    await unlink(join(config.dataDir, 'tokens.json'))
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code !== 'ENOENT') throw error
  }
}

function bearerToken(req: IncomingMessage): string | undefined {
  const authorization = req.headers.authorization
  return authorization?.startsWith('Bearer ') ? authorization.slice(7) : undefined
}

function limit(value: string | null): number {
  const parsed = Number(value ?? 20)
  if (!Number.isFinite(parsed)) return 20
  return Math.min(Math.max(Math.trunc(parsed), 1), 50)
}

async function authenticatedAgent(req: IncomingMessage): Promise<Agent | undefined> {
  const token = bearerToken(req)
  if (!token) return undefined

  const record = await tokens.authenticate(token)
  if (!record) return undefined

  return atproto.forInstallation(record)
}

async function route(req: IncomingMessage, res: ServerResponse): Promise<void> {
  const url = new URL(req.url ?? '/', config.baseUrl)

  if (req.method === 'GET' && url.pathname === '/health') {
    return json(res, 200, { ok: true, service: 'platinum-bridge', version: config.version })
  }

  if (req.method === 'GET' && url.pathname === '/client-metadata.json') {
    return json(res, 200, oauth.clientMetadata)
  }

  if (req.method === 'GET' && url.pathname === '/login') {
    const handle = url.searchParams.get('handle')
    if (!handle) {
      return html(res, 400, '<h1>Platinum</h1><p>A Bluesky handle is required.</p>')
    }

    const target = await oauth.authorize(handle)
    return redirect(res, target.toString())
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
    let input: { code?: string; clientVersion?: string; installationLabel?: string }

    try {
      input = JSON.parse(await readBody(req, config.maxBodyBytes)) as { code?: string }
    } catch (error) {
      if (error instanceof RequestBodyTooLargeError) {
        throw new BridgeError('invalid_json', 413, 'The request body is too large.')
      }
      return json(res, 400, errorBody('invalid_json', 'The request body is not valid JSON.'))
    }

    if (!input.code || !/^[A-Z2-9]{6}$/i.test(input.code)) {
      return json(res, 400, errorBody('invalid_code', 'The pairing code is invalid.'))
    }

    const record = pairing.exchange(input.code)
    if (!record) {
      return json(res, 401, errorBody('invalid_or_expired_code', 'The pairing code is invalid or expired.'))
    }

    const issued = await tokens.issue(record.did, record.token, {
      clientVersion: input.clientVersion,
      installationLabel: input.installationLabel,
    })
    return json(res, 200, {
      protocol: 1,
      token: issued.token,
      did: issued.record.did,
      installationId: issued.record.id,
    })
  }

  if (req.method === 'POST' && url.pathname === '/v1/login/app-password') {
    if (!config.allowAppPassword) {
      return json(res, 403, errorBody('app_password_disabled', 'App-password sign-in is not enabled on this bridge.'))
    }
    const peer = req.socket.remoteAddress ?? 'unknown'
    if (loginFailures.blocked(peer)) {
      return json(res, 429, errorBody('too_many_attempts', 'Too many failed sign-in attempts. Try again later.'))
    }

    let input: { identifier?: unknown; password?: unknown; service?: unknown; clientVersion?: string; installationLabel?: string }
    try {
      input = JSON.parse(await readBody(req, config.maxBodyBytes)) as typeof input
    } catch (error) {
      if (error instanceof RequestBodyTooLargeError) {
        throw new BridgeError('invalid_json', 413, 'The request body is too large.')
      }
      return json(res, 400, errorBody('invalid_json', 'The request body is not valid JSON.'))
    }
    if (typeof input.identifier !== 'string' || typeof input.password !== 'string' ||
        !input.identifier || !input.password || input.identifier.length > 256 || input.password.length > 256) {
      return json(res, 400, errorBody('invalid_request', 'A handle and an app password are required.'))
    }
    let service: string
    try {
      service = validateService(typeof input.service === 'string' ? input.service : undefined)
    } catch (error) {
      if (error instanceof InvalidServiceError) return json(res, 400, errorBody('invalid_service', error.message))
      throw error
    }

    let signedIn
    try {
      signedIn = await appPassword.login(input.identifier, input.password, service)
    } catch (error) {
      if (error instanceof InvalidCredentialsError) {
        loginFailures.fail(peer)
        return json(res, 401, errorBody('invalid_credentials', 'The handle or app password is not valid.'))
      }
      throw error
    }
    const issued = await tokens.issue(signedIn.did, undefined, {
      clientVersion: typeof input.clientVersion === 'string' ? input.clientVersion : undefined,
      installationLabel: typeof input.installationLabel === 'string' ? input.installationLabel : undefined,
      authKind: 'app-password',
    })
    await appPassword.save(issued.record.id, signedIn.service, signedIn.session)
    return json(res, 200, { protocol: 1, token: issued.token, did: issued.record.did, installationId: issued.record.id })
  }

  if (req.method === 'POST' && url.pathname === '/v1/revoke') {
    const token = bearerToken(req)
    if (!token) {
      return json(res, 401, errorBody('missing_bearer_token', 'A Platinum installation token is required.'))
    }

    const revoked = await tokens.revokeRecord(token)
    if (revoked?.authKind === 'app-password') await appPassword.forget(revoked.id)
    if (!revoked) {
      return json(res, 404, errorBody('invalid_token', 'The Platinum installation token is not valid.'))
    }

    return json(res, 200, { ok: true })
  }

  const token = bearerToken(req)
  const agent = await authenticatedAgent(req)
  if (!agent) {
    return json(
      res,
      401,
      errorBody(
        token ? 'invalid_token' : 'missing_bearer_token',
        token
          ? 'The Platinum installation token is not valid.'
          : 'A Platinum installation token is required.',
      ),
    )
  }

  if (req.method === 'GET' && url.pathname === '/v1/profile') {
    const asked = url.searchParams.get('actor')
    let actor: string | undefined
    if (asked !== null) {
      actor = validActor(asked)
      if (!actor) return json(res, 400, errorBody('invalid_actor', 'actor must be a handle or a DID.'))
    }
    const profile = await domain.profile(agent, actor)
    if (!profile) return json(res, 404, errorBody('actor_not_found', 'No such account.'))
    return json(res, 200, profile)
  }

  if (req.method === 'GET' && url.pathname === '/v1/timeline') {
    const cursor = url.searchParams.get('cursor') ?? undefined
    return json(res, 200, await domain.timeline(agent, limit(url.searchParams.get('limit')), cursor))
  }

  if (req.method === 'GET' && url.pathname === '/v1/notifications') {
    const cursor = url.searchParams.get('cursor') ?? undefined
    return json(res, 200, await domain.notifications(agent, limit(url.searchParams.get('limit')), cursor))
  }

  if (req.method === 'POST' && (url.pathname === '/v1/like' || url.pathname === '/v1/repost')) {
    let input: { uri?: unknown; cid?: unknown; on?: unknown }
    try {
      input = JSON.parse(await readBody(req, config.maxBodyBytes)) as typeof input
    } catch (error) {
      if (error instanceof RequestBodyTooLargeError) {
        throw new BridgeError('invalid_json', 413, 'The request body is too large.')
      }
      return json(res, 400, errorBody('invalid_json', 'The request body is not valid JSON.'))
    }
    const ref = validPostRef(input.uri, input.cid)
    if (!ref || typeof input.on !== 'boolean') {
      return json(res, 400, errorBody('invalid_post_ref', 'A post uri, cid and a boolean "on" are required.'))
    }
    const kind = url.pathname === '/v1/like' ? 'like' : 'repost'
    const result = await domain.toggle(agent, kind, ref, input.on)
    if (!result.on && input.on) {
      return json(res, 404, errorBody('post_not_found', 'The post no longer exists.'))
    }
    return json(res, 200, result)
  }

  if (req.method === 'POST' && url.pathname === '/v1/notifications/seen') {
    let input: { seenAt?: unknown }
    try {
      input = JSON.parse(await readBody(req, config.maxBodyBytes)) as typeof input
    } catch (error) {
      if (error instanceof RequestBodyTooLargeError) {
        throw new BridgeError('invalid_json', 413, 'The request body is too large.')
      }
      return json(res, 400, errorBody('invalid_json', 'The request body is not valid JSON.'))
    }
    const seenAt = validSeenAt(input.seenAt)
    if (!seenAt) {
      return json(res, 400, errorBody('invalid_seen_at', 'seenAt must be an ISO 8601 timestamp.'))
    }
    return json(res, 200, await domain.markSeen(agent, seenAt))
  }

  if (req.method === 'GET' && (url.pathname === '/v1/follows' || url.pathname === '/v1/followers' || url.pathname === '/v1/author-feed')) {
    const actor = validActor(url.searchParams.get('actor'))
    if (!actor) return json(res, 400, errorBody('invalid_actor', 'actor must be a handle or a DID.'))
    const cursor = url.searchParams.get('cursor') ?? undefined
    const count = limit(url.searchParams.get('limit'))
    try {
      if (url.pathname === '/v1/author-feed') return json(res, 200, await domain.authorFeed(agent, actor, count, cursor))
      return json(res, 200, await domain.actors(agent, url.pathname === '/v1/follows' ? 'follows' : 'followers', actor, count, cursor))
    } catch (error) {
      if ((error as { status?: number }).status === 400) return json(res, 404, errorBody('actor_not_found', 'No such account.'))
      throw error
    }
  }

  if (req.method === 'POST' && (url.pathname === '/v1/follow' || url.pathname === '/v1/mute' || url.pathname === '/v1/block')) {
    let input: { did?: unknown; on?: unknown }
    try {
      input = JSON.parse(await readBody(req, config.maxBodyBytes)) as typeof input
    } catch (error) {
      if (error instanceof RequestBodyTooLargeError) {
        throw new BridgeError('invalid_json', 413, 'The request body is too large.')
      }
      return json(res, 400, errorBody('invalid_json', 'The request body is not valid JSON.'))
    }
    const did = typeof input.did === 'string' && input.did.startsWith('did:') ? validActor(input.did) : undefined
    if (!did || typeof input.on !== 'boolean') {
      return json(res, 400, errorBody('invalid_actor', 'A DID and a boolean "on" are required.'))
    }
    const result = url.pathname === '/v1/mute' ? await domain.mute(agent, did, input.on) : url.pathname === '/v1/block' ? await domain.block(agent, did, input.on) : await domain.follow(agent, did, input.on)
    if (!result) return json(res, 404, errorBody('actor_not_found', 'No such account.'))
    return json(res, 200, result)
  }

  if (req.method === 'GET' && (url.pathname === '/v1/post/likes' || url.pathname === '/v1/post/reposts')) {
    const ref = validPostRef(url.searchParams.get('uri'), 'x')
    if (!ref) return json(res, 400, errorBody('invalid_post_ref', 'uri must be an app.bsky.feed.post AT URI.'))
    const kind = url.pathname === '/v1/post/likes' ? 'likes' : 'reposts'
    const result = await domain.engagement(agent, kind, ref.uri, limit(url.searchParams.get('limit')), url.searchParams.get('cursor') ?? undefined)
    if (!result) return json(res, 404, errorBody('post_not_found', 'The post no longer exists.'))
    return json(res, 200, result)
  }

  if (req.method === 'GET' && (url.pathname === '/v1/search/actors' || url.pathname === '/v1/search/posts')) {
    const q = validQuery(url.searchParams.get('q'))
    if (!q) return json(res, 400, errorBody('invalid_query', 'q must be 1 to 100 characters.'))
    const cursor = url.searchParams.get('cursor') ?? undefined
    const count = limit(url.searchParams.get('limit'))
    return json(res, 200, url.pathname === '/v1/search/actors'
      ? await domain.searchActors(agent, q, count, cursor)
      : await domain.searchPosts(agent, q, count, cursor))
  }

  if (req.method === 'GET' && url.pathname === '/v1/feeds') return json(res, 200, await domain.savedFeeds(agent))
  if (req.method === 'GET' && url.pathname === '/v1/image') {
    const ref = validImageRef(url.searchParams.get('ref'))
    const params = parseImageParams(url.searchParams.get('w'), url.searchParams.get('depth'))
    if (!ref || !params) {
      return json(res, 400, errorBody('invalid_image', 'ref must be an image reference the bridge gave you; w is 16 to 320; depth is 4 or 8.'))
    }
    try {
      const out = await images.get({ ref, ...params })
      return binary(res, 200, 'application/x-platinum-image', out.blob)
    } catch (error) {
      if (error instanceof ImageError) {
        return json(res, error.code === 'too_large' ? 413 : 502, errorBody('image_unavailable', 'The image could not be prepared.'))
      }
      return json(res, 502, errorBody('image_unavailable', 'The image could not be fetched.'))
    }
  }

  if (req.method === 'GET' && url.pathname === '/v1/muted-words') {
    return json(res, 200, { words: await domain.mutedWords.list(agent) })
  }

  if (req.method === 'POST' && url.pathname === '/v1/muted-words') {
    let input: { value?: unknown; on?: unknown }
    try {
      input = JSON.parse(await readBody(req, config.maxBodyBytes)) as typeof input
    } catch (error) {
      if (error instanceof RequestBodyTooLargeError) {
        throw new BridgeError('invalid_json', 413, 'The request body is too large.')
      }
      return json(res, 400, errorBody('invalid_json', 'The request body is not valid JSON.'))
    }
    const value = validMutedWord(input.value)
    if (!value || typeof input.on !== 'boolean') {
      return json(res, 400, errorBody('invalid_word', 'A word of 1 to 100 characters and a boolean "on" are required.'))
    }
    return json(res, 200, await domain.mutedWords.set(agent, value, input.on))
  }

  if (req.method === 'GET' && url.pathname === '/v1/lists') return json(res, 200, await domain.lists(agent))

  if (req.method === 'GET' && (url.pathname === '/v1/feed' || url.pathname === '/v1/list')) {
    const isFeed = url.pathname === '/v1/feed'
    const uri = validCollectionUri(url.searchParams.get('uri'), isFeed ? 'app.bsky.feed.generator' : 'app.bsky.graph.list')
    if (!uri) return json(res, 400, errorBody('invalid_uri', 'uri must be an AT URI of a feed or a list.'))
    const cursor = url.searchParams.get('cursor') ?? undefined
    const count = limit(url.searchParams.get('limit'))
    const result = isFeed ? await domain.feed(agent, uri, count, cursor) : await domain.listMembers(agent, uri, count, cursor)
    if (!result) return json(res, 404, errorBody('not_found', 'No such feed or list.'))
    return json(res, 200, result)
  }

  if (req.method === 'POST' && url.pathname === '/v1/post/delete') {
    let input: { uri?: unknown }
    try {
      input = JSON.parse(await readBody(req, config.maxBodyBytes)) as typeof input
    } catch (error) {
      if (error instanceof RequestBodyTooLargeError) {
        throw new BridgeError('invalid_json', 413, 'The request body is too large.')
      }
      return json(res, 400, errorBody('invalid_json', 'The request body is not valid JSON.'))
    }
    const ref = validPostRef(input.uri, 'x')
    if (!ref) return json(res, 400, errorBody('invalid_post_ref', 'uri must be an app.bsky.feed.post AT URI.'))
    if (await domain.deletePost(agent, ref.uri) === 'not_yours') {
      return json(res, 403, errorBody('not_your_post', 'You can only delete your own posts.'))
    }
    return json(res, 200, { uri: ref.uri, deleted: true })
  }

  if (req.method === 'GET' && url.pathname === '/v1/thread') {
    const ref = validPostRef(url.searchParams.get('uri'), 'x')
    if (!ref) return json(res, 400, errorBody('invalid_post_ref', 'uri must be an app.bsky.feed.post AT URI.'))
    const thread = await domain.thread(agent, ref)
    if (!thread) return json(res, 404, errorBody('post_not_found', 'The post no longer exists.'))
    return json(res, 200, thread)
  }

  if (req.method === 'POST' && url.pathname === '/v1/post') {
    let input: { text?: string; replyTo?: { uri?: unknown; cid?: unknown }; quote?: { uri?: unknown; cid?: unknown }; replyGate?: unknown }

    try {
      input = JSON.parse(await readBody(req, config.maxBodyBytes)) as typeof input
    } catch (error) {
      if (error instanceof RequestBodyTooLargeError) {
        throw new BridgeError('invalid_json', 413, 'The request body is too large.')
      }
      return json(res, 400, errorBody('invalid_json', 'The request body is not valid JSON.'))
    }

    if (!input.text?.trim()) {
      return json(res, 400, errorBody('missing_text', 'Post text is required.'))
    }
    if (input.text.length > 300) {
      return json(res, 400, errorBody('text_too_long', 'Post text must not exceed 300 characters.'))
    }

    let replyTo: { uri: string; cid: string } | undefined
    if (input.replyTo !== undefined) {
      replyTo = typeof input.replyTo === 'object' && input.replyTo !== null
        ? validPostRef(input.replyTo.uri, input.replyTo.cid)
        : undefined
      if (!replyTo) {
        return json(res, 400, errorBody('invalid_post_ref', 'replyTo must name a post by uri and cid.'))
      }
    }
    let quote: { uri: string; cid: string } | undefined
    if (input.quote !== undefined) {
      quote = typeof input.quote === 'object' && input.quote !== null
        ? validPostRef(input.quote.uri, input.quote.cid)
        : undefined
      if (!quote) {
        return json(res, 400, errorBody('invalid_post_ref', 'quote must name a post by uri and cid.'))
      }
    }
    let replyGate: ReturnType<typeof validReplyGate>
    if (input.replyGate !== undefined) {
      replyGate = validReplyGate(input.replyGate)
      if (!replyGate || (replyTo && replyGate !== 'everyone')) {
        return json(res, 400, errorBody('invalid_reply_gate', 'replyGate is everyone, nobody, mentioned, following or followers, and only on a post that is not itself a reply.'))
      }
    }
    const posted = await domain.post(agent, input.text, replyTo, { quote, replyGate })
    if (!posted) return json(res, 404, errorBody('post_not_found', 'The post being replied to or quoted no longer exists.'))
    return json(res, 200, posted)
  }

  return json(res, 404, { error: 'not_found', message: 'The requested endpoint does not exist.' })
}

createServer((req, res) => {
  route(req, res).catch(error => {
    if (error instanceof BridgeError) {
      json(res, error.status, errorBody(error.code, error.message))
      return
    }

    const mapped = upstreamError(error)
    if (mapped.code === 'upstream_error') {
      json(res, mapped.status, errorBody(mapped.code, mapped.message))
      return
    }

    console.error('Platinum Bridge request failed')
    json(res, 500, errorBody('bridge_error', 'The backend could not complete the request.'))
  })
}).listen(config.port, config.host, () => {
  console.log('Platinum Bridge listening on ' + config.baseUrl)
})
