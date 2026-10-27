## Math: rigid-body helpers (physics overhaul P1, 2026-10-01)

`src/Math/RigidBody.hpp`, namespace `EmEn::Base::Math::RigidBody`. Needed by the engine's rotational physics (engine
item `rotational-physics`, closed 2026-10-02): until P3 the inertia tensor defaulted to the identity whatever the mass
and the shape. The engine now derives it from the collision shape through these functions (engine
`docs/subsystems/physics/16-rigid-body-rotation.md`).

| Function | Returns |
|---|---|
| `solidBoxInertia(mass, fullSize)` | diag(m (h² + d²)/12, m (w² + d²)/12, m (w² + h²)/12) |
| `solidSphereInertia(mass, radius)` | 2/5 m r² on every axis |
| `solidCylinderInertia(mass, radius, height)` | along Y: m r²/2 about Y, m (3 r² + h²)/12 across |
| `solidCapsuleInertia(mass, radius, cylinderHeight)` | along Y, the mass split by volume between the cylinder and the two caps (each cap's centre 3 r / 8 beyond the cylinder's end) |
| `parallelAxis(inertia, mass, offset)` | I + m (\|d\|² E − d dᵀ) |
| `compoundInertia(parts)` | a compound body (2026-10-27): the total mass, the centre of mass (the masses' weighted mean) and the tensor about it — every part's tensor moved there by `parallelAxis()` first; the parts' centres and tensors in the BODY frame (a part turned in the body brings R I Rᵀ). Refused (std::nullopt): no part, a negative or non-finite mass, a non-finite centre or tensor element, a total mass of 0 |
| `skewSymmetric(v)` | [v]× (so that [v]× u = v × u) |
| `integrateOrientation(q, worldω, dt)` | exp(ω dt) ∘ q, renormalised — the WORLD-space rotation |

- The inertia functions answer `std::optional`: a negative or non-finite input is REFUSED (std::nullopt); zero is valid
  (a massless or a point body). A wrong tensor would spin a body silently.
- `integrateOrientation()` applies the rotation on the LEFT (world space). Applying a world ω in the body's local
  space is exactly the defect the bench proved in the engine's `Node::rotateFromPhysics()`; the unit test
  `MathRigidBody.integrationAboutAWorldAxisKeepsTheAxis` is the base-level form of the bench's `BenchSpinner`.
- Fixed on the way: `Quaternion::setFromScaledAxis()` computed the half angle as `theta / 2.0`, promoting the float
  path to double and narrowing the sine and cosine back (-Wfloat-conversion, MSVC C4244 under /WX) — the first float
  instantiation is `integrateOrientation()`.

- Fixed on the way (P2): `Quaternion::toAngleAxis()` answered 0 for a small rotation (`2 acos(w)` with `w` rounding to 1
  in float); now `2 atan2(|v|, w)` (`docs/caution-points.md` § Math).

### Tests (`src/Testing/test_MathRigidBody.cpp`, 9)
The closed forms (box, the collision-debug cube 6.667, sphere, cylinder), the capsule's limits (no cylinder = a sphere,
a thin one = a rod m h²/12), refused inputs, the parallel-axis theorem (a point mass, a product of inertia), the skew
matrix vs the cross product, 10 000 world-axis steps keeping the axis and the angle, ω = 0 leaving the orientation,
a 1e-5 rad rotation keeping its angle through `toAngleAxis()`.

Added 2026-10-27 (owner: implement the P1 leftovers, consumer or not): `compoundInertia()` (tests
`aBoxSplitInTwoHalvesIsTheWholeBox` — two halves give exactly the whole box's tensor — `aCompoundOfTwoPointMassesTurnsAboutItsCentre`,
`aCompoundRefusesWhatHasNoCentre`), and `Quaternion(const Matrix< 3 > &)` / `operator=(const Matrix< 3 > &)`: the 3 × 3
rotation is the upper-left block of the 4 × 4 one `Quaternion(Matrix< 4 >)` reads, embedded and converted by that ONE
path (tests `MathQuaternion.FromMatrix3`, `FromMatrix3IsFromMatrix4`: bit-identical on every branch of the conversion).
The engine still derives an entity's tensor from its ONE collision shape (engine `docs/subsystems/physics/16-rigid-body-rotation.md`);
a compound collision model is where `compoundInertia()` comes in.
