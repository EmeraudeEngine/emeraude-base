---
id: http-total-timeout-hard-budget
title: `totalTimeout` is not a hard budget: per-operation timeouts add up and DNS resolution cannot be cancelled
status: open
priority: unranked
scope: Network/HTTPSClient
opened: 2026-08-27
tags: [network, tls]
---

# `totalTimeout` is not a hard budget: per-operation timeouts add up and DNS resolution cannot be cancelled

## Why

`HTTPSClientOptions::totalTimeout` (`src/Network/HTTPSClient.hpp:143`, default 120 s) reads as a deadline for the
whole request, but the per-operation timeouts (connect, TLS handshake, write, shutdown) add up independently, and
the DNS resolution is not cancellable. A caller that relies on the total can wait far longer than it asked.

## What remains

- Turn `totalTimeout` into one deadline every operation consults (each step gets `min(own timeout, time left)`).
- Make the resolution cancellable or bounded (asynchronous resolve under the same deadline).
- A test with a stalling server proves the request ends at the total, not at the sum.

## References

- `docs/plans/network-tls/README.md` (2026-08-27 network audit, § "Not fixed here, tracked separately")
