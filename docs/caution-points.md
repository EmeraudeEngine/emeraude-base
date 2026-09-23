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

## Network

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

### ⚠️ A test server that stops at the header terminator cannot prove a request body

`HTTPSTestServer` read until `\r\n\r\n` and handed that to the handler. A POST body therefore
appeared only if it happened to share the last TLS record with the headers — so a test asserting on
it passed or failed **by timing**, which is worse than not testing it. It now reads out what
`Content-Length` announced (`Testing::declaredContentLength()`).

## VertexFactory

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

### A negative float converted straight to an unsigned integer is UNDEFINED — `PerlinNoise` did it for every coordinate below zero (Sept 2026, FIXED)

`PerlinNoise::generate()` found its lattice cell with `static_cast< uint32_t >(std::floor(x))`: undefined
behaviour for any negative `x`. x86 happens to wrap (so the noise "worked" on Linux and Windows), ARM saturates
to 0 — every point west or south of the origin would read cell 0 on the Mac. Fixed through a signed integer
(`static_cast< uint32_t >(static_cast< int32_t >(std::floor(x)))`, two's complement then wraps under the 255
mask and the noise stays periodic across zero). Tests `AlgorithmsPerlinNoise.*`. ⚠️ On x86 they pass without
the fix too: the discriminating machine is ARM.

## PixelFactory

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
