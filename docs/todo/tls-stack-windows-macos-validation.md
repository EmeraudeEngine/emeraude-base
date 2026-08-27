---
id: tls-stack-windows-macos-validation
title: Run the network stack on Windows and macOS — nothing of it has ever executed there
status: open
priority: high
scope: src/Network
opened: 2026-07-04
tags: [tls, cross-platform, handover]
---

# Run the network stack on Windows and macOS — nothing of it has ever executed there

## Why

Everything the cascade does over the network is **Linux-verified only**. The Windows and macOS code
paths compile and have never been executed once. Since 2026-08-27 this is no longer a testing
detail: the engine's resource downloading (`Net::Manager`) runs on this stack, so on those two
platforms **a `"Source": "ExternalData"` resource has never been fetched by anyone**.

Priority raised from medium to high on 2026-08-27 for that reason.

## The handover — what to run there, in this order

Each step is independent; stop at the first that fails and record what it said.

### 1. It builds at all

- Windows: MSVC, both `EMERAUDE_USE_FULL_EXPORTS` (default) and app_system's `LEAN` variant.
- macOS: the Objective-C++ TUs (`SerialPort.mac.mm`, `WiFiScanner.mac.mm`) still compile with the
  shared STL PCH — the base auto-sets `SKIP_PRECOMPILE_HEADERS` on `.mm`, verify it still does.
- ⚠️ **New since the last cross-platform build**: `src/Net/SerialPort.linux.cpp` grew a mirrored
  kernel `termios2`; that file is Linux-only, but check the Windows/macOS siblings did not need the
  same baud-rate work (an arbitrary rate like **250000**, Marlin's default, is what triggered it).

### 2. The trust store — the one that gates everything else

`EmeraudeBaseUnitTests --gtest_filter='NetworkTrustStore.*'`, then a real download.

- **Windows** (`TrustStore.cpp`, `importWindowsSystemStore`): a hand-written CryptoAPI import,
  because LibreSSL ships no OpenSSL≥3.2 `winstore` loader. ⚠️ **It imports `ROOT` only since
  2026-08-27** — importing `CA` as well installed per-user, non-admin-writable intermediates as
  **trust anchors**. If a real server now fails to verify because its chain was incomplete, that is
  the expected trade-off (RFC 8446 §4.4.2: sending the chain is the server's job) — do not "fix" it
  by putting `CA` back.
- **macOS**: `applySystemTrustStore()` loads `/etc/ssl/cert.pem` and nothing else — no Keychain, no
  fallback list. On a hardened or containerised macOS that file may be absent: the only signal is
  one error line, and every handshake then fails.
- Check `certificateCount()` is non-zero and that `NetworkTrustStore.systemTrustStoreLoads` passes
  (⚠️ that test is **not** gated: on a machine without a CA bundle it fails for environmental
  reasons).

### 3. The client, hermetic then live

```
EmeraudeBaseUnitTests --gtest_filter='Network*'            # 2010 tests, all platforms
EMERAUDE_RUN_LIVE_NETWORK_TESTS=1 EmeraudeBaseUnitTests --gtest_filter='NetworkHTTPSClientLive.*'
```

The hermetic suite generates EC P-256 credentials at runtime and runs a TLS server in-process —
⚠️ on Windows that means a listening socket on 127.0.0.1: a firewall prompt is expected the first
time, and a blocked one looks like a client bug.

### 4. The engine's downloader, from the remote console

Open the console (`Core/Console/EnableRemoteListener = true`, or Shift+F10), then:

```
Core.NetManagerService.isEnabled()      # must be {"enabled":true,...}; a false names its reason
Core.NetManagerService.download(https://raw.githubusercontent.com/EmeraudeEngine/emeraude-base/main/README.md)
Core.NetManagerService.status(1)        # Done + filepath + bytes
Core.NetManagerService.download(https://expired.badssl.com/x.bin)
Core.NetManagerService.status(2)        # Error + "reason":"TLSFailure"  <- the trust store working
Core.NetManagerService.listCache()
Core.NetManagerService.clearCache()
```

The cache lives in `cacheDirectory("downloads")`: check the platform's path is writable, that
`index.json` is written (atomically — a `.tmp` sibling must not survive), and that a `.part` file
left behind by a kill is swept at the next start.

### 5. The `ExternalData` resource chain

Drop a store declaring an https resource, then
`Core.ResourcesManagerService.loadResource(ImageResource, <name>)` and poll `resourceStatus` until
`Loaded`. This is the path that has never run on Windows or macOS.

## ⚠️ Traps recorded on Linux, likely to bite differently there

- **`path::string()` throws on MS-STL** for unconvertible content, and the whole stack is built
  `-fno-exceptions` — that is a terminate, not an error. Sites: `HTTPSClient.cpp` (the destination
  path in log messages), `TrustStore.cpp` (the bundle path). Prefer `u8string()` if it bites.
- **Windows needs `crypt32`, `bcrypt`, `ws2_32`** (`SetupLibreSSL.cmake`); `Iphlpapi` for
  `NetworkInterfaces`; `SetupAPI.lib` and `wlanapi.lib` come from `#pragma comment(lib, …)` inside
  the `.windows.cpp` files, i.e. **MSVC only** — a MinGW/clang-cl build will not link.
- **macOS `IOKit`** (needed by `SerialPort.mac.mm`) is not linked by the engine: it arrives
  transitively through `SetupHWLOC.cmake`. If hwloc ever goes, the serial port stops linking.
- **`TCPServer` on Windows**: binding `"::"` opens an AF_INET6 socket with **no explicit
  `v6_only`** — Windows defaults to v6-only, so IPv4 peers are silently refused there while they
  are accepted on Linux.
- **UDP multicast on macOS 15+** needs the *Local Network* privacy entitlement
  (`NSLocalNetworkUsageDescription`): field reports describe multicast working from a terminal and
  not from a double-clicked bundle. Test a **signed, packaged** binary — see
  `emeraude-engine/docs/todo/udp-multicast-macos-verification.md`.
- **`SO_SNDTIMEO`** on the remote console's accepted sockets takes a `DWORD` of milliseconds on
  Windows and a `timeval` elsewhere; both are written, only the POSIX one has run.

## References

- `docs/plans/network-tls/README.md` — the design, the closure, the 2026-08-27 hardening table.
- `emeraude-engine/src/Net/AGENTS.md` — the download manager's contract and its console commands.
- `emeraude-engine/docs/todo/udp-multicast-macos-verification.md` — the sibling macOS item.
