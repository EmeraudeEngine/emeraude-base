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
work on all three. What the title said was what was left: the `ExternalData` chain itself. **macOS ran it on
2026-08-28** (steps 4 and 5, through the replayable fixture `app_system/tools/external-data-check/`)
and it took **two engine fixes** to get a single green row — see § *What the macOS ExternalData run
found*. Windows is now the only platform where that chain has never run.

## Progress

| Step | macOS 26.5.2 / arm64 | Windows |
|---|---|---|
| 1. It builds | ✅ 2026-08-28 | ✅ 2026-08-28 |
| 2. The trust store | ✅ `NetworkTrustStore.*` 6/6, `/etc/ssl/cert.pem` loaded (128 certs) | ✅ **43 CAs imported from the `ROOT` store** — the hand-written CryptoAPI import works |
| 3. The client, hermetic then live | ✅ `Network*` 78 passed / 3 skipped, then the 3 live ones passed | ✅ `Network*` **78/78**, plus a live HTTPS download with a clean cache |
| 4. The downloader from the console | ✅ 2026-08-28 — `Done` + 13566 B, badssl → `TLSFailure`, cache clean | ✅ live download, cache clean |
| 5. The `ExternalData` chain | ✅ 2026-08-28 — 6/6 via `app_system/tools/external-data-check/`, **2 engine defects fixed to get there** | ❌ **the last gap on Windows** |

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

### Linux — done, 2026-08-28

Every box below is ticked, and the last one closed on 2026-08-28. **The list had been left
unchecked while the work was already recorded elsewhere** (the sibling multicast item and
`app_system/src/AGENTS.md`), which understated Linux and would have sent the next session
re-running it. Ticked with its evidence, so the claim can be audited rather than trusted.

- [x] **L1 `IP_MULTICAST_TTL` / `IP_MULTICAST_LOOP` width change** (`unsigned char` → `int`,
  `MulticastOptionValue`). `tools/net-check`: the `int` width is accepted and the TTL reads back as
  **255** through it — and this kernel, like macOS 26, accepts **both** widths, which is why each
  platform keeps the type its own manual specifies. App-level mDNS round trip re-run too.
- [x] **L2 The sliced `waitReadable()`** (50 ms `PollSliceMs`). No regression in the mDNS round
  trip; `close()` returns a parked `receive()` in ~300 ms, bounded by the poll slice and no longer
  by `shutdown()`'s leniency. Deadline accounting: **0 % drift over 1200 ms**.
- [x] **L3 `tools/net-check` under ASan+UBSan** — **48 pass / 0 fail / 1 warn**, identical to the
  plain run. It also surfaced a defect of its own, as old as `NetworkInterfaces`:
  `enumerateMulticastCapable()` dropped `lo` because it trusted `IFF_MULTICAST`, which Linux never
  sets on loopback while supporting multicast there. Fixed, Linux-scoped.
- [x] **L4 Step 5 through the fixture** (`app_system/tools/external-data-check/`) — **6/6 green**,
  2026-08-28, on engine `034342c5` + base `0fc2516`, i.e. the only column taken after both fixes.
  It is the reference row of that results table. Registration went through `Core.openFiles` on an
  application owning **no `data-stores/` at all**, which closes the half Windows had to leave open.
- [x] `SerialPort.linux.cpp` unchanged; covered by a full 1070/1070 Release build.

⚠️ **What is NOT covered on Linux, and does not belong to this checklist**: this repository's own
**JS path for TCP client/server** (`--mode=test` → the dev-check TCP card). Linux has only ever run
the **mDNS** card; Windows ran mDNS *and* TCP. See the platform table in
`app_system/src/AGENTS.md`.

### macOS — steps 4-5, plus what a terminal cannot prove

- [x] **M1 — done 2026-08-28**, through `app_system/tools/external-data-check/` (its results table
  carries the detail). Step 4: `isEnabled()` true, `download(README.md)` → `Done` + filepath +
  13566 B, `download(https://expired.badssl.com/x.bin)` → `Error` + `"reason":"TLSFailure"`,
  `listCache()`/`clearCache()` both clean, `index.json` written with no `.tmp` sibling surviving.
  Step 5: 6/6 rows — store registers, nominal `Loaded` with **8169 B exactly**, expired certificate
  `Failed`/`TLSFailure`, cleartext refused at `download()`, cache left with exactly one entry and no
  `.part`, and the cache genuinely re-read after a relaunch. ⚠️ **Two engine defects had to be fixed
  before any of it could pass**, both platform-independent — see § *What the macOS ExternalData run
  found* below.
- [x] **M2 — MEASURED 2026-08-28. The packaged bundle needs the *Local Network* grant, and works
  with it.** A real Developer-ID-signed, hardened-runtime bundle (`satisfies its Designated
  Requirement`, Info.plist bound, 1675 files sealed, `NSLocalNetworkUsageDescription` present)
  launched through LaunchServices, **while denied**, has every outbound local-network send refused —
  multicast *and* LAN unicast — while joins report success and inbound multicast still arrives.
  ⚠️ That is the failure shape to recognise: only egress dies, nothing says "permission". Once the
  owner authorised *Local Network*, the same bundle relaunched the same way completed the full
  DNS-SD round trip (**6 LAN hosts**). Signing and packaging are therefore not the obstacle, and
  there is no multicast entitlement to chase.
  The corollary is now demonstrated in both directions: **a terminal run proves nothing** — the same
  signed binary worked from a terminal while the app itself was still denied, inheriting Terminal's
  grant. Ruled out as causes: the helper plists (all five, no effect) and `/Applications`.
  ⚠️ **Left open, and it is the shipping-relevant half**: no prompt was ever observed — the grant
  was applied by hand — so whether macOS asks a real user on first use is unverified. Either way the
  app must survive the denied state, where discovery silently finds nothing.
  Two by-products carried elsewhere: a **dev build cannot be code-signed at all**
  (`Contents/Resources/bin` is an absolute symlink out of the bundle), so
  `APP_SYSTEM_PUBLIC_RELEASE=ON` is a signing prerequisite and not just a JS freeze; and
  app_system's `UDPModule` was reporting a **refused send as a success**, platform-neutral, which
  made this whole investigation read as "sends succeed but nothing arrives". Both fixed/recorded —
  see `app_system/docs/packaging.md § macOS`.
- [x] **M3 — done 2026-08-28.** `--mode=test`, driven over CDP like the Windows run. dev-check mDNS
  card: bind `0.0.0.0:5353` beside `mDNSResponder`, TTL 255 + loopback, join on **both** real NICs
  with zero failures, DNS-SD answered by **5 LAN hosts**, idempotent re-join, tolerant drop,
  `close(cb)` + `"close"` event. Plus the native `TCP.server`/`TCP.client` bindings, the `net`
  wrapper's full loopback demo, and `getNetworkInterfaces()` returning IPv4+IPv6 with MAC, index,
  scope id and CIDR. `close()` against a parked `receive()`: **20.0 ms at 3000 ms, 14.7 ms at
  1000 ms** — the macOS half of a measurement that existed only for Windows.
  ⚠️ No CDP tooling and no WebSocket library exist on this machine; the run used a ~110-line pure
  Python CDP client. Worth keeping in mind before assuming the Windows recipe is portable.
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
- [x] **W6 — CLOSED 2026-08-28 by retraction, not by a fix.** Measured on Windows: the **pre-fix**
  acceptor already accepted an IPv4 peer on a `"::"` listener (4/4), identical to the post-fix one.
  Reading the option back says why — a raw `::socket(AF_INET6)` does default to `IPV6_V6ONLY = 1`
  there, but Asio clears it on every `AF_INET6` socket it creates on Windows, so the acceptor reports
  0 before any `bind()`. `TCPServer` goes through Asio, therefore the silent IPv4 refusal **could not
  happen on this path**. The explicit `v6_only(false)` added that day is kept — it states the intent
  in our own code instead of depending on an Asio internal — but it **corrects nothing observable**,
  and the "IPv4 peers are silently refused on Windows" claim belongs to **raw Winsock**. Recorded
  here, and retracted in `emeraude-engine/src/Net/AGENTS.md` and in the `TCPServer` sources, so the
  symptom is not hunted a third time.
- [x] **W7** The hermetic suite's listening socket on 127.0.0.1 — no firewall problem in practice.
- [ ] **W8 ⚠️ The `ExternalData` resource chain (step 5)** — the one Windows gap left, and the one
  that matters most for shipping: it is the path a `"Source": "ExternalData"` resource takes.
  ⚠️ **Pull the engine first.** The macOS run of 2026-08-28 found two defects on this exact path
  that were blocking it on *every* platform (a sterile container binding, and `TLSFailure` reported
  as `Unreachable`) — on an older engine this step cannot pass here either, and the symptoms look
  like a Windows network problem. Then run `app_system/tools/external-data-check/` verbatim and fill
  the Windows column of its results table; read its Traps section first, four of them cost real time.
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

## What the macOS `ExternalData` run found (2026-08-28)

Neither defect is macOS-specific. Both sat on the shared path, in front of all three platforms, and
were only exposed because this was the **first time anyone drove that chain from a resource store**
rather than exercising `HTTPSClient` through the unit suites. Linux's own green history does not
clear them: it had never run this path either.

1. ⚠️⚠️ **`Core.openFiles(store.json)` registered resources that no container could ever see.**
   `listResources` answered `[]`, `openFiles` answered success, and nothing was logged anywhere —
   the most expensive shape a bug can take. `Resources::Manager::getLocalStore()` returned `nullptr`
   for any store the boot-time discovery had not produced; a container captures that pointer **once**,
   at registration, and keeps it for life; the later `Manager::update()` then created a *fresh* map
   under the same name that only the manager could see. Because app_system ships no store
   sub-directories at all, **all 34 containers were sterile** and the entire runtime `update()` path
   was dead code. Fixed engine-side (`getLocalStore()` creates on demand and is documented as never
   returning null). See `emeraude-engine/src/Resources/AGENTS.md`.
2. ⚠️ **`DownloadOutcome::TLSFailure` was never produced.** An expired certificate came back as
   `Unreachable`: `TLSConnection::connect()` returns a single bool for "never reached the peer" and
   "peer refused the handshake", and `HTTPSClient` labelled both the same — the enum's own comment
   says it is coarse *"one value per thing a caller would do differently"*, and this is precisely the
   distinction a caller must not lose, since `Unreachable` invites a retry that a refused certificate
   must never get. It also made **step 5's own discriminator unusable**: the fixture said to treat
   `Unreachable` as "badssl was down, void run", which would have voided every correct run forever.
   Fixed via `TLSConnection::handshakeRefused()`. Measured: the same load reports `failed: TLSFailure`
   where it reported `failed: Unreachable` on the build before, with the identical
   `certificate verify failed` line above it.

> [!NOTE]
> Step 2 of this checklist (the trust store) was already green on macOS and stays green — it was
> never wrong. What defect 2 broke was our ability to *tell* a working trust store from an
> unreachable host, which is the whole point of the negative test.

## ⚠️ Traps recorded on Linux, likely to bite differently there

- **`path::string()` throws on MS-STL** for unconvertible content, and the whole stack is built
  `-fno-exceptions` — that is a terminate, not an error. Sites: `HTTPSClient.cpp` (the destination
  path in log messages), `TrustStore.cpp` (the bundle path). Prefer `u8string()` if it bites.
  ⚠️ **This list was not exhaustive, and naming files rather than the pattern is what hid the rest.**
  The same defect reaches any `TraceX{} << somePath`, because `path::operator<<` emits
  `quoted(p.string())` — the conversion is implicit and there is nothing to grep for but the
  streaming itself. `emeraude-engine/src/Net/Manager.cpp` carried **four** such sites, found from
  Windows and fixed 2026-08-28 (`5032b9e5`, each wrapped in `IO::toU8String()`); one was on the init
  path, so the crash landed at startup rather than during a download and looked like anything but a
  tracing bug. The download cache sits under the user's profile, so a Windows account name with a
  non-ANSI character is the whole trigger. Sweep for `<< *<path variable>` rather than for a file
  name — and note that **no Linux or macOS run can surface this**, both convert without throwing.
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
