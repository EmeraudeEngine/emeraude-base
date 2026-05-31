# Plan — "Ave robustus!"

> **Status:** VALIDATED 2026-05-31 (owner-approved; feature freeze + perimeter in effect). Not yet executed.
> **Type:** Long-term governance plan for **emeraude-base**.
> **Scope:** emeraude-base first. The engine and projet-alpha inherit the hardening later.

---

## 0. North star — *robustness of software intent*
    
This plan is **not** "make the code not crash." That is only a part of it.

> **"Ave robustus!" means: there is no gap between what the software *claims to do*
> and what it *actually does* — and what it does is proven dependable and proven useful.**

A half-finished feature betrays the software's intent just as much as a segfault does.
A capability that is *announced* (in `README.md`, `AGENTS.md`, `module-map.md`) but only
partially delivered is an **unkept promise**. A capability that exists but is fragile,
incomplete on hostile input, or never exercised by a test is an **untrusted promise**.
"Robustus" closes both kinds of gap.

This is a **consolidation epoch**, not a feature push.

> [!IMPORTANT]
> **Feature freeze.** While this plan is in progress, **no new features** are added to
> emeraude-base. Every effort goes to finishing, hardening and proving what is already
> intended. New capabilities resume only once "Ave robustus!" is declared complete.

> [!IMPORTANT]
> **No fix without a test (owner-ruled 2026-05-31).** Every correction — bug fix, hardening
> change, or finished capability — **MUST ship with an associated unit test** in
> `EmeraudeBaseUnitTests`, in the same change. A fix with no test is an incomplete delivery.
> This is how a fix becomes *proven* rather than *claimed*: the test fails before the fix and
> passes after (and, for hardening, runs green under the A.1 sanitizers). No exceptions —
> including the A.0 compile-breakers.

---

## 1. The two axes

Every capability of every module is measured against its declared intent, producing
**two kinds of gap** to close:

| Axis | Gap | Meaning | Response |
|------|-----|---------|----------|
| **A — Execution robustness** | *present-but-fragile* | The capability exists but can betray the intent via crash / UB / undefined behaviour on bad input. | Harden it (error contract, I/O boundaries, tests, tooling, memory). |
| **B — Completeness robustness** | *promised-but-missing* | The capability is announced but unfinished or absent. | Finish it, then **prove its utility** (a test that shows it serves its purpose, not merely that it exists). |

The two axes **meet**: a feature finished under Axis B then enters the Axis A pipeline
(tests → boundary hardening → memory). Nothing is "done" until it is both **complete**
and **hardened**.

---

## 2. Phase 0 (foundational) — The Intent Contract

Before any code changes, we build the **inventory** that defines the real perimeter of
the plan. For each module, we write what it **promises**, then measure **promised vs
delivered**. This produces the list of Axis-A and Axis-B gaps.

> [!IMPORTANT]
> **Adjudication policy (owner decides, case by case).** The inventory only *lists* gaps.
> For each Axis-B gap, the **project owner decides** whether it is:
> - a **real intent** → must be finished, or
> - an **assumed limitation** → documented as a deliberate boundary, not a defect.
>
> Example to adjudicate: MDx geometry write is read-only — is that a missing feature, or
> a deliberate "read-only format" decision? The AI does **not** assume; it presents, the
> owner rules.

### 2.1 The inventory — DONE 2026-05-31

> [!IMPORTANT]
> The full per-module promised-vs-delivered inventory (all 15 modules) lives in the
> companion file **[`ave-robustus-inventory.md`](ave-robustus-inventory.md)**. It was built
> from a parallel read-only audit and includes severity tiers, cross-cutting findings, the
> list of concrete correctness bugs (🐞), and the new open decisions surfaced for the owner.

Row template used there:

| Promised capability | Source of the promise | Real state | Gap (A / B / none) | "Utility blinded" criterion |
|---------------------|-----------------------|------------|---------------------|------------------------------|

### 2.2 Seed findings — ADJUDICATED 2026-05-31

First-survey gaps, ruled by the project owner. The Phase 0 inventory expands these into
per-capability rows.

| # | Observation | Source | Owner verdict |
|---|-------------|--------|---------------|
| 1 | Per-module CMake split promised (`emeraude::base::math`, `::io`, …) — only `platform` DONE, 14 targets TODO. | `module-map.md`, `README.md`, `AGENTS.md §7` | ✅ **In-scope (B)** — finish the split. Additive internal refactor, **no API break** (`emeraude::base` umbrella keeps working). Closes an API promised everywhere. |
| 2 | 34 `TODO`/`FIXME` markers (Math 15, PixelFactory 11, IO 6). | source scan | 🔀 **Triaged** — see §2.3. |
| 3 | VertexFactory MDx formats are write read-only (`writeStream` returns false). | memory + source | 🚫 **Assumed limitation** — MDx are third-party import formats; read-only is deliberate. Close by **correcting the claim** (document the boundary in VertexFactory `AGENTS.md`), no code. |
| 4 | VertexFactory StreamIO supports only the Native (ee3d) format. | memory | ✅ **In-scope (B)** — full StreamIO↔FileIO format parity required. |
| 5 | WaveFactory StreamIO supports only libsndfile formats (WAV/FLAC/OGG). | memory | ✅ **In-scope (B)** — full StreamIO↔FileIO format parity required. |
| 6 | Largest modules — VertexFactory (22k), WaveFactory (7.4k), Network (3.2k), IO (2.5k) — have **zero tests**, and are the untrusted-input I/O surface. | test-dir scan | ✅ **In-scope (A)** — no safety net; addressed in phase A.2. |

### 2.3 Marker triage — ADJUDICATED 2026-05-31

The 34 `TODO`/`FIXME` markers, bucketed and ruled.

| Bucket | Items | Verdict |
|--------|-------|---------|
| **Real correctness gaps** | `TriangleGenerator` remove internal triangles; `OrientedCuboid` extract bounds from vertices; `URI` scheme parsing (`URI.hpp`); `Processor` mask-value check (`Processor.hpp:964`) + `:1008`; `ShapeBuilder:636` "check this". | ✅ **In-scope (A/B).** URI touches the untrusted-I/O surface — priority. |
| **Stale docs (not missing code)** | IO `readable/writable/executable` Windows perms — `IO.hpp` `@todo`×3 claim a stub, but `IO.cpp` **fully implements** the Windows `AccessCheck` path. | 📝 **Correction to seed #2.** NOT a missing feature — a **doc defect**: close by fixing the `IO.hpp` comments to match the code (Axis B "correct the claim"). The code still needs tests (Axis A). |
| **Performance** | PixelFactory `Processor` SIMD/parallelization ×3, swap-buffer, block-copy, `Pixmap` avoid full copy. | ✅ **In-scope** — folded into the new **A.5 Performance** dimension (measured, never blind). |
| **Code dedup / refactor** | Math `Space3D/Intersections` Line* "same as segment*" ×8; `Processor:2247` simplify. | 🟦 **In-scope (A), low priority** — divergent-fix risk; fold into quality sweep. |
| **Future language version** | Math/Base.hpp `constexpr in C++23/26` ×6. | ⏸️ **Deferred** — not actionable in C++20. Kept as a "when we move to C++23" milestone, outside this plan. |
| **Misc** | `std_source_location` deprecated; `String` pad-as-string enhancement. | 🟦 dedup-cleanup in-scope; pad-as-string = post-freeze feature (out of scope). |

---

## 3. Axis A — Execution robustness (5 phases)

Sequenced on the engine principle *"no guesswork — every decision must be measurable":*
**instrument before fixing**, so every fix is proven (sanitizer-green + regression test),
never assumed.

### A.0 — Error contract (doctrine, lightweight)
Exceptions are disabled (`-fno-exceptions` is ON by default). "Robustness" therefore can
**never** mean `try/catch`. Codify the no-exceptions error contract first: propagation via
`bool` / status code / an `Expected`-like; forbidden constructs (`throw`, `.at()`,
`std::stoi`, unchecked `new`); the rule that **every value read from a file must be
bounds-checked**.

> [!IMPORTANT]
> **Abort policy (owner-ruled 2026-05-31).** Library code **MUST NEVER `std::abort()` on a
> runtime or input error** — those must propagate gracefully (bool/code/`Expected`).
> `abort`/`assert` are allowed **only** for programmer-contract violations (internal
> invariant breaches), and only in Debug. → The Network `asio_throw_exception.hpp`
> abort-on-error path is in violation and must be replaced with graceful propagation.

**Also in this phase — fix the compile-breakers (owner-ruled 2026-05-31):** the trivially
broken / clearly wrong bugs are fixed up front, before tooling, because they block builds or
are unambiguous: `Time::EventTrait::resetTimer` (calls non-existent `resetTop()`),
`Time::Elapsed::CPUTime` (tick→ns unit error). Each — like every fix in this plan — ships
with its regression test (the No-fix-without-a-test rule, §0). The *remaining* 🐞 bugs are
folded into A.2/A.3, each fixed alongside the test that proves it.
- *Deliverable:* an "Error Contract" section here + `docs/error-handling.md`; compile-breakers fixed **with tests**.
- *Exit:* doctrine written, build green, the up-front fixes covered by tests. (Exhaustive hidden-`throw` audit happens in A.3.)

> [!IMPORTANT]
> **A.0 doctrine — decided design (2026-05-31, owner-ruled):**
> - **Fallible ops:** hybrid — `bool` for void-fallible, `std::optional<T>` for value-or-failure
>   (codify the existing convention; no new `Expected` type during consolidation; C++20).
> - **Abort policy:** never `abort`/`terminate` on runtime/input error → propagate; `abort`/`assert`
>   only for programmer-contract violations, Debug-only.
> - **Diagnostics → shared logging, NOT a new silo.** The engine already has a rich `Tracer`
>   service with `Sink = std::function<void(Severity, const char* tag, std::string_view)>`.
>   Plan: (1) descend the `Severity` enum (Debug/Info/Success/Warning/Error/Fatal + to_string)
>   into **base** as `EmEn::Base::Severity` (engine `CoreTypes.hpp` re-exports via `using`);
>   (2) base gets a thin `src/Logging/` hook with a settable sink matching `Tracer::Sink`,
>   default → `cerr`; (3) the engine `Tracer` registers itself as base's sink in `earlySetup`.
>   → one `Severity`, no duplication, base stays engine-agnostic, base logs flow through Tracer.
> - **Rule:** no new raw `std::cerr` in base; the 65 existing sites migrate to the hook
>   progressively (per module, during A.2/A.3), not in one diff.
> - **`throw` audit (CORRECTED 2026-05-31):** the audit flagged 8 `throw`s in `StaticVector.hpp`,
>   but they are **exception-build-only** — each sits under `#if defined(__cpp_exceptions)` with an
>   `#else std::abort()` branch. Under `-fno-exceptions` (base default) capacity/bounds violations
>   abort (defined programmer-contract behaviour) — **NOT terminate-bombs** (the subagent audit was
>   wrong: it saw `throw` and missed the dual `#if/#else` impls). Nothing to fix; the fail-fast
>   contract (previously untested) is now locked with `StaticVectorDeathTest` death-tests.

### A.1 — Tooling (infrastructure; fixes nothing yet)
Stand up a `Debug-san` build (ASan + UBSan), extend `_FORTIFY_SOURCE` / stack-protector to
Debug, a libFuzzer harness skeleton, a CI gate.
- *Immediate ROI:* running the **existing** test suite under ASan+UBSan surfaces latent UB
  for free, on day one.
- *Exit:* `EmeraudeBaseUnitTests` runs green under sanitizers (or a logged list of UB found).

### A.2 — Test safety net (characterization)
Write tests for the untested large modules (VertexFactory, WaveFactory, Network, IO). Under
ASan/UBSan these tests double as bug-finders. Build the **malformed-input corpus**
(truncated / hostile files) here — it feeds the fuzzer.
- *Exit:* nominal paths covered + edge-case corpus assembled.

### A.3 — I/O boundary hardening (the core)
Now instrumented + tested + corpus ready: harden the parsers. Bounds, graceful
no-exception failure, hidden-`throw` audit. Each fix = sanitizer-green + a corpus entry
that no longer crashes.
- *Exit:* no parser exhibits UB/crash on hostile input; fuzzer runs clean for N hours.

### A.4 — Memory / resource / arithmetic safety
Final sweep: RAII audit (zero owning raw pointers), arithmetic overflow on **allocation
sizes derived from file headers** (the #1 parser vulnerability). Partially overlaps A.3.
- *Exit:* audit closed, invariants documented.

### A.5 — Performance (production-grade, measured)
Performance is part of the intent: emeraude-base is "STL++" feeding a production-grade
runtime. But per the golden rule (**RUNTIME > READABILITY > COMPILE TIME**) and the engine
principle *"no blind optimization, no guesswork"* — perf work is **benchmark-gated**: no
optimization lands without a before/after measurement proving the gain.
- *Scope:* the PixelFactory `Processor` SIMD/parallelization/block-copy markers, and any
  hot path surfaced by benchmarking. Comes **last** so it optimizes already-hardened code.
- *Deliverable:* a benchmark harness in the test suite; each perf change ships its numbers.
- *Exit:* targeted hot paths measured and improved; no regression vs the safety net (A.2).

**Order: A.0 → A.1 → A.2 → A.3 → A.4 → A.5.** (A.5 last — optimize only hardened, tested code.)

---

## 4. Axis B — Completeness robustness (policy)

Driven by the Phase 0 inventory and the owner's per-gap verdicts.

For each gap the owner rules **"real intent → finish"**:
1. **Finish** the capability to professional quality (no placeholder, per the project's
   Zero-Mediocrity directive).
2. **Blind its utility** — add a test that proves it serves its intended purpose, not
   merely that it compiles/exists.
3. **Feed it into Axis A** — the finished capability then goes through A.2 → A.3 → A.4.

For each gap the owner rules **"assumed limitation"**:
- Document it as a deliberate boundary in the module's `AGENTS.md`, so the promise and the
  delivery match again (the gap is closed by *correcting the claim*, not the code).

**Adjudicated Axis-B work (from §2.2 / §2.3 and the Phase 0 inventory):**
- **Per-module CMake split** — finish the 14 remaining targets, additive, no API break.
- **StreamIO ↔ FileIO format parity** — VertexFactory and WaveFactory StreamIO must read
  every format their FileIO counterpart does.
- **Network → production-grade (owner-ruled 2026-05-31).** Full RFC 3986 URI parsing
  (percent-encoding, IPv6, correct authority/TLD handling) **and** a robust HTTP**S** client
  (TLS, redirects, chunked transfer), with graceful no-abort error handling.
  > [!WARNING]
  > **Architectural consequence:** TLS requires a **new external dependency** (e.g. OpenSSL
  > or ASIO-SSL). Per the base axiom, it must be added through **`ext-deps-generator`**, never
  > vendored or hardcoded. This is the single largest item in the plan and pulls in a new dep —
  > flag for sequencing.
- **Finish these capabilities (owner-ruled 2026-05-31 — real intent):**
  - **Pixel** colored-source `stencil()` (currently a `return false` stub).
  - **Hash** `Types` enum aligned to implementations — expose SHA512, resolve SHA1
    (implement it or remove it from the enum), round-trip-tested.
  - **Animation** `CubicSpline` — add in/out tangent storage so the declared mode is deliverable.
  - **platform** compiler-identity detection (`PLATFORM_COMPILER` + version: GCC/Clang/MSVC).
  - **Debug** nanosecond timer — make cross-platform (currently Linux-only, link-errors elsewhere)
    and fix the `tv_sec`-ignoring subtraction.
- **Real correctness gaps (markers)** — `TriangleGenerator` internal triangles,
  `OrientedCuboid` bounds-from-vertices, `URI` scheme parsing (subsumed by the Network
  production-grade work), `Processor` mask checks, `ShapeBuilder` review.
- **Closed by documentation (no code):**
  - MDx write read-only is a deliberate boundary (document in VertexFactory `AGENTS.md`).
  - IO `readable/writable/executable` — the Windows path **is implemented**; fix the stale
    `@todo`/`@warning` comments in `IO.hpp` to match the code (then cover with tests, Axis A).

---

## 5. Scope & non-goals

**In scope (this epoch):** emeraude-base — every module, both axes.

**Non-goals:**
- **No API break.** `emeraude::base` (the umbrella) keeps working throughout. The
  per-module split (gap #1), if adjudicated in-scope, is an *additive* internal refactor.
- **No new features** beyond closing declared intent gaps (feature freeze, §0).
- Engine / projet-alpha hardening is **out of scope here** — they inherit base's hardening
  in a later, separate effort.

---

## 6. Progress tracker (living — update every session)

Long-term, multi-session plan: this table is the institutional memory of where we are.

| Phase | State | Notes |
|-------|-------|-------|
| Phase 0 — Intent Contract inventory | ✅ done | full inventory in [`ave-robustus-inventory.md`](ave-robustus-inventory.md); 4 new owner decisions surfaced |
| A.0 — Error contract + abort policy + compile-breakers (w/ tests) | ✅ done | compile-breakers (CPUTime, EventTrait); logging architecture (Severity→base, Logging hook, Tracer sink); StaticVector death-tests; `docs/error-handling.md`. Suite 1660 green; cascade builds. cerr→hook migration deferred to A.2/A.3. |
| A.1 — Tooling | ⬜ not started | |
| A.2 — Test safety net | ⬜ not started | |
| A.3 — I/O boundary hardening | ⬜ not started | |
| A.4 — Memory/resource/arithmetic | ⬜ not started | |
| A.5 — Performance (measured) | ⬜ not started | benchmark-gated; runs last |
| Axis B — Completeness | ⬜ adjudicated, not started | per-module |
| ↳ Per-module CMake split | ⬜ not started | 1/15 done (platform); additive, no API break |
| ↳ StreamIO↔FileIO format parity | ⬜ not started | VertexFactory + WaveFactory |
| ↳ **Network → production-grade** | ⬜ not started | **adds a TLS ext-dep** via ext-deps-generator; largest item |
| ↳ Finish capabilities | ⬜ not started | Pixel stencil, Hash enum, Animation CubicSpline, platform compiler-id, Debug timer |
| ↳ Real correctness gaps (markers) | ⬜ not started | TriangleGenerator, OrientedCuboid, Processor mask, ShapeBuilder |
| ↳ MDx write read-only | 🚫 ruled out | document the boundary (no code) |
| ↳ IO Windows-perms docs | ⬜ not started | code complete; fix stale comments + add tests |

> Legend: ⬜ not started · 🟦 in progress · ✅ done · ⏸️ paused · 🚫 ruled out by owner

---

## 7. How to use this document across sessions

1. Read §0 (north star) and §6 (where we are) first.
2. Phase 0 inventory drives everything — never start Axis A/B on a module before its
   intent row exists and (for B) the owner has ruled.
3. Doc-first: any change ships with its doc update **in the same session**; update this
   tracker (§6) and the affected module `AGENTS.md`.
4. The owner is the architect and rules all adjudication (Axis B) and all sequencing
   choices. The AI inventories, proposes, implements, and proves — it does not assume intent.