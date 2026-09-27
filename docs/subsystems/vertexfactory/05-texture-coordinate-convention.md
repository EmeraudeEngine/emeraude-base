## Texture coordinate convention (Y-up world)

`V = 0` is the **top row of the image** (Vulkan image origin is top-left). Pairing that with a
Y-up world gives the two rules every hand-authored generator must follow:

- **Vertical faces** (normal in the XZ plane): `V = 0` pairs with the **`+Y`** edge, `V = 1` with
  `-Y`. Reference: `generateQuad`.
- **Horizontal faces**: follow `generatePlane` — on a `+Y`-facing surface `U` grows with `+X` and
  `V` grows with `+Z`. The `-Y`-facing face of a closed shape is the same mapping with `V` negated.

> [!IMPORTANT]
> **`generateScreenQuad()` is the one deliberate exception, and it must stay that way.** It is a
> fullscreen NDC quad (`-1..1`, no options, no scale) for the post-processor and the overlay manager,
> whose source images are already in **screen space** — so its `V` pairs with **`+Y`**, the exact
> opposite of every world-space generator. It is not a `generateQuad` that someone forgot to migrate.
> Locked by `screenQuadPairsVWithPositiveYOnPurpose`, so a "harmonising" sweep over the `V` axis
> fails loudly instead of silently flipping the whole post-process chain and the entire overlay.

> [!CAUTION]
> **The V pairing is a defect class of its own, distinct from winding, from vertex coordinates and
> from declared normals.** The Y-up switch reversed the emission order of `generateCuboid`'s faces
> (winding) but left every `setTextureCoordinates` paired with the position it had in the Y-down
> era, so all six faces rendered **V-flipped** while compiling clean and passing the whole unit suite.
> No assertion on the **geometry** can see it — the shape, its normals and its winding are all
> correct, only the image is upside down. But an assertion on the **pairing** catches it outright,
> and there is now one: `test_VertexFactoryShapeGenerator.cpp` walks every vertex of a shape, keeps
> the vertical faces (`|normal.Y| < 0.5`) and requires `V = 0` above mid-height, `V = 1` below.
> Fixed and measured for both `generateCuboid` overloads (Aug 2026) — negating `V` on all six faces
> restores the pre-migration relationship, in which the up-facing horizontal face agreed with
> `generatePlane`. `generateHollowedCube` is deliberately excluded: its UVs are parameterised
> per beam (`U` = beam width, `V` = length/width ratio) and assembled through `ShapeAssembler`
> rotations, so its `V` is not tied to world `Y`.

> [!CAUTION]
> **A sphere is the one shape where a `V`-versus-`Y` probe cannot see a UV defect.** `generateSphere`
> carried TRANSPOSED coordinates until Aug 2026 — the accumulator named `U` advanced per STACK
> (latitude), the one named `V` per SLICE (longitude) — so every texture came out rotated a quarter
> turn. Comparing `V` against `Y` reads a flat 0.5 above and below the equator when `V` is really
> longitude: it looks like a symmetric shape, not a defect, which is how this survived a full audit.
> **The discriminator is a LATITUDE RING**: along one, longitude must sweep and latitude must hold.
> Transposed, the ring gives span `U` = 0 and span `V` = 1 — the perfect signature.
> Pinned by `sphereMapsUToLongitudeAndVToLatitude`. ⚠️ Not a Y-up residue: it predates the flip and
> depends on no axis sign.

### Sphere longitude: direction and seam

> [!IMPORTANT]
> **U grows EASTWARD, and the seam sits on `+Z`.** With north at `+Y`, east is the POSITIVE rotation
> about `+Y` (the right-hand rule — which is why the Earth turns counter-clockwise seen from above
> the north pole). Measured: `U = 0` and `U = 1` both fall on `+Z`, `U = 0.5` on `-Z`. On an
> equirectangular map the U edges are the ANTIMERIDIAN and `U = 0.5` is the prime meridian, so
> Greenwich faces `-Z`, the engine's FORWARD, and the seam falls mid-Pacific where cartographers
> already put it so it cuts no land.
> Pinned by `sphereUGrowsEastwardNotWestward` and `sphereSeamSitsOnPositiveZ`.

> [!CAUTION]
> **`generateSphere` parameterises theta the OTHER WAY, on purpose-looking but load-bearing detail.**
> It uses `sTheta = -sin(theta)`, so a growing theta walks `+Z -> -X -> -Z -> +X`: the NEGATIVE
> rotation about `+Y`. Pairing U with a growing theta therefore grows it WESTWARD and MIRRORS every
> texture. U is deliberately run backwards against theta to compensate. Do not "tidy" that away.
> ⚠️⚠️ **A polar screenshot cannot catch this.** A mirrored globe still converges cleanly at the pole
> and still shows a plausible Arctic; only the east-west handedness gives it away. This defect was
> declared fixed on the strength of exactly such a capture, and the owner caught it on screen
> afterwards. Judge handedness, never convergence.

> [!IMPORTANT]
> **Seam vertices are DUPLICATED** at the same position, one carrying `U = 0` and one `U = 1`. That
> is what stops U interpolating from 1 back to 0 across the last quad and squeezing the whole map
> into one slice. A generator sharing a single vertex there is broken even though its UVs look fine
> in isolation.
