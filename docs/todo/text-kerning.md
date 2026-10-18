---
id: text-kerning
title: TextProcessor applies no kerning between glyph pairs
status: open
priority: unranked
scope: src/PixelFactory/Font.cpp, src/PixelFactory/TextProcessor.hpp
opened: 2026-10-08
tags: [pixelfactory, text]
---

# TextProcessor applies no kerning between glyph pairs

## Why
Since 2026-10-18 the pen advances in pixels, by each glyph's width (plus the array's `letterSpacing()` for a cropped
pixmap glyph). Pairs such as "AV", "To", "LT" keep the gap of their cells: no pair adjustment exists. Split from
`text-processor-proportional-advance` (done: base `caution-points.md` § the text advances in pixels).

## What remains
- TrueType: read the pair adjustments (FreeType `FT_Get_Kerning`, the font's `kern` or GPOS table) at load time and
  store a pair table beside the glyph array (Latin-1: at most 256 x 256 entries, sparse).
- `TextProcessor` adds the pair's adjustment to the advance; the word wrap measures with it.
- A pixmap font has no kerning data: nothing changes for it.
- Not a defect: an improvement waiting for the owner's ranking.

## References
- `src/PixelFactory/Font.cpp` `readTrueTypeFile()` (cells as wide as their advance).
