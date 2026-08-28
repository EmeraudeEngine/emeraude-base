---
id: httpsclient-keep-alive
title: "HTTPSClient opens one TLS connection per hop — no keep-alive reuse"
status: open
priority: medium
scope: src/Network
opened: 2026-08-28
tags: [network, tls, performance]
---

# HTTPSClient opens one TLS connection per hop — no keep-alive reuse

## Why

`performHop()` sends `Connection: close` and builds a fresh `TLSConnection` for every hop. That was
acceptable while the only consumer was `download()`, where one handshake amortises over megabytes.

It stopped being acceptable on 2026-08-28, when `request()` made the stack an **API** client: an
engine polling an endpoint, or issuing a burst of small calls, pays a full TCP connect **and** a
full TLS handshake per call — typically two round trips of latency and a signature verification for
a response of a few hundred bytes. `Net::APIClient` performs exactly that shape of traffic.

## What remains

- A connection pool keyed by (host, port), with idle eviction.
- Stop sending `Connection: close` when the pool is in play; honour the server's `Connection`
  response header and HTTP/1.1's default-persistent semantics.
- `TLSConnection` must expose "is this still usable" — a pooled connection the peer closed while
  idle must be detected and replaced, not handed out.
- Decide the ownership: the pool cannot live in `HTTPSClient` as it stands, because the client is
  `const`-everything and shared across worker threads.

⚠️ That last point is the real design question, and it is the same one the coarse-outcome member
raised: **`HTTPSClient` is used concurrently by several workers on one instance**
(`Net::Manager` and `Net::APIClient` both do it). A pool is mutable shared state, so it needs its
own synchronisation — or the pool moves up a level and the client takes a connection as a
parameter.

## ⚠️ Traps

- A pooled connection carries the previous exchange's TLS session. Reusing one **across origins**
  is a security hole; the key must include host, port, and the verification parameters.
- The request builder currently hard-codes `Connection: close`, and `isRequestHeaderAcceptable()`
  refuses a caller-supplied `Connection` precisely so the client stays in control of framing. That
  refusal must stay when keep-alive lands — it is not the caller's decision.
- Measure before and after on a burst of small calls. A pool that is never hit costs complexity for
  nothing.

## References

- `docs/plans/network-tls/README.md` § Post-freeze increment — `request()` (2026-08-28), "Still not
  done"
- `src/Network/HTTPSClient.cpp` — `performHop()`, the `Connection: close` line
