---
id: processor-move-int32-min
title: Processor::move() takes std::abs() of a possibly INT32_MIN direction
status: open
priority: high
scope: src/PixelFactory/Processor.hpp
opened: 2026-10-08
tags: [ave-robustus-ii, pixelfactory, defect, ub]
---

# Processor::move() takes std::abs() of a possibly INT32_MIN direction

## Why
`Processor::move()` computes `std::abs(xDirection)` / `std::abs(yDirection)` on `int32_t`: `INT32_MIN` overflows (UB).
`shiftTextArea()` got the 64-bit magnitude fix on 2026-10-08; `move()` did not.

## What remains
- Same 64-bit magnitude as `shiftTextArea()`; a test with `INT32_MIN` / `INT32_MAX` under UBSan.

## References
- Found by the Ave Robustus II warning pass (2026-10-08, projet-alpha `docs/plans/ave-robustus-ii.md`); not raised by a warning, so left for its own fix.
