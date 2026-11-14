## Math: halfToFloat() — an IEEE 754 binary16 decoded into a float (2026-11-14)

`src/Math/Base.hpp`, `EmEn::Base::Math::halfToFloat(uint16_t bits)`: the 16 bits of a half-float (the storage of the
`*_SFLOAT` 16-bit image formats) decoded into a `float`. `constexpr`, `noexcept`, no loop and no table.

- **Exact for each of the 65 536 patterns**: binary16 is a subset of binary32. A signed zero keeps its sign; a
  subnormal half (mantissa × 2^-24) becomes a NORMAL float (its leading bit is found with `std::countl_zero()`); an
  infinity stays one; a NaN keeps its sign and its payload, in the upper mantissa bits (a quiet NaN stays quiet).
- **Why it lives here**: the engine carried four hand-written copies (the tone-mapping read-back, the ocean's FFT
  read-back, two debug dumps of cubemaps). Two were complete; the two debug ones ignored the sign, the subnormals and
  the infinities. A fifth caller (the read-back of an HDR render target) was the cue for one tested function (owner
  decision 2026-11-14).
- **A NaN is returned, never filtered**: a caller that cannot use one tests `std::isfinite()` itself (a NaN in a
  rendered image is information).
- There is no `floatToHalf()`: nothing in the cascade encodes a half on the CPU. Add it with its own exhaustive test
  the day a caller exists (the rounding mode is then a decision).

**Tests** (`test_MathBasics.cpp`): `Math.halfToFloatKnownValues` (thirteen bit patterns written by hand: the zeros,
1, -2, the half nearest to 1/3, the largest finite, the smallest normal, the two ends of the subnormal range, the
infinities, NaN), `Math.halfToFloatIsConstantEvaluated` (`static_assert`), `Math.halfToFloatEveryPattern` (the 65 536
patterns against the value built with `std::ldexp()`, compared bit for bit).
