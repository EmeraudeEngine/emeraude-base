---
id: httpsclient-happy-eyeballs
title: A name resolving to IPv6 and IPv4 costs ~2 s per connection on Windows when the server listens on IPv4 only
status: in-progress
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

Done on Linux (2026-10-07): base `Network/HappyEyeballs.hpp` (`interleaveAddressFamilies()`,
`connectFirstReachable()`, 250 ms attempt delay), used by `TLSConnection::establishTcp()` and the engine's
`Net::TCPClient::connect()`; private-only check kept on every address, before the race. Tests `NetworkHappyEyeballs.*`
(8, among them an unanswered endpoint giving way after 250 ms: 450 ms with the 200 ms probe) and
`NetworkHTTPSClient.aNameResolvingToBothFamiliesReachesAnIPv4OnlyServerWithoutWaiting`; suites 2427 run, 2424 passed,
3 skipped, Release and ASan/UBSan. Engine `TCPClient` proved by a scratch program (IPv4-only listener reached through
"localhost", a closed port still refused). Knowledge in `docs/caution-points.md` § Network.

- [ ] **Windows proof**: `NetworkHappyEyeballs.*` pass, none skipped (the full-queue listener must leave the connect
      unanswered or slowly refused there); the IPv4-only HTTPS test well under 1500 ms; the `NetworkTLSConnection`
      tests that took ~2 s (its own server is IPv4-only behind "localhost": handshakeAndEchoWithTrustedServer 2020 ms,
      handshakeFailsWithUntrustedServer 2028 ms) back near 250 ms. macOS: the same suites. Then delete this item.

## References

- RFC 8305, *Happy Eyeballs Version 2*; RFC 6555.
- `src/Network/HappyEyeballs.cpp`, `src/Network/TLSConnection.cpp` (`establishTcp()`), engine `src/Net/TCPClient.cpp`,
  `src/Testing/TLSTestHelpers.hpp` (`HTTPSTestServer`, dual stack / `listenOnIPv6`).
