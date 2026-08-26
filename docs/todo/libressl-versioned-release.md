---
id: libressl-versioned-release
title: Ship a versioned LibreSSL release in ext-deps-generator
status: open
priority: medium
scope: dependencies
opened: 2026-07-04
tags: [tls, build]
---

# Ship a versioned LibreSSL release in ext-deps-generator

## Why

Machines without the symlink layout have no LibreSSL to link against: the dependency is consumed
from a local checkout instead of a versioned archive. `ext-deps-generator` produces per-platform
archives for every other third-party library — LibreSSL is the vendored exception.

Carried as a post-plan follow-up of "Ave robustus!" — not a blocker to its closure.

## What remains

- [ ] Produce a versioned LibreSSL release in `ext-deps-generator` and consume it like the others.
