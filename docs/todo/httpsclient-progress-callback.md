---
id: httpsclient-progress-callback
title: HTTPSClient::download() — progress callback for consumers that display transfers
status: blocked
priority: low
scope: src/Network
opened: 2026-08-27
blocked-by: [ave-robustus-formal-closure]
tags: [network, https, feature]
---

# HTTPSClient::download() — progress callback for consumers that display transfers

## Why

The engine's `Net::Manager` now downloads through `HTTPSClient::download(uri, filepath)`
(2026-08-27). The call is opaque: the consumer learns the outcome and the final size, nothing in
between. The manager therefore dropped its `Progress` notification and its byte accumulators —
an application that wants a progress bar for a large resource has nothing to bind to.

This is a **new feature** of emeraude-base, so it waits for the feature freeze to be lifted.

## What remains

- [ ] Add an optional progress hook to `HTTPSClient::download()` (bytes received, total when
  `Content-Length` is known — `HTTPResponseParser::bodyBytesDecoded()` already counts), invoked
  from the read loop, `noexcept`, cheap enough not to matter at 4 KiB granularity.
- [ ] Decide the shape: `std::function< void (uint64_t received, std::optional< uint64_t > total) >`
  in `HTTPSClientOptions`, or a per-call parameter. Per-call fits the manager (one ticket per call).
- [ ] Then, in the engine: `Net::Manager` re-exposes a `Progress` notification (ticket, received,
  total) emitted through its main-thread event queue, and `DownloadItem` regains the byte counters.

## ⚠️ Traps

- The hook runs on the transfer thread (the engine's pool worker). The engine must queue the
  values and notify from `dispatchCompleted()`, never call observers from the hook.

## References

- `src/Network/HTTPSClient.cpp` — `download()` read loop.
- Engine `src/Net/Manager.cpp` — `performDownload()`, the single call site.
