---
id: font-freetype-leak-on-early-return
title: FreeType library and face leaked on two early returns of Font loading
status: open
priority: high
scope: src/PixelFactory/Font.cpp
opened: 2026-10-08
tags: [ave-robustus-ii, raii, defect]
---

# FreeType library and face leaked on two early returns of Font loading

## Why
`PixelFactory/Font.cpp:63` returns `false` after `FT_New_Face()` fails without `FT_Done_FreeType(library)`; `:72` returns
after `FT_Set_Pixel_Sizes()` fails without `FT_Done_Face()` / `FT_Done_FreeType()`. Every failed font load leaks a
FreeType library (and a face). Ave Robustus II rule 1: a native handle lives in an RAII holder.

## What remains
- Hold `FT_Library` and `FT_Face` in `std::unique_ptr` with deleters (or a small handle class) — every return path
  releases by construction.
- Test (failing-then-passing): a corrupt / truncated font file and an impossible pixel size; LSan clean.

## References
- projet-alpha `docs/plans/ave-robustus-ii.md` § 3.1 H4.
