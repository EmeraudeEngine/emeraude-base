---
id: tls-stack-windows-macos-validation
title: Run the network stack on Windows and macOS — down to the ExternalData chain
status: open
priority: medium
scope: src/Network
opened: 2026-07-04
tags: [tls, cross-platform, handover]
---

# Run the network stack on Windows and macOS — down to the `ExternalData` chain

## Why

Everything the cascade does over the network was **Linux-verified only**. Since 2026-08-27 this is
no longer a testing detail: the engine's resource downloading (`Net::Manager`) runs on this stack,
so on an unverified platform **a `"Source": "ExternalData"` resource has never been fetched by
anyone**.

Priority raised from medium to high on 2026-08-27 for that reason. **Lowered back to medium on
2026-08-28**: both platforms ran that day — macOS steps 1-3 through an out-of-tree harness, Windows
steps 1-4 through app_system's JS path — and the trust store, the TLS client and a live download now
work on all three. What the title says is what is left: **the `ExternalData` chain itself has still
never run on either platform**, which is the exact reason this item was raised to high. Do not close
it on the strength of the green rows above.

## Progress

| Step | macOS 26.5.2 / arm64 | Windows |
|---|---|---|
| 1. It builds | ✅ 2026-08-28 | ✅ 2026-08-28 |
| 2. The trust store | ✅ `NetworkTrustStore.*` 6/6, `/etc/ssl/cert.pem` loaded (128 certs) | ✅ **43 CAs imported from the `ROOT` store** — the hand-written CryptoAPI import works |
| 3. The client, hermetic then live | ✅ `Network*` 78 passed / 3 skipped, then the 3 live ones passed | ✅ `Network*` **78/78**, plus a live HTTPS download with a clean cache |
| 4. The downloader from the console | ❌ not yet — needs a full engine + app build | ✅ live download, cache clean |
| 5. The `ExternalData` chain | ❌ not yet | ❌ **the last gap on Windows** |

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
- [ ] **L4** Replay step 5 through the new fixture (`app_system/tools/external-data-check/`) to seed
  the reference row of its results table. Linux *has* run this chain (2026-08-27) but through an
  ad-hoc store, so there is no row the other two machines can be compared against.
- [ ] Nothing changed in `SerialPort.linux.cpp`; a build check is enough.

### macOS — steps 4-5, plus what a terminal cannot prove

- [ ] **M1** Steps 4 and 5 below (the downloader from the remote console, then the `ExternalData`
  resource chain). **Never run on macOS** — the unit suites do not cover `Net::Manager`, its cache
  directory, `index.json`, or the `.part` sweep. Step 5 is no longer improvised: run
  `app_system/tools/external-data-check/` verbatim and fill its results table.
- [ ] **M2 ⚠️ A SIGNED, PACKAGED binary**, for the macOS 15+ *Local Network* authorisation.
  Everything on 2026-08-28 ran from an already-authorised Terminal, which is precisely why it
  proves nothing. See the sibling item. ⚠️ **The precondition was missing and is now in place**:
  `app_system/resources/mac/Info.plist` had **no `NSLocalNetworkUsageDescription` at all** — with
  the key absent the system cannot even present the prompt, so a bundle test run before 2026-08-28
  would have measured the missing key, not the stack. Two things that run must still answer: does
  TCC attribute the **renderer helper's** multicast to the main bundle (the key is deliberately in
  the main plist only, so the answer is a clean measurement rather than a guess), and does an
  **ad-hoc, linker-signed** bundle even qualify — the local build is `adhoc`, `Info.plist=not
  bound`, `Sealed Resources=none`, which is not what a shipped bundle looks like.
  ⚠️ **Orthogonal to M1**: the `external-data-check` fixture is an HTTPS *unicast* download, which
  the Local Network authorisation does not gate. A green step 5 says nothing about M2.
- [ ] **M3** The app's own JS path — `--mode=test` → the dev-check mDNS card. Only the engine layer
  was exercised.
- [ ] **M4** `SerialPort` at **250000 bauds against a real printer**. The `IOSSIOSPEED` path is new
  and was only ever exercised against a pty, which refuses the rate — so only the *failure* branch
  has run, never the success one.

### Windows — ran 2026-08-28, one gap left

Done through app_system's own JS path (`--mode=test`, dev-check fixtures over CDP), which is the
layer **macOS has never run** — the two platforms validated different things, neither substitutes
for the other.

- [x] **W1** Steps 1-4. Step 5 remains — see W8.
- [x] **W2** The `close()` stall: **measured, not assumed**. Pre-fix symptom visible (~107 ms, one
  drain-loop block); post-fix `close()` returns in **62 ms with a `receive()` parked on 3000 ms, and
  the same 62 ms at 1000 ms** — bounded by the poll slice, independent of the receive timeout. The
  deadline-based timeout accounting also holds: 201/200, 1001/1000, 3013/3000 ms.
- [x] **W3** The `DWORD` branch of `MulticastOptionValue` — TTL 255 and loopback both took.
- [x] **W4** `NetworkInterfaces` on Windows: enumerated, joins on the real NIC.
- [x] **W5 — resolved, and it needed no work.** `SerialPort.windows.cpp` assigns
  `dcb.BaudRate = config.baudRate` directly: an arbitrary `DWORD`, **no `CBR_*` table**, so the
  silent-fallback bug that hit both POSIX legs structurally cannot occur. Untested against real
  hardware, but there is no lookup to be wrong.
- [ ] **W6** `TCPServer` binding `"::"` — **fixed 2026-08-28, needs measuring HERE.** `listen()`
  now requests `IPV6_V6ONLY` off explicitly when binding the IPv6 any-address, and **fails** if the
  stack refuses rather than coming up IPv6-only. Written and compiled on macOS, where the symptom
  never existed (`net.inet6.ip6.v6only = 0`), so Windows is the only machine that can confirm it:
  `TCP.server.listen(port, backlog, "::")` then connect an **IPv4** client to `127.0.0.1:port`.
  Before the fix that connection was refused on Windows and accepted everywhere else.
- [x] **W7** The hermetic suite's listening socket on 127.0.0.1 — no firewall problem in practice.
- [ ] **W8 ⚠️ The `ExternalData` resource chain (step 5)** — the one Windows gap left, and the one
  that matters most for shipping: it is the path a `"Source": "ExternalData"` resource takes.
  Run `app_system/tools/external-data-check/` verbatim and fill its results table.

> [!NOTE]
> What the Windows run found first was **not** in this cascade: app_system's
> `SharedDataManager::createJob<>()` locked a non-recursive mutex twice, which MS-STL turns into a
> `std::system_error` thrown inside a `noexcept` binding — instant renderer death on **every**
> `JobInterface` module, network or not. glibc self-deadlocks rather than throwing, so Linux would
> have hung instead of crashing. Fixed in app_system. Noted here because it blocked the run, and
> because it is a good reminder that a green Linux run does not clear this class of mistake.

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

**Do not improvise this step any more.** It used to read "drop a store declaring an https resource",
which is not a definition: three machines improvising three stores produce three runs that cannot be
compared, and comparability is the whole point of a cross-platform handover.

The fixture is now in the tree, in the consuming application:
`app_system/tools/external-data-check/` — one store file (`ExternalDataCheck.store.json`, three
entries: a nominal one, an expired certificate, a cleartext URL), one command sequence identical on
the three OSes, the expected outcomes, and a results table to fill. Run it verbatim.

Two things that cost time when this was pinned down (2026-08-28, Linux), both recorded in that
README:

- `Core::openFiles()` runs `openResourceIndex()` **before** the application's `onCoreOpenFiles()`,
  so the console path works under app_system even though that override sends dropped files to
  JavaScript. But a **malformed** store falls through to the application and vanishes silently,
  behind a cheerful `1 file(s) submitted to the opening pipeline.` — that message is not proof,
  `listResources` is.
- Installing the store by dropping it into a `data-stores/` directory **cannot work**:
  `Core/Resources/UseDynamicScan` defaults to `true`, and in that mode `readResourceIndexes()` is
  never called at all. A dynamic scan can only produce `LocalData` entries anyway.

## ⚠️ Traps recorded on Linux, likely to bite differently there

- **`path::string()` throws on MS-STL** for unconvertible content, and the whole stack is built
  `-fno-exceptions` — that is a terminate, not an error. Sites: `HTTPSClient.cpp` (the destination
  path in log messages), `TrustStore.cpp` (the bundle path). Prefer `u8string()` if it bites.
- **Windows needs `crypt32`, `bcrypt`, `ws2_32`** (`SetupLibreSSL.cmake`); `Iphlpapi` for
  `NetworkInterfaces`; `SetupAPI.lib` and `wlanapi.lib` come from `#pragma comment(lib, …)` inside
  the `.windows.cpp` files, i.e. **MSVC only** — a MinGW/clang-cl build will not link.
- **macOS `IOKit`** (needed by `SerialPort.mac.mm`) is not linked by the engine: it arrives
  transitively through `SetupHWLOC.cmake`. If hwloc ever goes, the serial port stops linking.
- **`TCPServer` on Windows**: binding `"::"` used to open an AF_INET6 socket with **no explicit
  `v6_only`** — Windows defaults to v6-only, so IPv4 peers were silently refused there while they
  were accepted on Linux and macOS. **Fixed 2026-08-28** (explicit dual-stack request, hard failure
  if refused), **unmeasured on Windows** — see W6. The lesson survives the fix: a kernel default is
  not a contract, and the two platforms that "worked" only ever hid the trap.
- ⚠️⚠️ **`shutdown()` does not wake a reader on an unconnected datagram socket, and Windows is
  very likely affected — CONFIRMED on macOS 2026-08-28.** POSIX makes it fail with `ENOTCONN`
  (Winsock: `WSAENOTCONN`); **Linux is the lenient outlier** that wakes the reader anyway, which
  is exactly why the two-phase `close()` looked portable. Measured on macOS: `UDPClient::close()`
  waited out the full 10 s receive timeout instead of returning. Fixed platform-neutrally
  (`m_closing` + 50 ms poll slices). **Confirmed on Windows by measurement the same day**: the
  pre-fix symptom was visible (~107 ms), and post-fix `close()` returns in 62 ms whether the parked
  `receive()` had a 1000 ms or a 3000 ms timeout — bounded by the slice, not the timeout.
  `TCPClient` shuts down a **connected** socket and is not concerned; `TCPServer` uses Asio
  `acceptor->cancel()`.
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
