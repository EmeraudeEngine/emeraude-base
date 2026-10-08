---
id: processor-swap-buffer-wrong-units
title: Processor swap buffer and row strides are in the wrong units for non-8-bit pixmaps
status: open
priority: high
scope: src/PixelFactory/Processor.hpp
opened: 2026-10-08
tags: [ave-robustus-ii, pixelfactory, defect]
---

# Processor swap buffer and row strides are in the wrong units for non-8-bit pixmaps

## Why
`Processor::prepareSwapBuffer()` sizes a `std::vector< pixel_data_t >` with `bytes()` (elements × `sizeof`), `swapBuffers()`
checks against `bytes()`, and `move()` / `shift()` / `shiftTextArea()` use `pitch()` (BYTES) as an ELEMENT stride. For a
`pixel_data_t` wider than one byte (float, uint16_t) the swap buffer is `sizeof(T)` times too large and every offset is
in the wrong unit: the result is wrong, and an offset can run past the data. Only `uint8_t` is correct.
Also: `prepareSwapBuffer()` does not clear a buffer that already has the right size, so the areas `move()` /
`shiftTextArea()` do not overwrite keep the previous swap's pixels.

## What remains
- One unit (elements) through the five functions; clear the swap buffer.
- Tests with `Pixmap< float >` and `Pixmap< uint16_t >` (move, shift, shiftTextArea), ASan.

## References
- Found by the Ave Robustus II warning pass (2026-10-08, projet-alpha `docs/plans/ave-robustus-ii.md`); not raised by a warning, so left for its own fix.
