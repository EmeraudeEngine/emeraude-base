---
id: http-client-stop-does-not-wake-a-parked-read
title: A stop requested on an HTTP exchange is only seen when the transport read in flight returns
status: open
priority: high
scope: Network/HTTPSClient (performHop), Network/TLSConnection (read, connect, handshake)
opened: 2026-11-14
tags: [defect, network, cancellation, ave-robustus-ii, owner-decision]
---

# A stop requested on an HTTP exchange is only seen when the transport read in flight returns

## Why

`HTTPRequestOptions::stopToken` (a `std::stop_token` since 2026-11-14; an atomic flag before) is POLLED by
`HTTPSClient::performHop()` before each transport read. A read parked on a silent peer does not see the stop: it
returns at its read timeout or at the total budget, and only then does the exchange end `Cancelled`. Neither the
connect nor the TLS handshake polls it at all.

Ave Robustus II asks for a bounded latency in EVERY stage. The latency here is bounded, by the caller's timeouts —
which are tens of seconds:

- **measured 2026-11-14** through the engine's resource sharing (two instances on one machine): a cancel during a
  download, where blocks arrive continuously, is effective after 0.04 s; a cancel while the client waits for a
  slow answer is effective after 3.5 s (the answer's own delay) and after 29.8 s with a slower peer — the engine's
  30 s request budget. The engine's shutdown waited that long.

## What remains

- **Owner decision — how a parked operation is woken.** A token makes it possible (the flag could only be polled): a
  `std::stop_callback` that cancels the pending socket operation (through the connection's `asio::io_context`, never
  from the foreign thread directly), or reads sliced under a short timer. The first is immediate; both touch
  `TLSConnection`'s read / connect / handshake.
- A failing-then-passing test with the hermetic server of `test_NetworkHTTPSClient.cpp`: a handler that sleeps 2 s,
  a stop requested after 100 ms, the request must end `Cancelled` well before 2 s (today it ends when the answer
  comes).
- Then the connect and the handshake.

## References

- `src/Network/HTTPSClient.cpp` — `performHop()`, the check before `connection->read()`.
- `docs/subsystems/source-tree/24-network-http-server.md` — `HTTPRequestOptions::stopToken`.
- The engine's `docs/subsystems/resources/12-resource-sharing.md` (the measurements).
