---
id: tree-generator-skeleton-and-growers
title: Tree generator — the skeleton type and its two growers
status: open
priority: unranked
scope: src/VertexFactory
tags: [vegetation, procedural, vertexfactory]
opened: 2026-09-21
---

# Tree generator — the skeleton type and its two growers

## Why

`TreeGenerator` is a stub whose whole body is commented out and whose approach (one capped
cylinder merged per segment) cannot produce a usable mesh. See `src/VertexFactory/AGENTS.md`
§ *TreeGenerator / Grid / GridQuad* for the exact state.

**Owner decision (2026-09-21)**: separate a **skeleton** phase from a **skinning** phase, and
implement **both growers** — the parametric one and the space-colonization one — behind one
skeleton type. The skinning phase is the sibling item
`tree-generator-skinning-lod-and-wind-channels`.

The skeleton is what makes everything downstream possible: LOD chains re-skin it, imposters are
baked from it, physics capsules follow it, the wind hierarchy is its branch order, and leaves hang
off its attachment points. A generator that returns only a mesh throws all of that away.

## What remains

1. **`TreeSkeleton`** — a flat `std::vector` of segments: parent index, `Math::CartesianFrame`
   (origin + local +Y along the branch), length, start/end radius, branch order, arc length from
   the root, and the parametric position along the parent. Plus a leaf-attachment list (position,
   orientation, scale) and the bounding box. Flat and index-addressed, never a node graph with
   pointers.
2. **Parametric grower** (Weber & Penn, SIGGRAPH '95 — the Arbaro / Blender Sapling / SpeedTree
   lineage): per-level arrays (branch count, length, curve, split angle, down angle, taper). This
   is the one that carries recognisable species presets, and it serialises naturally to JSON like
   the parametric material library does.
3. **Space-colonization grower** (Runions, Lane & Prusinkiewicz, EGWNP 2007): attractor cloud
   inside a crown envelope, iterative growth toward unclaimed attractors. Needs a uniform grid or
   a kd-tree for the nearest-attractor query, or it is quadratic per iteration.
4. Seeding through `Base::Randomizer< float >` — **never** `std::srand`, which the stub used: it
   is process-global, so two generators cannot be reproducible independently.
5. No `std::cout` anywhere: the stub printed its progress. Use the base tracing facilities.

## Traps

- ⚠️ The world is **Y-UP** since Aug 2026. A branch frame's local **+Y** is the growth axis. The
  stub predates the flip and its rotation order was written for the old frame.
- ⚠️ A grower must be **deterministic for a given seed** across platforms: no dependence on
  `unordered_*` iteration order, no accumulation whose order changes with threading.
- ⚠️ `-fno-exceptions` everywhere: no `.at()`, no `std::stoi`, no throwing `std::thread`
  construction.

## References

- Weber & Penn, *Creation and Rendering of Realistic Trees*, SIGGRAPH '95.
- Runions, Lane & Prusinkiewicz, *Modeling Trees with a Space Colonization Algorithm*,
  Eurographics Workshop on Natural Phenomena 2007 —
  https://algorithmicbotany.org/papers/colonization.egwnp2007.large.pdf
- Palubicki et al., *Self-organizing tree models for image synthesis*, ACM TOG 28(3), 2009 —
  https://algorithmicbotany.org/papers/selforg.sig2009.html (the model that subsumes the second
  grower, considered and not retained for the first delivery).
