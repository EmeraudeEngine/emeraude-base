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

## Measured (2026-10-07, Linux, owner: "measure first")

20 sequential `HEAD` of `https://raw.githubusercontent.com/EmeraudeEngine/emeraude-base/main/README.md` (a throwaway
program on `HTTPSClient::head()`, 3 runs), against curl as the reference:

| Case | 20 calls | Per call |
|---|---|---|
| `HTTPSClient` today (one TLS connection per call) | 1.60 – 1.85 s | median 77 – 82 ms |
| curl, 20 processes (no reuse) | 1.49 – 1.52 s | ~75 ms |
| curl, one process (connection reused) | 0.28 – 0.31 s | ~15 ms |

One curl request: TCP connect 36 ms, TLS done at 52 ms, total 63 ms — the connection setup is ~80 % of a small call. A
pool would make a burst of small calls to one host about **5× faster** here (more on a farther host). The gain is
real: the design question below goes to the owner with both designs (pool inside the client with its own lock, or
the pool above the client), costed.

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
