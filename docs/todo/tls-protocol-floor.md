---
id: tls-protocol-floor
title: No minimum TLS version or cipher floor is set on the TLS context
status: open
priority: unranked
scope: Network/TLSConnection, Network/HTTPSClient
opened: 2026-08-27
tags: [network, tls]
---

# No minimum TLS version or cipher floor is set on the TLS context

## Why

The `asio::ssl::context` the clients receive has no explicit protocol floor (`SSL_CTX_set_min_proto_version`) or
cipher policy: what is negotiated depends on the TLS library's defaults and on how the caller built the context.

## What remains

- Set TLS 1.2 as the floor (1.3 preferred) where base creates or prepares a context, and document the cipher policy.
- A test proves a TLS 1.0/1.1-only server is refused.

## References

- `docs/plans/network-tls/README.md` (2026-08-27 network audit, § "Not fixed here, tracked separately")
