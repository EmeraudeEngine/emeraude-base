## Open Axis-B markers (not yet done)

Tracked in `docs/plans/ave-robustus.md` (§6, "Real correctness gaps"): all resolved.
- `ShapeBuilder.hpp:636` — `FIXME: Check this` verified correct (TriangleFan vertex shift) + test.
- `TriangleGenerator` — the unused `generateEnvelope` ("bad algorithm") generator was **removed**
  as dead code (no caller anywhere); the whole `TriangleGenerator.hpp` is gone.
- `ShapeDecimator` arithmetic was audited clean in A.4 (`width*height` already 64-bit).
(The inventory's `OrientedCuboid` marker lives in `Math`, not VertexFactory — also resolved.)
