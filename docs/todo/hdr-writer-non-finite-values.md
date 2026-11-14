---
id: hdr-writer-non-finite-values
title: The Radiance HDR writer converts a non-finite or huge component to a byte — undefined behaviour, or a wrong pixel
status: open
priority: high
scope: PixelFactory/FileFormatHDR.hpp (writeStream)
opened: 2026-11-14
tags: [defect, pixel-factory, hdr, undefined-behaviour, owner-decision]
---

# The Radiance HDR writer converts a non-finite or huge component to a byte — undefined behaviour, or a wrong pixel

## Why

`FileFormatHDR::writeStream()` encodes each pixel as RGBE without looking at its values first (read 2026-11-14, not
run):

- **A NaN or an infinity** reaches `std::frexp()` (unspecified exponent for a non-finite argument), then
  `static_cast< uint8_t >(component * scale)` with a NaN product: a float-to-integer conversion whose value is not
  representable is undefined behaviour. `std::max(red, std::max(green, blue))` also keeps or drops a NaN depending on
  which channel carries it.
- **A finite component at or above 2^127** gets `exponent + 128 = 256`, which wraps to 0 in the exponent byte: the pixel
  is written, and reads back as a value near zero.

A float pixmap comes from anywhere (a rendered image read back from the GPU can hold a NaN): the writer is a boundary
and must not rely on its caller. The engine's render-target dump filters the non-finite values itself before it calls
the writer, so it does not depend on this.

## What remains

- **Owner decision — what the writer does with such a pixel** (an intent, not a detail): refuse the whole file (false
  and a log), or write the pixel as zero, or clamp to the largest RGBE value; and whether the count is reported.
- A failing-then-passing test in `test_PixelFactoryFileIO` style: a pixmap holding a NaN, +infinity, -infinity and
  `std::numeric_limits< float >::max()`, written under UBSan (`-fno-sanitize-recover=all` stops on the conversion).
- The reader's side for symmetry: an exponent byte of 255 decodes to values up to 2^127 — check the round trip at the
  top of the range.

## References

- `src/PixelFactory/FileFormatHDR.hpp` — `writeStream()`, the RGBE encoding block.
- G. Ward, "Real Pixels", *Graphics Gems II* (1991): the format has no encoding for a non-finite value.
