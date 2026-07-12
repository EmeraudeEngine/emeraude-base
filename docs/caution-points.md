# Caution Points — emeraude-base

> Cross-cutting pitfalls for the foundation library. Add a site here the moment the
> compiler, a platform, or a toolchain surprises you — the next AI session should not
> re-diagnose it from scratch.

## Build / Compiler

### GCC 14 `-Wstringop-overflow` / `-overread` false positives on `std::string`

GCC 14 raises a **`-Werror=stringop-overflow`** (or its sibling `-overread`) inside
`<bits/char_traits.h>` — `__builtin_memcpy writing/reading N bytes into a region of size 16` —
on perfectly valid `std::string` code. The `region of size 16` is the 15-byte **SSO buffer**
(+ NUL) of a `basic_string`. GCC's value-range analysis mis-judges that a string whose inferred
length exceeds SSO could still live in that inline buffer during a move-construct, and flags the
allocated-buffer `memcpy` as an overflow. The code is correct; the diagnostic is wrong.

The same GCC bug family has two known triggers. Base is a standalone repository (the engine is
*downstream*, so no link points up to it from here); a consumer that builds base inside the engine
cascade with PCH will also meet the first row below, documented in full on the engine side.

**Two triggers**

| Context | Warning | Trip-wire | Minimal fix that worked |
|---------|---------|-----------|-------------------------|
| `EMERAUDE_ENABLE_PCH=ON` (engine cascade) | `-overread` (read) | move-construct on `return` | **`reserve()`** — `+=` alone did *not* help |
| `_FORTIFY_SOURCE=2` + `-O2` (base Release policy) | `-overflow` (write) | temporary move-constructed inside an `operator+` chain | **`+=` on a named local** — the temporary disappears |

The base sites were `Network/TrustStore.cpp` (`std::string{"…"} + LinuxHashedCertsDirectory + "'."`)
and `Network/HTTPSClient.cpp` (`request += std::string{HTTPRequest::AcceptEncoding} + ": identity\r\n"`,
plus the sibling `Host` / `User-Agent` lines that had the same shape and would have tripped next).
Both are triggered by `_FORTIFY_SOURCE=2` (part of the base Release compile policy), amplified by
the PCH-shifted inlining context — GCC 14 + glibc 2.41.

**Wrong fixes** — the project never disables warnings:
- `-Wno-stringop-overflow` / `-Wno-stringop-overread` (global or per-file).
- `#pragma GCC diagnostic ignored`, `NOLINT`.
- Weakening `_FORTIFY_SOURCE` — it is useful hardening and the analysis is correct everywhere else.

**Correct fix — remove the SSO ambiguity at the source, escalating only as needed:**
1. Rewrite the `operator+` chain into `+=` on a **named local** `std::string`. This deletes the
   move-constructed temporary that GCC mis-analyses. Sufficient for the `_FORTIFY_SOURCE` /
   `-overflow` trigger.
2. If `+=` alone does **not** clear it (the `-overread`-on-`return` trigger), also `reserve()`
   past 15 bytes before appending, so the buffer is unambiguously heap-allocated. This is what
   the engine's `Saphir/LightGenerator.cpp` needed.

Neither step changes behaviour.

**Do not pre-emptively rewrite every concatenation in the library.** Only a handful of sites trip
this today. Fix each site as the compiler actually flags it — a broad sweep is churn with no
verification signal, and the trigger is context-dependent (PCH on/off, optimisation level).