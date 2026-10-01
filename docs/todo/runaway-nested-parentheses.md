---
id: runaway-nested-parentheses
title: Expressions wrapped in dozens of nested parentheses by an old automated fix
status: open
priority: unranked
scope: Math/Matrix, Math/Space3D/SAT, PixelFactory/Color, PixelFactory/Processor, Algorithms/PerlinNoise, VertexFactory/Shape, VertexFactory/TextureCoordinates
opened: 2026-10-01
tags: [cleanup, readability]
---

# Expressions wrapped in dozens of nested parentheses by an old automated fix

## Why

Found in the engine triad 11 (2026-10-01). `Math/Space3D/Collisions/SamePrimitive.hpp` computed `denom` as `a * e - b * b`,
but wrapped in about 90 levels of parentheses. That is probably a fix-it applied again and again (for example
`readability-math-missing-parentheses`, once per translation unit that includes the header); nobody verified it. It was there when the base
was extracted (`26c9cbe`). Triad 11 rewrote that line as `(a * e) - (b * b)`; its value was unchanged.

21 lines in 7 files still have 10 or more nested parentheses: `Math/Space3D/SAT.hpp` 5, `Math/Matrix.hpp` 4,
`PixelFactory/Color.hpp` 8, and one each in `PixelFactory/Processor.hpp`, `Algorithms/PerlinNoise.hpp`,
`VertexFactory/Shape.hpp` and `VertexFactory/TextureCoordinates.hpp`. Each value should be right, but such a line cannot
be reviewed.

## What remains

- [ ] Rewrite each line with the parentheses its precedence needs. Check each one: evaluate the old and the new
  expression on sample values (as triad 11 did), and run the unit tests of the touched classes (Release + ASan).
- [ ] Find `((((((((((` with `/usr/bin/grep -rn` (ugrep skips the repository): 0 lines left.

## ⚠️ Traps

- A clang-tidy `--fix` run over several TUs applies a header fix-it once per TU. Run header fixes from ONE TU, or
  review the diff of every header after a mass fix (`docs/clang-tidy-ledger.md`).

## References

- projet-alpha `docs/plans/triad-engine-pass-report.md` (per-section record) § 11.
