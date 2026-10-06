// The public half of Platinum's release signing key: 32 bytes of Ed25519 as 64
// hex characters. The private half exists only as the GitHub Actions secret
// UPDATE_SIGNING_KEY, which .github/workflows/sign-release.yml uses to sign each
// release's update.json. Replacing this key means shipping a bridge that
// carries the new one, signed by the old one; an updater trusts only the key it
// has.
export const UPDATE_PUBLIC_KEY_HEX = 'cefc93876f3acc5490251f539d73e583d63e0591aad73f6324a6c98b4b17a8a6'
