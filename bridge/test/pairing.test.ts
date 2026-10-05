import assert from 'node:assert/strict'
import test from 'node:test'
import { PairingService } from '../src/auth/pairing.js'

test('pairing codes are single-use', () => {
  const pairing = new PairingService()
  const code = pairing.create('did:plc:test')

  const first = pairing.exchange(code)
  const second = pairing.exchange(code)

  assert.equal(first?.did, 'did:plc:test')
  assert.equal(second, undefined)
})

test('expired pairing codes cannot be exchanged', () => {
  const pairing = new PairingService()
  const code = pairing.create('did:plc:test', -1)

  assert.equal(pairing.exchange(code), undefined)
})
