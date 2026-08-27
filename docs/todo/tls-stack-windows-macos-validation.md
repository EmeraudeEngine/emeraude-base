---
id: tls-stack-windows-macos-validation
title: Run the network stack on Windows and macOS — Windows still has not
status: open
priority: high
scope: src/Network
opened: 2026-07-04
tags: [tls, cross-platform, handover]
---

# Run the network stack on Windows and macOS — Windows still has not

## Why

Everything the cascade does over the network was **Linux-verified only**. Since 2026-08-27 this is
no longer a testing detail: the engine's resource downloading (`Net::Manager`) runs on this stack,
so on an unverified platform **a `"Source": "ExternalData"` resource has never been fetched by
anyone**.

Priority raised from medium to high on 2026-08-27 for that reason. **Kept high on 2026-08-28**:
macOS cleared steps 1-3, Windows has still executed nothing at all.

## Progress

| Step | macOS 26.5.2 / arm64 | Windows |
|---|---|---|
| 1. It builds | ✅ 2026-08-28 | ❌ |
| 2. The trust store | ✅ `NetworkTrustStore.*` 6/6, `/etc/ssl/cert.pem` loaded (128 certs) | ❌ |
| 3. The client, hermetic then live | ✅ `Network*` 78 passed / 3 skipped, then the 3 live ones passed | ❌ |
| 4. The downloader from the console | ❌ not yet — needs a full engine + app build | ❌ |
| 5. The `ExternalData` chain | ❌ not yet | ❌ |

macOS notes worth keeping:

- `applySystemTrustStore()` found `/etc/ssl/cert.pem` on a stock macOS 26 install, and
  `NetworkTrustStore.systemTrustStoreLoads` passed. The single-file, no-fallback design held.
- The live suite really exercised the store both ways: a genuine HTTPS download succeeded, and
  `rejectsUntrustedRealServer` refused the handshake as it should.
- ⚠️ Steps 4 and 5 are the ones that matter for `ExternalData`, and they are still **unrun on
  macOS** — the unit suites do not cover `Net::Manager`, its cache directory, `index.json`, or
  the `.part` sweep.

> [!NOTE]
> Step 3 below says "2010 tests"; under `--gtest_filter='Network*'` the suite actually ran **81**
> on 2026-08-28. Treat the old figure as stale, not as a missing half of the suite.

## ⚠️ Full re-test run-list — all three machines available (noted 2026-08-28)

The owner has Linux, macOS and Windows on hand. This is the consolidated list to run, per machine,
so nothing is re-derived from scratch next time. **Linux is on it too**: the 2026-08-28 sitting
changed a line on its path (see L1).

The runtime harness now lives IN the tree — no need to rewrite it: `emeraude-engine/tools/net-check/`
(`net_check.cpp`, `shutdown_semantics.cpp`, one compiler command per OS in its README, 47
assertions). Path given plainly, not as a link: this repository is also consumed standalone, where
the engine is not above it.

### Linux — a re-run, it was green

- [ ] **L1 ⚠️ `IP_MULTICAST_TTL` / `IP_MULTICAST_LOOP` changed width on this platform**, from
  `unsigned char` to `int` (`MulticastOptionValue`, top of `UDPClient.cpp`) — `int` is what `ip(7)`
  documents, and both widths were measured as accepted on macOS, but **Linux has not run since the
  change**. `tools/net-check` § 3 settles it in one command; then re-run the app-level mDNS fixture
  (`--mode=test` → UDP Client card) for the full round trip.
- [ ] **L2** The sliced `waitReadable()` (50 ms `PollSliceMs`) replaced the single `select()` on
  every platform. Confirm no regression in the mDNS round trip and that `close()` still returns a
  parked `receive()` — Linux got it from `shutdown()` before, it now also gets it from the flag.
- [ ] **L3** `tools/net-check` under ASan+UBSan, for the moved-from and close-race sections.
- [ ] Nothing changed in `SerialPort.linux.cpp`; a build check is enough.

### macOS — steps 4-5, plus what a terminal cannot prove

- [ ] **M1** Steps 4 and 5 below (the downloader from the remote console, then the `ExternalData`
  resource chain). **Never run on macOS** — the unit suites do not cover `Net::Manager`, its cache
  directory, `index.json`, or the `.part` sweep.
- [ ] **M2 ⚠️ A SIGNED, PACKAGED binary**, for the macOS 15+ *Local Network* authorisation
  (`NSLocalNetworkUsageDescription`). Everything on 2026-08-28 ran from an already-authorised
  Terminal, which is precisely why it proves nothing. See the sibling item.
- [ ] **M3** The app's own JS path — `--mode=test` → the dev-check mDNS card. Only the engine layer
  was exercised.
- [ ] **M4** `SerialPort` at **250000 bauds against a real printer**. The `IOSSIOSPEED` path is new
  and was only ever exercised against a pty, which refuses the rate — so only the *failure* branch
  has run, never the success one.

### Windows — nothing has ever executed

- [ ] **W1** Steps 1-5 below, in order, both `EMERAUDE_USE_FULL_EXPORTS` and app_system's `LEAN`.
- [ ] **W2 ⚠️ Measure the `close()` stall BEFORE trusting the fix.** `shutdown()` on an unconnected
  datagram socket returns `WSAENOTCONN`, so Winsock very likely had the macOS symptom (a `close()`
  that waits out the whole receive timeout). `shutdown_semantics.cpp` answers it in one run. The fix
  is platform-neutral and should already cover it — confirm, do not assume.
- [ ] **W3** `tools/net-check` § 3: Winsock is documented as `DWORD` and is the one stack with no
  measurement at all behind that claim.
- [ ] **W4** The `NetworkInterfaces` Windows leg (`GetAdaptersAddresses(AF_UNSPEC)`, netmask from
  `OnLinkPrefixLength`, per-family index) — never run. Check a multi-homed host.
- [ ] **W5** `SerialPort.windows.cpp` at 250000 bauds — **the Windows leg was never even read**
  during the 2026-08-28 sitting. Linux and macOS both turned out to be silently wrong here.
- [ ] **W6** `TCPServer` binding `"::"` — no explicit `v6_only`, so IPv4 peers are silently refused
  on Windows while accepted on Linux (see the traps below).
- [ ] **W7** The hermetic TLS suite runs a listening socket on 127.0.0.1: expect a firewall prompt,
  and do not read a blocked one as a client bug.

## The handover — what to run there, in this order

Each step is independent; stop at the first that fails and record what it said.

### 1. It builds at all

- Windows: MSVC, both `EMERAUDE_USE_FULL_EXPORTS` (default) and app_system's `LEAN` variant.
- macOS: the Objective-C++ TUs (`SerialPort.mac.mm`, `WiFiScanner.mac.mm`) still compile with the
  shared STL PCH — the base auto-sets `SKIP_PRECOMPILE_HEADERS` on `.mm`, verify it still does.
- ⚠️ **New since the last cross-platform build**: `src/Net/SerialPort.linux.cpp` grew a mirrored
  kernel `termios2`; that file is Linux-only, but check the Windows/macOS siblings did not need the
  same baud-rate work (an arbitrary rate like **250000**, Marlin's default, is what triggered it).
  **macOS did need it, badly — answered 2026-08-28.** `toBaudConstant()` fell back to `B9600` in
  its `default:` branch and `open()` returned **true**: measured on a pty, `open(250000)` → `true`
  with the line at 9600 bauds. macOS stops at `B230400` where Linux carries constants to
  `B4000000`, so every Marlin printer hit that branch. Now mirrors the Linux design with
  `ioctl(IOSSIOSPEED)` (`<IOKit/serial/ioss.h>`, ⚠️ applied **after** `tcsetattr`, which would
  otherwise undo it) and fails the open when the adapter refuses the rate. **Not yet tried against
  real hardware** — no serial adapter on the validation machine. **The Windows sibling has not
  been looked at.**

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
- ⚠️⚠️ **`shutdown()` does not wake a reader on an unconnected datagram socket, and Windows is
  very likely affected — CONFIRMED on macOS 2026-08-28.** POSIX makes it fail with `ENOTCONN`
  (Winsock: `WSAENOTCONN`); **Linux is the lenient outlier** that wakes the reader anyway, which
  is exactly why the two-phase `close()` looked portable. Measured on macOS: `UDPClient::close()`
  waited out the full 10 s receive timeout instead of returning. Fixed platform-neutrally
  (`m_closing` + 50 ms poll slices), so Windows inherits the fix — **but nobody has confirmed the
  symptom there**, and it is worth measuring rather than assuming. `TCPClient` shuts down a
  **connected** socket and is not concerned; `TCPServer` uses Asio `acceptor->cancel()`.
  Same sitting, same class of bug: a moved-from `UDPClient` **segfaulted on destruction** on every
  platform (`close()` dereferenced the mutex the move gave away) — also fixed.
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
