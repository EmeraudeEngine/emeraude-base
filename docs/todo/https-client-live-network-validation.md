---
id: https-client-live-network-validation
title: Live-network validation of the HTTPS client
status: open
priority: medium
scope: src/Network
opened: 2026-07-04
tags: [tls, testing]
---

# Live-network validation of the HTTPS client

## Why

The HTTPS client is proven by hermetic tests; the live-network (non-hermetic) path is exercised
only by opt-in tests that are **skipped** in the standard run (3 of them in the 1960-test suite).

Carried as a post-plan follow-up of "Ave robustus!" — not a blocker to its closure.

## What remains

- [ ] Run the opt-in live tests against real endpoints, and record what the 404/unavailable
  diagnostic reports.
