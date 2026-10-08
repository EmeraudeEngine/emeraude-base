---
id: mdx-zero-skin-size
title: MDL / MD2 loaders divide by a zero skin size from the file
status: open
priority: high
scope: src/VertexFactory/FileFormatMDx.hpp
opened: 2026-10-08
tags: [ave-robustus-ii, loader, defect, hostile-input]
---

# MDL / MD2 loaders divide by a zero skin size from the file

## Why
A `skinwidth` / `skinheight` of 0 in an MDL / MD2 header divides by zero: inf / NaN texture coordinates enter the
geometry (untrusted input). The MD5 mesh path reads past a block when `numJoints` exceeds the joint lines (garbage, no
crash).

## What remains
- Refuse a zero skin size at the header check; bound the MD5 joint block like `MD5AnimParser` (2026-10-08);
  hostile-input tests.

## References
- Found by the Ave Robustus II warning pass (2026-10-08, projet-alpha `docs/plans/ave-robustus-ii.md`); not raised by a warning, so left for its own fix.
