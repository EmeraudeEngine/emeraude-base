---
id: tls-stack-windows-macos-validation
title: Validate the TLS stack build + run on Windows and macOS
status: open
priority: medium
scope: src/Network
opened: 2026-07-04
tags: [tls, cross-platform]
---

# Validate the TLS stack build + run on Windows and macOS

## Why

The TLS stack (LibreSSL via `asio::ssl`, under `-fno-exceptions` / `ASIO_NO_EXCEPTIONS`) is
validated on Linux only. The cascade is strict cross-platform.

Carried as a post-plan follow-up of "Ave robustus!" — explicitly NOT a blocker to its closure.

## What remains

- [ ] Build and RUN the TLS stack on Windows.
- [ ] Build and RUN it on macOS.

## References

- `docs/plans/network-tls/README.md` (the gating PoC and the provider decision).
