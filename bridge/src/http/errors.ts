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
  | 'app_password_disabled'
  | 'too_many_attempts'
  | 'invalid_request'
  | 'invalid_service'
  | 'invalid_credentials'
  | 'invalid_post_ref'
  | 'post_not_found'
  | 'invalid_seen_at'

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
