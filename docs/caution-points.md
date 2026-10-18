# Caution Points — emeraude-base

> Cross-cutting pitfalls for the foundation library. Add a site here the moment the
> compiler, a platform, or a toolchain surprises you — the next AI session should not
> re-diagnose it from scratch.

## Build / Compiler

### GCC 14 `-Wstringop-overflow` / `-overread` false positives on `std::string`

GCC 14 raises a **`-Werror=stringop-overflow`** (or its sibling `-overread`) inside
`<bits/char_traits.h>` — `__builtin_memcpy writing/reading N bytes into a region of size 16` —
on perfectly valid `std::string` code. The `region of size 16` is the 15-byte **SSO buffer**
(+ NUL) of a `basic_string`. GCC's value-range analysis mis-judges that a string whose inferred
length exceeds SSO could still live in that inline buffer during a move-construct, and flags the
allocated-buffer `memcpy` as an overflow. The code is correct; the diagnostic is wrong.

The same GCC bug family has two known triggers. Base is a standalone repository (the engine is
*downstream*, so no link points up to it from here); a consumer that builds base inside the engine
cascade with PCH will also meet the first row below, documented in full on the engine side.

**Two triggers**

| Context | Warning | Trip-wire | Minimal fix that worked |
|---------|---------|-----------|-------------------------|
| `EMERAUDE_ENABLE_PCH=ON` (engine cascade) | `-overread` (read) | move-construct on `return` | **`reserve()`** — `+=` alone did *not* help |
| `_FORTIFY_SOURCE=2` + `-O2` (base Release policy) | `-overflow` (write) | temporary move-constructed inside an `operator+` chain | **`+=` on a named local** — the temporary disappears |

The base sites were `Network/TrustStore.cpp` (`std::string{"…"} + LinuxHashedCertsDirectory + "'."`)
and `Network/HTTPSClient.cpp` (`request += std::string{HTTPRequest::AcceptEncoding} + ": identity\r\n"`,
plus the sibling `Host` / `User-Agent` lines that had the same shape and would have tripped next).
Both are triggered by `_FORTIFY_SOURCE=2` (part of the base Release compile policy), amplified by
the PCH-shifted inlining context — GCC 14 + glibc 2.41.

**Wrong fixes** — the project never disables warnings:
- `-Wno-stringop-overflow` / `-Wno-stringop-overread` (global or per-file).
- `#pragma GCC diagnostic ignored`, `NOLINT`.
- Weakening `_FORTIFY_SOURCE` — it is useful hardening and the analysis is correct everywhere else.

**Correct fix — remove the SSO ambiguity at the source, escalating only as needed:**
1. Rewrite the `operator+` chain into `+=` on a **named local** `std::string`. This deletes the
   move-constructed temporary that GCC mis-analyses. Sufficient for the `_FORTIFY_SOURCE` /
   `-overflow` trigger.
2. If `+=` alone does **not** clear it (the `-overread`-on-`return` trigger), also `reserve()`
   past 15 bytes before appending, so the buffer is unambiguously heap-allocated. This is what
   the engine's `Saphir/LightGenerator.cpp` needed.

Neither step changes behaviour.

**Do not pre-emptively rewrite every concatenation in the library.** Only a handful of sites trip
this today. Fix each site as the compiler actually flags it — a broad sweep is churn with no
verification signal, and the trigger is context-dependent (PCH on/off, optimisation level).

### ⚠️ The Linux clang build of the cascade does NOT compile the base TESTS — syntax-check them with clang (2026-10-08)

projet-alpha's `.claude-build-clang` builds the libraries, not `EmeraudeBaseUnitTests`, so a clang-only finding in a test
reaches the macOS peer first. It did on base `320e8f9`: `static constexpr bool is_steady{false};` in a test clock inside
an anonymous namespace — libc++ never reads it, AppleClang breaks on `-Werror,-Wunused-const-variable`; GCC is silent.
Fixed by USING it (`static_assert(!ScriptedStatisticsClock::is_steady)` states the clock's point), not by
`[[maybe_unused]]`. **Before pushing a test change:** run `clang++ -fsyntax-only` on every touched test TU with the flags
of its `compile_commands.json` entry, GCC-only `-W…` options removed (`-Wduplicated-cond`, `-Wduplicated-branches`,
`-Wlogical-op`, `-Wdangling-reference`, `-Wsuggest-attribute=…`), `-Wno-deprecated*` removed, plus `-Wall -Wextra`.
Checked: it reproduces the macOS error on the faulty file.

### Clang `-Wunused-lambda-capture`: never capture a `constexpr` local — Debug-only breakage

`ShapeGenerator.hpp` captured its `constexpr` locals (`half`, `one`, `zero`, `twoPi`) in the
per-shape `volumetricColor` / `sphericalUV` / `computeFaceUV` lambdas. Clang rejects that with
**`-Werror,-Wunused-lambda-capture`** — "lambda capture 'half' is not required to be captured for
this use": a variable usable in a constant expression needs no capture, so the capture is dead.
31 lambdas were affected across the file (fixed Aug 2026).

**Why it went unnoticed:** the warning comes from `-Wall`/`-Wextra`, which the consumer's **Debug**
configuration adds and its **Release** configuration does not. A Release-only habit hides this class
of breakage entirely; the whole Debug build of the cascade was broken (`ResourceGenerator.cpp`,
`GeometryDataPrinter.cpp`) while Release stayed green.

**The nuance that matters — capture is NOT always removable.** Only drop the entries the compiler
actually flags. A `constexpr` local still needs capturing when the body **binds a reference** to it:

```cpp
constexpr auto one = static_cast< vertex_data_t >(1);

/* 'one' MUST stay captured: std::clamp takes const T &, which odr-uses it. */
const auto sphericalUV = [one](const Math::Vector< 3, vertex_data_t > & v) {
    const auto latitude = one - (std::acos(std::clamp(v[Math::Y], -one, one)) / std::numbers::pi_v< vertex_data_t >);
    /* … */
};
```

Removing `one` there yields a hard error, not a warning: *"variable 'one' cannot be implicitly
captured in a lambda with no capture-default specified"*. Clang's own diagnostics are the authority
on which entries are dead — never sweep a capture list by hand.

**Rule:** do not capture `constexpr` locals. Capture only non-`constexpr` locals (`invRadius`,
`invExtent`, …) and any `constexpr` whose address or reference the body actually takes.

### MSVC: `/EHs-` alone is NOT `-fno-exceptions` — the STL keeps its `try`/`catch`, and `/wd4530` hid them (Sept 2026, FIXED)

The MSVC STL guards its internal `try`/`catch` behind `_HAS_EXCEPTIONS`, which defaults to `1`
whatever `/EH` says. So a `/EHs- /EHc-` build still compiled hundreds of `try` blocks without unwind
semantics and MSVC reported each one as **C4530**; the policy answered with `/wd4530`, which also
silenced the two real `try`/`catch` in the engine's `Helpers.windows.cpp` / `SystemInfo.windows.cpp`
for years. Fix: `_HAS_EXCEPTIONS=0` goes with `/EHs-` (the STL stops emitting exception handlers),
and `/wd4530` is gone so a home-made `try` is now a `/WX` error. Contract:
[`error-handling.md`](error-handling.md) § 1.

- **Never re-add `/wd4530`.** If it fires, a `try` slipped into a `/EHs-` translation unit — find it.
- **Throwing standard calls are the Windows trap**: `std::thread`'s constructor throws from inside
  the CRT on OS refusal. Use the API that returns the failure (`_beginthreadex()` → `0`, `errno`).
- **Mixing `_HAS_EXCEPTIONS` values across a link is an ODR hazard** (`std::exception` differs).
  A prebuilt C++ third-party library built with the default is the first suspect on an unexplained
  Windows link error or crash in exception machinery.

### `numeric_limits< T >::max()` as a float divisor — cast it explicitly

`ColorFromInteger()` divided by `std::numeric_limits< input_t >::max()` and let the conversion to
`output_t` happen implicitly. For a wide `input_t` the exact maximum is **not representable**, so
clang rejects the silent rounding: **`-Werror,-Wimplicit-const-int-float-conversion`**, "changes
value from 4294967295 to 4294967296" (and the `uint64_t` equivalent). Only
`test_PixelFactoryColor.cpp` instantiates those wide types, so the whole unit suite failed to build
on macOS/clang while the library itself compiled (fixed Aug 2026).

**Fix:** hoist the divisor into a `constexpr auto scale = static_cast< output_t >(…::max())`. The
rounded divisor is bit-for-bit what the implicit conversion produced — the cast only states the
intent — and the expression is evaluated once instead of four times.

**Rule:** never let an integer maximum reach a floating-point division implicitly. Cast at the
source, where the precision loss is a deliberate, reviewable decision.

### ⚠️⚠️ `Color< float >{255U, 140U, 40U}` built WHITE — integer components are now refused (Sep 2026)

`Color< float >`'s component constructor clamps each channel to [0, 1] (`Math::clampToUnit`). An 8-bit literal
colour brace-initialised into a `Color< float >` parameter compiled silently — `255U` converts exactly to `255.0F` —
and clamped to 1 on every non-zero channel: `{255U, 140U, 40U}` (a fire orange) was `(1, 1, 1)`. Nineteen lights
and ambient colours of the testbed demos were white for that reason (found 2026-09-25 when the `sprite` pin-ups,
finally lit, came out neutral under "orange" fire lights).

**Fix:** a deleted constructor template for integral components (`Color (integral_t, integral_t, integral_t,
integral_t = 1) = delete`) — an exact match that wins overload resolution, so every such call is a compile error.
A mixed call (`{1.0F, 0, 0}`) fails template deduction and keeps the float constructor.

⚠️ **The migration trap:** `ColorFromInteger(255U, 140U, 40U)` DEDUCES `input_t = unsigned int` — a deduced
argument beats the `uint8_t` default — and divides by 4 294 967 295: black. Name the type:
`ColorFromInteger< uint8_t >(255, 140, 40)`. Unit tests: `PixelFactoryColor.IntegerComponentsAreRefusedAtCompileTime`,
`PixelFactoryColor.ColorFromIntegerOfAnEightBitOrange`.

### ⚠️⚠️ A `requires (A, B)` is a COMMA expression — only `B` is checked (Sep 2026, FIXED)

`requires (std::is_arithmetic_v< T >, std::is_floating_point_v< U >)` parses as a parenthesised comma expression whose
value is its LAST operand: the first constraint is evaluated and thrown away. Write `requires (A && B)`. Nineteen
clauses of the cascade were written that way (17 here — `FlagTrait`, `Math/Base.hpp` ×6, `Color` ×6 `ColorFromInteger`,
`Gradient`, `Margin`, `Pixmap::dataConversion`, `Wave` — and 2 in the engine, `OctreeSector.hpp`, `Toolkit.hpp`); all
fixed on 2026-09-26. Closing the hole broke exactly the callers that lived in it: the engine's `Sequence.cpp`, this
repository's `CartesianFrame.hpp` and `Grid.hpp` interpolate VECTORS and MATRICES through the scalar
`linearInterpolation()` / `cosineInterpolation()`. They are now accepted ON PURPOSE by the concept
`Math::LinearlyInterpolable< T, S >` (`a + (b - a) * t` is defined and gives a `T`), and everything else is refused.
A too-wide constraint never breaks a build — only a NEGATIVE test proves one (`test_MathBasics.cpp`,
`test_PixelFactoryColor.cpp`: every fixed clause whose dropped half was reachable). `FlagTrait`, `Margin` and the two
`dataConversion` have none (their dropped half is implied by the other one), nor the engine's `OctreeSector` and
`BuiltEntity` (the engine has no unit suite).

⚠️ **A requires-expression outside a template is checked EAGERLY**: `static_assert(!requires { f(bad); })` at namespace
scope is a hard error, not `false`. Put the probe in a concept (`template< typename T > concept Accepts = requires (T v)
{ f(v); };`) so the call is dependent.

⚠️ A header must include what it calls: `BSpline.hpp` used `linearInterpolation()` without including `Base.hpp` and
compiled only in translation units that had included it first — its first unit test did not compile (fixed
2026-09-25 with an explicit lerp; the same fix removed an out-of-bounds read of `m_points[index + 1]` for the last
point, pinned by `MathBSpline.lastPointIsTheTerminalSampleForEveryCurveType`).

### Fixed: `Vector::linearInterpolation()` ran BACKWARDS — every Bezier segment, and three actors, with it (Sep 2026)

It returned `a * t + b * (1 - t)`: factor 0 gave **b**. The Vector Bezier helpers are de Casteljau on it, so they
returned their LAST point at factor 0, and every Bezier segment of a `BSpline` (BezierQuadratic / BezierCubic) and of
`BezierCurve` was drawn from the next point back to the current one — a sawtooth jumping ~1 850-2 270 units at each
point of projet-alpha `basic-scenery`'s Green and Blue flying lights. Three actors (Paladin, Fox, Drone) called it as "move the heading a fraction toward the player per
tick" and SNAPPED in one tick instead of turning. Fixed 2026-09-26 (`a + (b - a) * t`, the scalar convention; the
other 34 interpolations of the cascade already followed it). Why nobody saw it: the only test computed
`start + (end - start) * 0.5` by hand and never called the function — and at t = 0.5 the two directions agree.
⚠️ **Test an interpolation at its ENDS.** The actors now turn with `Vector::rotateTowards(from, to, maxRadians)` (an angular-rate
turn: a correct fraction-LERP would have taken seconds and STALLED at 180°, where the two headings stay collinear).

### Fixed: `BezierCurve` chained OVERLAPPING segments — it jumped at every boundary and never closed (Sep 2026)

Segment i was the quadratic (P[i], P[i+1], P[i+2]), so segment i ended at P[i+2] while segment i+1 started at P[i+1]:
~2 200-2 650-unit jumps on projet-alpha `basic-scenery`'s White flying light, and a closed curve stopped short of its
start (141 units on `game-logic`'s smoke circuit). The class's own comment described the intended scheme and the code
said "for simplicity". Fixed 2026-09-26 (owner decision): the MIDPOINT chain — segment i from midpoint(P[i], P[i+1]) to
midpoint(P[i+1], P[i+2]) with P[i+1] as its handle, the uniform quadratic B-spline, C1; an open curve is clamped to start
at P[0] and end at P[n-1], a closed one closes exactly. The interior points are handles: the curve passes NEAR them.
⚠️ Do not repeat the first point as the last one before `close()` (both closed consumers did): it gives the control
polygon a zero-length closing edge, so the two segments around P[0] turn into straight lines meeting there at a corner,
and the motion slows to a stop at that point. Tests: `test_MathBezierCurve.cpp`.

### A `double` literal brace-initialising a `vertex_data_t` constant — MSVC `/WX` C4305 (Sep 2026)

`TreeGrowthCurve< vertex_data_t >` declared `static constexpr vertex_data_t CrownLengthExponent{0.6};`.
GCC and clang accept it in silence (a brace-init from a constant expression within range is not
narrowing), while MSVC reports **C4305** "truncation from 'double' to 'const vertex_data_t'" — an error
under `/WX` — for every literal that float cannot represent exactly: `0.6` and `0.8` failed, `2.5`
passed. The vertex library, and everything linking it, failed to build on Windows only.

**Fix:** `static_cast< vertex_data_t >(0.6)`, as the file's own members did: the `double`
instantiation keeps its precision and the rounding is stated.

**Catch it on Linux before pushing:** compile the translation unit with GCC `-Wfloat-conversion`
("conversion from 'double' to 'float' changes value") — it flagged exactly the two lines MSVC did.

### A template's local constant shadows a name of the INCLUDING file — MSVC `/WX` C4459 (Sep 2026)

`CurveTessellation::subdivided()` declared a local `constexpr auto Slack`; the unit test that instantiates it has its
own `constexpr double Slack` in an anonymous namespace, where its vector type `V3` lives too. MSVC checks the template
at INSTANTIATION, finds the test's `Slack` through the argument's namespace and reports **C4459** "declaration of
'Slack' hides global declaration" — an error under `/WX`: `EmeraudeBaseUnitTests` did not build on Windows. GCC and
clang (`-Wshadow` included) check at definition time and say nothing: nothing on Linux catches it.

**Fix:** a specific name for a template's local constant (`RoundingSlack`), never a generic word a caller may also use
(`Slack`, `Tolerance`, `Epsilon`, `Step`). **Before pushing** a header template: compare its `constexpr` locals with the
names at namespace scope of the test file that instantiates it.

⚠️ **It happened again on 2026-10-01** (`Casts/ShapeCast.hpp`'s `constexpr auto Tolerance` vs the cast test's `Tolerance`,
`a6343db`), by an author who had not read this entry. The physics overhaul's `Contacts/`, `Casts/` and `OrientedBox`
headers now prefix every local constant per file (`CastContactTolerance`, `BoxBoxFaceBias`, …); measured: clang's
`-Wshadow-all` does not catch it either for a template inside a nested namespace.

### The paranoid MSVC set, compiled for the first time on Windows — what it found (Ave Robustus II, 2026-10-08)

The MSVC block of the PARANOID option (`/W4 /permissive- /w14242 … /WX`, without the former `/wd4100 /wd4127 /wd4702
/wd4996`) found, in base: 6 C4702, 5 C4996, 1 C4242 (through the STL, attributed to the engine's caller). GCC and clang
say nothing about any of them. The fixes, all kept by the cascade's three compilers:

- **C4702 after an `if constexpr` that returns.** `Vector::positiveX()` … `negativeZ()` wrote `if constexpr (dim == 2)
  return …; if constexpr (dim == 3) return …; if constexpr (dim == 4) … else return {};` — in the 3D instantiation the
  trailing `else` follows a `return`. **Chain the branches** (`else if constexpr`), never a fall-through after one.
  ⚠️ **C4702 is a per-CONFIGURATION, per-INSTANTIATION diagnostic** (2026-10-10): the Release pass above left the MSVC
  **Debug** build broken — `Matrix::operator*(Vector)`, `Matrix::determinant()`, `Vector::crossProduct()`,
  `quickRandom()`, `operator<<`, `String::toNumber()` here, five engine headers, and `Pixmap`'s random fill that only
  the unit tests instantiate. A source scan then found the same shape, not yet reported, in `Utility::isZero()`,
  `Matrix::rightVector()` / `upwardVector()` / `forwardVector()` / `fastDeterminant()`, `Vector::toVector3()` /
  `toVector4()` / `random()` and the macOS branch of `String::toNumber()`: all chained the same way, the former trailing
  fallback in the final `else` (a pure rewrite, no exhaustiveness argument needed); the Windows branch of
  `String::toNumber()`, two identical `from_chars` branches, became one call. A compiler only reports what a TU
  instantiates: **scan the source** for an `if constexpr` block ending in `return` followed by more code, and build
  BOTH configurations, tests included, on Windows before calling the MSVC set clean (the engine's share: its
  `caution-points.md` § Build / Compiler).
- **C4996, the CRT functions MSVC deprecates** — replaced, never `_CRT_SECURE_NO_WARNINGS`:
  `std::getenv()` → `_dupenv_s()` (an owned copy, freed by a deleter TYPE: taking the address of `std::free` is
  unspecified) on Windows, `std::getenv()` elsewhere (`HTTPSClient.cpp` `readEnvironmentVariable()`);
  `std::sscanf()` → `std::from_chars()` (`FileFormatHDR::parseResolutionLine()`, `FileFormatSTL::parseThreeFloats()`);
  `std::fopen()` in a test → `IO::fileGetContents()` + a memory BIO (no `path::string()` either).
- **`std::sscanf()` hid two real defects.** The ASCII STL reader never checked its result: a malformed `vertex` /
  `facet normal` line became (0, 0, 0) silently and `nan` / `inf` passed. **Owner decision (2026-10-08): a malformed or
  non-finite triplet REFUSES the file** (an error naming the line). The HDR resolution line was scanned with `%lu`:
  `unsigned long` is 32 bits on Windows, so an oversized value was undefined behaviour there; now `uint64_t`, digits
  only, out of range refused. Tests: `VertexFactorySTL.ascii*`, `PixelFactoryFileFormats.hdrResolutionLineGrammar`.
- **The `<cctype>` functions take an `unsigned char` value.** `String::toUpper()` / `toLower()` / `ucfirst()` passed a
  plain `char`: a UTF-8 byte (>= 0x80) is negative there, undefined behaviour (the MSVC Debug CRT asserts). Cast through
  `unsigned char`; never `std::ranges::transform(s, s.begin(), ::toupper)` (C4242, and the same UB) — use
  `String::toUpper()`. Test: `String.caseConversionKeepsNonASCIIBytes`.
- **An `int` literal into a `std::pair< …, uint16_t >`** (`return {Outcome::Success, 0};`) is C4242 inside `<utility>`:
  write `uint16_t{0}`.

**Before pushing** C or CRT calls: grep the diff for `getenv`, `sscanf`, `fopen`, `_wfopen`, `strcpy`, `sprintf`, `::toupper`
/ `::tolower` — each one is an MSVC error now.

## Math

### ⚠️⚠️ A small rotation read back with `2 acos(w)` is ZERO in float — use `2 atan2(|v|, w)` (2026-10-01, FIXED)

> [!CAUTION]
> `Quaternion::toAngleAxis()` computed `angle = 2 acos(w)`. For a rotation of 1e-5 rad, `w = cos(5e-6)` rounds to exactly
> `1.0F`, and `acos(1) = 0`: the angle came back as 0. The engine's physics step (physics overhaul P2) converts each
> body's per-step rotation with it — a body turning at less than ~1e-3 rad/s never turned, while the solver's angular
> velocity, believing it did, kept growing: the bench's stack crept sideways faster and faster. Now
> `2 atan2(sin(angle/2), w)`, exact for small angles and unchanged for large ones (test
> `MathRigidBody.aTinyRotationKeepsItsAngle`). Any other `acos` of a near-1 cosine has the same trap.

### ⚠️⚠️ Overlap MTVs: SAT measured containment as zero, and capsule pairs were approximate (2026-10-07, FIXED)

Item collision-pair-test-defects (physics overhaul P1 survey), each proven by a failing test (`MathSpace3D.Collision*`):
- **SAT (3D triangle ↔ triangle)**: the penetration along an axis was `min(maxA, maxB) − max(minA, minB)`, which is
  0 when one interval contains the other — a flat triangle on its own normal — so that axis won with a ZERO MTV; and
  the MTV pointed from A to B, i.e. INTO B. Now `min(maxA − minB, maxB − minA)` with the side it gives, pushing A out
  of B like every other overload. **Rule:** a projected-interval penetration is the shorter way OUT, never the
  intersection length.
- **Capsule ↔ triangle / capsule ↔ AABB** now answer from the exact contact manifolds of `Contacts/` (bool: the
  manifold's overlap; MTV: `−normal × maximumDepth()`). Before: four alternating projections from the axis centre
  (stopped short for an axis nearly parallel to a face), an axis piercing a triangle got `normal × radius` (a
  winding-chosen side, too short past the radius), and a capsule deep in a box pushed only its centre point out.
  `closestPointsCapsuleTriangle()` / `closestPointsCapsuleCuboid()` are exact too (the `Contacts/` routines).
- Checked, no defect: coincident centres fall back on −Y in every overlap (the manifolds' +Y normal from A to B,
  owner decision), and the "same side" inside tests take the triangle's own normal, so the winding does not matter —
  both pinned by tests.

### `CartesianFrame::getPitchAngle()` / `getYawAngle()` / `getRollAngle()` were NOT Euler angles (2026-10-07, FIXED)

They answered the angle between the backward axis and -Z / +X / +Y: 180° / 90° / 90° for an untouched frame (found by
the engine's editor panel, 2026-09-29). Owner decision: real Tait-Bryan angles. They now answer the components of
`toQuaternion().eulerAngles()`, the ZYX order R = Rz(roll) · Ry(yaw) · Rx(pitch) — 0 / 0 / 0 for an untouched frame
(tests `MathCartesianFrame.EulerAngles*`). ⚠️ The MIDDLE angle is the yaw (Y is up): at a yaw of ±90° the
decomposition is singular and the pitch reads 0, the roll carrying what is left. Each getter converts the frame to a
quaternion: read the three at once through `toQuaternion().eulerAngles()`.

Found on the way: `Quaternion::eulerAngles()` took `asin()` of the middle sine unclamped; a 90° rotation whose
quaternion is a few ulps too long (accumulated rotations) gave NaN for all three angles. Clamped to [-1, 1] (test
`MathQuaternion.EulerAnglesOfANearlyUnitQuaternionAtTheGimbalLockAreFinite`, failing before). Same family as the
`acos` above: an inverse trigonometric function of a computed sine or cosine takes a clamp.

## Time

### ⚠️⚠️ An `EventTrait` timer is OWNED through a `TimerHandle` (2026-10-16, Ave Robustus II P4)

`EventTrait` is a BASE class: its destructor joins the timers AFTER the derived class's members are destroyed, while a
callback capturing `this` still uses them (the `Notifier`'s timer, when its `onInitialize()` failed after creating it
and `onTerminate()` never ran). `createTimer()` / `fire()` now return a `[[nodiscard]] TimerHandle` (the ID and a
`ScopedRegistration`): its end destroys the timer (stop + join; deferred when reset from the timer's own callback).
The holder declares the handle AFTER the state the callback uses — the last member — so the timer stops before that
state goes. Keep the ID for `startTimer()` / `pauseTimer()` / … through `handle.id()`. Discarding the handle destroys
the timer at once. Tests `TimeEventTrait.theEndOfItsHandleStopsTheTimer`, `aMovedHandleKeepsTheTimer`,
`aCallbackMayResetItsOwnHandle` (Release, ASan/UBSan, TSan, 20 runs each).

### ⚠️⚠️ `EventTrait` joined its timers UNDER its lock, and a callback destroying its own timer self-joined (2026-10-07, FIXED)

`destroyTimer()` / `destroyTimers()` erased the `TimedEvent`s under `m_eventsAccess`, and `~TimedEvent()` joins the
timer thread. A callback calling ANY trait method (`isTimerPaused()`, `createTimer()`…) while another thread destroyed
the timers deadlocked (test `TimeEventTrait.aCallbackMayUseTheTraitWhileItsTimersAreDestroyed`: stuck past 5 s
before); a callback destroying its own timer joined its own thread — `std::system_error` "Resource deadlock avoided",
an abort (`aCallbackMayDestroyItsOwnTimer`). Now the doomed events are EXTRACTED under the lock and destroyed after it;
an event destroyed from its own callback is RETIRED (owner decision: deferred) — `TimedEvent::requestExit()` ends its
thread when the callback returns, and the next `createTimer()` / `destroyTimer()` / `destroyTimers()` (or the trait's
destructor) joins it from another thread. **Rule:** never destroy an object owning a thread while holding a lock its
thread may take; never destroy it from that thread. Still forbidden: destroying the OWNER of the trait from one of its
timers' callbacks (the destructor joins every timer thread).

### ⚠️⚠️ A `TimedEvent` slept with `sleep_for()`: destroying or re-timing it waited for the whole interval (2026-10-12, FIXED)

Between two ticks the timer thread called `std::this_thread::sleep_for(remaining)`, which nothing interrupts. Once the
thread was asleep, `~TimedEvent()` (its `join()`) blocked until the interval ended — an HOUR in
`TimeEventTrait.fullTimerLifecycle`, whose 1 h timer was always destroyed before its thread reached the sleep, except
under ThreadSanitizer's slower scheduling (the first TSan run of the suite hung there 40 min). `setGranularity()` and
`reset()` waited for the old interval too. The wait is `m_condition.wait_for(lock, remaining, predicate)` now: the
destructor, `stop()`, `pause()`, `setGranularity()` and `reset()` notify it, and the state is `Base::Mutex` +
`EMEN_GUARDED_BY` (`elapsed()`, `isIntervalElapsed()` and `granularity()` read it unlocked before: a data race).
Tests `TimeTimedEvent.destroyingASleepingTimerReturnsAtOnce` and `aNewGranularityAppliesToASleepingTimer` (both hung
past the 20 s `timeout` before; 200 ms now). **Rule:** a thread that waits must wait on something its owner can signal
(Ave Robustus II: a stop is seen within a bounded latency), never a plain sleep longer than that latency.

### ⚠️ Process CPU time is in nanoseconds everywhere, but moves by 15.625 ms on Windows (2026-10-07)

`GetProcessTimes()` accounts CPU time per scheduler quantum: every Windows reading of `processCPUTimeNanoseconds()` is
a whole multiple of 15.625 ms, so an interval shorter than that reads 0 (`DebugStatistics.timerMeasuresBusyWork`
failed 2 runs in 6 there with a fixed busy loop). **Rule:** compare a CPU-time interval against
`Time::processCPUTimeResolutionNanoseconds()`, never against 0, and size a CPU-time measurement in resolutions, not in
loop iterations. Do not "fix" it with `QueryPerformanceCounter`: that is elapsed time, not CPU time.
Accepted 2026-10-07: Windows `DebugStatistics.*` 30/30 (was 4 pass / 2 fail in 6), macOS 30/30; the resolutions are
1 ns (Linux), **1 µs** (macOS 26 / M2, `clock_getres`), 15.625 ms (Windows).

### `Statistics::CPUTime` recorded NOTHING at an unusual `CLOCKS_PER_SEC`; bad samples are dropped AND counted (2026-10-08, FIXED)

`CPUTime::stop()` converted ticks with `if constexpr` branches for 1e3, 1e6 and 1e9 ticks per second only: any other
rate recorded no duration, silently. `CPUTime::ticksToMilliseconds(ticks, rate)` converts for any rate, exactly and
without overflow (seconds and remainder apart). A sample that is not a measurement (`std::clock()` failed, a 32-bit
`clock_t` wrapped, a non-monotonic clock went backwards in `RealTime`) is dropped and COUNTED:
`Statistics::Abstract::droppedSampleCount()`, printed by `print()` (owner decision). Tests
`TimeStatisticsCPUTime.ticksToMillisecondsForAnyRate`, `TimeStatisticsRealTime.aBackwardsSampleIsDroppedAndCounted`.

## Threads

### ⚠️⚠️ A thread is started through `Base::Thread`, never `std::thread` (2026-10-07)

`std::thread`'s constructor throws `std::system_error` when the system cannot start a thread (resource exhaustion, a
thread limit): under `-fno-exceptions`, an abort — and `std::thread::join()` from the thread itself throws too.
`Base::Thread` (`src/Thread.hpp`) starts on `pthread_create()` / `_beginthreadex()` and answers `false`; it requests
the stop then joins on destruction (the `std::jthread` model since 2026-10-15 — see below) and ABORTS on a self-join
(since 2026-10-08, decision D2 — see below; it used to detach). **Rule:** no `std::thread` in the cascade (none is left,
2026-10-07: `/usr/bin/grep -rn "std::thread"` finds only `std::thread::id` / `hardware_concurrency()` and tests); a
caller of `start()` decides what a refusal means. Owner policy: a feature thread refuses its feature (`false` +
trace); the Tracer writes synchronously; `executeCommandPumpingEvents()` blocks; `ThreadPool` keeps the workers that
started and, with none, runs the tasks on the calling thread; `Core`'s logic / rendering threads end the start-up
with a non-zero exit code. `Thread::failNextStartsForTesting(n)` makes the next n starts fail (tests only).
Accepted 2026-10-07 on the three OS (build clean, `BaseThread.*` / `ThreadPool.*` / `TimeEventTrait.*` 20× green, MCP
1881/0, logs complete). Not exercised at run time anywhere: the engine sites' refusal paths and the Tracer's synchronous
fallback (the engine has no failure seam), and `Desktop::Notification::show()` on Windows (thread-free since that day,
compiled clean) — it has NO caller in the engine or projet-alpha.

### ⚠️⚠️ A new thread must not run before start() has recorded it — the start handshake (FIXED 2026-10-07)

Found by the macOS peer under load (16 parallel processes), on base `4aa29ae`: `start()` wrote `m_handle` and
`m_joinable` AFTER `pthread_create()` returned, so the new thread could run first. Its self-join saw an idle object
and returned silently (no "cannot join itself" trace), then `start()` marked the object joinable: test
`BaseThread.aThreadJoiningItselfIsDetachedInsteadOfAborting` failed 17 times in 8000 runs in Release, 223 in 4800
under ASan — and it was a C++ data race besides (the child read what the parent was writing). Every thread body
touching its own `Thread` at once had the window. **Fix:** the task carries `m_published`; `start()` sets it
(release) as its LAST access to the task, after recording the thread, and `Thread::entryPoint()` yields until it sees
it (acquire) before running the callable — whatever `start()` writes happens-before the callable. ⚠️ Not
`atomic::wait()`/`notify`: the notify would come after the store, when the new thread may already have deleted the
task. Proof: `BaseThread.aNewThreadSeesItsObjectAlreadyRecorded` holds `start()` in the window with the test seam
`Thread::delayNextPublicationForTesting(ms)` — failed every time before the fix, passes after; the macOS load
(16 × 500 Release, 16 × 300 ASan) gives 0 failure and every trace on Linux. **Rule:** an object handing work to
another thread publishes its own state BEFORE the other thread can observe it, never after the hand-over call.

### ⚠️⚠️ A `Base::Thread` stops through its `std::stop_token` — the `std::jthread` model, no `detach()` (2026-10-15)

Ave Robustus II § 4.2 (owner decision 2026-10-15: one `start()`, `std::jthread`'s dispatch rule). Each `start()` creates
a fresh stop state; a body invocable with a `std::stop_token` receives its token (that form wins when both are
possible), any other body is called with no argument. `~Thread()` and the move assignment `requestStop()` then
`join()`; `requestStop()` returns whether this call made the request; `stopToken()` hands the token to the owner's own
waits (no stop source leaves the object: only the owner stops its thread). A refused start keeps the previous stop
state; an idle or moved-from object has none (`stop_possible()` false). A started body ALWAYS runs, even when the stop
comes before its first instruction (a dedicated thread is not queued): it sees the stop at its first check — unlike a
`TaskHandle` task stopped before it starts, which never runs: the stop request removes it from the pool's queue and
destroys it on the stopping thread, without invoking it (owner decisions 2026-10-15; `TaskGroup` the same). `detach()` is GONE: a started
thread always has an owner that stops and joins it (it had no production caller in base, the engine or projet-alpha).
**Rule:** a thread body that loops or waits takes `const std::stop_token &` and checks it at a bounded interval; a wait
is `std::condition_variable_any::wait(lock, token, predicate)` — it wakes on the stop with no notify, so neither a
hand-made `std::atomic_bool m_stop` nor a condition variable declared "before the thread" is needed (the H2 class of the
census). A body taking no token still compiles and is still joined, but nothing can stop it: that is the transitional
form of the engine owners (engine item `jobs-owned-by-their-starter`); once they migrated, the no-argument form can go.
⚠️ Like `std::jthread`, one object is used by its owner: `requestStop()` from the owner's thread reaches the body (the
stop state is thread-safe), but no member may run concurrently with `start()`, `join()`, a move or the destruction.
Proof: `BaseThread.aStopRequestedBeforeTheBodyChecksIsSeenAtItsFirstCheck`, `aStopWakesAConditionVariableWaitWithoutANotify`,
`theDestructorRequestsTheStopThenJoins`, `aMoveAssignmentOverARunningThreadStopsAndJoinsIt`,
`aRestartedThreadGetsAFreshStopState`, `aBodyTakingNoTokenStillRunsAndTheDestructorStillJoinsIt` (failed to compile
before, pass after); macOS M6 (AppleClang, paranoid warnings, 0 warning): 2513/2513 in Release, ASan/UBSan and
TSan (0 report), `BaseThread*` 20 rounds under TSan 0 report; `BaseThreadDeathTest.aThreadJoiningItselfAborts` kept.

### ⚠️⚠️ A job that touches an object is `submit()`ted, and the object keeps its `TaskHandle` (Ave Robustus II, 2026-10-08)

`ThreadPool::enqueue()` returns nothing: no owner, no individual wait, no cancellation — so ad-hoc flags grew and an
outsider (`Core`) drained the pool for objects it did not own. `ThreadPool::submit(callable)` returns a move-only
`TaskHandle` (`src/ThreadPool.hpp`, owner decision D1): the callable receives a `std::stop_token`; the handle's
destructor and move-assignment request the stop and wait for THAT task only; when `wait()` returns, the callable and
its captures are already destroyed (the owner may free what they pointed to). **Rule:** a task touching an object
(`this`, a reference) is submitted and the object keeps the handle as a member, declared AFTER the state the task
uses; `enqueue()` stays for fire-and-forget work capturing values only. A long task checks its token at a bounded
interval in every stage (`ShapeDecimator::setStopToken()`: every 4096 iterations of every stage loop).
⚠️ A task waiting for (or destroying) its OWN handle would never return: a contract fault, aborted (decision D2, below).
⚠️⚠️ **A task stopped BEFORE it starts never runs (owner decision 2026-10-15, the `std::execution` semantics: a stop
before the start completes as "stopped").** The stop request REMOVES it from the pool's queue
(`ThreadPool::purgeQueued()`: every pool task carries its owner's state as a tag) and destroys its callable on the
STOPPING thread, outside the pool's lock, WITHOUT invoking it; a task already dequeued but not started is discarded by
its worker. So after a stop a wait depends on a RUNNING task only — never on the queue, nor on other owners' work
queued before it — and when `wait()` returns the callable is destroyed. Two defects this removes: an owner destroyed on
a pool worker (a resource whose last reference a loading task drops) waited for its job still queued behind it — with
every worker in that state the pool deadlocked for good; and a first version (2026-10-15, morning) that discarded
queued tasks only when a worker DEQUEUED them still made a shutdown wait behind other owners' long work (macOS M6:
the engine's resource stop waited 5.8 s for the whole automatic-LOD generation queued before its discarded loads).
Consequences: a task that never runs releases in its DESTRUCTOR whatever a waiter expects from it (a "never ran"
guard: engine `Resources::ServiceAccess::DiscardedTaskGuard`, `ResourceTrait::LocalDataReloadJob`); that guard runs
on the STOPPING thread (or a worker for a dequeued one) and touches only what it holds. ⚠️ The pool must outlive a
handle / group while one of its tasks may be queued (`@pre`; a pool runs its whole queue before it dies, and a finished
task is never purged). A `wait()` WITHOUT a stop still waits for a queued task: never from a pool worker. Proof:
`ThreadPoolTaskHandle.TaskStoppedBeforeStartNeverRuns`, `DestroyingAQueuedHandleFromTheOnlyWorkerDoesNotDeadlock`
(hung past a 20 s timeout on the first contract), `DestroyingAQueuedHandleDestroysItsCallableAtOnce` and
`ThreadPoolTaskGroup.AStopNeverWaitsForOtherOwnersQueuedWork` (11.4 s without the purge, under 100 ms with it).
⚠️ The measured stop latency of a 2.24 M-triangle decimation is 16 ms worst on Linux (2.5 ms on the M2): 1.52 s, then
0.74 s before the D7 rewrite, where the rest was the DEALLOCATION of its node-based containers; 25 ms after D7 step 4
(`9fb8f86`), then 16 ms once the per-vertex lists became trivially destructible (`fd9dc5c`) — the worst row is now the
start (allocating the work mesh). Detail below in § VertexFactory and item `task-handle-and-stop-token`.
Proof: 8 `ThreadPoolTaskHandle.*` tests, 20 rounds under TSan with 0 report; Release and ASan/UBSan green.

### ⚠️⚠️ An object running SEVERAL tasks owns them through a `TaskGroup` (Ave Robustus II, 2026-10-15)

A resource container's loads, a network manager's transfers: N tasks at once, one owner. `TaskGroup` (`src/ThreadPool.hpp`,
owner decision 2026-10-15) is the `TaskHandle` contract for N tasks: `ThreadPool::submit(group, callable)` (returns
`false` when the group or the pool is stopping — the callable never runs and is destroyed before it returns); the
tasks share ONE `std::stop_source`; `~TaskGroup()` = `requestStop()` + `wait()`. State of the art: the async scope of
C++26 `std::execution` (P3149, `counting_scope`) and Trio's nurseries (N. J. Smith, 2018): no task outlives the scope
that started it. Contract:
- the stop is FINAL: a stopped group admits nothing more;
- a task stopped before it starts never runs (as `TaskHandle`): `requestStop()` removes the group's queued tasks from
  the queue (their guards run on the stopping thread); `wait()` then waits for the RUNNING tasks only;
- a group serves ONE pool (the first `submit()` records it; another pool is refused, traced);
- `waitForAll()` is `wait()` refused from a pool worker (the strict form for an owner shutting down on the main thread;
  aborted in every build — the worker is marked by a `thread_local` set in `ThreadPool::worker()`);
- a task of the group waiting for its own group (or destroying its owner) aborts (D2: a `thread_local` chain of
  `TaskGroupRunScope`s, nested inline runs included);
- the owner declares the group LAST (destroyed first). State: `Base::Mutex` + annotations (D12), notifications after
  the guard.
Proof: 11 `ThreadPoolTaskGroup.*` tests (+ 2 death tests); the deadlock tests hung on the first contract,
`AStopNeverWaitsForOtherOwnersQueuedWork` took 11.4 s without the purge; Release full suite 2529; ASan/UBSan and TSan in
the item `task-handle-and-stop-token`; clang `-Wthread-safety` clean.

### ⚠️⚠️ A self-join ABORTS, in every build — never destroy or stop an owner from its own thread (D2, 2026-10-08)

Owner decision D2: joining a `Base::Thread` from itself (its owner destroyed or stopped on its own thread), a task
waiting for or destroying its own `TaskHandle`, `HTTPServer::stop()` from its network thread (a request handler) and an
`EventTrait` owner destroyed from one of its timers' callbacks are CONTRACT FAULTS: `Logging::fatal()` then
`std::abort()`, Release included. The self-join used to be detached silently — the thread then ran on a destroyed
object, a hidden use-after-free. For `EventTrait` there is no safe alternative (nothing would be left to join the timer
later); `destroyTimer()` / `destroyTimers()` from a callback stay legal (retired, joined later).
Audit before the switch (every `Base::Thread` and `TaskHandle` of the three repositories, read-only, 2026-10-08): no
path reaches a self-join today — servers' requests and console commands run on the main thread (queued), the observers
of a thread's notifications never stop it, no task holds the last reference to its owner. Two places only stand on a
caller's discipline, now documented as `@pre`: `~EventTrait` (projet-alpha's `LightenMarbles` timer copies a
`shared_ptr< Scene >`, defused because `Act::~Act` destroys the scene's timers first) and `HTTPServer::stop()`.
Tests: `BaseThreadDeathTest.aThreadJoiningItselfAborts`, `ThreadPoolTaskHandleDeathTest.TaskWaitingForItsOwnHandleAborts`
(death tests: the faulty statement runs in a child process).

### ⚠️ TSan reports OpenMP regions as races — run a TSan test with `OMP_NUM_THREADS=1` (2026-10-08)

libgomp is not built with TSan: its barriers are invisible, so the end of a `#pragma omp parallel for` looks
unsynchronized. Measured on `VertexFactoryShapeDecimator.aRequestedStopTokenStopsTheDecimation` (its sphere goes through
`Shape::computeTriangleTangent()`'s OpenMP loop): 7 "data race" reports between an OpenMP worker and the main thread's
later stack frames — 0 with `OMP_NUM_THREADS=1`, same test, same binary. **Rule:** run TSan with `OMP_NUM_THREADS=1`
(it then checks our own threads only); a race INSIDE an OpenMP region needs an Archer / TSan-aware OpenMP runtime.

## IO / std::filesystem (triad, 2026-09-30)

### ⚠️⚠️ Every std::filesystem call WITHOUT an error_code throws — and under -fno-exceptions that is std::terminate

- **Symptom:** the process dies on the first unreadable directory, broken link, vanished file or empty path — no log.
- **The traps:** `exists(p)`, `is_directory(p)`, `relative(a, b)`, `canonical(p)`, `create_directories(p)`,
  `permissions(...)`, `current_path(p)`, a directory_entry's `is_regular_file()` / `file_size()`… AND the
  range-for over `directory_iterator` / `recursive_directory_iterator`: even built with an `error_code`, its
  `operator++` throws. Unqualified calls find the throwing overload by ADL (`is_directory(path)`).
- **Rule:** the `error_code` overloads, the `IO::` wrappers (`IO::exists`, `IO::fileExists`, `IO::directoryExists`,
  `IO::createDirectory`), and `IO::forEachDirectoryEntry(path, recursive, visitor)` for every walk (or
  `IO::directoryEntries()`). `file_size(ec)` answers `static_cast< uintmax_t >(-1)` on error: never add it blindly.
- 2026-09-30: 38 calls + 9 walks + 9 ADL calls moved, cascade-wide (base, engine, projet-alpha).

### ⚠️⚠️ Windows: a path of MAX_PATH (260) characters or more fails in any IO — unless it takes the `\\?\` form (2026-10-07)

Found by the Windows peer (2026-10-01): under a ~142-character `--cache-directory`, 118 shader binaries (273-282
characters) could not be written. Owner decision: both supports.
- Every `IO::` wrapper hands the system `IO::systemPath(path)`: on Windows, a path whose absolute form reaches
  `WindowsLongPathThreshold` (248 = MAX_PATH − 12, the CreateDirectory() limit) becomes absolute, lexically normal,
  backslashed and prefixed — `\\?\C:\…` or `\\?\UNC\server\share\…` (`IO::windowsExtendedLengthPath()`, pure and
  tested on every OS). It needs no system setting. Elsewhere, and below the threshold, the path is unchanged.
- `IO::renameFile()` is the `std::filesystem::rename` wrapper (the "write aside, then rename" commit); the engine's
  shader-binary and pipeline caches use it.
- projet-alpha's executable declares `longPathAware` (its `cef-integration.md`): it covers the code OUTSIDE `IO::`
  (the engine's other direct `std::filesystem` calls, the 17 base files opening their own streams, third-party
  libraries) — only when the system's `LongPathsEnabled` is 1.
**Rule:** a file access goes through `IO::` (or `IO::systemPath()`), not through a bare `std::filesystem` / fstream
call, or a long Windows path fails there. The `\\?\` form turns off every normalisation: never build one by hand.
⚠️ **A whole-tree operation takes the extended form for its ROOT whatever the root's own length** —
`IO::systemTreePath()`, used by `eraseDirectory(recursive)` and a recursive `forEachDirectoryEntry()`: the
descendants' length decides, and MS-STL's `remove_all()` from a short root stopped at the first descendant past MAX_PATH
("145: The directory is not empty", Windows peer 2026-10-07, proven by an MSVC probe). The engine's `TextureCache` used
raw `std::filesystem` / fstreams and lost its cache under a long `--cache-directory` without `longPathAware`: now
through `IO::` (engine commit of the same day).
⚠️⚠️ **A walk hands its entries back in the CALLER's form.** The first `systemTreePath()` walk (base `1852e65`) gave
the visitor `\\?\C:\…` paths: every caller computing `entry.path().lexically_relative(itsRoot)` got an EMPTY path
across the two root names — on Windows the engine's dynamic resource scan registered 0 of 9854 resources, and the
sharing server was hit the same way (Windows peer, same day). `forEachDirectoryEntry()` now rebuilds each entry as
the caller's root plus its relative part; only an entry whose caller-form path reaches the threshold stays extended
(its only usable form). Off Windows nothing is rebuilt. **Rule:** a wrapper that changes a path's form for the system
gives it back in the caller's form.

**Accepted on three OS (2026-10-07, base `094127a`).**
- Windows (NVIDIA): IO* 22/22. The no-`longPathAware` copy, run with a 301-character `--cache-directory`, wrote:
  - 35 shader binaries;
  - 15 texture-cache files;
  - the pipeline cache;
  with the longest path at 404 characters and 0 IO error.
- The resource scan found 9854 resources on Linux, macOS and Windows.
- The sharing server serves the same 9854 entries, with no empty, absolute or `\\?\` path, and byte-identical files.

### ⚠️⚠️ Windows: a path's narrow conversions are the ANSI code page — `IO::u8path()` in, `IO::toU8String()` out (rule C, 2026-10-12)

`path::string()`, `generic_string()` and std's `operator<<` (ostream, path) convert through the ANSI code page: a
character outside it THROWS (std::terminate), the others become invalid UTF-8. A path built from a narrow string
(`path{text}`, `dir / text`, `p = text`, `append(text)`) decodes it as ANSI. Every `std::string` of the cascade is UTF-8:
`IO::u8path()` in, `IO::toU8String()` / `toGenericU8String()` out (engine `docs/cpp-conventions.md` § "Paths and Text
Encoding", guarded by the engine's `tools/check-path-encoding.py`). In the base, 2026-10-12:
- `BlobTrait` has an `operator<< (const std::filesystem::path &)`: UTF-8, quoted as std quotes it (test_BlobTrait: the
  accented case wrote CP-1252 bytes, the Ω case and a lone surrogate aborted, `0xC0000409`);
- `String::isValidUTF8()` (the strict decoder's round trip) lets a trust boundary refuse what a third-party API would
  throw on (MSVC's `char8_t` path conversion);
- `Network::TrustStore::applyCABundleFile()` ABORTED on a bundle under a non-ASCII directory (`path::string()` handed to
  `load_verify_file()`, whose TLS library opens a narrow name with `fopen()` anyway): it reads the file through
  `IO::fileGetContents()` and calls `add_certificate_authority()`, which adds every PEM certificate of the buffer
  (test `bundleFileUnderANonASCIIDirectory`, aborting before);
- `Network::HTTPRequest::toString()` wrote `" /" << path` through std's `operator<<`: a quoted path after a second slash
  (`GET /"/a/b"`); it writes one leading slash and the path as is (tests `NetworkHTTPRequest.requestLine*`: "/a/b", "a/b",
  "" → `GET /a/b`, `GET /a/b`, `GET /`);
- `Network::URI` keeps its path UTF-8 both ways; `IO`'s own error logs, the zip reader / writer, the file IO helpers and
  the tests' `uniqueTemporaryPath()` convert explicitly. The test binary has no UTF-8 manifest: a test proves the
  conversion itself.

### ⚠️⚠️ A path from DATA goes through IO::confinedPath() — path::append() REPLACES the base with an absolute path

`base / "/etc/passwd"` is `/etc/passwd`; `base / "../../x"` leaves `base`. Every path written in data (a resource
definition, a ZIP entry name — "Zip Slip") is joined with `IO::confinedPath(base, relative)`, which refuses empty,
absolute, root-named and `..`-escaping paths (lexical, no system call; a symlink inside the base is not resolved).
`ZipReader::extract()` refuses such an entry (test `ZipArchive.extractionRefusesEntriesThatLeaveTheDestination`).

### ⚠️ A ZIP entry is untrusted data — its declared size, its method, its CRC (2026-10-17, FIXED)

Found by the coverage pass with archives built byte by byte (`ZipArchive.emptyEntriesAndDirectoryEntriesExtract`,
`entriesThatCannotBeReadAreRefused`, `declaredSizeThePayloadCannotBackIsRefused`; all three failed on the former code):
- `extract(name, buffer)` resized the buffer to the size the ARCHIVE declares: 200 bytes declaring 1 TiB threw
  `std::bad_alloc` (abort under `-fno-exceptions`). Now an entry declaring more than its compressed size ×
  `ZipReader::MaxCompressionRatio` (1032, deflate's maximum, every method — owner decision) is refused before the
  allocation. A bzip2 / xz / zstd entry compressed beyond 1032:1 is refused too: the known trade-off.
- `zip_fopen()` and `zip_fread()` results were ignored: an unsupported method, an encrypted entry and a CRC mismatch were
  returned as SUCCESS with wrong data. libzip verifies the CRC when it reaches the end of the data: `extract()` reads one
  byte beyond the declared size and demands the end (0).
- `stat.size == 0` meant "missing": an empty file and a directory entry (`name/`) were refused, so `extractAll()`
  failed on any archive holding a directory entry. Only `zip_stat()`'s result means missing now; a directory entry
  creates its directory.
- The file was opened (truncated) before the extraction: a refused entry left an empty file. Extraction runs in memory
  first; the write is checked.

### ⚠️ A test's temporary file or directory gets a PER-PROCESS name — `Testing::uniqueTemporaryPath()` (2026-10-08, FIXED)

Two copies of the suite running at once (the macOS peer's 8-copy stress run, two developers, a CI) shared ~25 FIXED
names in the temporary directory across 9 test files and truncated or deleted each other's files:
`NetworkHTTPServer.ServesFilesWithRanges` / `CleartextDownloadFromAPrivatePeer` failed 115 and 94 times in 160 on
macOS; reproduced on Linux (8 copies × 10 repeats of the old binary: every copy failed, 8 to 26 times). `ctest -j`
never runs one test twice at once, so the normal gate never saw it. `src/Testing/TemporaryPath.hpp`'s
`uniqueTemporaryPath(leaf)` puts a per-process token before the extension and calls `temp_directory_path()` through
its `error_code` overload (10 call sites used the THROWING one). After: 8 copies × 10 repeats green, nothing left in
`/tmp`. **Rule:** a test never names a temporary path itself. Exception, on purpose: the PixelFactory tests' visual
outputs `resources/assets/tmp_*` (git-ignored) keep fixed names so a human can open them; no test reads one back
(checked), so a parallel run only overwrites a picture. Validated on the three OS (macOS 8 copies × 20, Windows 4 × 5).

### ⚠️ Windows cannot delete a file a handle still holds — a test destroys its readers BEFORE its cleanup, and checks it (2026-10-08)

The Windows peer found `emeraude_zip_roundtrip`, `emeraude_zip_directory` and `emeraude_zip_slip` left in `%TEMP%` after
every ctest run: those tests called `remove_all()` while their `ZipReader` was still alive, holding the archive open.
Linux unlinks an open file, Windows refuses, and the `error_code` was never read — a silent leftover on one OS only.
Fixed in the tests (the library releases its handle at destruction, `readerDestructorReleasesHandleWithoutExplicitClose`):
every reader / writer lives in its own scope, and `expectTempDirRemoved()` checks the error code and that the directory
is gone. **Rule:** a test's cleanup runs after every object holding a file is destroyed, and its result is checked.

### `ZipWriter::addFilepathToSources()` / `addDirectoryToSources()` accepted the wrong kind of path (FIXED 2026-09-30)

`!is_regular_file(p) && !exists(p)` accepted an existing DIRECTORY as a file (and the reverse), while the message
said "or"; given a file, the directory walk then threw. Test `ZipArchive.sourcesRefuseTheWrongKindOfPath`.

## Algorithms

### Diamond-square: a CONE TIP at every coarse point — the linear averages of midpoint displacement (Sep 2026, FIXED)

Owner report (projet-alpha `water-world`, 2026-09-28): isolated thin spikes out of smooth dunes, one right under the spawn
camera. Not an implementation bug: a point created at a coarse level is never touched again and every finer level filled
around it by LINEAR averages, so the slope broke at each coarse point — a cone tip — with creases along the coarse grid
lines (the midpoint-displacement artefact, G. S. P. Miller, "The Definition and Rendering of Terrain Maps", SIGGRAPH 1986).

`Algorithms::DiamondSquare` now builds each new point with the 4-point cubic rule (Dyn, Levin, Gregory, CAGD 4, 1987):
(−p₋₃ + 9 p₋₁ + 9 p₊₁ − p₊₃) / 16 along both diagonals (diamond step) or both axes (square step), the quadratic through
the three remaining samples when an outer one lies outside the grid, the linear mean when both do. Owner decision
2026-09-28, chosen over Miller's square-square subdivision (approximating: the coarse points would move), random additions
(the linear slopes still meet at the coarse points) and switching to fBm noise.

Measured on `water-world`'s relief (513 points, roughness 0.5, hurst 1.25, seed 0, × 75 m), |Laplacian| = a point against
the mean of its 4 neighbours:

| | linear | 4-point cubic |
|---|---|---|
| median at coarse points (step ≥ 32) / elsewhere | 0.218 / 0.028 m (7.8×) | 0.056 / 0.025 m (2.2×) |
| maximum | 0.87 m | 0.30 m |
| grid centre (the spawn) | 0.75 m | 0.12 m |

The large relief is kept (correlation 0.96 between the two, same seed: the random draws come in the same order).
⚠️ A higher `hurst` did not cause the cones — it UNMASKED them, by removing the fine noise that hid them (7.8× at 1.25,
3.3× at 1.0). ⚠️ Every diamond-square terrain changed shape slightly with the fix (same seed, other interpolation).
Regression test `AlgorithmsDiamondSquare.TheCoarsePointsAreNoConeTips` (bound 3.5×).

## Hash

### ⚠️⚠️ SHA-1 / SHA-256 / SHA-512 were WRONG for every input of 512 MiB or more (2026-10-04, FIXED)

The final block carries the message length in bits, a 64-bit big-endian field (FIPS 180-4 § 5.1; SHA-512's is
128-bit). The inherited code (zedwood) wrote only its LOW 32 bits: from 2^32 bits (512 MiB) on, the digest was
silently wrong. Found by engine resource sharing: the server's SHA-256 of a 1.6 GB archive disagreed with
`sha256sum`. Every small input was right, so the known-answer tests never saw it. Fixed in the three `final()`;
`Hash.lengthFieldPastFourGigabits` hashes 512 MiB + 3 bytes against Python's `hashlib` (failed before, passes now).
MD5 keeps its RFC 1321 two-word counter and was correct.

### `Hash::hmacSha256()` returns HEX, keyed per RFC 2104 (2026-10-07)

Added for a downstream application's crash report (a request signed with a shared secret). Lower-case hexadecimal like every other
`Hash::` function; a key longer than the 64-byte block is hashed first, as RFC 2104 requires — a hand-rolled HMAC that
truncates or zero-pads a long key instead signs differently from every server library. Proven by the RFC 4231
vectors 1, 2 and 6 (`Hash.hmacSha256KnownAnswer`), and accepted by a server checking with Python's `hmac` (a downstream application's test server).

## Network

### ⚠️⚠️ HTTPSClient keeps connections alive — what makes a connection reusable, and what never rides one (2026-10-07)

Owner decision after a measurement: a small HEAD cost ~80 ms through a new TCP connect + TLS handshake (~80 % of the
call); with the pool, **11.3 ms median** (20 calls: 0.29-0.45 s instead of 1.6-1.85 s, live, Linux). The client keeps
a connection (`HTTPSClientOptions::reuseConnections`, default on) when the response was framed by its LENGTH (a body
read until close spends the connection) and `HTTPResponse::keepConnectionAlive()` (RFC 9112 § 9.3); at most 4 idle per
(host, port, proxy, cleartext), 30 s each. A pooled connection is probed (`TLSConnection::isOpenAndIdle()`: a
non-blocking peek — the peer's close_notify or FIN, unexpected bytes, a reset all disqualify) and, if it still fails
before the first response byte, the request is retried ONCE on a new connection. **Rules:**
- A non-idempotent request (POST, PATCH, CONNECT) NEVER takes a pooled connection: one the server closed while idle
  could fail it after it was acted upon, and it cannot be replayed. It may still leave its connection to the pool.
- The pool is SHARED state (several workers on one client), guarded by its own mutex; a connection taken from it
  belongs to one exchange until given back. Nothing is closed under the lock.
- `TLSConnection::disconnect()` sends its close_notify and waits for the peer's answer at most 200 ms (RFC 8446 § 6.1
  does not require waiting): closing an idle connection the server still held open used to block the caller for the
  whole write timeout — 30 s per connection (test `closingAnIdleConnectionDoesNotWaitForASilentPeer`, 30 047 ms before).
- A test server that keeps connections must answer the client's close_notify, as real servers do
  (`HTTPSTestServer::setKeepAlive()`, one thread per connection).

### ⚠️⚠️ Closing a socket over unread bytes is a RST — and a RST loses the last answer on Windows (fixed Oct 2026)

`HTTPServer` wrote its 503 at the connection cap, then `close()`d the socket while the client's request was still
unread in the receive buffer. A TCP close over unread bytes sends a RST instead of a FIN (RFC 1122 § 4.2.2.13), and
on Windows a RST discards what the client had received but not yet read: `NetworkHTTPServer.BoundsConnections`
read NO byte 6 times in 200 there (Windows-PA, 2026-10-06; its Python model: close-unread is a RST 300/300, the 503
lost 300/300 when the client reads after the RST arrived, 0 RST once the server reads first). Linux keeps the bytes
it received, which is why the test only failed on Windows; on Linux the RST shows as a read ending on
`connection_reset` instead of EOF — the tests now assert THAT. `shutdown(both)` before `close()` does not help: the
FIN goes first, then the RST, and Windows may still drop the answer. **Rule**: after a final answer, never close a
socket the peer may still be writing to — `Network::GracefulCloser` (shutdown(send), bounded drain, close). Same
defect in the engine's console (`RemoteListener::disconnect()`), fixed with the same closer.

### ⚠️⚠️ A `mutable` member is NOT a per-call output — it was a data race for a year (fixed Aug 2026)

`HTTPSClient` recorded why a transfer failed in `mutable DownloadOutcome m_lastOutcome`, written by
methods that are all `const`. The reasoning was "the API is const, the object is logically
unchanged". It was wrong for a reason the const-ness hides: **`Net::Manager` runs several
`download()` calls concurrently on ONE shared client**, so two workers wrote that member at the
same time and a failing transfer could report the reason belonging to another transfer.

The tell was there in the header — the comment said *"it describes the last call, not the object"*.
**A value that describes A CALL belongs to the call**: it is an out-parameter, a return value, or a
local. It is never a member, however `mutable`. Fixed by threading it through
`run()`/`performHop()`.

⚠️ The same shape is still present elsewhere in the cascade wherever a `const` method caches "the
last error" on the object. Look for `mutable` next to a word like *last*, *cached*, *current*.

### ⚠️⚠️ A query value holding `&`, `=` or `+` was re-emitted LITERALLY — a presigned S3 URL broke (2026-10-07, FIXED)

`URI` stores the query DECODED (`Query::fromString()`) and re-encodes it on output (`URI::resource()`, the request
target). The re-encoding used `Component::Query`, which keeps the sub-delimiters `& = + ;` literal — right for a whole
query string, wrong for ONE key or value: `a%26b` came back as `a&b` (a new variable), `%3D` as `=`, and `%2B` as `+`,
which S3 and every form decoder read as a SPACE. The `X-Amz-Security-Token` of an S3 presigned URL holds `%2B` and
`%3D`: the PUT reached S3 with a token that no longer matched its signature. `Query`'s output now encodes each key and
value with `Component::QueryVariable` (`Query` minus `& = + ;`). Order is the `std::map` order (sorted), not the
original one — harmless for AWS SigV4 (it sorts too), but a scheme signing the raw query string as received would still
break: such a URL needs the original string, not a `URI`. Test `NetworkURI.queryValueDelimitersSurviveARoundTrip`.

### ⚠️ Anything concatenated into a request line must be validated, not just the host

The 2026-08-27 audit fixed CRLF injection through the **host** (`URIDomain` validates it once). The
2026-08-28 `request()` increment re-opened the same class from a new direction: caller-supplied
**header names and values**, and the **media type**, all reach the wire through `request += …`.
A single `\r\n` in any of them injects arbitrary headers, and with a body, splits the request.

`HTTPSClient::isRequestHeaderAcceptable()` is the gate, and it is applied in `run()` **before the
first connection is opened** — refusing at concatenation time would already have resolved and
contacted the target, which the caller cannot tell apart from a transport failure.

⚠️ **The media type does not travel through `options.headers`**, so it needed its own explicit
check. Any future field that reaches the request the same way needs one too: the validation is not
automatic, it is a list.

### ⚠️ A response's status line is read strictly (RFC 9112 § 4); a parsed request may target a path (2026-10-17, FIXED)

Found by the coverage pass (`NetworkHTTPResponse.statusLineIsStrict`, `NetworkHTTPRequest.parsedRequestKeepsItsLineAndHeaders`,
both failing before; owner decisions):
- `HTTPResponse::parseFirstLine()` read the code through `String::toNumber()`, which stops at the first non-digit:
  "HTTP/1.1 200abc" was a 200. And an unknown version token ("FOO", "SSH-2.0") silently became HTTP/0.9. Now the
  version is exactly `HTTP/` DIGIT `.` DIGIT — 1.0, 1.1, a higher 1.x read as 1.1 (RFC 9110 § 2.5), 0.9; any other is
  refused, a textual "HTTP/2.0" included — and the code exactly three digits, still within 100-599.
- `HTTPRequest::isValid()` demanded `!URI::empty()`, which means "has a scheme": a parsed request in origin-form
  ("POST /upload HTTP/1.0", the normal request line) was never valid. A path now qualifies too. (`HTTPServer` has its
  own request parser; nothing in the cascade parsed requests through `HTTPRequest`.)

### ⚠️ A test server that stops at the header terminator cannot prove a request body

`HTTPSTestServer` read until `\r\n\r\n` and handed that to the handler. A POST body therefore
appeared only if it happened to share the last TLS record with the headers — so a test asserting on
it passed or failed **by timing**, which is worse than not testing it. It now reads out what
`Content-Length` announced (`Testing::declaredContentLength()`).

### ⚠️ "localhost" is `::1` first: a test server on 127.0.0.1 alone costs ~2 s per connection on Windows (2026-10-07)

`getaddrinfo("localhost")` answers `::1` before `127.0.0.1` (Linux and Windows alike), and `TLSConnection` tried the
endpoints in order. Linux refuses `::1` at once; Windows retries the SYN after the RST, so a refused connect there
costs ~2 s (Windows peer: 2021 ms on `::1` vs 0 ms on `127.0.0.1`) — every hermetic HTTPS test paid it, and a timing
assertion (the silent-peer test) failed on it. `HTTPSTestServer` therefore listens on `::1` too, same port, `v6_only`
(best effort: without IPv6 it stays IPv4-only; `listenOnIPv6 = false` keeps it IPv4-only on purpose). A new test
server or timing test: listen on both families, or connect by literal. Port 1 (nothing listening) still costs ~2 s
on Windows: a refusal there is slow by nature.

### ⚠️ A TCP connect to a resolved name goes through Network::connectFirstReachable() — Happy Eyeballs (2026-10-07)

The client side of the same defect: a sequential connect (`asio::async_connect` over the resolver's results) pays
the WHOLE failure of each address before the next — ~2 s per refused address on Windows, the system's SYN timeout
(minutes) per unanswered one. `Network/HappyEyeballs.hpp` (RFC 8305 § 4-5): `interleaveAddressFamilies()` alternates
the families from the first one (getaddrinfo already applied RFC 6724), `connectFirstReachable()` starts the next
attempt after `ConnectionAttemptDelay` (250 ms, § 8) or at once on a failure, keeps the first connected socket and
closes the others; the timeout bounds the whole race. Used by `TLSConnection::establishTcp()` and the engine's
`Net::TCPClient::connect()`. **Rules:** a new client connect uses it, never `async_connect` over a list; the
private-only check of the cleartext path judges EVERY resolved address before the race (any may win).
Tests: `NetworkHappyEyeballs.*` — the "unanswered" endpoint is a listener whose accept queue is full (SYN dropped on
Linux / macOS, slowly refused on Windows); `NetworkHTTPSClient.aNameResolvingToBothFamiliesReachesAnIPv4OnlyServerWithoutWaiting`.
**Accepted on three OS (2026-10-07, base `441a3ce`, engine `0b112e32`).**
- No `NetworkHappyEyeballs` test is skipped on any of the three: a full accept queue leaves the connect unanswered.
- The unanswered endpoint gives way after 450 ms on Linux, 468 ms on macOS and 469 ms on Windows.
- Windows:
  - the IPv4-only "localhost" HTTPS test takes 276 ms;
  - `NetworkTLSConnection` handshake tests: 2020 → 267 ms;
  - `readTimesOutOnMuteServer`: 2291 → 536 ms;
  - the `NetworkTLSConnection` suite: 8382 → 3105 ms.
- `Network*` × 20 green on Windows and macOS.
- MCP conformance: 1881 checks passed, 0 failed, on both.

## VertexFactory

### ⚠️⚠️ A triangle index from a FILE was trusted — the native format's raw blob (2026-10-17, FIXED)

`FileFormatNative` reads its triangles as a raw blob: their vertex, color and edge indices come from the file, and the
common read path (`readFiniteStream()`, FileIO / StreamIO) only refused non-finite values. A forged `.ee3d` whose
triangle pointed at vertex 999 of a 3-vertex shape was ACCEPTED, and the first consumer read out of bounds
(`VertexFactoryNative.aTriangleIndexOutOfItsTableIsRefused`, an ASan / libstdc++ assertion abort before). Owner
decision: every format. `Shape::hasConsistentIndices()` (vertex index below the vertex count, color index below the
color count when there are colors, edge index "none" or below the edge count) is checked by `readFiniteStream()`
beside `hasOnlyFiniteData()`. The native reader resets the stored edge indices to "none": the edges themselves are not
stored, so an index into them is stale. Demos geometry-generator (OBJ) and animation-debug (MD5, the data-store
geometries through `IndexedVertexResource`) load with 0 refusal.

### ⚠️⚠️ A sealed cut of a HOLLOW shape covered its holes; the cut points overflowed an int32 (2026-10-17, FIXED)

Found by the coverage pass (`VertexFactoryShapeSplitter.aTorusCutThroughItsHoleKeepsTheHole`,
`nestedLoopsKeepTheirGapsAndIslands`, `largeCoordinatesAreSealedWithoutOverflow`, all failing before):
- `ShapeProcessor::sealAllBoundaryLoops()` ear-clipped EACH loop on its own: the cut of a torus (an annulus — an outer
  and an inner loop) got a full disk over its hole plus a second disk on top (57 cap triangles in the hole; a solid
  sphere in a hollow shell: 46 in the gap). Closed, and the signed volumes CANCEL — a volume check does not see it;
  only the cap's position does. Owner decision: in-house hole bridging. With a cap normal (a planar cut), the loops are
  nested by inclusion in the cap plane (even depth: a contour; odd: a hole of its smallest enclosing contour) and each
  hole is bridged into its contour (D. Eberly, "Triangulation by Ear Clipping", § polygons with holes) before the
  existing ear clipper, which now ignores a bridge's duplicate of the candidate ear's own corners.
- `ShapeSplitter` welded the cut points by quantizing them to 1e-4 into an `int32_t` (`chainBoundaryEdges()`,
  `mirrorBoundaryLoops()`): undefined behaviour from 214 748 units on (2 km of a model in centimetres). The same
  unchecked conversion sat in `ShapeProcessor` (an `int32_t` grouping, an `int64_t` key) and `ShapeDecimator` (an
  `int64_t` key: a NaN was UB). All five go through `mergeGridCell()` now (no cell: a sentinel cell — the positions are
  finite by contract). Left as they are: the tree growers' spatial grids (`TreeColonizationGrower`, `TreeSkinner`),
  which quantize their own generated positions by their own cell size.

### ⚠️ The OBJ writer lost float precision; `computeVertexTBNSpace()` did not declare its normals (2026-10-17, FIXED)

Found by the coverage pass (`VertexFactoryOBJ.writerRoundTripKeepsEveryFloat`, `requestedNormalsAndTangentsInEveryFaceForm`,
both failing before):
- `FileFormatOBJ::writeStream()` wrote through an `std::ostringstream` at its default precision, 6 significant digits:
  0.123456789 came back 0.123457, 1.00000012 came back 1 — every exported model moved. It writes `max_digits10` digits
  now (9 for a float: a write / read round trip gives the same floats), in `std::locale::classic()` whatever the global
  locale. (`std::to_chars` would give the shortest form, but its floating-point overloads need macOS 13.3; the target
  is 12.0.)
- `Shape::computeVertexTBNSpace()` computes the vertex normals but did not set the "normals available" flag, which
  `computeVertexNormal()` sets: an OBJ `v/vt` file read with a tangent space (and every MD5 model) carried normals the
  shape declared absent, and the OBJ writer dropped them. It declares them now.

### `ShapeDecimator` can be interrupted — `setStopToken()` (2026-10-08)

A decimation of a large mesh lasts SECONDS, and the engine runs it on a worker (automatic LODs): a shutdown waited for
every running one. `setStopToken(std::stop_token)` (the token of the job's `TaskHandle`, Ave Robustus II D1) is read
every `CancellationCheckInterval` (4096) iterations of EVERY stage loop (deduplication read phase, initialisation,
corner UV table, quadrics, penalties, collapse queue, collapses, output, orientation check, normal-map bake); a stop
makes `decimate()` return an EMPTY shape (`isCancelled()` tells it from a failure; test
`aRequestedStopTokenStopsTheDecimation`). An unrequested token changes nothing (same triangles, same vertices — test
`VertexFactoryShapeDecimator.anUnrequestedStopTokenChangesNothing`). The transitional `setCancellationFlag(const
std::atomic_bool *)` (the engine's former global shutdown flag) is GONE since 2026-10-15: the engine's LOD jobs own a
`TaskHandle` and hand its token. ⚠️ The stop latency on 2.24 M triangles is **16 ms worst
on Linux, 2.5 ms on the M2, since `fd9dc5c`** (2026-10-09; Windows to re-measure), and its worst row is now the START
(allocating and zeroing the work mesh's flat table). History: 0.74 s before D7, bounded by the DEALLOCATION of the
stages' node-based containers; 25 ms after D7 step 4 (`9fb8f86`: a lean work mesh, the adjacency in one arena, every
hash table a `FlatHashMap`, a total collapse order); then the per-vertex lists became `ArenaList`s, trivially
destructible, so a stop no longer pays a million destructors (~20 ms on Linux, ~65 ms on a Windows laptop before
`fd9dc5c`). Same figures as § *A job that touches an object is `submit()`ted* above; item
`task-handle-and-stop-token`. ⚠️ Its QEM costs were 0 on fine meshes (`Vector::normalized()` zeroes small normals:
item `vector-normalized-absolute-epsilon`).
⚠️⚠️ **The decimation is the same on the three OS only since 2026-10-09 (D7 step 3):** it iterated per-vertex
`std::unordered_set`s, whose order differs per standard library — the same mesh gave three different decimations on
Linux, macOS and Windows (even triangle counts). Its adjacency lists are now insertion-ordered arrays; never iterate an
unordered container where the order drives the result (`FlatHashMap` with `PortableHash` iterates the same everywhere).

### ⚠️ No `.at()` in the base: an inconsistent shape is REFUSED, a construction invariant is `find()` + Debug `assert` (2026-10-08, FIXED)

`.at()` throws `std::out_of_range`, an abort under `-fno-exceptions` (Ave Robustus: no throwing std call). The 21 calls of
`src/` are gone (only `src/Testing` and `StaticVector`'s own non-throwing `at()` remain):
- **In range by construction** (a bounded loop, a check just above): plain `[]` with the bound stated —
  `FileFormatTarga` (the header is a list of `{field, bytes}` walked by range-for), `TextProcessor::write()`, the PNG
  row pointers, the OBJ face corners (`extractFaceIndices()` guarantees ≥ 3 indices).
- **From data**: a triangle referring to a vertex (or a vertex color) that does not exist. `Shape::indicesInRange()` tells
  it; `createIndexedVertexBuffer()` refuses (0, empty buffers — the engine's `IndexedVertexResource` already treats 0 as
  a failed upload; the vertex colors were read OUT OF BOUNDS there), `ShapeProcessor::findBoundaryLoops()` refuses
  (no loop), `operator<<` prints "OUT OF RANGE". `findBoundaryLoops()`'s canonical map became a dense vector.
- **Internal invariants** (the boundary walks of `ShapeProcessor` / `ShapeSplitter`, the UV charts' `globalToLocal`):
  `find()`, an `assert` in Debug, a defined degradation in Release (the walk stops, the candidate or the triangle is
  skipped — as a degenerate triangle already is).
Tests: `VertexFactoryShape.anInconsistentShapeIsRefusedNotAborted`, `aVertexColorIndexOutOfRangeIsRefused`.

### ⚠️⚠️ The decimator's QEM numerics: double quadrics, a RELATIVE singularity test, a point-quadric seam anchor (2026-10-09, FIXED)

Once `Vector::normalize()` gave the small triangles their normals back (above), the QEM still could not work on a fine
mesh: (1) quadrics and costs in FLOAT — a collapse's error is a small difference of large area-weighted terms (areas
~1e-4, errors ~1e-12): rounding noise, 22 % exact zeros; (2) the optimal-position solve was gated by an ABSOLUTE
`|det| > 1e-6` — det ~ area³ ~ 1e-12 on a fine mesh, never met: every collapse went to an end point or the midpoint, fans
grew around a few vertices (32 requeued neighbours per collapse instead of 9) and a 2.24 M-triangle decimation took
13.8 s; (3) the UV-seam "penalty" set all ten coefficients to the weight — not a valid (positive semi-definite) quadric:
its optimum was arbitrary and threw a seam vertex 0.053 off a unit sphere. Now: `Quadric` holds doubles (Garland &
Heckbert, SIGGRAPH 1997), the singularity test is `|det| > 1e-10 · trace³` (scale-free), and a seam vertex is anchored by
a weighted POINT quadric (squared distance to it). Measured: source → output max distance sphere300 0.035 → 0.00028,
geodesic 0.078 → 0.0029, capsule 0.112 → 0.011, torus 0.020 → 0.005, hollowedCube output → source 0.455 → 0; the
2.24 M decimation 13.8 s → 5.8 s; stop latency unchanged (21 ms). Test
`VertexFactoryShapeDecimator.aFineMeshIsDecimatedByItsQuadrics` (worst radial distance < 1e-3: 9.9e-3 → 1.9e-4).

### ⚠️⚠️ The computed tangent frame was backwards on every MIRRORED UV island — tangent AND handedness (2026-10-07, FIXED)

`Math::Vector::tangent()` normalises without dividing by the UV determinant r = Δu1·Δv2 − Δu2·Δv1: it answers
−sign(r)·dP/du, and nothing ever set the handedness. In the engine's UV space (V grows DOWN the image) an unmirrored
island has r < 0 — `generateQuad()`: T = +X, B = cross(N, T) = +Y, the image's up — so it was right; a MIRRORED
island (r > 0) got T = −dP/du with the default handedness +1: its normal map lit backwards in X. Owner decision:
derive the frame from the UV winding (Lengyel 2001 / MikkTSpace). `Shape::triangleTangentFrame()` gives each
triangle T = dP/du and a handedness (−1 when mirrored); `computeVertexTangent()` / `computeVertexTBNSpace()` give
each vertex its triangles' majority side and average only that side's tangents. Every computed path inherits it
(generators through `ShapeBuilder`, OBJ, the decimator, the UV unwraps of `ShapeProcessor` — which no longer mixes two
conventions in one shape). A vertex shared by both sides sits on a MIRROR SEAM and cannot be right for both: the OBJ
loader splits it (its vertex key is (v, vt, vn, side)); `Shape` itself does not (it would invalidate edges and
boundary loops). Tests: `computedTangentFrameOfTheReferenceQuad` / `…OfAMirroredUVIsland`,
`VertexFactoryOBJ.aMirrorSeamSplitsItsVerticesAndEachSideGetsItsFrame` (failing before).
⚠️ **Rendering changes on generated shapes** whose UVs run mirrored (census 2026-10-07): torus and capsule entirely,
the hollowed cube on half its triangles, one cap of the cylinder and the cone. Quad, cuboid, sphere and geodesic
sphere are unchanged. **Rule:** a tangent from UVs needs the sign of their determinant; never normalise it away.
Runtime proof (2026-10-07, projet-alpha `normal-map-debug --demo-options 4,0,3`, the torus, under the right / left omnis): its joints read exactly like the unmirrored reference quad's — the wall facing the lamp lit (~230), the other dark (0-20); 0 VUID.
⚠️ **`ShapeDecimator` folded UVs over** (27 of 448 triangles on a 50 % sphere; FIXED 2026-10-07, base item
decimator-uv-fold-overs): mostly NOT the collapses — its work copy is deduplicated by POSITION, so a UV seam's two
vertices were one, and the output gave it ONE UV: every triangle beside the seam spanned the texture backwards (~30 on a
32 × 16 sphere at ANY ratio). The output now takes each corner's UV from its source triangle (`CornerUVTable`: the work
triangles keep the source order and corner slots) and splits the seam again (one output vertex per vertex and UV); and a
collapse that would reverse a neighbouring triangle's UV winding is refused (`checkUVFoldOver()`, the UV counterpart of
the 3D flip check) — 3 of 192 at 25 % before it. Test `decimationNeverFoldsTheUVsOver` (0.95 / 0.75 / 0.5 / 0.25: 0
folded, the same triangle counts reached). **Rule:** a mesh simplified on a position-merged copy gives its output the
attributes PER CORNER, never per merged vertex.

### ⚠️⚠️ `Shape::transform()` zeroed every bitangent — a `Vector< 4 >` product picked the HANDEDNESS overload (2026-10-06, FIXED)

> [!CAUTION]
> `setTangent((M * Vector< 4 >(tangent, 0)).normalize())` resolves to `setTangent(Vector< 4 >)`, whose W is the
> bitangent handedness since `f2d4ba7` (2026-08-28): W = 0 → `biNormal() = cross(N, T) * 0`. Every transformed shape
> (a generator ending on `transform()`, `setCenterAtBottom`, the engine's `ResourceGenerator` transform matrix) lost
> its bitangent. Seen as the engine's "textured geodesic sphere renders black" (opened 2026-09-08): four attributions
> fell (POM, the UV transposition, the baked vertex colour, the winding) before a one-variable material A/B — albedo
> only lit, albedo + normal map black — pointed at the TBN, and a dump of the shape found B = 0 on 2619/2619
> vertices. Fix: the product goes through a `Vector< 3 >`, and the handedness flips under a negative determinant.
> Runtime proof (`light-and-shadow-debug`, pose (-4, 2, 8) → (-4, 2, 1), f/16 · 1/125 · ISO 100): sphere crop
> 8.8 → 183.2 / 255 (UV sphere 182.4), 0 VUID. **When a material renders black on ONE geometry: A/B the material's
> components one at a time first, then dump the shape's T, B, N — before any hypothesis.**
> The same review fixed two neighbours (same day): `transform()` now carries normals by the inverse transpose
> (cofactor form), and `ShapeDecimator` / `ShapeSplitter` copy the handedness of the vertices they rebuild — a
> mirrored glTF island lost its -1 in every automatic LOD level and every split part.
> **ACCEPTED on the three OS (base 44911ae, 2026-10-07):** Windows (MSVC /W4 /WX, 0 warning) 2369 = 2366 + 3 skipped,
> sphere crop (179.4, 152.8, 121.3) on an RTX 3060 and (178.3, 151.7, 120.3) on the AMD iGPU, 0 VUID; macOS M2
> (MoltenVK) 2366 + 3 skipped, ASan/UBSan clean, crop (177.5, 153.5, 119.8), 0 VUID, 0 SYNC-HAZARD.

### ⚠️ `Grid`'s point count is computed in its INDEX type — a large division wrapped it (2026-10-01, FIXED)

> [!CAUTION]
> `Grid::pointCount()` is `(cellCount + 1)²` in `index_data_t`: with `uint32_t`, a division of 65535 wrapped the count
> to 0 (65536²), and `UINT32_MAX` wrapped `cellCount + 1` to 0, so `initializeByCellSize()` / `initializeByGridSize()`
> returned TRUE on a grid of 0 points and every later index walked out of it. Both now refuse a count above
> `Grid::MaxCellCount` (65534 for `uint32_t`, 254 for `uint16_t`: `2^(D/2) - 2`) and a non-finite size. Tests
> `VertexFactoryGrid.ACellCountWhosePointCountOverflowsTheIndexTypeIsRefused`, `ANonFiniteSizeIsRefused`. The bound is
> the TYPE's; a caller reading data caps far lower (the engine's JSON grounds: 4096, terrains: 16384).

### `exceedsStream()` is a PRE-READ bound — using it after the parse rejects every valid file (Aug 2026)

> [!CRITICAL]
> `FileFormatMDx::exceedsStream(file, count)` answers *"could this stream possibly hold `count`
> elements?"* by comparing the count to the **remaining file size**. It is a cheap DoS guard for a
> count that has just been read from the header and is **about to drive a read** — its only valid
> position is the parsing phase.
>
> Two calls in `loadMD5()` had drifted into the **post-parse** phases (vertex→weight conversion,
> skin build). By then the whole file has been consumed: the stream carries `eofbit`, `tellg()`
> returns **-1**, and the `current < 0` clause makes the guard answer **"exceeds" unconditionally**.
>
> **Effect:** `cyberdemon.md5mesh` — and every MD5 model owning a vertex with more than four
> weights, or simply owning a skeleton — was rejected with
> `readStream(), weight count exceeds the stream size !`. The file was perfectly valid.
> `animation-debug --demo-options 1` and the `ID/cyberdemon` store resource were both dead.
>
> **Both guards were also REDUNDANT**: the validation phase that runs right after parsing already
> checks the real invariants on the parsed data — joint parents, `weight.jointIndex < jointCount`,
> `startWeight + countWeight <= weights.size()`, triangle→vertex indices. That phase is what makes
> the later allocations safe; the stream has nothing to say about them. Both calls were removed,
> not replaced.
>
> **The rule:** an `exceedsStream()` call belongs where a count is read from the stream and drives
> the very next read. Anywhere downstream, the invariant to check is a **container size**, not a
> file size. A guard on an already-parsed, in-memory count is at best noise and at worst — as here
> — a permanent rejection.
>
> Regression test: `VertexFactoryMDx.md5VertexWithMoreThanFourWeightsLoads`
> (`src/Testing/test_VertexFactoryFileFormats.cpp`) — a synthetic MD5 whose first vertex declares
> five weights; asserts the load succeeds AND that the four largest biases survive, renormalized.

### ⚠️ `MemoryStream{buffer}` on a NON-const vector opens for WRITING

> [!WARNING]
> `IO::MemoryStream` has two constructors: `MemoryStream(const std::vector< std::byte > &)` reads,
> `MemoryStream(std::vector< std::byte > &)` writes into the vector. A test that builds its input
> in a **mutable** local and passes it directly picks the WRITE overload — every subsequent read
> fails with `failed to read stream data !`, **before the parser is ever reached**.
>
> A test asserting `EXPECT_FALSE(format.readStream(...))` then passes for entirely the wrong
> reason: it never exercised the loader. Bind the buffer to a `const` reference (or go through
> `StreamIO::read()`, which takes a `const &`) and check the log for that message before trusting
> a negative-path test.
>
> **Measured, not guessed**: run the whole suite and correlate each `[ RUN ]` with
> `failed to read stream data !` / `stream is not open !`. That sweep found **three** vacuous
> tests — `md3HostileSurfaceDoesNotCrash`, `md5HostileReferencesDoNotCrash`,
> `md3HugeTriangleTotalDoesNotOOM` — all three now **activated** (buffer bound to a `const &`).
> The other hits are legitimate: `NetworkTLSConnection.*` and the two `emptyStreamIsRejected`
> tests exercise those error paths ON PURPOSE.
>
> **What the activation revealed** (Aug 2026):
> - `md3HostileSurfaceDoesNotCrash` and `md3HugeTriangleTotalDoesNotOOM` now pass **for the right
>   reason** — `loadMD3(), triangle total exceeds the stream size !`. The MD3 hardening was sound;
>   it had simply never been executed. No 64 GB allocation.
> - `md5HostileReferencesDoNotCrash` **FAILS**: see below.

### `loadMD5()` used to return SUCCESS on a shape it built from nothing (Aug 2026, FIXED)

> [!WARNING]
> `loadMD5()` ends on an unconditional `return true`. Fed the 136-byte hostile blob of
> `md5HostileReferencesDoNotCrash`, it parses no joint and no mesh, builds an **empty** shape
> (`Shape::computeTriangleTBNSpace(), geometry data is empty !`), attaches a 0-joint skeleton and
> skin — and reports **success**.
>
> It no longer crashes (the validation phase did its job), but a loader that succeeds with zero
> vertices and zero triangles pushes an empty resource down the pipeline instead of cancelling.
> The test's authored intent — *"the validation pass must cancel the load"* — is the correct one.
>
> **Fix (owner decision: keep it LOCAL to `loadMD5()`, do not generalise yet):** two
> post-conditions, both inside `loadMD5()`.
> 1. **Fail fast, end of Phase 1** — `joints.empty() || meshes.empty()` cancels the load before
>    any skeleton or geometry is built. A real MD5 always carries a skeleton and at least one
>    mesh. This also spares the caller the misleading `geometry data is empty !` TBN warnings the
>    old path emitted on its way to reporting success.
> 2. **Post-condition on the result** — `geometry.empty()` (no triangle) cancels the load. Meshes
>    can parse and still yield nothing; success on an empty shape is a false success.
>
> **Deliberately NOT done**: the same post-condition shared across MDL/MD2/MD3/MD5 (or hoisted
> into `FileFormatInterface`). That is a **contract** change, not an implementation detail — a
> format may legitimately describe an empty geometry, and no engine code relies on that today.
> Left as an open architectural question rather than decided silently.
>
> **Verified**: suite back to `1967/1967`, and the real `cyberdemon.md5mesh` still loads with its
> skeleton and 10 animation clips — the new checks reject garbage without touching valid models.

### ⚠️⚠️ Building a shape WAS quadratic in its triangle count — two separate linear scans (Sept 2026, both FIXED)

`ShapeBuilder` routes every triangle it emits through `Shape::addTriangle()`, which calls
`Shape::addEdge()` three times; with `dataEconomy` (the **default**) it also routes every corner
through `Shape::addVertex()` and `Shape::addVertexColor()`. Each of those four used to walk the
whole existing list. The primitives in `ShapeGenerator` are small enough (a 16x8 sphere is 256
triangles) that nobody ever felt it — a procedural generator cannot ignore it.

Measured on this workstation (GCC, `-O2`, `generateSphere`, 2026-09-21):

| Sphere | Triangles | before the fix, economy ON | before, economy OFF + dedup | after, economy ON | after, economy OFF + dedup |
|---|---|---|---|---|---|
| 64x32 | 4 096 | 56.9 ms | 25.8 ms | 48.5 ms | 1.7 ms |
| 128x64 | 16 384 | 925.6 ms | 329.3 ms | 567.9 ms | 6.3 ms |
| 256x128 | 65 536 | **14 012 ms** | **5 299 ms** | 8 743 ms | **24.9 ms** |

**BOTH FIXED** (`addEdge()` on 2026-09-21, `addVertex()` / `addVertexColor()` on 2026-09-22).
Each now looks its candidate up in a construction-time hash index that holds no geometry. Final
numbers on the 65 536 triangle sphere: **14 012 ms → 21.6 ms with data economy ON**, a **405x**
gain, and the path is linear.

⚠️⚠️ **Do not conclude from that sphere that the default is now always the faster path.** Which
one wins depends on the MERGE RATIO. The sphere merges 83 % of its corners, so the in-build hash
wins (21.6 ms against 25.5 ms for economy OFF + a batch dedup). A tree canopy merges almost
nothing — every leaf card carries its own positions and UVs — so the in-build hash pays an
insertion per corner for no merge: **122 ms batch against 208 ms in-build** on the aspen level
chain, 124 against 263 on the conifer. `TreeSkinner` keeps `enableDataEconomy(false)` for that
measured reason. This very session nearly shipped the opposite claim, generalised from the
sphere alone.

⚠️⚠️ **Making it fast CHANGED a merge semantic, deliberately** (owner decision, 2026-09-22).
`addVertex()` compared with `Utility::equal()`, an **absolute** epsilon of 1.19e-7. Epsilon
equality is not transitive, so **no hash can reproduce it** — a hashed merge is necessarily a grid
merge. The grid (1e-4, the same `ShapeProcessor::deduplicateVertices()` uses, so the library's two
merge paths agree) merges **more**: 33 169 vertices against 36 405, because that epsilon is finer
than the rounding a generator's own trigonometry produces.

⚠️ **Progressive merging is order-dependent at a cell boundary**, the batch pass is not. Expect a
handful of vertices of difference between them — 2 of 2 145 measured on a 64x32 sphere. Never pin
an exact vertex count across the two paths.

⚠️ **`generateSphere()` is STILL not watertight in the edge sense, and that is correct.** 48
unpaired edges on a 16x8 sphere, after the change as before it. The two sides of a UV seam carry
u = 0 and u = 1 and the poles fan out: those vertices are genuinely distinct whatever the
tolerance, and merging them would break the mapping. ⚠️ An earlier version of this section blamed
the seam on the epsilon comparison — it never was that. "A closed shape has every edge paired" is
FALSE here, and a unit test written on that premise fails on correct code.

⚠️⚠️ **A value without a grid cell is never merged** (2026-10-16, FIXED; found by `fuzz_mdx`). The cell was
`static_cast< int64_t >(std::round(value / tolerance))`, unchecked: undefined behaviour for a NaN, an infinity, or a
coordinate beyond about ±9.2e14 at the default 1e-4 cell. On x86-64 every such value became INT64_MIN, so unrelated
far vertices — and every NaN vertex — MERGED into one (measured: 6 such vertices → 1). Both merge paths now go
through `mergeGridCell()` (`Shape.hpp`), which returns no cell for those values: `addVertex()` / `addVertexColor()`
store such a vertex without indexing it, `deduplicateVertices()` keeps it as it is. Owner decision 2026-10-16, both
forms: the merge never UB, AND the file readers refuse the data — `FileIO::read()` / `StreamIO::read()` go through
`readFiniteStream()` (`FileFormatInterface.hpp`), which refuses a shape whose `hasOnlyFiniteData()` is false and clears
it. ⚠️ A format's `readStream()` called DIRECTLY (the fuzzers do) does not refuse non-finite data: only the two
`read()` entries do. ⚠️ Fuzzing: UBSan does not halt by default — run with `UBSAN_OPTIONS=halt_on_error=1`, from a
scratch directory (the `fuzz-N.log` files land in the current one). Tests: `VertexFactoryShapeBuilder.
verticesOutsideTheMergeGridAreNeverMerged`, `.deduplicationNeverMergesVerticesOutsideTheGrid` (both failed before),
`VertexFactoryStreamIO.nonFiniteGeometryIsRefused`.

### `Shape::addEdge()` returned the index PLUS ONE (Sept 2026, FIXED)

`addEdge()` ended with `const auto newEdgeIndex = static_cast< index_data_t >(m_edges.size());`
**after** the `emplace_back` — that is the size, not the index of what was just inserted. So every
edge index stored in a triangle by `addTriangle()`, and one half of every shared-edge cross-link,
pointed at the **next** edge, and the last one pointed one past the end.

Measured on a 16x8 sphere before the fix: **0 of 768** edge indices joined the two vertices of
their own triangle corner, 767 were wrong and **1 was out of range**. `Silhouette.hpp:98-100`
dereferences exactly those indices (`edges[triangle.edgeIndex(0..2)]`), so silhouette extraction
read the wrong edges and, on the last triangle, read **past the end of the vector**. It is latent
rather than live only because nothing in the cascade calls `Silhouette` today.

Regression tests: `VertexFactoryShapeBuilder.triangleEdgeIndexesPointAtTheirOwnEdge` and
`…sharedEdgeCrossLinksAreReciprocal` — both fail on the pre-fix header, both pass after. Suite
`2051/2051`.


### ⚠️⚠️ A tree fork must DIVIDE the stem, never restart it (Sept 2026, FIXED before shipping)

Weber & Penn's stem splitting (`nSegSplits`) is the one place in the parametric grower where a
naive reading multiplies the tree instead of shaping it. Three things must hold, and the first
draft got all three wrong — with visible, escalating symptoms:

1. **A clone inherits the REMAINING segments**, not a fresh full-length stem. Restarting a stem of
   `nCurveRes` segments means the clone forks again at the same relative place, forever: it
   **overflowed the stack and segfaulted**. `StemRequest::segmentCountOverride` is what bounds it —
   a clone is always strictly smaller than what it replaces.
2. **The clones SHARE the children the stem was going to carry** (`StemRequest::childShare`), they
   do not each get the full budget.
3. **The clones carry on the stem's fork error** (`StemRequest::inheritedSplitError`), they do not
   each start a fresh accumulator.

With 2 and 3 reset, the `broadleaf()` preset reached the **400 000 segment ceiling** and 2 916 030
leaves; fixed, the same seed gives **7 276 segments and 49 410 leaves**. The ceiling
(`TreeParametricGrower::MaxSegments`) is what turned an out-of-memory into a diagnosable number —
keep it.

⚠️ The lesson generalises: when a recursive generator guards its recursion with a RELATIVE test
("stop when the remainder is below 2 % of my length"), the guard is worthless, because the clone's
own length is the new reference. Bound the recursion on something that strictly decreases in
absolute terms — here, a segment count.

### The colonization grower is seeded, so nothing may iterate a hash map

`TreeColonizationGrower::NodeGrid` is an `unordered_map` of cells. It is read **by key only**, and
each cell keeps its nodes in insertion order, so a given seed always regrows the same tree. Add a
loop over the map itself and the result starts depending on the hash table layout — the tree would
still look fine, and the bench comparing two captures of "the same" tree would quietly stop being
valid. `VertexFactoryTreeColonizationGrower.sameSeedGivesTheSameTree` is the guard.

Same reason the growers build their `Randomizer` **inside** `grow()`: it is seeded per call, so two
growers, or two calls, never disturb each other. The old stub used `std::srand`, which is
process-global.


### ⚠️⚠️ A vegetation level of detail keeps the CANOPY and drops the twigs, not the reverse (Sept 2026)

The first skinner pruned branches thinner than a threshold, and then dropped every leaf hanging on
a branch it had pruned — because a leaf floating where its twig used to be looks like a defect. It
is not: at the distance where a two-pixel twig is worth dropping, the canopy **is** the tree.
Measured on a quaking aspen, that rule cost the first coarser level **100 % of its foliage**
(97 500 triangles of leaves down to 0) while keeping 801 triangles of bare sticks.

Two more numbers that shape the ladder, same tree:

- The canopy is **97 500 of the 119 709 triangles** of the finest level. Halving the ring
  resolution and the leaf count *together* barely moves the total — the leaves have to fall
  **faster** (`leafFraction * factor²`), with the survivors enlarged by `1/sqrt(fraction)` so the
  coverage holds.
- Pruning at 2 % of the trunk radius per level ate every branch carrying foliage on the very first
  step. 1 % doubling per level is the ladder that works here.
- A skeleton of many short segments (space colonization: 874 segments in 402 branches) is barely
  reducible by radius and resolution alone — its triangle count is set by its TOPOLOGY. The axial
  stride, which skips ring stations, is what moves it: 12 591 → 6 286 → 4 476.

Final ladder, aspen levels 0 to 3: **119 709 / 29 457 / 7 403 / 1 736**.

### ⚠️⚠️ A tube built on per-segment frames corkscrews, and the AREA does not see it

A generalized cylinder must carry a **rotation-minimizing frame** along the branch (double
reflection, Wang, Jüttler, Zheng & Liu, ACM TOG 27(1), 2008). The segment frames cannot be used:
`makeTreeFrame()` derives its spin from the world axis least aligned with the direction, so it
flips when a branch crosses that threshold, and the two rings on either side of the flip are out
of phase.

⚠️ **The trap is in the measurement, not in the geometry.** A corkscrew is not a hole and not an
inversion: it passes every topological check, and it barely moves the surface area — measured
**1.002** of the analytic tube with the correct frame against **1.019** with per-segment frames.
The first version of the regression test asserted on the area, passed on **both** variants, and
therefore tested nothing. The quantity that separates them is the **longest edge**: 1.04–1.10
segment lengths against 1.42–1.46, because an out-of-phase ring makes the joining edges cut across
the tube instead of running along it.

The lesson is general: before trusting a test, run it against the defect it is supposed to catch.

### ⚠️ Renumbering the vertices invalidates the edges — all three passes did (Sept 2026, FIXED)

A `ShapeEdge` holds VERTEX indices and a `ShapeTriangle` holds EDGE indices, so any pass that
renumbers or splits vertices leaves both stale. Three did it, and all three were silent:

- `deduplicateVertices()` renumbered and remapped the triangles only. Measured on a 16x8 sphere
  taken from 768 to 153 vertices: **765 of the 768 edge indices wrong, and 615 edges still naming
  vertices that no longer existed**.
- `generateLightmapUV()` and `generateUVUnwrap()` SPLIT vertices along the seams, so triangles end
  up pointing at indices that did not exist when the edges were built. Same class, found by
  looking for the sibling rather than by a failure.

All three now end on `Shape::rebuildEdges()`, which clears the edge list and the pairing index and
re-runs `addEdge()` over the current triangles. ⚠️ That is only affordable **because `addEdge()`
became a hashed lookup on 2026-09-21** — rebuilding an edge list used to mean a quadratic scan,
which is presumably why nobody did it.

⚠️ `rebuildEdges()` also drops the boundary loops and clears `m_boundaryLoopsAnalyzed`: the
adjacency changed, so a loop found on the old one describes nothing.

Regression test: `VertexFactoryShapeBuilder.deduplicatingVerticesKeepsTheEdgeListValid`, which
fails on the pre-fix source with "an edge still names a vertex the merge removed".


## Math

### ⚠️⚠️ `normalize()` refused every vector under ~3.5e-4 — a "flat triangle" is tested RELATIVELY to its edges (2026-10-09, FIXED)

`Vector::normalize()` / `normalized()` tested `Utility::isZero(lengthSquared())`, an ABSOLUTE epsilon (1.19e-7 in
float): every valid vector shorter than ~3.45e-4 was refused — `normalized()` answered ZERO, `normalize()` left it
unchanged (not unit). The normal of every triangle with edges under ~2 cm was zero or not unit: the mesh decimator had
ZERO quadrics on fine meshes (99.9 % of its collapse costs were 0, the queue's tie-break chose the collapses), the
small gem facets were black, the collision normals of small triangles were not unit… Now (owner decision: at the root)
any non-zero finite vector normalizes, at any scale (`computeUnit()`: a squared length that underflows or overflows is
rescaled by the largest component first); only a zero or non-finite vector is refused.
**Rule — a degenerate (flat) triangle is a RELATIVE test:** `Vector::isDegenerateCrossProduct(cross, a, b)`
(|a × b| ≤ 16·ε·|a|·|b|) and `areNearlyParallel(a, b)`; `Vector::normal(A, B, C)` and `Vector::tangent()` answer EXACTLY
zero for a degenerate triangle (so sums of normals ignore it and `isZero()` detects it). Moved to it: `SAT`'s axes and
`pointInTriangleWithMTV()` (refused every triangle with edges under ~2.6 cm), `Space2D::SegmentSegment` (declared
every pair of short segments parallel), the decimator's quadric and boundary planes, the 11 gem generators. Kept on
purpose: `CartesianFrame::getUprightSpriteModelMatrix()` (a camera at the sprite's vertical, a distance guard with a
fallback). Audit of the 243 call sites (2026-10-09): ~140 safe, ~75 already wrong and fixed by it (small-triangle
normals), 8 that leaned on "short → zero" — guarded where a UNIT vector is projected (engine wheel `12374f4c`,
projet-alpha Fox / Paladin `168dd9b1`), judged safe otherwise (a push field's centre, `lookAt` with target ≈
position). The decimator's remaining QEM numerics: item `task-handle-and-stop-token`.
Tests: `MathVector/*.NormalizeAnyScale`, `NormalizeRefusesZeroAndNonFinite`, `MathVectorDegeneracy.*`,
`MathSpace2DIntersections.shortCrossingSegmentsIntersect`, `VertexFactoryShapeDecimator.aFineMeshIsDecimatedByItsQuadrics`;
`gemCutsKeepTheirGeometry`'s princess sumAbsNormal +6 (6 corners had a ZERO normal).

### `BSpline` / `BSplinePoint` constructors clamp 0 segments to 1 (2026-10-08, FIXED)

The setters refused 0 segments but the constructors took it, and `synthesize()` then divided by zero (non-finite
times). A constructor cannot refuse: `clampBSplineSegments()` makes 0 a 1 with a trace (owner decision, the setters'
minimum). Test `MathBSpline.zeroSegmentsAreClampedToOne` (non-finite times before the fix).

### ⚠️ `Utility::quickRandom()` for integers went OUT of [min, max] for 8 / 16-bit signed types — fixed 2026-10-01

`static_cast< number_t >(std::rand())` truncated rand() into a NEGATIVE `int8_t` / `int16_t`, so 46 % of
`quickRandom< int8_t >(0, 10)` were below 0, and `1 + max - min` overflowed (signed, UB) on a wide `int32_t` range. It is
computed in the unsigned type of the same width now (a wrapping difference, `rand() % (range + 1)`), always in range;
`bool` is excluded. Only the first RAND_MAX + 1 values of a wider range are reachable (RAND_MAX is 32767 on Windows):
`Randomizer` is the tool for real randomness. Proof: `BaseUtility.QuickRandomIntegerStaysInRange` / `…Bounds`
(failing before). Found from the engine triad 12 (`Animations::RandomValue`, whose Windows build had replaced the 8-bit
draws by a constant `Variant{0}`).

### ⚠️ `Vector / s` and `Quaternion / s`: IEEE 754 for floating-point types, exact-zero guard for integers

- Floating-point: `Vector::operator/(scalar)` and `operator/=` divide plainly, and so do `Quaternion`'s (aligned the
  same day; it returned an identity quaternion, `/=` left it unchanged, for `|s| <= epsilon`). A divisor below epsilon gives a finite
  result; an exact zero gives ±inf or NaN. Guard `s > 0` where the divisor can be zero (a depth, a length).
- Integer: an exact zero is guarded (an integer division by zero is undefined behaviour). `/` returns a zero vector,
  `/=` leaves the vector unchanged.
- Never gate a floating-point divisor with `Utility::isZero()`: its tolerance is absolute (`|s| <= epsilon`, 1.19e-7
  for a float) and rejects valid divisors.
- `Quaternion::inverse()` / `inversed()` and the other `Quaternion` helpers still gate on `Utility::isZero()` of a
  squared length: unchanged, an absolute test of the same kind (left for an owner decision).
- Proof: `MathVector.ScalarDivisionBySubEpsilonDivisor`, `MathVector.ScalarDivisionByZero`,
  `MathQuaternion.ScalarDivisionBySubEpsilonDivisor`, `MathQuaternion.ScalarDivisionByZero` (all failing before).

### ⚠️⚠️ `Matrix` singularity is RELATIVE since 2026-10-01; `inverse()` still returns the matrix ITSELF when singular

- `determinant()` skipped every cofactor term with `|value| <= epsilon`: `diag(1e-8, 1, 1)` gave 0. Only an exact zero is
  skipped now.
- `inverse()` / `isInvertible()` tested `|det| <= epsilon` (absolute): the inertia tensor of a 1 kg sphere of 10 cm
  radius (0.004 per axis, det 6.4e-8) came back un-inverted, an angular response 62,500× too weak. The test is now
  `|det| <= epsilon × ∏ (largest |entry| of each column)` (a Hadamard-type bound, scale invariant). For an affine 4x4
  (bottom row 0, 0, 0, 1) the bound uses the upper 3x3 only, so a large translation does not make a regular transform
  singular.
- `inverse()` keeps its contract (a singular matrix comes back UNCHANGED, with no signal). Use
  `tryInverse()` (`std::optional`) where a singular input must be detected (engine `MovableTrait` takes a zero
  inverse inertia then).
- Proof: `MathMatrix.DeterminantKeepsSmallTerms`, `InverseOfSmallRegularMatrix`, `InverseOfSingularMatrix`,
  `InverseOfAffineTransformFarFromOrigin`; a sponza pixel A/B in projet-alpha stayed inside the run-to-run noise.

### ⚠️⚠️ The octahedral map is 2-to-1 on the BORDER — two atlas cells legitimately hold the same view (Sept 2026)

`Math/OctahedralMapping.hpp` is the shared core of an imposter atlas: the baker asks
`octahedralCellDirection(cellX, cellY, gridSize)` which direction to render a cell from, and the
shader asks `octahedralBlend(direction, gridSize)` which three cells a view falls between. They
MUST agree, so they live in one header rather than one in each consumer.

**On the outer border of the square the parametrisation is not injective**: two different border
points denote the very same direction. On an 8x8 grid, cells `(3, 7)` and `(4, 7)` both decode to
`(0, -0.143, 0.857)`. Encoding that direction back lands on `(4, 7)`, so cell `(3, 7)` receives a
weight of `2.4e-07` for **its own** direction.

That is a property of the fold, not a defect: the blend still selects the right VIEW and its
weights still sum to 1. A baker may skip re-rendering a duplicate; it must **never** try to make
the border cells distinct, and it must never "fix" the blend to force a cell onto itself.

⚠️ **The trap is in the TEST, and it caught me.** The first version of
`aCellDirectionBlendsBackOntoItsOwnCell` asserted that a cell recognises its own **index** and
failed on `(3, 7)`. The property that matters for an imposter is the **direction it shows**, so the
test now blends the selected cells' own directions back together and compares that vector to the
one asked for (`aCellDirectionBlendsBackOntoThatSameDirection`). Assert on what the pixel will be,
not on the bookkeeping that gets there.

The three other tests cover what a hand-written octahedral map usually gets wrong: the round trip
over a dense direction sweep, the three weights being positive and summing to 1, and a small
rotation moving the encoded point only a little (the fold must not teleport across the square
away from the border).

Reference: Cigolle, Donow, Evangelakos, Mara, McGuire & Meyer, *A Survey of Efficient
Representations for Independent Unit Vectors*, JCGT 3(2), 2014, § 3.3.

**The HEMI variant has no such border (Sept 2026).** `hemiOctahedralEncode/Decode/CellDirection/Blend()` — the
imposter's mapping (owner decision: views above the horizon only) — turn the upper diamond by 45° to fill the square
(u = x + z, v = z − x): the square's edge is the horizon and every point of it a distinct direction, so there a cell
DOES blend back onto itself with weight 1 (`everyHemiCellBlendsBackOntoItself`). `imposterCellFrame(direction)` is
the camera frame a view is baked and read with (back = direction, up = +Y made orthogonal, −Z at the pole). The GLSL
copy is the engine's `Saphir/ImposterGLSL.hpp`: change both at once.

### A negative float converted straight to an unsigned integer is UNDEFINED — `PerlinNoise` did it for every coordinate below zero (Sept 2026, FIXED)

`PerlinNoise::generate()` found its lattice cell with `static_cast< uint32_t >(std::floor(x))`: undefined
behaviour for any negative `x`. x86 happens to wrap (so the noise "worked" on Linux and Windows), ARM saturates
to 0 — every point west or south of the origin would read cell 0 on the Mac. Fixed through a signed integer
(`static_cast< uint32_t >(static_cast< int32_t >(std::floor(x)))`, two's complement then wraps under the 255
mask and the noise stays periodic across zero). Tests `AlgorithmsPerlinNoise.*`. ⚠️ On x86 they pass without
the fix too: the discriminating machine is ARM.

### ⚠️⚠️ A seeded draw through std's distributions or `std::default_random_engine` is NOT portable (2026-10-02, FIXED)

The standard fixes `std::mt19937`'s sequence and seeding, not `std::default_random_engine` (a different engine per
standard library), nor `std::uniform_int_distribution` / `std::uniform_real_distribution` / `std::shuffle`. One seed
gave citadel three terrains: macOS off Linux by up to 1.05 m, Windows by up to 0.45 m. Every seeded draw goes through
`PortableRandom` (or `Randomizer`, which uses it): `docs/subsystems/source-tree/23-portable-random.md`. Only a
`std::random_device` seed may keep std's distributions. ⚠️ A golden test that fails on one OS is the defect, never a
value to re-record.
2026-10-08: `GameTools::CardDeck` / `CardHand` moved too (they were seeded from `std::random_device`, so not yet
wrong — until someone seeds them for a replay); `std::shuffle` is gone from `src/`, and std's distributions remain only
on `std::random_device`-seeded draws (`Dice`, WaveFactory noise / dither — checked that day).
`CardDeck::Where::Randomly` now inserts into any of the size + 1 slots: the draw was in [0, size − 1], so the slot
after the last card was never chosen (owner decision; test `GameToolsCardDeck.releaseRandomlyReachesEverySlotTheEndIncluded`).

### ⚠️ `back()` / `front()` on an empty container is UB that libstdc++ hides — `String::extractNumbers()` did it (2026-10-02, FIXED)

`extractNumbers("No digits here!")` ended with `output.back() == ' '` on an EMPTY string. libstdc++ reads the byte
before its inline buffer, still inside the string object (its length field, 0), so the test passed on Linux, under
ASan too; libc++ (macOS) keeps the short buffer at the object's start, so its ASan reported a stack-buffer-overflow.
Fixed (`!output.empty() &&`). The base sanitizer build now defines `_GLIBCXX_ASSERTIONS` on Linux, so libstdc++
checks `back()` / `front()` / `[]` too: the same test aborts there without the fix.

## PixelFactory

### ⚠⚠⚠ `Processor::resize(Linear)` is NOT a mip filter — use `Processor::downsample()` (Sept 2026, FIXED)

`resizeLinear()` is a bilinear RESAMPLE for display: one point per destination pixel, at x·(w−1)/W from the top-left
corner, so a 1 × 1 target is the corner pixel and the mean drifts at every halving. The engine built its BC7 mip
chains with it: a 23 % leaf mask (1022 × 2048) read 0.001 at 1 × 4, a 35 % one (1024²) 0.000 at 1 × 1, and distant
foliage vanished. `Processor::downsample(source, width, height, destination)` is an exact area-weighted box filter:
the mean holds at every size, odd and non-power-of-two widths included (tests `PixelFactoryProcessor.downsample*`).

### `Pixmap< pixel_data_t >` is **not** byte-typed — never `memset`/`memcpy` an element count

`Pixmap` is `template< typename pixel_data_t = uint8_t, … > requires std::is_arithmetic_v< pixel_data_t >`.
The default instantiation is `uint8_t`, and for **one byte per element only**, a byte-oriented
`std::memset(data, value, …)` happens to produce the right result and an element count happens to
equal a byte count. Both coincidences break for every other `pixel_data_t`.

HDR made this live: `FileFormatHDR` (RGBE `.hdr`) instantiates **`Pixmap< float, uint32_t >`**
— today from `emeraude-engine`'s `Graphics/CubemapResource.cpp` (equirectangular HDR → cubemap)
and from `Testing/test_PixelFactoryFileFormats.cpp`. MSVC surfaced it as a hard error, `/W4 /WX`:

```
Pixmap.hpp(2349): warning C4244: 'argument' : conversion de 'pixel_data_t' en 'int' (float)
  → fillChannel → initAlphaChannel → initialize → FileFormatHDR::readStream
```

That warning was **not cosmetic**. `std::memset(m_data.data(), 1.0F, bytes)` truncates the value to
`int` 1 and then smears the *byte* `0x01` over the buffer, so an alpha channel initialised to "one"
became `0x01010101` ≈ `2.4e-38F` — a silently black/transparent HDR image, not a compile nit.

**Rules**
- Filling a value: `std::fill(m_data.begin(), m_data.end(), value)`. It is not slower — the compiler
  lowers it to `memset`/vector stores when the element type permits. `<algorithm>` is included for it.
- Copying: a byte count is `count * sizeof(pixel_data_t)`. An element count is not a byte count.
- Use `Pixmap::one()` / `Pixmap::zero()` for the channel extremes; `one()` is `1` for floating-point
  types and `numeric_limits::max()` for integers, so a literal `1` is wrong for `uint8_t`.

Fixed sites: `fill(pixel_data_t)` (Grayscale/RGB branch), `fillChannel(Channel, pixel_data_t)`
(Grayscale branch — which additionally forgot `markEverythingUpdated()` before its early return),
and `zeroFill()` for consistency.

### ⚠️⚠️ `Pixmap::pitch()` counts BYTES; a `data()` offset counts ELEMENTS (2026-10-08, FIXED)

`pitch()` is `width × colorCount × sizeof(pixel_data_t)`: a byte count, right for a `memcpy` size, wrong as an index
into `data()` (a vector of `pixel_data_t`). `Processor` used it as both — `move()`, `shift()`, `shiftTextArea()`,
`blit()`, `crop()`, `mirrorX()` and its swap buffer (sized in bytes) were right for 8-bit pixmaps only and read / wrote
2× (16-bit) or 4× (float) too far. Fixed: offsets in elements, copy sizes in bytes; the swap buffer holds
`elementCount()` elements and is CLEARED at every use (it used to keep the previous swap's pixels where `move()` copies
nothing). `move()` / `shiftTextArea()` take `std::abs()` in 64 bits (`std::abs(INT32_MIN)` overflows an `int32_t`).
**Rule:** name a value `…Elements` or `…Bytes`, never `…Size`. Proof: `PixelFactoryProcessor.everyPrecisionMatchesEightBit`
(the same operations on 8-bit, 16-bit and float pixmaps give the same pixels), `moveVacatesWithEmptyPixels`,
`moveExtremeDirections`; ASan/UBSan green.

### ⚠️ A TrueType font size is the LINE height; a truncated `.ttf` loads as an INVISIBLE font unless checked (2026-10-08, FIXED)

`Font::readTrueTypeFile()` never rendered a glyph (the copy was commented out), so every `.ttf` was refused, and the
two TrueType tests were commented out too. Now (owner decision: keep TrueType) each of the 256 cells is rendered:
- **Size = line height.** The glyph array and `TextProcessor` use one fontSize-high cell per glyph. A NOMINAL (em) size
  puts the ascenders and descenders of most fonts outside it, so the size is requested with
  `FT_SIZE_REQUEST_TYPE_REAL_DIM`: ascender − descender = fontSize pixels. Every glyph sits on the baseline (the scaled
  ascender, rounded up, from the cell top); a negative left bearing shifts the glyph right inside its cell.
- Cells: Latin-1 codes through the face's Unicode charmap; a control code (C0, DEL, C1) is an empty cell as wide as the
  space; a code the font lacks shows the font's missing-glyph box (on purpose: a visible hole). `fixedWidth`: every
  cell as wide as the widest, the glyph centred.
- **Trust boundary:** the size must be in [1, 256] (the height of ONE glyph cell; owner decision: ample for any title —
  256 cells take ~10 MB for a usual font, 128 MiB at worst); a glyph wider than 8 font sizes is refused; the file is read
  through `IO::fileGetContents()` (UTF-8 paths) and opened from memory (`FT_New_Memory_Face`, the buffer declared
  BEFORE the face). ⚠️ FreeType is LENIENT with a truncated file: the face opens and every cut glyph renders EMPTY —
  measured with the test font cut to a third: the load "succeeded" with blank glyphs. An sfnt font (TrueType,
  OpenType, a `ttcf` collection) is therefore refused when a table of its directory ends past the end of the file.
- `TextProcessor(Pixmap &)` read a `Pixmap::area()` that does not exist: the template constructor had never been
  instantiated (the engine uses `setPixmap()`). It reads `rectangle()` now.
Tests: `PixelFactoryFont.trueTypeFontRendersItsGlyphs`, `trueTypeGlyphsShareABaseline`, `trueTypeFixedWidthCellsAreEqual`,
`trueTypeHostileInputsRefused`, and `PixelFactoryTextProcessor.write` with both TrueType blocks back.
Open: `TextProcessor` lays every font on a fixed grid (item `text-processor-proportional-advance`).

### `fill(const pixel_data_t * data, size_t size)` — the tiling was wrong four ways

The same byte-vs-element confusion, plus arithmetic bugs, in the Grayscale/RGB branch. All four are
fixed; recorded here because the *intended* semantics were never written down anywhere:

| Bug | Effect |
|-----|--------|
| `memcpy(…, m_data.size())` | byte count given an **element** count — copied ¼ of the buffer for `float` |
| `std::ceil(m_data.size() / size)` | integer division happens *before* `ceil`, so `ceil` was a no-op — last partial tile lost |
| `memcpy(…, data + shift, …)` | re-read the **source** at the destination offset — out of bounds past the first tile |
| `remain = m_data.size() % size` | `0` when `size` divides evenly → last tile copied nothing |

**Intended semantics, now implemented:** the source pattern repeats **cyclically from `data[0]`**
until the pixmap is full, the final tile being truncated to what is left. This matches what the
`GrayscaleAlpha` / `RGBA` branches of the same function already did element-wise, so the function is
now internally consistent. Both this overload and `fillChannel(Channel, const pixel_data_t *, size_t)`
also reject `data == nullptr` / `size == 0` up front — `size == 0` previously drove the interleaved
branches into an endless `data[0]` read.

No caller in the cascade uses these pointer overloads today (every engine site calls
`fill(const Color &)`), so the semantic correction carries no regression risk.
### ⚠️ A shift by a count of RECORDS is a shift by an unbounded amount — HDR legacy RLE (2026-10-07, FIXED)

Radiance's legacy RLE shifts a repeat count 8 bits further for each CONSECUTIVE repeat record, so the shift is driven
by the input. The fifth record shifted a 32-bit `dimension_t` by 32: undefined behaviour, which x86 executes as a shift
by 0 — a forged stream then read as a valid scanline (found by the new `fuzz_hdr`, 26 M executions). Refused past a
shift of 24 (`LegacyRLEMaximumShift`), and the count and `x + count` computed in 64 bits. **Rule:** a shift amount
that comes from data is bounded below the operand's width BEFORE the shift.

### An OOM-guard test proves nothing on a big-RAM host — the HDR reader allocated 51 GB from a 40-byte file (Aug 2026, FIXED)

> [!WARNING]
> `PixelFactoryFileFormats.hdrHugeDimensionsDoNotOOM` feeds a ~40-byte stream whose resolution
> line declares `-Y 65535 +X 65535` and asserts the load is refused. It **passed** on the
> development hosts (Linux 125 GB RAM + 96 GB swap, macOS, Windows) and **failed on Ubuntu** with
> `terminate called after throwing an instance of 'std::bad_alloc'`.
>
> Nothing platform-specific happened. `FileFormatHDR::readStream()` called
> `pixmap.initialize(65535, 65535, RGB)` **before** reading a single pixel byte: 4.29e9 pixels ×
> 3 floats = **51.5 GB**. A host that can back that reservation allocates it, zero-fills it, then
> reaches the truncated-scanline guard and returns `false` — the test goes green after **2716 ms
> and 16.8 GB of peak RSS**. A host that cannot, dies in the allocation. The test was measuring
> the machine, not the code.
>
> **⚠️ The library is built `-fno-exceptions`** (verified in the cascade build flags), so
> `m_data.resize()` inside `Pixmap::initialize()` cannot *fail*: a throwing `operator new`
> unwinding into a `-fno-exceptions` frame is `std::terminate`. `initialize()` returning `bool`
> therefore **cannot** report an allocation failure — every reader must bound the declared
> dimensions **before** calling it. There is no second line of defence.
>
> **Fix** (`FileFormatHDR::readStream()`, the same guard and rationale as `FileFormatTarga`,
> where `fuzz_targa` found the identical defect):
> - reject a resolution line wider than the pixmap dimension type — it is scanned as
>   `unsigned long` and `4294967296` used to truncate to `0`;
> - compare the declared pixel count against the **remaining** stream payload
>   (`stream.size() - stream.tell()`; both `FileStream` and `MemoryStream` implement `tell()`)
>   times a max-expansion factor of **128**, before `initialize()`.
>
> The 128 factor: the tightest legitimate encoding is the adaptive RLE — 4 row-header bytes plus,
> per each of the 4 component planes, 2 bytes per run of at most 127 pixels ⇒ **~16 pixels per
> payload byte**. 128 keeps a wide margin and matches the Targa constant. **Known trade-off:** the
> legacy RLE can in theory expand further (its repeat count is shifted 8 bits left per consecutive
> repeat record, so 12 bytes can cover a 65535-wide scanline), so a legacy scanline compressing
> better than 128:1 is refused. No Radiance-era or modern writer emits that, and the alternative
> is an unbounded allocation driven by an untrusted header.
>
> **Result**: 2716 ms / 16.8 GB peak RSS → **0 ms / 11.7 MB**. Suite `2029/2029`.
>
> **How to verify an OOM guard on a host with plenty of RAM** — do not trust the green verdict:
> ```bash
> # 1. Peak RSS must stay flat. A "DoesNotOOM" test that peaks in GB is not guarding anything.
> /usr/bin/time -f "peak RSS: %M KB" ./EmeraudeBaseUnitTests --gtest_filter='*DoesNotOOM'
> # 2. Emulate a small-RAM host in-place: cap the address space (here 4 GB).
> ( ulimit -v 4000000; ./EmeraudeBaseUnitTests --gtest_filter='*DoesNotOOM' )
> ```
> **Measured, not guessed** (2026-08-28): the sweep was run over all 20 tests whose name matches
> `oom|huge|large` — `targaHugeDimensionsDoNotOOM`, `md3HugeTriangleTotalDoesNotOOM`,
> `hugeVertexCountIsRejectedNotAllocated`, `forgedHugeFrameCountNeverOverAllocates`, … — and every
> one of them peaks at the bare process baseline (~11.5 MB). `hdrHugeDimensionsDoNotOOM` was the
> **only** guard in the suite that guarded nothing.
>
> **`FileFormatTIFF` had NO guard at all (found by the coverage pass, 2026-10-17)**: its comment credited
> `TIFFRGBAImageOK()` with bounding the raster, but that call checks the pixel FORMAT only. A 70-byte TIFF declaring
> 100000x100000 asked `raster.resize()` for 40 GB — `std::bad_alloc` (`tiffHugeDimensionsDoNotOOM` failed so before the
> fix). Now the same guard: the declared pixel count against `stream.size()` (libtiff seeks over the whole file) times
> **1024**, deflate's worst expansion (~1032:1) — owner decision. Peak RSS 14.9 MB; the five TIFF tests pass under a
> 4 GB `ulimit -v`. (⚠️ Never cap the address space of an ASan run: its shadow memory reservation fails at start.)
>
> **`Pixmap` itself WRAPPED its element count in uint32 (found 2026-10-17 by the JPEG reader's test)**:
> `elementCount()` / `pixelCount()` / `bytes()` default to `uint32_t`, and `initialize()` did
> `m_data.resize(this->elementCount())`. 65000x65000 RGB (12.7 G elements) allocated ~4.08 G and REPORTED its real
> size; libjpeg-turbo then wrote 12.7 GB into it — a SEGFAULT in `jsimd_ycc_rgb_convert_avx2`, not a clean failure.
> 65536x65536 grayscale (2^32) allocated nothing and `initialize()` returned `true`. Owner decision: every entry
> point (both `initialize()`, the three constructors, `addAlphaChannel()`) refuses a pixmap beyond the uint32
> accounting of its getters (`elementCountFits()`, 4.29 G elements — the TYPE's limit; the arbitrary ceiling refused
> above stays refused); `pixelCount()` multiplies in 64 bits. Test `Pixmap.elementCountBeyondUint32IsRefused` (it
> failed on the former code: `true` for both). The JPEG reader got the payload guard as well (× 1024; a flat
> PROGRESSIVE JPEG can compress beyond it and is refused — owner decision).
>
> **Deliberately NOT done (owner decisions, 2026-08-28):**
> - **No allocation ceiling inside `Pixmap::initialize()`.** The guard stays the *reader's*
>   responsibility. A ceiling would be a foundation contract change needing an arbitrary constant,
>   and a legitimate pixmap can be enormous (16K×16K RGBA float = 4 GB). A nothrow allocator for
>   `m_data` was rejected for the same reason it is tempting — it changes the type behind
>   `data()`/`std::span` and the whole API around it.
> - **The test keeps asserting only the return value.** A forked death test capping `RLIMIT_AS`
>   (which *would* fail on any host) and a self-measured peak-RSS assertion were both considered
>   and refused: POSIX-only / three platform implementations to maintain, for a defect the recipe
>   below catches. **Consequence to accept: a future regression of this class will again only be
>   visible on a small-RAM host, or by running the two commands below.**
>
> Both axes must be applied to **every** `DoesNotOOM` / `HugeDimensions` test — this class of test is
> the one most likely to pass for the wrong reason. `FileFormatHDR` was added (`ceb83c2`) after
> the fuzzing campaign (`42a26bb`), so it has **no fuzz target**; the unit suite on a smaller host
> is what caught it (`docs/todo/fuzz-hdr-target.md`).

## Compression — the LZMA stream classes are reusable; the Compressor's threads are clamped (2026-10-18)

Found by the coverage pass (`Compression.LZMAStreamClassesAreReusable`):
- `LZMA::Compressor` / `LZMA::Decompressor` initialized their liblzma stream once, in the constructor: a finished
  stream produces nothing more, so a SECOND `compressString()` / `decompressString()` on the same object answered TRUE
  with 0 bytes (and the old decompressor even "accepted" a truncated input that way). Owner decision: reusable — every
  call after the first goes through `restart()` (`lzma_end()`, a fresh `LZMA_STREAM_INIT`, the constructor's
  parameters); a call that failed halfway no longer leaves a stream in mid-flight. The classes have no user in the
  cascade (the free functions of `LZMA.cpp` serve); `fuzz_compression` now feeds one `Decompressor` kept across the
  whole campaign.
- `Compressor::initMT()` computed `max(min(1, cpus), maxThreads)`: the request, uncapped (2000 threads: 838 MB peak). The
  Decompressor's identical formula was fixed on 2026-10-17 and this one was missed; both are `clamp(maxThreads, 1,
  cpus)` now (the 2000-thread request runs 32 threads on the 32-thread bench: 700 MB, the encoder's per-thread memory
  at preset 6).
- The test failed on the former code (an empty second output; a truncated input accepted).

## Variant — an 8-bit integer prints as a number; the type order is checked at compile time (2026-10-18)

Found by the coverage pass (`Variant.*`): `operator<<` streamed an `int8_t` / `uint8_t` as is, i.e. as a CHARACTER —
`Integer8{65}` printed "A", `UnsignedInteger8{0}` a NUL byte, `{200}` a raw byte (invalid UTF-8 in a log). They print
as numbers now. `type()` casts the storage index to `Type`: the order of the enumerators and of the `Storage`
alternatives was a comment to keep in mind; 22 `static_assert`s check it now (a swap of two alternatives fails to
compile). The printing test failed on the former code.

## IO — the FileIO writers write aside; ZipReader re-opens cleanly and opens an empty archive (2026-10-18)

Found by the coverage pass (`IOFileUtils.*`, `ZipArchive.*`):
- **A refused write destroyed the existing file**: `PixelFactory`, `VertexFactory` and `WaveFactory` `FileIO::write()`
  opened — truncated — the target BEFORE refusing an extension (.bmp, .tif, .xyz), a precision (float -> PNG), a
  read-only format (.md2) or an encoder failure (an invalid pixmap / wave): with `overwrite`, "precious content" became
  0 bytes; a refused new file was left empty. Owner decision: the writers encode into a `MemoryStream`, then
  `IO::filePutContentsAside()` writes a temporary sibling (`IO::temporarySiblingPath()`: "<name>.writing-<process
  token>-<counter>", unique per call — two writers of one target never share it) and `IO::renameFile()` replaces the
  target only once it is whole; any failure removes the temporary file. Cost: one in-memory copy of the encoded file.
  A side effect: those writers no longer open the target at all, a FIFO there is replaced, not blocked on.
  Windows: replacing a file another program holds open WITHOUT `FILE_SHARE_DELETE` (a viewer showing that very PNG)
  fails with a sharing violation where Linux succeeds — the write is then refused, the temporary file removed and the
  old file kept (Windows peer, 2026-10-18; screenshots get unique names and never hit it).
- `ZipReader::open()` called twice read the entry list again on top of itself (each entry listed twice): it closes
  first now. A valid EMPTY archive answered false while staying open (`isOpen()` true, `isArchiveFile()` true): it
  opens (owner decision), `extractAll()` succeeds with nothing to write.
- `IO::filesize()` of a DIRECTORY: MSVC's `file_size()` answers its size with no error (implementation-defined;
  libstdc++ fails with EISDIR) — a regular file is checked first (Windows peer, 2026-10-18).
- The writer and zip tests failed on the former code; the directory size is proven red by the Windows run.

## IO — every reader refuses what is not a regular file; four contracts kept (2026-10-18)

Found by the coverage pass (`IOFileUtils.*`):
- **A FIFO without a writer blocked `std::ifstream::open()` FOREVER**: a path from data (a store, a settings file, a
  dropped file) could freeze a loading thread. 7 of the 9 readers tested blocked; `FileStream` and `INIParser` opened
  a DIRECTORY as a file. Owner decision: one helper for every reader — `IO::isReadableRegularFile()` (a symbolic link
  to a regular file is followed; a FIFO, a device, a socket or a directory is refused and logged; a missing file stays
  silent, the caller's to report) and `IO::openInputFile()` (the check, then the open). Through it: `fileGetContents()`
  (both), `fileGetRange()`, `FileStream`, `FastJSON::getRootFromFile()`, `INIParser::read()`, `MD5AnimParser::parse()`,
  `ZipWriter::readSource()`, `HTTPServer`'s file body; `ZipReader` checks before libzip opens the path itself. The
  check and the open are two calls: swapping the file for a FIFO between them needs write access to its directory.
  The writers have the same defect: `docs/todo/writers-refuse-non-regular-files.md`; the engine's own readers:
  engine `docs/todo/readers-through-io-open-input-file.md`.
- **A pseudo-file is refused explicitly** (owner decision): /proc cannot seek to its end, or claims 0 bytes while it
  holds some — `IO::fileSizeFromStream()` answers -1, `fileGetContents()` says "the size is unknown" (it used to fail
  as "unable to read"). Read /proc line by line with a stream (the engine does).
- `filesize()` answered `static_cast< uintmax_t >(-1)` on an error (a missing file was 18446744073709551615 bytes); its
  contract is 0.
- `createDirectory(path, true)` on a bare file name called `create_directories("")`, EINVAL: `filePutContents("out.json",
  …, createDirectories = true)` failed. No parent = the current directory = nothing to create.
- `isDirectoryContentEmpty()` answered true for an empty FILE (`std::filesystem::is_empty()` does).
- `directoryExists()` logged a path crossing a file (ENOTDIR) as an error, and on Windows a missing parent
  (ERROR_PATH_NOT_FOUND, 3 — only 2 was silenced): both compared through `std::errc` now.
- The tests failed on the former code (the FIFO ones by timing: a releaser opens the write end after 1 s, so a
  regression fails instead of hanging the suite — POSIX only, Windows has no FIFO in the file system).

## PixelFactory — a proportional pixmap font keeps each glyph's aspect; the text advances in pixels (2026-10-18)

Found by the coverage pass (`PixelFactoryFont.*`, `PixelFactoryTextProcessor.*`), two defects that hid each other:
- `Font::parsePixmap()` resized every proportional glyph to the CELL's aspect: at any size but the cell's, the font came
  out monospaced and stretched ('i' and '.' as wide as 'W'). The engine's default font (`FontResource::load()`, an
  embedded bitmap with 16-px cells) was right at 16 px only, stretched at 24 and 32; on the 32-px-cell
  `fixed-font.tga`, 16 and 24 were. Each glyph is now scaled with its own aspect ratio (at least 1 px wide); a
  fixed-width glyph keeps the cell's width.
- `TextProcessor` laid every character on a grid of the WIDEST glyph's width: a cropped proportional glyph sat in a
  cell as wide as 'W', the text letter-spaced like a monospaced font (the engine `Notifier`, 16 px: "U n a b l e").
  The pen advances in pixels now: a proportional font by each glyph's width plus the array's `letterSpacing()`, a
  fixed-width font by its cell (the same grid as before). Word wrapping is measured in pixels; the LAST word of a text
  wraps whole too (it used to be cut where the row ended: the space search ignored a word without a space after it),
  and a line feed ends a word.
- `ASCIIGlyphArray::letterSpacing()`: the crop of a proportional pixmap glyph drops its side bearings, so its array
  carries one pixel of spacing per 16 pixels of height (`PixmapLetterSpacingStep`); a TrueType cell is as wide as its
  advance and a fixed-width cell includes its margins: 0. `TextMetrics::maxColumns` is gone (a pixel layout has no
  column count).
- The space is the font's own: the empty cell's half width (8 px at 16 on the engine font). `fixed-font.tga`'s space
  cell holds ONE stray pixel of value 1, so `getUsableWidth()` crops it to a 1-2 px glyph: that test asset spaces its
  words badly in a proportional layout; the engine font is not affected.
- The proportional tests failed on the former code; the fixed-width grid test passes on both (the grid is unchanged).

## PixelFactory — the PNG reader bounds the declared size and refuses a short read (2026-10-17)

Found by the coverage pass (`PixelFactoryFileFormats.png*`):
- No size guard: 66 bytes declaring 30000 x 30000 RGBA allocated the 3.6 GB pixmap before libpng failed on the empty
  data (3.5 GB peak RSS; the test passed on its result only — the peak RSS is the proof). The raw image data the header
  implies, (row bytes + 1 filter byte) x height at the file's depth, is now bounded by the stream size x 1032 (deflate's
  maximum, the ZIP / TIFF / JPEG rule) BEFORE the allocation: 15 MB peak.
- The libpng read callback ignored a short read: libpng decoded on with a buffer the stream never filled (it caught
  the damage by luck — a CRC, an end of data). A short read calls `png_error()` now (it longjmps to the reader's
  setjmp, both phases armed).
- The 7 667 PNG files of the projet-alpha data stores and base resources load under the guard (0 refused); every
  colour type and depth (1 / 4 / 8 / 16 bits, gray, gray + alpha, RGB, RGBA, palette with and without tRNS) decodes as
  Pillow does; the writer's five filters and Adam7 interlacing round-trip. `fuzz_png` (the reader instrumented, a 1 GB
  RSS limit) 10 minutes: 217 M executions, 0 crash / OOM.

## PixelFactory — the Targa reader decodes every TGA 2.0 variant, oriented by the descriptor (2026-10-17)

Found by the coverage pass (owner decisions: implement the variants, follow the specification for the orientation):

- **The vertical order comes from the image descriptor's bit 5 (1 = top to bottom), never from `yOrigin`.** The reader
  used to flip whenever `yOrigin == 0`: a top-left file written with `yOrigin` 0 loaded UPSIDE DOWN
  (`projet-alpha.data` `Images/Woods/Stomp001-color_b.tga`, the only one of the 48 data-store files concerned). Bit 4
  (right to left) is honoured as well. The 48 files decode byte-identical to Pillow 11.1 (47 before the fix).
- **The colour-mapped (types 1 / 9), 15 / 16-bit true-colour (A1R5G5B5, the alpha bit counted only when the descriptor
  declares one) and 16-bit grayscale + alpha variants were ACCEPTED and decoded as garbage**: the indices were copied
  as colours, two 16-bit bytes as two channels. They are decoded now; an index outside the colour map refuses the file.
  A colour map beside true-colour / grayscale data (allowed by the specification) is skipped — it used to be read as the
  first pixels. A depth the kind does not define, an unknown type or colour map type: refused.
- **Uncompressed data larger than the stream is refused before any allocation** (the × 128 guard only bounds an RLE
  stream).
- **The writer was right and is unchanged**: it writes bottom-up (bit 5 clear) and mirrors only a top-down pixmap;
  with `invertYAxis` the pixmap is already bottom-up. Every pixel is now decoded straight to its place: no mirror and no
  channel-swap pass on read.

Tests `PixelFactoryFileFormats.targa*` (hand-built files: each orientation, each depth, colour-mapped raw / RLE / with a
first entry index, the refusals, grayscale RLE, an ID field and an attached colour map, the writer round trips); seven
failed on the former reader. `fuzz_targa` on the new reader (ASan + UBSan, seeded with the variants): 574 M executions
(10 + 3 min), 0 crash / OOM / timeout.

## Compression — a fuzz target must instrument its parser; ZLIB and LZMA bounded by what is really there (2026-10-17)

- **Six fuzz targets fuzzed their own `.cpp` parser BLIND**: `build-fuzzers.sh` linked them against the g++-built
  archive, uninstrumented — no coverage feedback, no ASan / UBSan on that code (`fuzz_http_response`: 59 edges, 654
  instrumented). The targets now compile their parser sources (`src/Fuzzing/README.md`). The first instrumented
  `fuzz_compression` run found the defects below at once (6 OOMs).
- **`ZLIB::decompressStream()` allocated a chunk's two buffers from its HEADER**: sizes capped at 1 GiB and the ratio
  checked (× 1032), but the declared compressed bytes were never required to be present — 16-74 bytes made 0.1-2 GB
  peak. The payload is now read in 1 MiB steps, the buffer growing with what arrives; the destination is allocated
  after (~4 MB peak on the same inputs). `Compression.ZLIBChunkHeaderWithoutItsPayloadIsRefused` (result-only: the proof
  is the peak RSS above).
- **LZMA had no output ceiling — a real bomb**: 156 KB of xz → 1 GiB of zeros, 2.1 GB peak, accepted (the function and
  the streaming `Decompressor`). Now refused beyond the input × `LZMA::MaxDecompressionRatio` (1032, every base reader's
  rule — owner decision). `Compression.LZMABombBeyondTheRatioIsRefused`.
- **The decoder memory limit is a HARDENING, not a proven defect**: 1 GiB in the function, none in the streaming class.
  An 80-byte header declaring a 512-768 MiB dictionary RESERVED it without touching it — no RSS on Linux. ⚠️ The
  fuzzer's OOMs were first attributed to it; they were ZLIB's (the harness calls both): measure each function on its own
  before naming a culprit. Owner decision, kept knowingly:
  `LZMA::DecoderMemoryLimit` = 65 MiB (the xz -9 preset's need, measured; 64 MiB refuses it) for both, multithreaded
  included (liblzma then falls back to one thread). `Compression.LZMAHeaderDeclaringALargeDictionaryIsRefused`.
- The streaming class's thread count was `max(min(1, cpus), requested)` — the request, uncapped: now clamped to
  [1, processor threads].
- After the fixes, `fuzz_compression` instrumented, 10 minutes: 41 M executions, 0 OOM / crash.

## UBSan — GCC's `-fsanitize=undefined` does NOT check a float-to-integer conversion (2026-10-17)

A `double` converted to `uint32_t` out of range is undefined behaviour; Clang's `undefined` group reports it
(`float-cast-overflow`), GCC's leaves it out. The base unit tests run under GCC's ASan + UBSan, so the MIDI renderer's
out-of-range sample count (a 36-byte file, `docs/subsystems/wavefactory/06-input-robustness.md`) passed them;
a clang probe found it. `EMERAUDE_ENABLE_UBSAN` now adds `-fsanitize=float-cast-overflow` on GCC (owner
decision). A float computed from input and converted to an integer is saturated BEFORE the conversion (NaN included).

## The paranoid warning set — what a base-only check does not see (Ave Robustus II, 2026-10-08)

- **A base TEMPLATE header is only judged where it is instantiated.** `TextProcessor`, `Math::linearInterpolation()`
  with `double`, `Statistics::RealTime< system_clock >` compiled clean in every base TU and failed `-Werror` in the
  engine, the first place they are instantiated with those types. **Rule:** a base change is verified by the WHOLE
  cascade build (base, engine, projet-alpha), never by the base alone.
- **`cmake --build .` does not build `EmeraudeBaseUnitTests`**: build that target explicitly, or the test count stays
  the old one and a new test "passes" by not existing.
- **The umbrella's compile OPTIONS did not reach the object modules**: `-fopenmp` sat on `emeraude_base` only, so
  VertexFactory's `#pragma omp parallel for` loops compiled inside the vertex module were silently serial (found by
  `-Wunknown-pragmas`). `CMakeLists.txt` now mirrors `INTERFACE_COMPILE_OPTIONS` onto `emeraude_base_flags`, like the
  definitions and include directories.
- **`-DMACRO=Off` is not a boolean to the preprocessor**: `JSON_USE_EXCEPTION=Off` evaluated as an undefined identifier
  (0 by accident) in jsoncpp's `#if`; `-Wundef` caught it. Use `0` / `1`.
- **Measure an interval with `std::chrono::steady_clock`**: libstdc++'s `high_resolution_clock` IS `system_clock`, which
  NTP moves backwards; the base timers defaulted to it until 2026-10-08.

## Tests — `IO::MemoryStream`'s constructor picks the MODE by the vector's constness (2026-10-17)

`MemoryStream(const std::vector< std::byte > &)` READS; `MemoryStream(std::vector< std::byte > &)` WRITES into the
vector. A test that builds its input in a vector it then modifies (`auto bytes = ...; bytes.resize(...);`) and passes it
non-const gets a WRITE stream: every `read()` fails at once, and a "the reader refuses this input" test passes for the
wrong reason. Found on 2026-10-17 in four tests: three new ones (TIFF garbage, WebP garbage / truncated) and
`targaUnsupportedPixelDepthRejected`, which had never reached the pixel-depth guard it is named after. Pass a read
source as `std::as_const(bytes)` — and check in the test's log that the refusal comes from the guard it targets.

