---
id: httpsclient-happy-eyeballs
title: A name resolving to IPv6 and IPv4 costs ~2 s per connection on Windows when the server listens on IPv4 only
status: open
priority: high
scope: emeraude-base Network (TLSConnection::establishTcp)
opened: 2026-10-07
tags: [network, latency, windows, ipv6]
---

# A name resolving to IPv6 and IPv4 costs ~2 s per connection on Windows when the server listens on IPv4 only

## Why

Measured by the Windows peer (2026-10-07): `getaddrinfo("localhost")` answers `::1` then `127.0.0.1`, and a REFUSED
TCP connect on Windows takes ~2 s (the SYN is retried after the RST): connecting `::1` to an IPv4-only listener was
refused in 2021 ms, `127.0.0.1` in 0 ms. The endpoints are tried one after the other, so every connection to such a
host pays the 2 s first — every hermetic HTTPS test did (fixed on the TEST side the same day: the test server listens
on `::1` too), and so does any real `HTTPSClient` call to a dual-resolving name whose server is IPv4-only (a local peer
reached as "localhost", a misconfigured AAAA record). Public dual-stack hosts are not affected.

## What remains

- [ ] A failing test: a client to "localhost" against an IPv4-ONLY test server (the dual-stack test server gets an
      option to stay IPv4-only) must connect well under 2 s on Windows (Linux refuses at once: the test only proves
      something on Windows — or simulate an unanswered first endpoint).
- [ ] Happy Eyeballs v2 (RFC 8305) in `TLSConnection::establishTcp()`: start the first address, then the next one of
      the other family after a short delay (RFC 8305 § 5: 250 ms recommended) if the first has not connected; keep the
      first that succeeds, cancel the others. The connect timeout keeps bounding the whole attempt.
- [ ] Keep the private-address rule of `connectCleartextPrivate()` (every candidate address checked) and the proxy path.

## References

- RFC 8305, *Happy Eyeballs Version 2*; RFC 6555.
- `src/Network/TLSConnection.cpp` (`establishTcp()`), `src/Testing/TLSTestHelpers.hpp` (`HTTPSTestServer`, dual stack).
