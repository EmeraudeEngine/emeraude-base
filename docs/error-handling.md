# Error Handling — emeraude-base

> The error-handling **contract** for the foundation library. Codified as part of the
> **"Ave robustus!"** plan, phase A.0. See [`plans/ave-robustus.md`](plans/ave-robustus.md).
> This document is normative: new and migrated base code MUST follow it.

## 1. The no-exceptions reality

emeraude-base builds with **`-fno-exceptions` by default** (`EMERAUDE_DISABLE_EXCEPTIONS=On`).
Under that flag a `throw` does not unwind — it calls **`std::terminate()`**. Therefore
"robust error handling" in base can **never** mean `try`/`catch`. Errors are values that
propagate, not exceptions that unwind.

> Base may still be consumed by a project built **with** exceptions. Header code that wants
> to serve both audiences uses the dual pattern in §4.

## 2. Error propagation — the contract

Hybrid, using only the standard library (C++20; no custom `Expected` type during the
consolidation epoch):

| Operation shape | Return | Meaning |
|-----------------|--------|---------|
| Fallible, no value to return | **`bool`** | `true` = success, `false` = failure |
| Fallible, returns a value | **`std::optional<T>`** | engaged = value, `std::nullopt` = failure |
| Cannot fail | plain value / `void` | — |

- The *reason* for a failure travels on the **diagnostic channel** (§6), not the return type.
- A function that can fail is **never** silently lossy: it returns `false`/`nullopt` AND logs
  a diagnostic explaining why.
- Prefer `[[nodiscard]]` on every fallible return so callers cannot ignore it.

```cpp
[[nodiscard]] bool writeStream (ByteStream & stream) noexcept;            // ok / not ok
[[nodiscard]] std::optional< Pixmap > readImage (const ByteStream &);     // value or nothing
```

## 3. Forbidden constructs

These either throw (→ `terminate` under `-fno-exceptions`) or hide failures:

- **`throw`** in base's own `-fno-exceptions` build (see §4 for the only sanctioned use via
  the dual `#if/#else` pattern).
- **Throwing standard-library calls**: `std::vector`/`std::map`/`std::string`/… `.at()`,
  `std::stoi` / `std::stol` / `std::stod` / … , `std::make_unique` is fine but a raw
  `new` whose failure is unchecked is not. Validate first, then use the non-throwing form
  (`operator[]` after a bounds check, `std::from_chars`, etc.).
- **Swallowing errors silently** (e.g. ignoring a `bool`/`optional`, empty `catch`).

## 4. Abort policy (owner-ruled)

> **Never `std::abort()` / `std::terminate()` on a runtime or input error.** Those must
> propagate gracefully via §2. `abort`/`assert` are allowed **only** for **programmer-contract
> violations** (an internal invariant the caller is responsible for), and only in Debug.

**Runtime/input error** (propagate): malformed file, truncated stream, missing key, network
failure, out-of-memory from untrusted sizes, …

**Programmer-contract violation** (may abort): indexing a fixed-capacity container past its
capacity, calling a method in a state its precondition forbids, …

**The dual `#if/#else` pattern** — the sanctioned way for a *header* type to serve both
exception and no-exception consumers (as `StaticVector` does):

```cpp
if ( count > max_capacity )
{
#if defined(__cpp_exceptions)
    throw std::length_error{"…"};   // exception build: throw
#else
    std::abort();                   // -fno-exceptions build: fail fast
#endif
}
```

Such fail-fast paths MUST be covered by a **death-test** (gtest `EXPECT_DEATH`), see §7.

## 5. Untrusted input — bounds before use

Anything read from a file, a network peer, or any external source is **hostile until
validated**. Before using a read value:

- **Bounds-check every size, count, offset and index** against the actual buffer/stream
  size *before* allocating or seeking. Allocation sizes derived from a file header are the
  #1 parser vulnerability.
- Guard arithmetic on those values against **overflow**.
- On any inconsistency: fail per §2 (return `false`/`nullopt` + diagnostic), never UB, never
  OOM, never crash.

### 5.1 C-library error callbacks — `setjmp`/`longjmp` (sanctioned)

Some third-party C decoders report a fatal error by invoking a caller-supplied callback that
**must not return** — if it returns, the library calls `abort()`/`exit()` and kills the process:

- **libpng**: a returning error handler triggers `PNG_ABORT()` → `abort()`.
- **libjpeg(-turbo)**: the default `error_exit` calls `exit()`.

Catching these without C++ exceptions requires `setjmp`/`longjmp` — it is the library-sanctioned
mechanism and is **allowed here** (it is the only way to honour §4's "never crash on input error"
for these decoders). Pattern: arm `setjmp` in the read/write function, make the error callback
`longjmp` back, and on the non-zero return free the library handle and fail per §2.

`setjmp`/`longjmp` interact badly with C++ automatic objects — respect these rules:

- A non-`volatile` local **modified between `setjmp` and the `longjmp`** has an indeterminate
  value afterwards. Keep any value the error branch reads **set before `setjmp`** (or split into
  multiple `setjmp` phases). Variables passed **by value** to a C call across the `setjmp` also
  trip GCC's `-Wclobbered` — consume them before arming `setjmp`.
- A `longjmp` does **not** run destructors of objects declared **after** the `setjmp` in the same
  scope → they leak. Declare RAII locals (e.g. a `std::vector` of row pointers) **before** the
  `setjmp`, and fill them before arming it so they are neither skipped nor clobbered.

`FileFormatPNG` (two phases: header, then image) and `FileFormatJpeg` (custom `jpeg_error_mgr`)
implement this; see their `fuzz_png` / `fuzz_jpeg` regression history in
[`/src/Fuzzing/README.md`](../src/Fuzzing/README.md).

### 5.2 Owning resource handles — RAII, never manual `close()` (A.4)

Every owning resource (heap allocation, OS handle, third-party C library handle) lives in an
**RAII wrapper**, never in a bare owning pointer released by a hand-written `close()`. The
"zero owning raw pointers" rule is the A.4 exit criterion: a handle that must be freed by a
discipline ("remember to call `closeArchive()` on every path") is one early-`return` away from
a leak, and `-fno-exceptions` does not make that discipline any safer.

- A C handle with a free function → `std::unique_ptr<T, decltype(&free_fn)>`. The deleter runs
  at destruction on **every** path (early return, abandonment, future edits) — no manual call to
  forget. Example: `IO::ZipReader` / `IO::ZipWriter` hold
  `std::unique_ptr<zip_t, decltype(&zip_close)> m_zip{nullptr, &zip_close}`; opening is
  `m_zip.reset(zip_open(...))`, closing is `m_zip.reset()`, and the object becomes move-only for
  free. The deleter is **not** called on a null pointer, so an abandoned-before-open object is
  also clean.
- A handle freed only inside a `setjmp`/`longjmp` error branch is the §5.1 exception: there the
  free is explicit (the longjmp skips destructors of objects declared after the `setjmp`).
- The test that proves it: open the resource, **abandon it without the manual close**, let it go
  out of scope — under ASan the missing leak proves the destructor released it (see
  `test_ZipArchive.cpp`).

## 6. Diagnostics — the Logging hook

Base does **not** own `stderr`. All diagnostics go through **`EmEn::Base::Logging`**
([`/src/Logging/Logging.hpp`](../src/Logging/Logging.hpp)), never raw `std::cout`/`std::cerr`.

```cpp
#include "Logging/Logging.hpp"

Logging::error("VertexFactory", "MD3 header claims more triangles than the stream holds");
Logging::log(Severity::Warning, "WaveFactory", message);
```

- The default sink splits by severity — Debug/Info/Success → `std::cout`, Warning/Error/Fatal
  → `std::cerr`; when the **engine Tracer** is running it registers itself
  as the sink, so base diagnostics flow into the Tracer (files, colour, tag filtering)
  automatically — base stays engine-agnostic.
- **No new raw `std::cerr`/`std::cout`** in base. The legacy raw-`cerr` sites migrate to the
  hook **progressively, per module**, during phases A.2/A.3 (not in one diff).
- `Severity` is `EmEn::Base::Severity` ([`/src/Logging/Severity.hpp`](../src/Logging/Severity.hpp)).

## 7. Testing the contract (no fix without a test)

Per the plan's standing rule, **every fix or hardening ships with a unit test** in
`EmeraudeBaseUnitTests`, in the same change — failing before, passing after, and green under
the A.1 sanitizers.

- Value/`bool`/`optional` paths: assert both the success and the failure return.
- Untrusted-input hardening: feed truncated / malformed / hostile fixtures, assert graceful
  failure (no UB/OOM/crash) — not just the happy path.
- Fail-fast abort paths (§4): a `*DeathTest` suite with `EXPECT_DEATH`. Keep template commas
  out of the macro arguments (use a `using` alias).

## Cross-references

- Plan & doctrine design: [`plans/ave-robustus.md`](plans/ave-robustus.md) (§A.0).
- Logging hook: [`/src/Logging/Logging.hpp`](../src/Logging/Logging.hpp), `Severity.hpp`.
- Compile policy (`-fno-exceptions`, `-Werror`, FORTIFY): root `CMakeLists.txt`, `AGENTS.md`.
