---
id: fuzz-hdr-target
title: Add a fuzz_hdr target for FileFormatHDR (RGBE)
status: open
priority: unranked
scope: src/Fuzzing, src/PixelFactory/FileFormatHDR.hpp
opened: 2026-08-28
tags: [fuzzing, pixelfactory, hardening]
---

# Add a fuzz_hdr target for FileFormatHDR (RGBE)

## Why

`FileFormatHDR` is a **hand-rolled** parser (three scanline encodings: adaptive RLE, flat, legacy
RLE) reached at runtime by `emeraude-engine`'s `Graphics/CubemapResource.cpp` on untrusted asset
files. Every other hand-rolled image/geometry parser has a fuzz target — `fuzz_targa`, `fuzz_png`,
`fuzz_jpeg`, `fuzz_native`, `fuzz_stl`, `fuzz_mdx`, `fuzz_obj`. HDR does not: it was added by
`ceb83c2`, **after** the extended fuzzing campaign of `42a26bb`.

That gap has already cost one defect. The reader trusted its resolution line and allocated a
51.5 GB pixmap from a 40-byte stream (fixed 2026-08-28; the whole measurement, the `-fno-exceptions`
reason it was a process kill rather than a failed load, and the guard's trade-off are in
[`docs/caution-points.md`](../caution-points.md) § PixelFactory). It was caught by the unit suite
on a smaller host, not by design — `fuzz_targa` had found the *identical* defect in Targa a
campaign earlier.

## What remains

- [ ] `src/Fuzzing/fuzz_hdr.cpp` on the model of `fuzz_targa.cpp` (`MemoryStream` bound to a
      `const` buffer — see the vacuous-test trap in `docs/caution-points.md` § VertexFactory —
      then `FileFormatHDR< float, uint32_t >::readStream`).
- [ ] Declare it in `src/Fuzzing/build-fuzzers.sh` and in the target table of
      `src/Fuzzing/README.md`.
- [ ] Seed corpus: a real `.hdr` from `resources/`, one adaptive-RLE and one flat scanline, plus
      the hostile headers already covered by `test_PixelFactoryFileFormats.cpp`.
- [ ] Run to millions of executions under ASan + UBSan (`UBSAN_OPTIONS=halt_on_error=1`), fix
      every finding **with a regression test** ("no fix without a test"), and record the findings
      in the README's *Findings* list.

## ⚠️ Traps

- **The legacy-RLE path is where to look first.** `readScanline()`'s legacy branch shifts its
  repeat count 8 bits left per consecutive repeat record (`repeatShift += 8`), so a handful of
  bytes can drive a very long run. The bounds check (`x + count > width`) looks right; a fuzzer
  is what proves it, including at `dimension_t` overflow.
- **A fuzz target that OOMs the fuzzer is not a finding by itself** — check whether the guard is
  missing or whether libFuzzer's `-rss_limit_mb` is simply doing its job. Reproduce the way
  § PixelFactory of `docs/caution-points.md` prescribes (peak RSS + `ulimit -v`).
- Related: [`remove-invalid-noexcept`](remove-invalid-noexcept.md) — `Pixmap::initialize()` is
  declared without `noexcept` but *cannot* report an allocation failure under `-fno-exceptions`;
  a reader must bound the header itself.
