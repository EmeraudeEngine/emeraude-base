## Critical Attention Points

- **MDx is read-only by design** — do not add a write path; document the boundary instead.
- **StreamIO needs an explicit `FileFormatType`** — a memory buffer has no extension; never try to
  content-sniff text formats (OBJ/ASCII-STL have no reliable magic).
- **Untrusted counts**: any new parser MUST validate header-derived counts against the actual
  stream size before allocating (the Tier-1 vuln above). Add a fuzz target under `src/Fuzzing/`.
- **Never change `ShapeVertex` or `ShapeTriangle` without bumping `FileFormatNative`'s version.**
  The payload is a raw blob of `sizeof(...)`; the count validation can PASS on a wrong stride, so
  the corruption is silent. Version 2 (2026-08-28) is the tangent-handedness layout; there is
  deliberately no version-1 read path — the format had no users yet (owner decision), and a loud
  refusal beats a plausible misparse.
- **`setTangent(Vector<4>)`'s W is the handedness, not a homogeneous coordinate.** Dropping it
  compiles, renders, and is wrong only on mirrored UVs — the worst kind of silent defect.
- **`sin(π)` and `cos(π/2)` are not 0 in float** (-8.74e-8 and -4.37e-8). A generator that computes
  its pole or equator ring by trigonometry puts the pole vertices on a ~1e-7 ring: a hole invisible in
  a render and to any exact-position weld. Set the extreme rings exactly (`generateSphere` -Y pole,
  `generateHemisphere` pole and equator, `generateCapsule` both poles, fixed 2026-09-28). Pinned by
  `VertexFactoryShapeGenerator.polesLieExactlyOnTheAxis`.
