---
id: uri-parser-fuzzer
title: No fuzz target covers the `URI` / `URIDomain` parsers
status: open
priority: unranked
scope: Fuzzing, Network/URI
opened: 2026-08-27
tags: [network, tls]
---

# No fuzz target covers the `URI` / `URIDomain` parsers

## Why

`src/Fuzzing/` covers the HTTP response parser and many file formats, but not the URI parsing. The host
percent-decode was a CRLF-injection site during the 2026-08-27 network audit: the parser handles untrusted input
and deserves the same treatment as `fuzz_http_response.cpp`.

## What remains

- Add `fuzz_uri.cpp` (parse, re-emit, parse again; no crash, no CR/LF in a host or a request line).
- Wire it into `build-fuzzers.sh` and `src/Fuzzing/README.md`.

## References

- `docs/plans/network-tls/README.md` (2026-08-27 network audit, § "Not fixed here, tracked separately")
