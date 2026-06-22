# Network TLS — gating PoC

Decision artifact for the **"Ave robustus!"** plan, Axis B → *Network production-grade*
(see [`../ave-robustus.md`](../ave-robustus.md), the Network row).

## Why this exists

Network production-grade needs an HTTPS client, which needs TLS, which needs a crypto
provider. **Owner decision (2026-06-22): OpenSSL 3.x via `asio::ssl`** — `asio::ssl` is a thin
C++ wrapper that delegates all cryptography to OpenSSL, reused here to stay on the existing
asio-async Network model instead of hand-rolling the TLS pump.

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

- A real runtime TLS handshake against a live server.
- Certificate-chain verification behavior.
- **Async** operations (`async_handshake` / `async_read_until`) under `ASIO_NO_EXCEPTIONS` —
  they go through the same `throw_exception` hook, so expected to hold, but must be exercised.

## Next steps (owner-sequenced)

1. Add OpenSSL to `ext-deps-generator` (new recipe + versioned release).
2. `SetupOpenSSL.cmake` in `cmake/` — base becomes the single source of the dep.
3. HTTPS client in `src/Network/` (TLS + redirects + chunked transfer), with tests under ASan/UBSan.
4. Finish RFC 3986 URI parsing (percent-encoding, IPv6, authority/TLD).
