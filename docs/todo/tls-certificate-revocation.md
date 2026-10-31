---
id: tls-certificate-revocation
title: No certificate revocation checking (OCSP / CRL) in the TLS client
status: open
priority: unranked
scope: Network/TLSConnection, Network/TrustStore
opened: 2026-08-27
tags: [network, tls]
---

# No certificate revocation checking (OCSP / CRL) in the TLS client

## Why

`TLSConnection` verifies the peer chain against the trust store (`verify_peer`, `src/Network/TLSConnection.cpp:315`)
but never checks revocation: a revoked server certificate is accepted until it expires.

## What remains

- Decide the mechanism with the owner (OCSP stapling check, OCSP query, CRL, or an explicit "not supported"
  documented in the API).
- If implemented: a soft-fail / hard-fail policy option and a test against a revoked test certificate.

## References

- `docs/plans/network-tls/README.md` (2026-08-27 network audit, § "Not fixed here, tracked separately")
