## PortableRandom — seeded numbers that are the same on every platform (2026-10-02)

`src/PortableRandom.hpp` (header-only, `EmEn::Base::PortableRandom`). Owner decision (2026-10-02): every SEEDED
draw of the cascade gives the same numbers on Linux, macOS and Windows.

### Why
The C++ standard fixes the sequence and the seeding of `std::mt19937` / `std::mt19937_64` (the 10000th draw of a
default `std::mt19937` is 4123659995), but NOT:
- what `std::default_random_engine` is (libstdc++ `minstd_rand0`, libc++ `minstd_rand`, MSVC `mt19937`);
- the algorithms of `std::uniform_int_distribution`, `std::uniform_real_distribution`, `std::shuffle` /
  `std::ranges::shuffle`.
Measured: citadel's seeded terrain (`PerlinNoise{1307}`, shuffled with `std::default_random_engine`) was three
different heightfields — macOS off Linux by up to 1.05 m, Windows by up to 0.45 m, at the same points.

### What it gives
- `FullRangeGenerator`: a generator over a full 32 or 64-bit range (`std::mt19937`, `std::mt19937_64`). The RANGE
  decides, not the result type (`std::mt19937::result_type` is `uint_fast32_t`, 64 bits on Linux).
- `draw32()`, `draw64()`: 32 / 64 uniform bits (a 32-bit generator: two draws for 64, the first one high; the draws
  are separate statements so their order is fixed).
- `uniformInteger< T >(generator, minimum, maximum)`: [minimum, maximum], unbiased — rejection of the 2^n mod range
  lowest draws, then a modulo (the method of OpenBSD's `arc4random_uniform()`; D. Lemire, ACM TOMACS 2019, as
  reference). 32-bit draws up to 32-bit types, 64-bit draws for 64-bit types; reversed bounds swapped; the full range
  takes the draw as is.
- `uniformReal< T >(generator, minimum, maximum)`: [minimum, maximum) — 24 high bits for a float, 53 bits from two
  draws for a double (Matsumoto & Nishimura's `genrand_res53()`, as reference); a result rounded up to the maximum is
  pulled below it; a width that overflows (−max to +max) interpolates; equal or non-finite bounds give the minimum.
- `shuffle(range, generator)`: Fisher-Yates from the end, each partner by `uniformInteger()` (32-bit draws while the
  range has at most 2^32 elements).
- `UniformReal< T >`, `UniformInteger< T >`: the same call shape as the std distributions (`distribution(generator)`),
  so a seeded call site changes its type only.

### Who uses it
`Randomizer` (every draw), `Algorithms::PerlinNoise` and `VoronoiNoise` (`std::mt19937{seed}` + `shuffle()`), the
engine's `IrradianceProbeVolume` (its ray rotation), projet-alpha's `forest` and `terrain` scatterings. The engine's
`CloudShapeResource` already had its own hash stream for the same reason. Left on std: the draws seeded by
`std::random_device` (cards, dice, audio noise and dither, the playlist shuffle): random anyway.

### ⚠️ Traps
- **Floating-point contraction**: clang fuses `a * b + c` inside ONE expression into an FMA by default
  (`-ffp-contract=on`, arm64), rounding once instead of twice. `uniformReal()` does one operation per statement so
  the mapping is the same everywhere. Code that only USES the numbers (a noise's interpolation) may still move by an
  ulp between platforms — never by the tenths a different permutation gives.
- Changing the generator, the order of the draws or a mapping changes every seeded world: the golden tests catch it.

### Tests
`src/Testing/test_PortableRandom.cpp` (9): the `std::mt19937` premise, GOLDEN integers / reals / shuffle (drawn on
Linux, must be the same on every OS: a failure is a portability defect, never a value to re-record), citadel's
Perlin seed, bounds and uniformity (60000 dice draws within 5 %), real edges, `Randomizer` and the distribution
objects on the same stream. Release and ASan / UBSan green.

### Validated (2026-10-02)
macOS M2 (arm64, libc++) and Windows (MSVC, RTX 3060 + AMD iGPU), base `b2ca695`: the 6 golden tests pass unchanged,
0 warning; citadel's `getGroundLevel()` at six probe points equals Linux to 1e-4 on both (before: up to 1.05 m off on
macOS, 0.45 m on Windows), the parked car at the same position; the bench bit-identical; 10 demos clean.
