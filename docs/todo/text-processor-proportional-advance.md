---
id: text-processor-proportional-advance
title: TextProcessor lays every font on a fixed grid, so a proportional font looks letter-spaced
status: open
priority: unranked
scope: src/PixelFactory/TextProcessor.hpp
opened: 2026-10-08
tags: [pixelfactory, text]
---

# TextProcessor lays every font on a fixed grid, so a proportional font looks letter-spaced

## Why
`TextProcessor::blitCharacter()` places character N of a row at `N × widestChar()`, and the word wrap counts characters
against `maxColumns = areaWidth / widestChar()`. Every font is laid out as a monospace one. With a TrueType font read
with `fixedWidth = false` (each cell as wide as its glyph's advance, since 2026-10-08), the text is readable but every
narrow letter sits in the widest letter's slot: wide gaps (seen on `PixelFactoryTextProcessor.write`'s output,
`resources/assets/tmp_textPixmap.png`, the green block). A pixmap font cropped with `fixedWidth = false` has the same
look.

## What remains
- Advance by each glyph's own width (the cell width) instead of `widestChar()`; the word wrap measures words in
  pixels, not in characters.
- Optional: kerning (FreeType `FT_Get_Kerning` / the font's `kern` or GPOS table) — would need a pair table stored
  beside the glyph array.
- Not a defect: an improvement waiting for the owner's ranking.

## References
- `src/PixelFactory/Font.cpp` `readTrueTypeFile()` (cells as wide as their advance).
