import { BridgeError } from '../http/errors.js'

export function upstreamError(error: unknown): BridgeError {
  if (error instanceof BridgeError) return error

  const status =
    typeof error === 'object' &&
    error !== null &&
    'status' in error &&
    typeof error.status === 'number'
      ? error.status
      : undefined

  if (status && status >= 400 && status < 600) {
    return new BridgeError('upstream_error', 502, 'The upstream AT Protocol service rejected or failed the request.')
  }

  return new BridgeError('bridge_error', 500, 'The backend could not complete the request.')
}
