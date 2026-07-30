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