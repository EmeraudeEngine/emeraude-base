---
id: uri-layer-preserves-encoding
title: The URI layer decodes `%2F` and re-orders query parameters, which breaks presigned URLs
status: open
priority: unranked
scope: Network/URI
opened: 2026-08-27
tags: [network, tls]
---

# The URI layer decodes `%2F` and re-orders query parameters, which breaks presigned URLs

## Why

`URI` / `URIDomain` (`src/Network/URI.hpp`, `src/Network/URIDomain.hpp`) decode `%2F` and re-emit `/`, and they
re-order and deduplicate query parameters. A presigned URL (object storage, signed API calls) signs the exact
path and query bytes: once re-emitted, the signature no longer matches. A unit test currently freezes the
re-ordering behaviour.

## What remains

- Keep the path and the query as their original encoded bytes for re-emission; decode only for inspection.
- Rewrite the test that freezes the current behaviour, and add one with a real presigned-URL shape.

## References

- `docs/plans/network-tls/README.md` (2026-08-27 network audit, § "Not fixed here, tracked separately")
