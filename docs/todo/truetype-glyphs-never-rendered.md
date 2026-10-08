---
id: truetype-glyphs-never-rendered
title: Font::readTrueTypeFile() never renders a glyph, so a TrueType font never loads
status: open
priority: high
scope: src/PixelFactory/Font.cpp, src/Testing/test_PixelFactoryTextPixmap.cpp
opened: 2026-10-08
tags: [pixelfactory, defect]
---

# Font::readTrueTypeFile() never renders a glyph, so a TrueType font never loads

## Why
Found during the Ave Robustus II P0 fix of the FreeType leak (2026-10-08). The glyph callback of
`Font::readTrueTypeFile()` loads each glyph with FreeType and then always returns an EMPTY `Pixmap`: the whole
rendering code (copy of `face->glyph->bitmap`, vertical offset, widest-char bookkeeping) is commented out.
`ASCIIGlyphArray::writeGlyphData()` refuses an invalid glyph, so `Font::readFile()` on any `.ttf` answers `false`.
The two TrueType tests of `test_PixelFactoryTextPixmap.cpp` are commented out, which is why nothing caught it.

## What remains
- Owner: is TrueType support wanted in the base (the engine's `FontResource` may only use pixmap fonts — see engine item
  `merge-font-pixelfactory-fontresource`)? If yes: implement the glyph copy (grayscale bitmap → `Pixmap`), the
  baseline offset and the fixed-width pass; re-enable the two tests; hostile fonts (truncated, huge sizes).
- If no: remove the TrueType path and FreeType from the base.

## References
- projet-alpha `docs/plans/ave-robustus-ii.md` § 5 (P0).
