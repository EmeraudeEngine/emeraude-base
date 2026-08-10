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

### Clang `-Wunused-lambda-capture`: never capture a `constexpr` local — Debug-only breakage

`ShapeGenerator.hpp` captured its `constexpr` locals (`half`, `one`, `zero`, `twoPi`) in the
per-shape `volumetricColor` / `sphericalUV` / `computeFaceUV` lambdas. Clang rejects that with
**`-Werror,-Wunused-lambda-capture`** — "lambda capture 'half' is not required to be captured for
this use": a variable usable in a constant expression needs no capture, so the capture is dead.
31 lambdas were affected across the file (fixed Aug 2026).

**Why it went unnoticed:** the warning comes from `-Wall`/`-Wextra`, which the consumer's **Debug**
configuration adds and its **Release** configuration does not. A Release-only habit hides this class
of breakage entirely; the whole Debug build of the cascade was broken (`ResourceGenerator.cpp`,
`GeometryDataPrinter.cpp`) while Release stayed green.

**The nuance that matters — capture is NOT always removable.** Only drop the entries the compiler
actually flags. A `constexpr` local still needs capturing when the body **binds a reference** to it:

```cpp
constexpr auto one = static_cast< vertex_data_t >(1);

/* 'one' MUST stay captured: std::clamp takes const T &, which odr-uses it. */
const auto sphericalUV = [one](const Math::Vector< 3, vertex_data_t > & v) {
    const auto latitude = one - (std::acos(std::clamp(v[Math::Y], -one, one)) / std::numbers::pi_v< vertex_data_t >);
    /* … */
};
```

Removing `one` there yields a hard error, not a warning: *"variable 'one' cannot be implicitly
captured in a lambda with no capture-default specified"*. Clang's own diagnostics are the authority
on which entries are dead — never sweep a capture list by hand.

**Rule:** do not capture `constexpr` locals. Capture only non-`constexpr` locals (`invRadius`,
`invExtent`, …) and any `constexpr` whose address or reference the body actually takes.

### `numeric_limits< T >::max()` as a float divisor — cast it explicitly

`ColorFromInteger()` divided by `std::numeric_limits< input_t >::max()` and let the conversion to
`output_t` happen implicitly. For a wide `input_t` the exact maximum is **not representable**, so
clang rejects the silent rounding: **`-Werror,-Wimplicit-const-int-float-conversion`**, "changes
value from 4294967295 to 4294967296" (and the `uint64_t` equivalent). Only
`test_PixelFactoryColor.cpp` instantiates those wide types, so the whole unit suite failed to build
on macOS/clang while the library itself compiled (fixed Aug 2026).

**Fix:** hoist the divisor into a `constexpr auto scale = static_cast< output_t >(…::max())`. The
rounded divisor is bit-for-bit what the implicit conversion produced — the cast only states the
intent — and the expression is evaluated once instead of four times.

**Rule:** never let an integer maximum reach a floating-point division implicitly. Cast at the
source, where the precision loss is a deliberate, reviewable decision.

## VertexFactory

### `exceedsStream()` is a PRE-READ bound — using it after the parse rejects every valid file (Aug 2026)

> [!CRITICAL]
> `FileFormatMDx::exceedsStream(file, count)` answers *"could this stream possibly hold `count`
> elements?"* by comparing the count to the **remaining file size**. It is a cheap DoS guard for a
> count that has just been read from the header and is **about to drive a read** — its only valid
> position is the parsing phase.
>
> Two calls in `loadMD5()` had drifted into the **post-parse** phases (vertex→weight conversion,
> skin build). By then the whole file has been consumed: the stream carries `eofbit`, `tellg()`
> returns **-1**, and the `current < 0` clause makes the guard answer **"exceeds" unconditionally**.
>
> **Effect:** `cyberdemon.md5mesh` — and every MD5 model owning a vertex with more than four
> weights, or simply owning a skeleton — was rejected with
> `readStream(), weight count exceeds the stream size !`. The file was perfectly valid.
> `animation-debug --demo-options 1` and the `ID/cyberdemon` store resource were both dead.
>
> **Both guards were also REDUNDANT**: the validation phase that runs right after parsing already
> checks the real invariants on the parsed data — joint parents, `weight.jointIndex < jointCount`,
> `startWeight + countWeight <= weights.size()`, triangle→vertex indices. That phase is what makes
> the later allocations safe; the stream has nothing to say about them. Both calls were removed,
> not replaced.
>
> **The rule:** an `exceedsStream()` call belongs where a count is read from the stream and drives
> the very next read. Anywhere downstream, the invariant to check is a **container size**, not a
> file size. A guard on an already-parsed, in-memory count is at best noise and at worst — as here
> — a permanent rejection.
>
> Regression test: `VertexFactoryMDx.md5VertexWithMoreThanFourWeightsLoads`
> (`src/Testing/test_VertexFactoryFileFormats.cpp`) — a synthetic MD5 whose first vertex declares
> five weights; asserts the load succeeds AND that the four largest biases survive, renormalized.

### ⚠️ `MemoryStream{buffer}` on a NON-const vector opens for WRITING

> [!WARNING]
> `IO::MemoryStream` has two constructors: `MemoryStream(const std::vector< std::byte > &)` reads,
> `MemoryStream(std::vector< std::byte > &)` writes into the vector. A test that builds its input
> in a **mutable** local and passes it directly picks the WRITE overload — every subsequent read
> fails with `failed to read stream data !`, **before the parser is ever reached**.
>
> A test asserting `EXPECT_FALSE(format.readStream(...))` then passes for entirely the wrong
> reason: it never exercised the loader. Bind the buffer to a `const` reference (or go through
> `StreamIO::read()`, which takes a `const &`) and check the log for that message before trusting
> a negative-path test.
>
> **Measured, not guessed**: run the whole suite and correlate each `[ RUN ]` with
> `failed to read stream data !` / `stream is not open !`. That sweep found **three** vacuous
> tests — `md3HostileSurfaceDoesNotCrash`, `md5HostileReferencesDoNotCrash`,
> `md3HugeTriangleTotalDoesNotOOM` — all three now **activated** (buffer bound to a `const &`).
> The other hits are legitimate: `NetworkTLSConnection.*` and the two `emptyStreamIsRejected`
> tests exercise those error paths ON PURPOSE.
>
> **What the activation revealed** (Aug 2026):
> - `md3HostileSurfaceDoesNotCrash` and `md3HugeTriangleTotalDoesNotOOM` now pass **for the right
>   reason** — `loadMD3(), triangle total exceeds the stream size !`. The MD3 hardening was sound;
>   it had simply never been executed. No 64 GB allocation.
> - `md5HostileReferencesDoNotCrash` **FAILS**: see below.

### `loadMD5()` used to return SUCCESS on a shape it built from nothing (Aug 2026, FIXED)

> [!WARNING]
> `loadMD5()` ends on an unconditional `return true`. Fed the 136-byte hostile blob of
> `md5HostileReferencesDoNotCrash`, it parses no joint and no mesh, builds an **empty** shape
> (`Shape::computeTriangleTBNSpace(), geometry data is empty !`), attaches a 0-joint skeleton and
> skin — and reports **success**.
>
> It no longer crashes (the validation phase did its job), but a loader that succeeds with zero
> vertices and zero triangles pushes an empty resource down the pipeline instead of cancelling.
> The test's authored intent — *"the validation pass must cancel the load"* — is the correct one.
>
> **Fix (owner decision: keep it LOCAL to `loadMD5()`, do not generalise yet):** two
> post-conditions, both inside `loadMD5()`.
> 1. **Fail fast, end of Phase 1** — `joints.empty() || meshes.empty()` cancels the load before
>    any skeleton or geometry is built. A real MD5 always carries a skeleton and at least one
>    mesh. This also spares the caller the misleading `geometry data is empty !` TBN warnings the
>    old path emitted on its way to reporting success.
> 2. **Post-condition on the result** — `geometry.empty()` (no triangle) cancels the load. Meshes
>    can parse and still yield nothing; success on an empty shape is a false success.
>
> **Deliberately NOT done**: the same post-condition shared across MDL/MD2/MD3/MD5 (or hoisted
> into `FileFormatInterface`). That is a **contract** change, not an implementation detail — a
> format may legitimately describe an empty geometry, and no engine code relies on that today.
> Left as an open architectural question rather than decided silently.
>
> **Verified**: suite back to `1967/1967`, and the real `cyberdemon.md5mesh` still loads with its
> skeleton and 10 animation clips — the new checks reject garbage without touching valid models.

## PixelFactory

### `Pixmap< pixel_data_t >` is **not** byte-typed — never `memset`/`memcpy` an element count

`Pixmap` is `template< typename pixel_data_t = uint8_t, … > requires std::is_arithmetic_v< pixel_data_t >`.
The default instantiation is `uint8_t`, and for **one byte per element only**, a byte-oriented
`std::memset(data, value, …)` happens to produce the right result and an element count happens to
equal a byte count. Both coincidences break for every other `pixel_data_t`.

HDR made this live: `FileFormatHDR` (RGBE `.hdr`) instantiates **`Pixmap< float, uint32_t >`**
— today from `emeraude-engine`'s `Graphics/CubemapResource.cpp` (equirectangular HDR → cubemap)
and from `Testing/test_PixelFactoryFileFormats.cpp`. MSVC surfaced it as a hard error, `/W4 /WX`:

```
Pixmap.hpp(2349): warning C4244: 'argument' : conversion de 'pixel_data_t' en 'int' (float)
  → fillChannel → initAlphaChannel → initialize → FileFormatHDR::readStream
```

That warning was **not cosmetic**. `std::memset(m_data.data(), 1.0F, bytes)` truncates the value to
`int` 1 and then smears the *byte* `0x01` over the buffer, so an alpha channel initialised to "one"
became `0x01010101` ≈ `2.4e-38F` — a silently black/transparent HDR image, not a compile nit.

**Rules**
- Filling a value: `std::fill(m_data.begin(), m_data.end(), value)`. It is not slower — the compiler
  lowers it to `memset`/vector stores when the element type permits. `<algorithm>` is included for it.
- Copying: a byte count is `count * sizeof(pixel_data_t)`. An element count is not a byte count.
- Use `Pixmap::one()` / `Pixmap::zero()` for the channel extremes; `one()` is `1` for floating-point
  types and `numeric_limits::max()` for integers, so a literal `1` is wrong for `uint8_t`.

Fixed sites: `fill(pixel_data_t)` (Grayscale/RGB branch), `fillChannel(Channel, pixel_data_t)`
(Grayscale branch — which additionally forgot `markEverythingUpdated()` before its early return),
and `zeroFill()` for consistency.

### `fill(const pixel_data_t * data, size_t size)` — the tiling was wrong four ways

The same byte-vs-element confusion, plus arithmetic bugs, in the Grayscale/RGB branch. All four are
fixed; recorded here because the *intended* semantics were never written down anywhere:

| Bug | Effect |
|-----|--------|
| `memcpy(…, m_data.size())` | byte count given an **element** count — copied ¼ of the buffer for `float` |
| `std::ceil(m_data.size() / size)` | integer division happens *before* `ceil`, so `ceil` was a no-op — last partial tile lost |
| `memcpy(…, data + shift, …)` | re-read the **source** at the destination offset — out of bounds past the first tile |
| `remain = m_data.size() % size` | `0` when `size` divides evenly → last tile copied nothing |

**Intended semantics, now implemented:** the source pattern repeats **cyclically from `data[0]`**
until the pixmap is full, the final tile being truncated to what is left. This matches what the
`GrayscaleAlpha` / `RGBA` branches of the same function already did element-wise, so the function is
now internally consistent. Both this overload and `fillChannel(Channel, const pixel_data_t *, size_t)`
also reject `data == nullptr` / `size == 0` up front — `size == 0` previously drove the interleaved
branches into an endless `data[0]` read.

No caller in the cascade uses these pointer overloads today (every engine site calls
`fill(const Color &)`), so the semantic correction carries no regression risk.