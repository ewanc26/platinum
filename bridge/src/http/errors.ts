export type BridgeErrorCode =
  | 'invalid_json'
  | 'invalid_code'
  | 'invalid_or_expired_code'
  | 'missing_bearer_token'
  | 'invalid_token'
  | 'missing_text'
  | 'text_too_long'
  | 'upstream_error'
  | 'bridge_error'

export class BridgeError extends Error {
  constructor(
    public readonly code: BridgeErrorCode,
    public readonly status: number,
    message: string,
  ) {
    super(message)
    this.name = 'BridgeError'
  }
}

export function errorBody(code: BridgeErrorCode, message: string): {
  error: BridgeErrorCode
  message: string
} {
  return { error: code, message }
}
