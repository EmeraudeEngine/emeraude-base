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

**MSVC counterpart (2026-09-08).** `-fno-exceptions` has no single MSVC equivalent, so the policy
is three switches, all driven by `EMERAUDE_DISABLE_EXCEPTIONS`:

| Switch | What it does |
|--------|--------------|
| `/EHs- /EHc-` | No unwind semantics: a `throw` crossing our frames terminates, exactly like GCC/Clang. |
| `_HAS_EXCEPTIONS=0` | Tells the **MSVC STL** to stop expanding `try`/`catch` inside its own headers (its `_TRY_BEGIN`/`_CATCH_ALL` macros). `/EH` does not set it — it defaults to `1` — so without it every `std::vector`/`std::string` instantiation is a `try` compiled without unwinding. |
| **No `/wd4530`** | C4530 ("C++ exception handler used, but unwind semantics are not enabled") is left ON, so with `/WX` a `try` in our code is a **build error** — the compiler enforces §3 on Windows. Until 2026-09-08 the warning was suppressed, which is how two `try`/`catch` survived in the engine's Windows platform files. |

> ⚠️ `_HAS_EXCEPTIONS=0` is honoured but not officially supported by the MSVC STL team, and it
> changes the definition `std::exception` gets. Every C++ translation unit **linked into one
> binary** must agree on it — a prebuilt C++ third-party library compiled with the default is the
> thing to check when a Windows link or crash looks like an ABI mismatch. **Verified by the owner on
> the Windows toolchain, 2026-09-08**: the whole cascade (emeraude-base, emeraude-engine,
> projet-alpha) builds green under `/WX` with **zero C4530** — the STL emits no exception handler
> once `_HAS_EXCEPTIONS=0` is set, and the `#if __cpp_exceptions` branches (`ThreadPool.hpp`,
> `StaticVector.hpp`) stay compiled out under `/EHs-`. Runtime checked the same day: the engine's
> native file dialog opens through its rewritten `_beginthreadex()` worker path
> (`runFileDialogOnDedicatedThread()`, the former `std::thread` + `try`/`catch` site).

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

### 2.1 What `noexcept` means here — allocating functions keep it (owner decision 2026-10-07)

`noexcept` on a cascade function means **"reports its failures by value (§2), never by an exception"** — not "cannot
fail". It stays on a function that allocates or grows a container:

- Under `-fno-exceptions` (the default, and the whole cascade), a failed allocation **aborts whatever the keyword
  says**: `operator new` cannot throw into our frames, so removing `noexcept` would change nothing at run time and
  cost the optimiser the knowledge that no unwind path exists.
- A consumer built **with** exceptions gets `std::terminate()` on `std::bad_alloc` crossing such a function: the
  same outcome, by design. Exhausting memory with a TRUSTED size is not a recoverable error in a real-time engine; an
  UNTRUSTED size is bounded before it is used (§5), which is what turns "out of memory" into a refused input.
- A `noexcept` IS wrong on a function that lets a non-allocation failure escape as an exception — a throwing std call
  (`.at()`, `std::stoi`, the `std::filesystem` overloads without `error_code`, `std::any_cast` value form, jsoncpp's
  `as*()`): that call is forbidden anyway (§3), and the function is fixed, not un-`noexcept`ed.

(Former item `remove-invalid-noexcept`, closed by this decision.)

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
> violations** (an internal invariant the caller is responsible for), and only in Debug — except where an owner
> decision makes the fault fail fast in EVERY build: a `StaticVector` overflow, and (D2, 2026-10-08) a self-join of
> `Base::Thread`, a task waiting for its own `TaskHandle`, `HTTPServer::stop()` from its network thread, an
> `EventTrait` destroyed from its own timer (traced with `Logging::fatal()`, then `std::abort()`).

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

## asio: the error_code out-parameter is the only channel (2026-09-30)

`cmake/SetupASIO.cmake` defines `ASIO_NO_DEPRECATED` (owner decision, plan Ave Robustus): every synchronous asio
operation returns `void` and reports through its `asio::error_code &` out-parameter. Read that code after EACH step
(never let a later call overwrite an unread one); a best-effort teardown (`close`, `shutdown` of a dying socket) may
ignore it, saying so in a comment. The deprecated returned copy of the code (and the clang-tidy
`bugprone-unused-return-value` it raised) is gone. The whole cascade built with the define on the first try
(2026-09-30): no other deprecated asio API is used.

## std::any payloads: `Base::anyValue< T >()`, never the value form of std::any_cast (2026-09-30)

`std::any_cast< T >(data)` (the VALUE form) throws `std::bad_any_cast` on a type mismatch — under -fno-exceptions the
process aborts, silently. An Observer notification's payload is read with `Base::anyValue< T >(data, context)`
(`src/AnyValue.hpp`): the pointer form, nullptr (and an error log naming `context`) on a mismatch or an empty any; the
caller skips the notification (owner decision, plan Ave Robustus). The 40 value-form sites of the cascade (engine 28,
projet-alpha 12, all in `onNotification()` handlers) were converted at once. Test
`Observer.anyValueNeverThrowsOnAMismatchedPayload`.

## The Observer payload is a `Base::Any`, not a `std::any` — RTTI-free (owner decision A, 2026-10-07)

`std::any` needs RTTI on MSVC: under `/GR-` the STL sets `_HAS_STATIC_RTTI` to 0 and `<any>` refuses to compile (STL1003)
— libstdc++ and libc++ compile it without RTTI, so a Linux build would hide it. `ObservableTrait::notify()` and
`ObserverTrait::onNotification()` therefore carry a `Base::Any` (`src/Any.hpp`): the same value semantics (implicit
construction from any copyable value, copy, move, `hasValue()`, `reset()`), read back with `Base::anyCast< T >(&any)` /
`Base::anyValue< T >(data, context)` (nullptr on a mismatch, never a throw). Its type identity is `typeHashOf< T >()`: the
compile-time FNV-1a hash of the compiler's function signature naming `std::remove_cvref_t< T >` (`__PRETTY_FUNCTION__`
/ `__FUNCSIG__`, EnTT's `type_hash` technique, MIT, cited at the point of use).
- **The same in the shared engine and in the executables**: a STRING hash, not an address — each binary carries its
  own copy of emeraude-base, so a function-address identity (libstdc++'s trick for `std::any`) would differ.
- **Exact type only**: no conversion, no base class (`anyCast< Base >` of a `Derived` is nullptr), cv and references
  ignored (`const std::shared_ptr< T >` reads a `std::shared_ptr< T >`).
- **Never persist or transmit a typeHashOf()**: the signature text differs between compiler families. A collision of two
  type names (FNV-1a 64-bit) is the accepted risk.
- A value of at most 32 bytes, nothrow-movable, is stored inline (a `std::shared_ptr` payload allocates nothing,
  unlike libstdc++'s `std::any`); a larger one on the heap.
- asio is built with `ASIO_NO_TYPEID` (it only detects Boost's `BOOST_NO_TYPEID`, never `-fno-rtti`).
Tests: `BaseAny.*`, `Observer.*`.

## JSON: the checked FastJSON reads, never jsoncpp's as*() (2026-09-30)

jsoncpp's LIBRARY is built with exceptions: `asUInt()` on `-1`, `asInt()` on `1e20`, `asFloat()` on a string,
`isMember()` / `operator[]` / `getMemberNames()` on an array all throw `Json::LogicError` — under -fno-exceptions,
through a `noexcept`, the process aborts (`JSON_USE_EXCEPTION 0` in our headers only changes jsoncpp's INLINE code; the
library would `abort()` anyway). Reproduced 2026-09-30 by dropping a scene definition on the engine. Rules (owner
decision, plan Ave Robustus, engine triad 6c):

- A value is read with `FastJSON::getValue< T >(object, key)` or, for a bare node (an array item), `FastJSON::asValue<
  T >(node)`: `std::nullopt` on a wrong type, an integer out of the target's range (no silent wrap), a non-finite float
  (the parser accepts `NaN` / `Infinity`, and `1e999` reads as infinity) or a finite double beyond the float range. A
  fractional number still truncates toward zero into an integer. A boolean is not a number. Vector / Matrix / Color
  reads check every element the same way.
- A parsed ROOT, or any node reached by key, is checked `isObject()` before a member access.
- The six raw converters left in the cascade are inside `asValue()` itself, each behind its predicate. Census method
  (exact, not a grep: base `Variant` has `asFloat()` too): a scratch copy of `json/value.h` with the converters
  `[[deprecated]]`, put first as `-isystem`, and `-fsyntax-only -Wno-everything -Wdeprecated-declarations` over the
  compile database — 89 sites in 19 files before, 6 (the helper) after.
- Tests: `FastJSON.outOfRangeIntegersAreRefused`, `nonFiniteFloatsAreRefused`, `nonNumericElementsAreRefused`,
  `asValueOnBareNodes`.

## Ownership and lifetime — Ave Robustus II (owner decision, 2026-10-08)

The whole cascade (emeraude-base, emeraude-engine and the applications built on them) follows the C++ Core Guidelines
and the standard library's own ownership model **scrupulously**, at a mission-critical bar (NASA/JPL Power of Ten,
JSF AV C++, MISRA C++:2023). It is a base rule for every new addition; existing deviations are being migrated.

1. **RAII for every resource** (R.1) — memory, native handle, lock, registration, thread, asynchronous job. The
   destructor releases / stops / joins it. No naked `new`/`delete`, no ownership through a raw pointer (R.11, I.11).
2. **Self-contained lifetime** — a class owns, stops and waits for every effect it starts. It never relies on another
   object to drain, cancel or order its shutdown: no global cancel flag, no extra pool drain elsewhere, no
   member-declaration-order luck.
3. **A deferred lambda never captures a raw `this` (nor `[&]`)** unless its object holds that job's handle and its
   destructor stops and waits for it; otherwise it captures values, `shared_ptr` or `weak_ptr`.
4. **Cancellation is `std::stop_source` / `std::stop_token`** (C++20), not a hand-made `std::atomic_bool *`; a long
   computation checks its token at a bounded interval in every stage.
5. **Threads follow the `std::jthread` model** (CP.25, CP.26) — stop request + join at destruction, never `detach()`;
   a self-join is a contract fault (§ 4), never a silent fallback.
6. **Constructor = valid object, destructor = everything released** (C.41, C.31); rule of zero, else a complete rule
   of five (C.20, C.21); every member initialized; thread members declared after the state their thread uses.
7. **Locks only through guards** (CP.20); never notify after releasing a lock the destructor could race.
8. **A registration is an object** — registering returns a move-only token whose destructor unregisters.
9. **No accepted undefined behaviour** — a tolerated dead `this`, a timeout-and-abandon or an "unlikely" race is a
   defect; every failure path exits cleanly, and is tested.
10. **A `switch` on an enum that lists every enumerator has NO `default:`** (owner decision D14, 2026-10-08), so `-Wswitch`
    breaks the build when an enumerator is added; what the old default did (a refusal, a safe return) sits AFTER the
    switch, because a cast can still bring an out-of-range value. This holds for the cascade's enums AND for a
    third-party enum the switch fully covers (a CEF upgrade adding an enumerator must be reviewed, not defaulted);
    a switch that does not cover a large external enum (`VkFormat`, `VkResult`) keeps its `default:`.

clang-tidy keeps `cppcoreguidelines-owning-memory`, `-special-member-functions`, `-pro-type-member-init` and
`-no-malloc` at zero new finding on the touched TUs (`docs/clang-tidy-ledger.md`).
