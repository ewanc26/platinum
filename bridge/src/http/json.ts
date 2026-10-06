import type { IncomingMessage, ServerResponse } from 'node:http'

export function json(res: ServerResponse, status: number, value: unknown): void {
  res.writeHead(status, {
    'content-type': 'application/json; charset=utf-8',
    'cache-control': 'no-store',
  })
  res.end(JSON.stringify(value))
}

export function binary(res: ServerResponse, status: number, contentType: string, body: Uint8Array): void {
  res.writeHead(status, {
    'content-type': contentType,
    'content-length': String(body.length),
    'cache-control': 'private, max-age=3600',
  })
  res.end(body)
}

export function html(res: ServerResponse, status: number, value: string): void {
  res.writeHead(status, {
    'content-type': 'text/html; charset=utf-8',
    'cache-control': 'no-store',
  })
  res.end(value)
}

export function redirect(res: ServerResponse, location: string): void {
  res.writeHead(302, { location, 'cache-control': 'no-store' })
  res.end()
}

export async function readBody(req: IncomingMessage, maxBytes: number): Promise<string> {
  const chunks: Buffer[] = []
  let length = 0

  for await (const chunk of req) {
    const part = Buffer.from(chunk)
    length += part.length
    if (length > maxBytes) {
      throw new RequestBodyTooLargeError()
    }
    chunks.push(part)
  }

  return Buffer.concat(chunks).toString('utf8')
}

export class RequestBodyTooLargeError extends Error {
  constructor() {
    super('request body too large')
    this.name = 'RequestBodyTooLargeError'
  }
}
