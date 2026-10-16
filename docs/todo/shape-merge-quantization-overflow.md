---
id: shape-merge-quantization-overflow
title: Shape's vertex-merge grid converts an out-of-range or non-finite coordinate to int64_t (undefined behaviour)
status: open
priority: high
scope: src/VertexFactory/Shape.hpp (quantizeScalar(), quantize(), addVertex(), addVertexColor(), restoreConstructionIndexes())
opened: 2026-10-16
tags: [ave-robustus, undefined-behaviour, vertex-factory, fuzzing, trust-boundary]
---

# Shape's vertex-merge grid converts an out-of-range or non-finite coordinate to int64_t (undefined behaviour)

## Why

Found by `fuzz_mdx` (2026-10-16, Linux, clang 19 libFuzzer + ASan/UBSan, base df64ecd + the P5 lot 2 MD3 header fix,
8 workers × 120 s, 28 M runs), in every worker:

```
src/VertexFactory/Shape.hpp:2708:35: runtime error: -4.53607e+29 is outside the range of representable values of type 'long'
```

`Shape::quantizeScalar()` is `static_cast< int64_t >(std::round(value / m_mergeTolerance))`. A float-to-integer
conversion whose value does not fit is undefined behaviour ([conv.fpint]); with the default tolerance (1e-4) any
coordinate beyond about ±9.2e14, and any infinity or NaN, reaches it. The values come from the loaders (MD3 here; every
format that builds through `addVertex()` / `addVertexColor()` or rebuilds the indexes in
`restoreConstructionIndexes()`): a trust boundary, so a hostile or broken file triggers it in Release.

Not a crash today on x86-64 (the conversion yields INT64_MIN), but the result is unspecified on other targets
(ARM64 saturates) and the merge keys then differ between the OS.

## What remains

Owner decision 2026-10-16: **both** forms below — `quantizeScalar()` never UB, and the loaders refuse a non-finite
coordinate. The options as they were presented:
- **refuse at the loaders**: every loader rejects a non-finite or absurdly large coordinate (a bound to choose), and
  `quantizeScalar()` documents it as `@pre` with a DEBUG check; or
- **saturate in `quantizeScalar()`**: a non-finite value maps to a reserved key (never merged), a finite value is
  clamped to the representable range before the conversion — no UB whatever the input, the loaders unchanged; or both.
- Then: a unit test with a huge and a non-finite coordinate (fails under UBSan before), `fuzz_mdx` and the other
  geometry fuzzers (`fuzz_obj`, `fuzz_stl`, `fuzz_native`) clean of this report.

## ⚠️ Traps

- UBSan in the fuzzers does not halt on this report (it prints once per location and keeps going): read the
  `SUMMARY:` lines of each `fuzz-N.log`, a clean exit code proves nothing.
- `fuzz-N.log` files land in the current directory: run the fuzzers from a scratch directory, not from the repository.

## References

- `src/Fuzzing/README.md` (build and run), `src/Fuzzing/fuzz_mdx.cpp`.
- projet-alpha `docs/plans/ave-robustus-ii.md` § 5.5, lot 2 (found on the way).
