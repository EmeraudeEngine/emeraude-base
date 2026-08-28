# Network TLS — gating PoC

Decision artifact for the **"Ave robustus!"** plan, Axis B → *Network production-grade*
(see [`../ave-robustus.md`](../ave-robustus.md), the Network row).

## Why this exists

Network production-grade needs an HTTPS client, which needs TLS, which needs a crypto
provider. **Owner decision (2026-06-22): OpenSSL 3.x via `asio::ssl`** — `asio::ssl` is a thin
C++ wrapper that delegates all cryptography to OpenSSL, reused here to stay on the existing
asio-async Network model instead of hand-rolling the TLS pump.

> **Provider since re-decided: LibreSSL** (owner-confirmed 2026-07-04) — see the
> "TLS provider — re-decided" section below. API-compatible, so everything this PoC
> proves carries over.

The single technical unknown before committing to that choice: emeraude-base compiles
**`-fno-exceptions`** and asio runs in **`ASIO_NO_EXCEPTIONS`** mode (via
[`src/Network/asio_throw_exception.hpp`](../../../src/Network/asio_throw_exception.hpp)).
Does `asio::ssl` over OpenSSL even compile and link under that regime? This PoC answers it.

`asio_ssl_poc.cpp` instantiates an `ssl::context{tls_client}`, sets peer verification, wraps a
TCP socket in an `ssl::stream`, and instantiates the no-throw (`error_code`) handshake codepath
— the HTTPS building blocks — without performing real network I/O.

## Build & run

Uses the **system** OpenSSL (the `ext-deps-generator` integration is deferred). Prerequisite:
OpenSSL dev headers + libs (`libssl-dev` on Debian). Mirrors the base compile regime:
`-std=c++20 -fno-exceptions -frtti` and the three asio defines.

```sh
# from the emeraude-base repo root
g++ -std=c++20 -fno-exceptions -frtti \
    -DASIO_STANDALONE -DASIO_NO_EXCEPTIONS -DASIO_DISABLE_CO_AWAIT \
    -I dependencies/asio/include \
    -I src/Network \
    -Wall \
    docs/plans/network-tls/asio_ssl_poc.cpp -o /tmp/asio_ssl_poc \
    -lssl -lcrypto
/tmp/asio_ssl_poc
```

> Note: `asio_throw_exception.hpp` MUST be included before any asio header — the PoC does this.

## Result — gate PASSED (2026-06-22)

Compiles with **zero warning**, links against `libssl.so.3` + `libcrypto.so.3`, runs:

```
asio::ssl + OpenSSL PoC: compiled and linked under -fno-exceptions.
OpenSSL: OpenSSL 3.5.6 7 Apr 2026
```

Environment: g++ (Debian 14.2.0), OpenSSL 3.5.6.

**The `-fno-exceptions` / `ASIO_NO_EXCEPTIONS` risk is cleared.** OpenSSL is C (no exceptions)
and asio routes its own throws through the `throw_exception` hook, so the regime holds.

### What this does NOT yet prove (for the build-out)

> **All three items PROVEN 2026-07-04** by the TLS transport (`TLSConnection`, see the
> step-3 progress below): real handshakes against production servers (github.com,
> khronos.org — HTTP exchange completed), certificate-chain + hostname verification
> exercised positively (hermetic server) and negatively (badssl.com: wrong-host,
> self-signed and expired certificates all rejected), and the transport is implemented
> entirely with **async** operations driven under `ASIO_NO_EXCEPTIONS` (blocking facade
> via `io_context::run_for`).

- A real runtime TLS handshake against a live server.
- Certificate-chain verification behavior.
- **Async** operations (`async_handshake` / `async_read_until`) under `ASIO_NO_EXCEPTIONS` —
  they go through the same `throw_exception` hook, so expected to hold, but must be exercised.

## TLS provider — re-decided (owner-confirmed 2026-07-04): LibreSSL

The 2026-06-22 "OpenSSL 3.x" ruling was superseded the same day during the
`ext-deps-generator` exploration, and confirmed by the owner on 2026-07-04:

- **Why not OpenSSL:** it builds via its bespoke perl `Configure` (no CMake) — canonical
  integration would require a fifth hand-written builder in the CMake-centric
  `ext-deps-generator` (whose 4 builders are cmake/autotools/meson/msys2), plus perl (+nasm
  on Windows) as build dependencies on every host regenerating the ext-deps.
- **Why LibreSSL:** LibreSSL-portable builds with CMake and is OpenSSL-API-compatible —
  `asio::ssl` and this PoC hold unchanged. (wolfSSL was ruled out earlier on GPLv2.)
- **Integration form (owner-ruled 2026-07-04, path A):** the **release tarball vendored**
  into `repositories/libressl/` — the first non-submodule dep in ext-deps-generator,
  accepted because the `libressl/portable` git repo is not self-contained (crypto/ssl/tls
  hold only CMakeLists; sources are pulled from OpenBSD by `update.sh` at build time =
  build-time network, weaker reproducibility).
- **PoC transferability:** this PoC ran against the system OpenSSL 3.5.6; API compatibility
  makes it transferable, but **re-run it against the built LibreSSL** once the ext-dep
  lands (cheap validation).

## CA trust strategy — DECIDED (owner-ruled 2026-07-04)

**Strategy C — hybrid: native system trust store per platform, plus a CA-override API.**

Context that forces an explicit strategy: the OpenSSL used here comes from
`ext-deps-generator` (custom build), so its `OPENSSLDIR` points into the build prefix —
`set_default_verify_paths()` finds the host system's certificates on **no** platform,
Linux included. The trust store must be bootstrapped explicitly everywhere.

Per-platform default:

- **Linux** — probe the well-known distro bundle paths (Debian/Ubuntu
  `/etc/ssl/certs/ca-certificates.crt`, RHEL/Fedora `/etc/pki/tls/certs/ca-bundle.crt`,
  etc.), the curl/Go practice.
- **Windows** — **hand-written wincrypt trust-store import** in `src/Network/`
  (enumerate the `ROOT` system store via CryptoAPI, convert to X509, inject into the
  `X509_STORE`), **compiled and exposed only on Windows** (platform-guarded; owner-ruled
  2026-07-04). Note: OpenSSL ≥ 3.2 has a native `winstore` loader that would make this one
  line, but **LibreSSL does not ship it** — the provider choice (see below) implies this code.
- **macOS** — load `/etc/ssl/cert.pem`, the Apple-maintained bundle extracted from the
  system Keychain. **Owner-ruled over** the Security.framework anchor extraction
  (~150 lines of Apple-specific code, deprecated-API churn); user/enterprise Keychain
  CAs are the override API's job instead.

Override API (part of the decision): a small `setCAFile(path)`-style hook to load a
custom bundle. Required anyway for **hermetic tests** (local self-signed TLS server
under ASan/UBSan — the handshake is never tested against the public internet), and the
escape hatch for corporate/MITM-proxy CA environments.

Rejected: **B** (host-supplied Mozilla `cacert.pem` bundle) — stale-bundle risk on root
rotation, ignores enterprise CAs, and base is a library with no data directory of its
own; **A** (system-only, no override) — would still need the override for tests.

## LibreSSL ext-dep — recipe landed + PoC re-validated (2026-07-04)

Step 1 executed on the Linux host:

- **LibreSSL 4.3.2** (latest stable, owner-pinned) vendored into ext-deps-generator
  `repositories/libressl/` — tarball SHA256 verified against the mirror's signed list,
  recorded in `libraries/libressl.yaml` with the upgrade procedure.
- Recipe: `libraries/libressl.yaml` (`LIBRESSL_APPS/TESTS=Off`, static, builder-managed
  MSVC CRT), `_build_order.yaml` (Security group), test project links `tls ssl crypto`
  + Windows `bcrypt`/`crypt32`, `src/main.cpp` gains `test_libressl()` (`tls_init()`
  exercises all three archives).
- **Validated (Linux x86_64):** Release + Debug build & install; link-test **39/39 passed**
  (`libressl: OK (LibreSSL 4.3.2)`).
- **This PoC re-run against the built LibreSSL 4.3.2 static libs: PASSED** (compile + static
  link + run under `-fno-exceptions`/`ASIO_NO_EXCEPTIONS`, `-Wall` clean) — the
  OpenSSL→LibreSSL transferability risk is closed.
- **Pending:** Windows (MSVC MD/MT) and macOS (ARM64 + x86_64 cross) validation of the
  recipe on their respective hosts.

## HTTPS client scope — DECIDED (owner-ruled 2026-07-04)

- **HTTP/1.1 only, h2-ready API.** The public API is protocol-agnostic — chunked transfer,
  keep-alive and connection management stay internal, nothing 1.1-specific leaks. HTTP/2 is
  an explicitly deferred **post-plan feature** (second codec + ALPN negotiation via nghttp2,
  additive by design — HTTP/2 supplements 1.1, the 1.1 path remains the mandatory ALPN
  fallback, so nothing built now is throwaway).
- **Redirects: automatic, bounded, no-downgrade.** Follows 301/302/303/307/308 with correct
  method semantics (303→GET, 307/308 preserve method+body), configurable cap (default 5),
  **https→http downgrade refused** (propagated as an error), http→https upgrade allowed.
- **Proxy: basic HTTP(S) proxy IN scope** (owner-ruled, over the AI's leaner no-proxy
  recommendation): CONNECT tunneling for https targets, `https_proxy`/`no_proxy` environment
  variables (`http_proxy` is deliberately not read: the client is HTTPS-only, so the variable
  can never apply). Accepted cost: additional test/fuzz surface.
- **API surface: synchronous facade, asio inside.** `request(HTTPRequest) →
  std::optional<HTTPResponse>`-style blocking calls plus a `download(uri, path)` on the new
  client (`HTTPSClient::download`); asio-async machinery + timeouts live behind the facade;
  the caller (engine) owns its threading. The legacy free function `Network::download()` was
  **not** upgraded but **removed** (2026-08-27, with `hasInternetConnexion()`, `Network.hpp/.cpp`
  and the `EMERAUDE_INTERNET_CHECK_DOMAIN` option): it spoke cleartext and used the throwing Asio
  overloads (abort under `ASIO_NO_EXCEPTIONS`). The engine's `Net::Manager` now downloads through
  `HTTPSClient::download()`.
- **Timeouts: full configurable set** (connect / TLS handshake / response / total) with sane
  defaults — stated as the only production-grade option, unobjected.

## Next steps (owner-sequenced)

1. ~~Add LibreSSL to `ext-deps-generator`~~ **done 2026-07-04** (see above); Windows/macOS
   recipe validation still pending.
2. ~~`SetupLibreSSL.cmake` in `cmake/`~~ **done 2026-07-04** — links `tls -> ssl -> crypto`
   (static, full paths per the Setup convention); on MSVC also links the system libs the
   static archives can't autolink (`ws2_32`, `bcrypt` for getentropy, `crypt32` for the
   future Windows trust-store import). Included from the base `CMakeLists.txt` right after
   `SetupASIO`. Verified: full projet-alpha cascade links (Release) and the unit suite
   holds its 1893/1893 baseline. Windows/macOS link validation pending on their hosts.
3. HTTPS client in `src/Network/` (TLS + redirects + chunked transfer) + the trust-store
   bootstrap (incl. the Windows-only wincrypt import), with tests under ASan/UBSan.
   - **Trust-store bootstrap DONE 2026-07-04** — `src/Network/TrustStore.{hpp,cpp}`:
     `applySystemTrustStore()` (Linux: 6-path distro-bundle probing + hashed-dir fallback;
     macOS: `/etc/ssl/cert.pem`; Windows: hand-written CryptoAPI import of the ROOT + CA
     system stores, `IS_WINDOWS`-guarded), `applyCABundleFile()` (the override API) and
     `certificateCount()` (diagnostic/test proof). Logging hook throughout, no-exception
     error contract (`bool` + `error_code` overloads). Fixtures: `tls-test-ca.pem` +
     `tls-test-leaf-localhost.pem` (SAN `localhost`/`127.0.0.1`, 100-year validity,
     **certificates only — no private key committed**; the leaf will serve the future
     hermetic TLS test server). 6 tests in `test_NetworkTrustStore.cpp`, including the
     utility proof: real `X509_verify_cert` chain verification succeeds with the fixture
     CA loaded and fails without it. Suite **1899/1899** Release AND ASan/UBSan; cascade
     links. Windows/macOS paths compile-pending on their hosts.
   - **TLS transport DONE 2026-07-04** — `src/Network/TLSConnection.{hpp,cpp}`: blocking,
     single-use client connection (the sync-facade transport layer). Internally 100% asio
     **async** ops driven by a private `io_context::run_for` — that is what provides the
     per-operation timeouts (`TLSConnectionOptions`: connect / handshake / read / write,
     default 30 s) under `-fno-exceptions`; on timeout the socket close aborts the pending
     operation. SNI + `verify_peer` + `asio::ssl::host_name_verification`
     (`X509_check_host`) always enforced — no insecure switch by design. 6 hermetic tests
     (`test_NetworkTLSConnection.cpp`) against an in-process TLS echo server on 127.0.0.1
     whose EC P-256 credentials are **generated at runtime** (no committed private key):
     trusted echo round-trip, untrusted-chain rejection, hostname-mismatch rejection,
     connection-refused, read-timeout honored (250 ms budget → 253 ms), not-connected
     contract. **Live validation (Linux, 2026-07-04):** system store (150 CAs) →
     github.com + www.khronos.org handshake + HTTP HEAD exchange OK; badssl.com
     wrong-host / self-signed / expired all rejected. Suite **1905/1905** Release AND
     ASan/UBSan; cascade links. Owner validates Windows/macOS on their hosts later
     (owner-ruled 2026-07-04: full Linux validation first).
   - **HTTP/1.1 codec (response side) DONE 2026-07-04** —
     `src/Network/HTTPResponseParser.{hpp,cpp}`: incremental feed-based parser (the
     client loop pumps transport bytes in, drains `body()` between feeds for streaming).
     Framing per RFC 9112 §6.3: Transfer-Encoding chunked (extensions ignored, trailers
     read-and-discarded, exact CRLF terminators) > Content-Length (strict digit-only
     parse) > read-until-close (`finish()`); interim 1xx skipped (bounded), 101 Upgrade
     refused; HEAD via `expectBodilessResponse()`, 204/304 automatic. **Untrusted-input
     hardening (A.3 doctrine):** bounded header section (64 KiB default), chunk-size line
     and trailer caps, caller-set body cap, uint64-overflow-safe hex/decimal parsing,
     **duplicate Content-Length refused via raw-section scan** (the header map keeps one
     value silently — smuggling defense) and **TE-overrides-CL** enforced. Supporting
     fixes on the existing classes, each RFC-motivated: `HTTPHeaders` field-name lookups
     made **case-insensitive** (RFC 9110 §5.1 — custom hash/equal on the map);
     `HTTPResponse` empty reason phrase accepted + status code bounds-checked 100-599
     (RFC 9112 §4) + new `keepConnectionAlive()` (RFC 9112 §9.3 semantics);
     cerr→Logging across the touched files. **21 tests** (`test_NetworkHTTPResponseParser.cpp`),
     nominal AND hostile, every payload also fed byte-by-byte to exercise the incremental
     paths. Suite **1926/1926** Release AND ASan/UBSan; cascade links. The request side
     needed no codec work: `HTTPRequest::toString()` already serializes correctly.
3b. **Proxy support DONE 2026-07-04.** `TLSConnection` refactored into reusable phases
    (`establishTcp` / `tunnelThroughProxy` / `performHandshake`) and gained
    `connectViaProxy(proxyHost, proxyPort, targetHost, targetPort)`: reaches the proxy,
    performs a **plaintext HTTP CONNECT** on the raw socket (bounded response read, 2xx
    required), then runs the **end-to-end TLS handshake with the target** (SNI + chain +
    hostname verification against the target — the proxy never sees the cleartext).
    `HTTPSClient` gained proxy options (`proxy` explicit authority, `useEnvironmentProxy`)
    and `resolveProxy()`: honors `https_proxy`/`HTTPS_PROXY` and the `no_proxy`/`NO_PROXY`
    bypass list (exact, `.suffix`, and `*`), default proxy port 8080. Test server gained a
    proxy mode (plaintext CONNECT → 200 → serves as the tunnelled target). Tests: explicit
    proxy tunnel, env-var proxy, `no_proxy` bypass. Suite **1957/1957** Release AND
    ASan/UBSan; cascade links.

4. Finish RFC 3986 URI parsing (percent-encoding, IPv6, authority/TLD). **DONE — see the sub-entry below.**

5. **Fuzz the response parser DONE 2026-07-04.** `src/Fuzzing/fuzz_http_response.cpp` drives
   `HTTPResponseParser` both whole-feed and byte-sliced (control byte selects strategy +
   bodiless flag), 5-seed corpus (fixed / chunked / redirect / interim / until-close).
   Campaign: **23.5M runs, 0 crashes** under ASan+UBSan `halt_on_error`. Added to
   `build-fuzzers.sh` and the `src/Fuzzing/README.md` target table.

## Status: Network production-grade COMPLETE (2026-07-04)

Every step of this plan is landed and verified — trust store, TLS transport, HTTP/1.1 codec,
HTTPS client with bounded no-downgrade redirects and timeouts, proxy (CONNECT tunnel +
env), RFC 3986 URI conformance, and the response-parser fuzz target. This closes the last
open item of the "Ave robustus!" plan (see [`../ave-robustus.md`](../ave-robustus.md) §6).
**The owner declared the plan complete and lifted the feature freeze on 2026-08-27.**

### Live end-to-end check (opt-in)

`src/Testing/test_NetworkHTTPSClientLive.cpp` (suite `NetworkHTTPSClientLive`) is a real-world
validation against a **secure public server**: it downloads the full-resolution Wikimedia
image `Rowan_Atkinson_and_Manneken_Pis.jpg` (Mr Bean beside the Manneken-Pis — a fitting
Belgian unit test) over HTTPS using the **operating-system trust store**, checks the JPEG
magic bytes and size, verifies the `image/jpeg` content type via `get()`, and confirms an
**empty trust store rejects the real (valid) Wikimedia certificate** (proof that verification
is truly enforced). It hits the internet, so it is **skipped by default** to keep the suite
hermetic; enable it explicitly:

```bash
EMERAUDE_RUN_LIVE_NETWORK_TESTS=1 \
  ./.claude-build-release/Release/EmeraudeBaseUnitTests --gtest_filter='NetworkHTTPSClientLive.*'
```

> Note: the direct upload URL (`upload.wikimedia.org/.../Rowan_Atkinson_and_Manneken_Pis.jpg`)
> is the file itself — the `fr.wikipedia.org/wiki/...#/media/...` article link is the HTML page.

Non-blocking follow-ups carried past closure:
- Windows (MSVC — the wincrypt trust-store leg) and macOS (`/etc/ssl/cert.pem`) build+run
  validation on those hosts.
- A versioned LibreSSL release in `ext-deps-generator` so machines without the local
  `output/` symlink can build.
- Live-network (non-hermetic) validation of the client's redirect/proxy paths.
   - **Scope decided 2026-07-04 (owner-ruled) after an audit of the quickly-written URI class.**
     Audit found: concrete bugs (`URIDomain::host()` omits the `:` before the port; IPv6
     literals shredded by the `explode(':')` port parse), and RFC 3986 gaps (no
     percent-encoding either way; no scheme validation/case-normalization; fragile
     split-on-delimiter parsing; no port range check; no §5 relative-reference resolution —
     which is what `HTTPSClient::resolveRedirect` needs for relative Locations; no §6
     normalization). **Rulings: (a) rewrite the parser** to the RFC 3986 Appendix-B grammar
     with validation (public API — scheme/uriDomain/path/query/fragment — unchanged);
     **(b) full normalization + relative-reference resolution** (§5 resolution, §6.2.2
     case + §5.2.4 dot-segment removal) — this also unblocks the client's relative
     redirects; **(c) store components decoded, re-encode per-component on output**
     (path/query/userinfo have different allowed sets). Canonical RFC §5.4 resolution
     examples and §6.2.2 normalization examples become test vectors.
   - **DONE 2026-07-04.** New `Network/PercentEncoding.{hpp,cpp}` (decode + per-component
     encode: Path/Segment/Query/Fragment/Userinfo allowed-sets; malformed `%XX` left
     verbatim, uppercase-hex canonical output). `URI` rewritten: hand-written RFC 3986
     Appendix-B decomposition (NOT `std::regex` — it trips a libstdc++
     `-Wmaybe-uninitialized` false positive under the sanitizer build, and is heavyweight
     for a fixed grammar), scheme validation + lowercase, host lowercase, path
     percent-decoded then dot-segment-removed, `resource()`/`operator<<` re-encode,
     `URI::resolve()` (§5.2.2 transform + §5.2.3 merge + §5.2.4 remove-dot-segments) and
     public `removeDotSegments()`. `URIDomain` rewritten: authority = `[userinfo@]host[:port]`
     with **IPv6-literal bracket handling**, port range check (0-65535), userinfo
     percent-decode, and the **`host()` `:`-separator bug fixed**. `Query` fixed (the
     `operator<<` pre-sized-vector bug that emitted leading `&`; value-less keys now emit
     bare; keys/values percent-decoded/encoded). `HTTPSClient::resolveRedirect` now uses
     `URI::resolve` (relative Locations work). **Tests:** `test_NetworkURI.cpp` — component
     parsing, lowercase normalization, IPv6, port range, percent round-trip, dot-segment
     removal, **the RFC §5.4 reference-resolution vectors**, **the RFC §1.1.2 scheme-diversity
     set** (mailto/news/tel/urn/ldap-IPv6), a **gnarly-but-valid batch** (empty components,
     encoded delimiters, UTF-8/IDN/punycode, embedded NUL, matrix params, 5000-deep `/..`
     underflow guard) + `PercentEncoding` round-trip/malformed. Suite **1954/1954** Release
     AND ASan/UBSan; cascade links. Owner validates Windows/macOS later.

## Live-network validation (2026-08-27, Linux)

The three opt-in tests (`EMERAUDE_RUN_LIVE_NETWORK_TESTS`) ran against real endpoints and
**passed 3/3** (`NetworkHTTPSClientLive.downloadsMrBeanImageToFile`, `getReturnsImageContentType`,
`rejectsUntrustedRealServer`), on top of the engine's own live use the same day (`Net::Manager`:
13 566-byte and 184-byte files over TLS with 150 system CAs, an expired-certificate host refused).

Diagnostics as they reach a consumer (`Logging::error`, one line each), recorded so nobody has to
provoke them again:

| Situation | Line |
|---|---|
| HTTP 404 (or any non-2xx on `download()`) | `[Network::HTTPSClient] download(), the server answered with status 404.` — the destination file is removed |
| Unresolvable host | `[Network::TLSConnection] establishTcp(), unable to resolve 'no-such-host.invalid' : Host not found (authoritative)` |
| Port closed / host down | `[Network::TLSConnection] establishTcp(), unable to reach '127.0.0.1:9' : Connection refused` |
| Untrusted / expired certificate | handshake failure reported by `TLSConnection`, `download()` returns false; nothing written |

Each failure is a `false` / `std::nullopt` to the caller — never an exception, never an abort — and
leaves no file behind. On the engine side every one of them ends as `DownloadStatus::Error` and the
resource falls back to its default (`ResourceTrait::failLoading()`).

## Post-freeze increment — `download()` progress hook (2026-08-27)

First feature added after the plan's closure: `HTTPSClient::download(uri, filepath, progress)` takes
an optional `DownloadProgress` (`bytesReceived`, `std::optional< uint64_t > bytesTotal`), invoked on
the calling thread after each transport read that carried body bytes of the final 2xx hop; the total
is the hop's `Content-Length` when the body is framed by it, `std::nullopt` for chunked /
read-until-close. Redirect hops report nothing. Three tests: Content-Length (monotonic, final ==
size, total known throughout), chunked (total unknown, final == decoded size), and the hook-less
call unchanged. Consumer: the engine's `Net::Manager` throttles it to one `Progress` notification
per ticket per main-loop cycle.

## Hardening pass — 2026-08-27 (post-closure audit)

A hostile review of the shipped stack found the following; all are fixed, each with a regression
test (suite 2009/2009, live 3/3):

| Was | Now |
|---|---|
| The host was percent-**decoded** and never validated, then concatenated into the proxy `CONNECT` line, `Host:` and SNI → CRLF injection / request smuggling from a URL or a hostile `Location` | `URIDomain` validates the decoded host once, for every consumer: a bracketed IP literal, or `[A-Za-z0-9.\-_]` up to 253 bytes. Anything else (CR, LF, NUL, space, `@`, a second `:`) leaves the host empty and the URI unusable |
| `maxBodySize` defaulted to `UINT64_MAX`, and the body was drained only for a File sink on a 2xx → a 404 body, a redirect body or a `head()` body accumulated whole in RAM | Two explicit ceilings in `HTTPSClientOptions`: `maxInMemoryBodySize` (64 MiB) and `maxDownloadSize` (4 GiB), applied per sink; a body that is not kept is cleared on every iteration |
| The Windows **`CA`** store (intermediates, per-user, writable without admin) was imported through `X509_STORE_add_cert`, i.e. as **trust anchors** | `ROOT` only. Chain completion is the server's job (RFC 8446 §4.4.2) |
| `ssl::error::stream_truncated` (connection cut **without** `close_notify`) was reported as a clean EOF → a half-delivered read-until-close body passed as `Complete` | It is an error. `TLSConnection::read()` returns `std::nullopt` and says so; `download()` fails and leaves no file |
| An IPv6 literal kept its brackets all the way to `getaddrinfo` → `https://[::1]/` could never connect; SNI was sent for IP literals (RFC 6066 forbids it) and they were verified with `X509_check_host` | The brackets are stripped for the resolver and the identity check, kept for the CONNECT authority; SNI is skipped for an IP literal, which is verified with `X509_check_ip_asc` against the iPAddress SAN |
| `download()` opened (and truncated) the destination before the status was known, never checked the final flush, and left a partial file on a transport failure → a full disk reported success | The file is removed on every failure exit, and `flush()`/`close()` are checked before success |
| A declared-but-invalid port (`:99999`, `:abc`) was dropped and the scheme's default used → `https://host:99999/` connected to 443 | `URIDomain::hasInvalidPort()` records it and the client refuses the URI |
| A `303` rewrote **HEAD** into GET (RFC 9110 §15.4.4 exempts it), whose body was then buffered | HEAD survives a 303 |
| One header with an empty value (`X-Cache:`, legal per RFC 9110 §5.5) rejected the whole response | The header line is split on its first colon; an empty value is kept |
| A zero-length 2xx body never called the progress hook, though the contract promises a terminal call | One final call is emitted |

Not fixed here, tracked separately: `totalTimeout` is still not a hard budget (per-operation timeouts
add up, DNS is not cancellable), error reporting is still `bool`/`nullopt` with no cause, and the URI
layer still decodes `%2F` and re-orders query parameters (breaks signed URLs).

## Post-freeze increment — `request()`, the API-traffic entry point (2026-08-28)

Owner-asked: *"is the engine able to talk to a web API at C++ level, without CEF?"* The answer was
**half** — the stack could read (`get`, `head`, `download`) but could not write: no `POST`, no
request body, and no caller headers, so no authenticated API was reachable. The request was built
from four hard-coded lines (`Host`, `User-Agent`, `Accept-Encoding`, `Connection`).

### What landed

| | |
|---|---|
| `HTTPRequestOptions` | `headers` (ordered `vector<pair>`, because a field may repeat and a signing scheme can be order-sensitive), `body`, `contentType` |
| `HTTPSClient::request(method, uri, options, report*)` | Any method; `get()`/`head()` are now façades over it. The response is held in memory under `maxInMemoryBodySize` |
| `HTTPSClient::isRequestHeaderAcceptable(name, value)` | **Static and public, so it is testable on its own.** RFC 9110 §5.6.2 token names; CR, LF, NUL, every C0 but HTAB, and DEL refused in values; the framing headers (`Host`, `Content-Length`, `Connection`, `Transfer-Encoding`, `Accept-Encoding`) refused outright — a duplicate framing header is a request-smuggling primitive. `User-Agent` is deliberately NOT reserved: overriding it is legitimate, and the caller's replaces the option rather than duplicating the line |
| `DownloadOutcome::BadRequest` | New value: the CALLER's request was refused before a byte left, which a caller must not confuse with the network being down |
| Redirect hygiene | A 301/302/303 that rewrites the method to GET also **drops the body** (a write must not be replayed); a redirect that changes host or port **drops every caller header** (an `Authorization` forwarded to a `Location` target is the classic credential leak) |
| Content-Length framing | `POST`/`PUT`/`PATCH` always send it, even at 0 — a server reading a POST without it waits for a body that never comes |
| A non-2xx | **Not a failure** for `request()`, unlike `download()`: an API says what went wrong in its body. `report.outcome` labels it `HTTPStatus` and the response is still returned |

### ⚠️ A data race fixed on the way

`m_lastOutcome` was a `mutable` member written by these `const` methods. `Net::Manager` runs
several `download()` calls **concurrently on one shared client**, so that member was a genuine
race, and a failing transfer could report the reason belonging to another one. It is now an
out-parameter threaded through `run()`/`performHop()`, which makes every call self-contained.
**Never put it back on the object.**

### Verification

18 new tests in `src/Testing/test_NetworkHTTPSClientRequest.cpp`; suite **2028/2028** (Release,
Linux/GCC, 2026-08-28), 3 live tests skipped by design.

⚠️ `HTTPSTestServer` had to be extended: it stopped reading at the header terminator, so a request
body was only whatever happened to share the last TLS record — a test asserting on it would have
passed or failed by timing. It now reads out what `Content-Length` announced
(`Testing::declaredContentLength()`).

### Still not done

- **No keep-alive**: one TLS connection per hop, so an API hit in rapid succession pays a full
  handshake each time. Fixing it is a `TLSConnection` change.
- `Method::DELETE` is untested on purpose: `DELETE` is a `winnt.h` macro and the test TU pulls in
  gtest. ⚠️ The enumerator itself (`HTTPRequest::Method::DELETE`) is a latent MSVC hazard for any
  TU that sees `windows.h` before `HTTPRequest.hpp` — it has not bitten yet because none does.
