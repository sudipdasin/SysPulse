# Verified Alpha release trust anchor

The release owner supplied a public-only OpenPGP export on 2026-11-27. Its binary and armored exports match, one RSA-4096 primary public key is present, and no secret-key packet is present. The owner-side wizard reports a successful local signing test and protected private key; these private-key claims were not independently tested by the repository maintainer.

- Primary fingerprint: DA269C59E903F28BD46DA217F88380E446488691
- SHA-256 of binary public export: 1d16b08d5bf50ba03cd8f294a94ef7f5259f9c5f4795a39134e9fde4b5968dc6
- Key expiry: 2028-11-26 UTC.
- Scope: Alpha only. No Beta or Stable trust is provisioned.

The public export (including its public OpenPGP identity) is stored as base64 to keep the original binary bytes exact. Build with -DSYSPULSE_USE_OWNER_ALPHA_TRUST=ON to decode it, verify the fixed SHA-256, then apply the existing public-key-only GnuPG validation and package it under /usr/share/syspulse/update-keys. The opt-in is explicit because disposable build fixtures require unprovisioned keys; do not mistake a default evaluation build for an owner-trusted build. The initial package must be authenticated separately.

No private key, passphrase, local path, user report, raw bundle or credential was committed. Only the release owner can sign with the private key. No production feed has been created; no package has yet been signed with this key. The update and session acceptance hold remains in force. Do not publish an incomplete installer as satisfying it.
