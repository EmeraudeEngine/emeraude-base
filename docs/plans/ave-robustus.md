# Plan — "Ave robustus!"

> **Status:** VALIDATED 2026-05-31 (owner-approved; feature freeze + perimeter in effect). **Axis A complete** (Phase 0, A.0–A.5 all done, owner-closed 2026-06-03); **Axis B complete — all gaps closed, including Network production-grade (last item, done 2026-07-04).** Every planned deliverable is now landed, verified (suite 1960/1960 incl. 3 opt-in live tests skipped, Release + ASan/UBSan; projet-alpha cascade links), **committed and pushed** to `develop` on both repos (emeraude-base tip `7c8f644` — feature `60cb19dc` + doc/test follow-ups: tracker push-state, "external resource" URL note, and the 404/unavailable live-test diagnostic; ext-deps-generator tip `7921762`). **AWAITING OWNER'S FORMAL CLOSURE:** per §0 the owner declares "Ave robustus!" complete and lifts the feature freeze — the AI does not self-declare it. Open follow-ups that are NOT blockers to closure (carried as post-plan items): Windows/macOS build+run validation of the TLS stack on those hosts; a versioned LibreSSL release in ext-deps-generator for non-symlink machines; live-network (non-hermetic) validation of the HTTPS client. See the §6 tracker.
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
  > **Architectural consequence:** TLS requires a **new external dependency**. Note that
  > `asio::ssl` is **not** an alternative TLS provider — it is a thin C++ wrapper that delegates
  > all cryptography to OpenSSL (or an API-compatible clone), so a crypto provider is unavoidable
  > either way. Per the base axiom, it must be added through **`ext-deps-generator`**, never
  > vendored or hardcoded. This is the single largest item in the plan and pulls in a new dep —
  > flag for sequencing.
  >
  > **Owner decision (2026-06-22): OpenSSL (3.x, Apache-2.0 → LGPLv3-compatible), driven via
  > `asio::ssl`** to reuse the existing asio-async Network model rather than hand-rolling the TLS
  > pump. **First gating risk to clear before committing to it:** base compiles `-fno-exceptions`
  > and asio is in `ASIO_NO_EXCEPTIONS` mode (handled via `Network/asio_throw_exception.hpp`);
  > `asio::ssl` over OpenSSL exposes `error_code` overloads and OpenSSL is C (no exceptions), so it
  > *should* compile under the same regime — **prove it with a compile PoC before building out.**
  >
  > **PoC PASSED 2026-06-22** — the `-fno-exceptions`/`ASIO_NO_EXCEPTIONS` risk is cleared
  > ([`network-tls/README.md`](network-tls/README.md)). **CA trust strategy owner-ruled
  > 2026-07-04: hybrid** (native system store per platform + CA-override API) — rationale and
  > per-platform mechanics in the same document; tracking in the §6 Network row.
  >
  > **Provider re-decided (2026-06-22 pivot, owner-confirmed 2026-07-04): LibreSSL, not OpenSSL.**
  > Reason: OpenSSL builds via its bespoke perl `Configure` (no CMake) — integrating it canonically
  > would mean a fifth hand-written builder plus perl/nasm as build deps in the CMake-centric
  > `ext-deps-generator`; LibreSSL-portable builds with CMake and is API-compatible, so `asio::ssl`
  > and the PoC hold unchanged. Integration form (owner-ruled 2026-07-04): the **release tarball
  > vendored** into `repositories/libressl/` — the first non-submodule dep, accepted because the
  > portable git repo is not self-contained (sources pulled from OpenBSD by `update.sh` at build).
  > CA-strategy consequence: LibreSSL has no `winstore` loader → the Windows trust-store leg is
  > hand-written wincrypt import code in `src/Network/`, compiled and exposed **only on Windows**.
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
| A.1 — Tooling | ✅ done | `EMERAUDE_ENABLE_SANITIZERS` (ASan+UBSan, -O1, halt-on-error, +stack-protector; no FORTIFY — ASan conflict). Suite 1660/1660 green under sanitizers; found+fixed 1 UB (`deserializeVector`). Manual gate (no CI yet). Fuzzing deferred to A.3 (g++ has no libFuzzer). |
| A.2 — Test safety net | 🟦 in progress | Per-module characterization + fixes (each fix shipped with a failing→passing test): Time, Logging, IO (MemoryStream overflow), FastJSON (nesting-depth pre-guard), StaticVector death-tests, INIParser (`getLineType` classification bug fixed + class renamed KVParser/KVSection/KVVariable → INIParser/INISection/INIVariable), **Variant** (10 tests; kept as a justified `-fno-exceptions`-safe facade over `std::variant` — NOT replaced by bare STL; 20 duplicated `asXxx()` cerr bodies collapsed into one private templated `as<T>()`, public API unchanged, single `cerr→Logging` site), **VertexFactory parsers** (16 tests; all four file formats hardened against the shared Tier-1 vuln — an untrusted count fed to `resize`/`reserve`/`vector(n)` without validation → `std::length_error`/`std::terminate`/OOM under `-fno-exceptions`. Native ee3d + STL: count validated against remaining stream bytes (overflow-safe division); MDx (MDL/MD2/MD3/MD5, read-only legacy): uniform `exceedsStream` guard at all 20 alloc sites + `skinwidth*skinheight` overflow fix; OBJ: face-index bounds-check before `.at()` (was `out_of_range`→terminate). All cerr→Logging incl. FileIO dispatch). **WaveFactory parsers** (14 tests; the 3 untrusted-input parsers hardened — MIDI: reject `division==0` (UB: div→`+inf`→`(uint32_t)` cast), clamp the `trackCount`-derived `reserve()` hint; libsndfile: bound the VBR-estimate `frames` decode buffer (`MaxDecodedSamples`, overflow-safe) + check `sf_writef_short` short-write; JSON SFX: cap `durationMs` at 30 min. All cerr→Logging incl. `Processor::resample`). **Build-guard cleanup (owner-ruled):** removed `LIBSNDFILE_ENABLED`/`SAMPLERATE_ENABLED`/`TINYSOUNDFONT_ENABLED` — these deps are always present; code uses them unconditionally. TinySoundFont impl stays out of the base lib (host owns the single `TSF_IMPLEMENTATION`; base's tests compile it via `Testing/TinySoundFontImpl.cpp`). Suite **1729/1729** green in Release **and** under ASan/UBSan. A.2 module pass complete. |
| A.3 — I/O boundary hardening | ✅ done | **Owner-ruled done 2026-06-02:** the 11 real crash fixes + multi-million-run clean campaigns (Targa 10.3M, JPEG 5.6M, MDx 6.4M, all under ASan+UBSan `halt_on_error`) judged sufficient evidence — the formal N-hour soak is **not** required to close the phase. **(1) Hidden-throw audit — done:** the untrusted boundary is throw/terminate-free — `throw` (only StaticVector's dual-impl), `std::sto*` (none), `.at()` (OBJ all 4 branches guarded + faces<3 rejected; PixelFactory in-range), `optional::value()` (IO are `error_code`, WaveFactory all `has_value()`-guarded), `.substr()` (no `pos>size`) all verified. One real defect fixed: `MD5AnimParser` `front()/back()` on an empty token (UB) + 3 tests (first Animation-module coverage). **(2) Fuzzing — clang libFuzzer harness in `src/Fuzzing/`** (4 targets: MIDI/OBJ/WAV/JSON-SFX; ASan+UBSan; `-fno-exceptions` so hidden throws surface as terminates). Found+fixed **2 real FastJSON terminate-bombs** (Core module, reachable by any untrusted JSON incl. engine scenes): (a) `getValue/getArray/getObject` on a non-object node → jsoncpp `isMember`/`operator[]` throw `LogicError` → guarded with `isObject()` short-circuit; (b) nesting depth == `stackLimit` → jsoncpp `readValue` throws `RuntimeError` → off-by-one in the `exceedsNestingDepth` pre-guard (`>`→`>=`). Both with regression tests. Clean campaigns: MIDI 15.8M, OBJ 2M, WAV 1.2M, JSON-SFX 2.4M runs, 0 crashes. **(3) Extended targets — done 2026-06-02:** added 8 targets (`fuzz_png/jpeg/targa/native/stl/mdx/compression/ini`). Surfaced + fixed **9 real malformed-input crashes** on engine-reachable parsers (textures / meshes / compressed payloads), each with a regression test: PNG `abort()` (returning libPNG error cb → `setjmp/longjmp`, 2-phase to keep `rowPointers` clear of `setjmp`); JPEG `exit()` (custom `jpeg_error_mgr` + `longjmp`); Targa OOM (17 GB pixmap from 16-bit dims → payload-bounded) + stack-overflow (depth ∉ {8,16,24,32} → 31-byte read into 4-byte buffer); MDx MD2/MDL OOB+null-deref, MD3 SEGV+64 GB `reserveData`+offset signed-overflow UB, MD5 null-deref (declared `numJoints` w/o `joints{}` block + unchecked weight/vertex/triangle cross-refs); LZMA decoder leak (`lzma_end` only on success → RAII guard); ZLIB OOM (untrusted chunk size → cap + max-ratio guard, `uncompress` check `>0`→`!=Z_OK`). cerr→Logging across every touched module. Clean post-fix (ASan+UBSan `halt_on_error`): PNG 4.7k, JPEG 5.6M, Targa 10.3M, MDx (4 sub-loaders) 6.4M, Compression 105k; `native`/`stl`/`ini` clean from the start. Suite **1741/1741** Release AND ASan/UBSan; engine cascade compiles all changes (`libEmeraude.so` links). Remaining: even longer soak campaigns on the remaining targets; exit = fuzzer clean for N hours. |
| A.4 — Memory/resource/arithmetic | ✅ done | Owner-closed 2026-06-02. **Scoping audit done** (parallel RAII + arithmetic-overflow audits, every finding hand-verified — one audit false positive caught: `ShapeDecimator.hpp:1440` already casts to `size_t` before multiplying). **Verdict: no live memory-safety bug remains** — A.3 had already hardened the arithmetic surface. Three fragility/defense-in-depth items closed, none exploitable: **(1) RAII (the only owning-raw-pointer gap):** `IO::ZipReader` + `IO::ZipWriter` `zip * m_zip` → `std::unique_ptr<zip_t, decltype(&zip_close)>` (`.reset()`/`.get()`; objects now move-only). Closes the "zero owning raw pointers" criterion for these classes; the leak-prone manual-`closeArchive()` discipline is gone. New `test_ZipArchive.cpp` (3 tests: round-trip + reader/writer abandoned-without-close — destructor release proven leak-free under ASan). Doctrine added to `docs/error-handling.md §5.2`. **(2) `FileFormatMDx.hpp:324` (MDL skin read):** recomputed the read size as signed `skinwidth*skinheight` instead of the already-validated `uint64 skinSize` (l.313/315) → read size could drift from the buffer. Verified **not** exploitable (divergent cases rejected by `exceedsStream` first); read `skinSize` now makes drift impossible. Owner-ruled clarity edit + inline comment; **no distinguishing test possible** (zero behavioral delta proven) — existing MDL rejection tests + ASan cover the surface. **(3) `FileFormatOBJ.hpp:1393` (`resolveIndex` negative OBJ indices):** `static_cast<int32_t>(listSize)` was UB once a list exceeds INT_MAX → widened to `int64_t` (behavior identical for all realistic inputs, pathological values still rejected downstream). New regression test `negativeRelativeIndicesResolveLikePositive`. **Verified:** suite **1751/1751** Release AND ASan/UBSan (1741→1751: +3 Zip, +1 OBJ); projet-alpha cascade compiles + links (`libEmeraudeBase.a` → `libEmeraude.so` → `projet-alpha`). **Audit scope (owner-accepted as exhaustive):** base src (Testing + vendored excluded); ThreadPool SBO + malloc/free in Jpeg/Zip sources reviewed clean. Commit `c85ed5b` (develop). |
| A.5 — Performance (measured) | ✅ done | Owner-closed 2026-06-03 (started 2026-06-02). **Harness up:** Google Benchmark (test-only FetchContent dep, `EMERAUDE_ENABLE_BENCHMARKS` OFF by default, separate `EmeraudeBaseBenchmarks` target, never in the ctest pass; build dir `.claude-build-bench`, Release). See [`/src/Benchmarking/README.md`](../../src/Benchmarking/README.md). **Parallelization mechanism (owner-ruled):** an optional non-owning `ThreadPool *` parameter on the static `Processor::resize` overloads (default `nullptr` → serial, no API break, base stays standalone); the caller lends a warmed-up pool, base never owns one. (The earlier "pool on the Processor instance" idea was dropped — it clashed with the static return-by-value `resize` used by the engine `TextureCompressor`/`Overlay::Surface` and `Font`.) **All three filters parallelized** (`resizeCubic`/`resizeLinear`/`resizeNearest`): each method's per-row work was made independent (`dstIndex` derived from the destination row, not a shared `dstIndex++` counter) and dispatched via `pool->parallelFor` or a serial loop from one shared lambda — serial path byte-identical, parallel path race-free. (`resizeCubic` also hoisted `numChannels` out of the inner loop to avoid a `-Wshadow`/`-Werror`.) **Measured (i9-14900K, 32 threads, median/8, no concurrent load):** Cubic 70.3→6.03 ms (down) / 519→43.1 ms (up) = ×11.7/×12.0; Linear 4.92→0.40 ms / 36.1→2.97 ms = ×12.3/×12.2; Nearest 1.16→0.149 ms / 8.55→1.00 ms = ×7.8/×8.6 (memory-bandwidth bound, lower ceiling). Correctness locked by `resize{Cubic,Linear,Nearest}ParallelMatchesSerial` (RGBA, parallel output == serial, race-checked under ASan). Suite **1754/1754** Release AND ASan/UBSan; cascade compiles+links (engine callers pass no pool → serial, unaffected). **`mirrorY` done:** rewritten as a channel-mode-agnostic per-pixel block copy (`memcpy` of `stride` elements, replacing the 4-case per-channel switch) + optional `ThreadPool` row dispatch; 4K RGBA **11.7→4.08 ms (×2.9, memory-bandwidth bound — pure copy, lower ceiling than the arithmetic resizes)**; test `mirrorYParallelMatchesSerial` (+ mirror-twice == original). **`Pixmap::addAlphaChannel` done — and a real bug fixed:** it had a **Grayscale-case fall-through** into the RGB case (a grayscale input was double-converted → out-of-bounds read / UB + wrong RGBA output). Rewritten as a copy-free **in-place back-to-front expansion** (closes the `@todo` "avoid whole copy of the pixel buffer" AND removes the fall-through); 3 regression tests (`addAlphaChannel{Grayscale,RGB,AlreadyHasAlpha}` — the Grayscale one fails before the fix, and the former OOB is now clean under ASan). Suite **1758/1758** Release AND ASan/UBSan; cascade links. **`toGrayscale` parallelized** (compute-bound per-pixel luminance, the heaviest single-pass op): 4K RGB→Gray **28.2→2.47 ms (×11.4)**, test `toGrayscaleParallelMatchesSerial`. **A.5 perf markers addressed** (resize ×3, mirrorY, addAlphaChannel, toGrayscale; the swap-buffer `@TODO` at Processor.hpp:49 is a broad refactor note, not a hot path). **Cubic kernel hoist DONE (the auto-vec prototype, owner-chosen):** the 4×4 neighbourhood gather was hoisted out of the channel loop (the former kernel called `safePixel()` up to numChannels×16 times per output pixel → now 16, cached, with a flat vectorizer-friendly inner loop). **Bit-identical** output. Serial **70.3→35.4 ms (down) / 519→260 ms (up) = ×2.0**; with parallelization the original 519 ms upscale is now **20.4 ms (≈×25 vs the original serial baseline)**. Guarded by `resizeCubicUniformImageStaysUniform` (interior only — `safePixel` returns Black out of bounds, *not* an edge clamp, so border pixels legitimately overshoot; pre-existing, identical before/after) plus the parallel==serial test. **Explicit hand-written SIMD stays deferred** (SSE/AVX/NEON + dispatch = portability/maintenance cost against the "one backend mastered" ethos; the parallel ×12 already covers CPU-side asset/tool work, and runtime image work belongs on the Vulkan GPU). Then close A.5 → Axis B (completeness). |
| Axis B — Completeness | ✅ done | all sub-rows closed; Network production-grade (the last item) done 2026-07-04. Awaiting owner's formal plan-closure + freeze-lift (§0). |
| ↳ Per-module CMake split | ✅ done | 15/15 (2026-06-03). Every `src/` subsystem is its own target: INTERFACE for header-only (platform, math, algorithms, animation, **pixel** — corrected from OBJECT: PixelFactory is all templates), OBJECT for compiled (core, hash, gametools, time, debug, compression, io, network, vertex, wave). Shared `emeraude_base_flags` INTERFACE carries the compile requirements (own/config/ext-deps includes, std, defs/options, link dir, sanitizer); OBJECT modules link it + PIC; the `emeraude::base` umbrella aggregates `$<TARGET_OBJECTS:…>` + links the ext libs. **No API break** — the umbrella keeps working; `emeraude::base::<module>` aliases now also exist. Source lists per module in `cmake/PrepareBaseSourceFiles.cmake`. Suite **1775/1775** Release AND ASan/UBSan; cascade links (`libEmeraude.so` + projet-alpha). Granular-linking caveat (a lone OBJECT module needs its siblings, e.g. core/Logging) documented in module-map.md. |
| ↳ StreamIO↔FileIO format parity | ✅ done | StreamIO now reaches every format FileIO does, via an explicit format selector (mirrors the existing `PixelFactory::StreamIO::read(data, format, …)` precedent — a memory buffer has no extension to dispatch on). **VertexFactory**: new `FileFormatType {Native,OBJ,STL,MDx}`; StreamIO read+write dispatch to all four (MDx read-only). **WaveFactory**: new `SoundFileFormat {Audio,MIDI,JSON}`; StreamIO read dispatches to libsndfile/MIDI/JSON (write stays SNDFile-only — the only writable format, already at parity). cerr→Logging in both. Tests `VertexFactoryStreamIO.formatParity` (OBJ read + Native/STL round-trip + MDx read-only) and `WaveFactoryStreamIO.formatParity` (Audio round-trip + MIDI decode + JSON dispatch). Suite **1773/1773** Release AND ASan/UBSan; cascade links (no engine caller of these StreamIO yet). |
| ↳ **Network → production-grade** | ✅ done | **adds a TLS ext-dep** via ext-deps-generator; largest item. **TLS provider decided: LibreSSL via `asio::ssl`** (owner-confirmed 2026-07-04; supersedes the 2026-06-22 "OpenSSL 3.x" ruling — OpenSSL's perl `Configure` doesn't fit the CMake-centric ext-deps-generator; LibreSSL is API-compatible so `asio::ssl` and the PoC hold). **Integration form (owner-ruled 2026-07-04): release tarball vendored** into `repositories/libressl/` (first non-submodule dep — the portable git repo pulls its sources from OpenBSD at build time, not self-contained). **Gating PoC PASSED 2026-06-22** ([`network-tls/`](network-tls/README.md)): `asio::ssl` compiles/links/runs under `-fno-exceptions`/`ASIO_NO_EXCEPTIONS` (g++ 14.2, system OpenSSL 3.5.6; LibreSSL API-compatible → transferable, re-validate against the built LibreSSL when the ext-dep lands); still unproven → real runtime handshake, cert-chain verification behavior, async ops under the no-exceptions hook. **CA trust strategy decided 2026-07-04 (owner-ruled): hybrid** — native system store per platform (Linux: distro-path probing; Windows: hand-written wincrypt trust-store import in `src/Network/`, compiled/exposed **only on Windows** — LibreSSL has no `winstore` loader; macOS: Apple-maintained `/etc/ssl/cert.pem`) **+ a CA-override API** (`setCAFile`-style; required for hermetic self-signed-server tests, covers enterprise/Keychain CAs). Custom-built TLS lib ⇒ default cert paths unusable ⇒ explicit trust-store bootstrap on ALL platforms, Linux included. Full rationale in [`network-tls/README.md`](network-tls/README.md). **Ext-dep recipe LANDED 2026-07-04:** LibreSSL **4.3.2** (latest stable, owner-pinned; SHA256-verified tarball) vendored + `libraries/libressl.yaml` + build order + link-test entry; validated on Linux (Release+Debug build, link-test 39/39) and the **PoC re-run against the built LibreSSL PASSED** (transferability risk closed); Windows/macOS recipe validation pending on their hosts. **`SetupLibreSSL.cmake` LANDED 2026-07-04** (links `tls→ssl→crypto` static + MSVC system libs `ws2_32`/`bcrypt`/`crypt32`; included after `SetupASIO`; cascade links Release + suite baseline **1893/1893** held). **Client scope DECIDED 2026-07-04 (owner-ruled, full detail in [`network-tls/README.md`](network-tls/README.md)):** HTTP/1.1 only with an h2-ready protocol-agnostic API (HTTP/2 = post-plan additive codec via ALPN/nghttp2); auto redirects (301/302/303/307/308, correct method semantics, cap default 5, https→http refused); **basic proxy in scope** (CONNECT + `http_proxy`/`https_proxy`/`no_proxy`); sync facade over internal asio (+ `download()` upgraded); full configurable timeout set. **Trust-store bootstrap LANDED 2026-07-04** (`Network/TrustStore.{hpp,cpp}`: `applySystemTrustStore` — Linux distro-bundle probing / macOS `/etc/ssl/cert.pem` / Windows CryptoAPI ROOT+CA import — + `applyCABundleFile` override + `certificateCount`; 6 tests incl. real-chain utility proof; fixtures = test CA + localhost leaf, certs only; suite **1899/1899** Release AND ASan/UBSan; cascade links). **TLS transport LANDED 2026-07-04** (`Network/TLSConnection.{hpp,cpp}`: blocking single-use client transport, async-inside + `run_for` timeouts, SNI + chain + hostname verification always on; 6 hermetic tests vs an in-process TLS server w/ runtime-generated EC credentials; live-validated on Linux — github.com/khronos.org handshakes OK, badssl.com wrong-host/self-signed/expired rejected; **all 3 formerly-unproven PoC items closed**; suite **1905/1905** Release AND ASan/UBSan; cascade links; Windows/macOS validation deferred to owner hosts, owner-ruled). **HTTP/1.1 codec LANDED 2026-07-04** (`Network/HTTPResponseParser.{hpp,cpp}`: incremental feed-based response parser, RFC 9112 framing incl. chunked+trailers, bounded everything, smuggling defenses — duplicate-CL rejected, TE-overrides-CL; supporting RFC fixes: HTTPHeaders case-insensitive field names, HTTPResponse empty-reason+code-range+`keepConnectionAlive()`, cerr→Logging in touched files; **21 tests** nominal+hostile, all byte-by-byte re-fed; suite **1926/1926** Release AND ASan/UBSan; cascade links). **Client loop LANDED 2026-07-04** (`Network/HTTPSClient.{hpp,cpp}`: sync facade `get`/`head`/`download` over TLSConnection+HTTPResponseParser; bounded no-downgrade redirects with method rewriting; total-time budget; body streamed to file for download; HTTPS-only, http rejected. Shared `Testing/TLSTestHelpers.hpp` = runtime-cert HTTPS test server, async-accept so it tears down cleanly. **10 client tests** + refactored TLSConnection test onto the shared helper. **Fixed a real pre-existing URI bug** (`URI::resource()`/`operator<<` serialized the path via `std::filesystem::path::operator<<` → quoted `/"path"`, broke every request-target; → `generic_string()`; regression `test_NetworkURI.cpp`). Suite **1939/1939** Release AND ASan/UBSan; cascade links. Proxy is the next increment (needs a two-phase TLSConnection connect for CONNECT tunneling) — deliberately not bundled here.) **RFC 3986 URI conformance LANDED 2026-07-04** (owner-requested after audit): new `Network/PercentEncoding.{hpp,cpp}`; `URI` rewritten (hand-written Appendix-B parse — no std::regex; scheme/host lowercase, percent decode/re-encode, `removeDotSegments`, `URI::resolve` §5); `URIDomain` rewritten (IPv6 brackets, port range, userinfo decode, `host()` colon bug fixed); `Query` fixed (leading-`&` bug + percent codec); `HTTPSClient::resolveRedirect` now uses `URI::resolve` (relative Locations work). Tests: RFC §5.4 resolution vectors + §1.1.2 scheme-diversity + gnarly-but-valid batch (IDN/UTF-8/NUL/matrix/5000-deep `/..`) + PercentEncoding. Suite **1954/1954** Release AND ASan/UBSan; cascade links. **Proxy support LANDED 2026-07-04:** `TLSConnection` split into `establishTcp`/`tunnelThroughProxy`/`performHandshake` + new `connectViaProxy` (plaintext CONNECT to proxy → end-to-end TLS handshake with target, proxy never sees cleartext); `HTTPSClient` proxy options + `resolveProxy` (`https_proxy`/`no_proxy` env, explicit authority, default port 8080); test server proxy mode; tests for explicit/env proxy + no_proxy bypass. Suite **1957/1957** Release AND ASan/UBSan; cascade links. **`fuzz_http_response` LANDED + campaign clean 2026-07-04** (`src/Fuzzing/fuzz_http_response.cpp`: drives `HTTPResponseParser` both whole-feed and byte-sliced from one input via a control byte; 5-seed corpus; **23.5M runs, 0 crashes** under ASan+UBSan `halt_on_error`). **Network production-grade is COMPLETE — COMMITTED + PUSHED 2026-07-04.** emeraude-base `develop` tip `7c8f644`: feature commit `60cb19dc` ("Network: production-grade HTTPS client over LibreSSL", rebased onto the concurrent `6d5f24f1` version bump 1.1.6→1.1.7, re-verified 1960/1960 before push) + three doc/test follow-ups (`4069884` tracker push-state, `1cdc631` "external resource may stop responding" note above the live URL, `7c8f644` explicit 404/unavailable diagnostic on the live test). ext-deps-generator `develop` tip `7921762`: `ccc7b57` (vendored LibreSSL 4.3.2) + `7921762` (README vendored-dep docs). Caveats carried forward (not blockers): validation is hermetic (no live proxy/redirect network test); Windows (MSVC, wincrypt trust-store leg) + macOS (`/etc/ssl/cert.pem`) compile/run validation pending on those hosts; LibreSSL ext-dep still needs a versioned ext-deps-generator release for non-symlink machines. |
| ↳ Finish capabilities | ✅ done (5/5) | ✅ **Pixel colored stencil** (was a `return false` stub → composites a solid colour through a grayscale mask: white passes, black blocks, grey = AA via blend opacity; test `coloredStencilRespectsMask`). ✅ **Hash enum** (exposed implemented `SHA512`; round-trip + FIPS-180-4 SHA512 KAT tests). **SHA-1 since RE-IMPLEMENTED (2026-06-04, commit `a569ae2`)** — `Hash/SHA1.{cpp,hpp}` (182-line impl), back in `HashType` enum + `Hash::sha1()`, covered by `test_Hash.cpp`. Owner-ruled a **necessary correction** → authorized freeze exception (see the freeze-exceptions note under §6). No consumer inside emeraude-base or the engine itself — it is **required by a downstream non-engine application that links `emeraude::base`** (the standalone-consumption use case this library exists for; that consumer drove the request). The gap is thus now closed by *finishing* the capability, **reversing** the earlier "removed SHA1" closure. ✅ **platform compiler-id** (`PLATFORM_COMPILER` + version macros & constexpr mirrors, Clang-before-GCC; `test_Platform`). ✅ **Debug ns timer** (was Linux-only + `tv_nsec`-only → now delegates to `Time::processCPUTimeNanoseconds()`, cross-platform + full-ns; `test_Debug`). ✅ **Animation CubicSpline**: added in/out tangent storage to `VectorKeyFrame`/`QuaternionKeyFrame` + a GLTF cubic Hermite evaluator `Math::cubicSplineInterpolation` (the mode was declared but had no tangent storage and no evaluator anywhere — base owns this animation data model, no engine consumer yet). Tests `test_AnimationCubicSpline` (scalar/vector endpoints + flat-tangent midpoint + keyframe-tangent round-trip). |
| ↳ Real correctness gaps (markers) | ✅ done | **Stencil family FIXED 2026-06-03:** the pixmap-source `Processor::stencil()` overloads now respect the grayscale mask (white passes / black blocks / grey = AA) and use the index-based `blendPixel(index, colour, mode, opacity)` — the former `blendPixel(index, row, …)` passed a full flat index as the X coord (wrong target indexing) and ignored the mask (FIXME). The core gained an `opacity` param so the convenience overload (which forwarded 6 args to a 5-param core — uncompilable had it ever been called) now matches. Test `pixmapStencilRespectsMask` (core + convenience). **Processor mask-value checks** were the same stencil FIXMEs — resolved by the stencil work above. **OrientedCuboid DONE** (`Math/OrientedCuboid.hpp`): `width/height/depth` now derived from the (transformed) vertices instead of stored separately (the `FIXME: Extract these from vertices!`) — no desync; identical result for rigid transforms; test `MathOrientedCuboid.extentsDerivedFromVertices`. **ShapeBuilder DONE** (`ShapeBuilder.hpp:636`): the `FIXME: Check this` on the TriangleFan vertex-shift was **verified correct** (the fan keeps vertex [0] fixed, [1]←[2]) — comment clarified, locked by `VertexFactoryShapeBuilder.triangleFanProducesNMinus2Triangles`. **TriangleGenerator — REMOVED (owner-ruled 2026-06-03):** the `generateEnvelope` "bad algorithm" envelope generator (+ its `verticesReduction` helper) had **no caller anywhere** in base/engine/projet-alpha; rather than rewrite a convex hull for dead code, the whole `TriangleGenerator.hpp` was deleted. **All correction-gap markers resolved.** |
| ↳ MDx write read-only | ✅ documented | deliberate boundary now in the FileFormatMDx class `@note` + writeStream `@note` (read-only import formats by design) |
| ↳ IO Windows-perms docs | ✅ done | stale `@todo`/`@warning` "Windows not implemented" corrected to match the real AccessCheck path (IO.cpp); `permissionsOnRealFile` test added |
| ↳ ThreadPool `parallelFor` — tests + arity hardening | ✅ done | Range overload `parallelFor(start, end, body(chunkStart, chunkEnd))` contributed by C. Lenhof (`1f224bc`, 2026-06-29), sharing a new private fork-join driver `parallelForChunks` with the per-index overload — an **owner-authorized freeze exception** (see the note below). Robustness backfilled on top: **9 GoogleTest cases** for the range overload (the only untested path) — empty/reversed range, exact-once index coverage, chunk-tiling (no gaps/overlaps), per-chunk reduction, offset start, grain size, and both sequential-fallback paths (single-thread pool + workload ≤ grain) — plus Doxygen corrections (the false "exception propagates, other chunks continue" note → the body also runs on **worker threads**, so a throw calls `std::terminate()` / abandons workers whose captured body is destroyed = UB → **must-not-throw** `@warning`; fixed the `IndexType` typo + a truncated `@param end`) (`4816f9b`). **Arity hardening** (`f67a30a`): the per-index (`invocable<F,I> && !invocable<F,I,I>`) and range (`invocable<F,I,I> && !invocable<F,I>`) `requires` clauses made mutually exclusive + a **guard overload** (`invocable<F,I> && invocable<F,I,I>`) that `static_assert`s a clear message when a body is invocable with BOTH arities (e.g. `[](auto...){}` — previously a silent overload ambiguity); a private `dependentFalse<T>` value template defers the assert to instantiation (a non-dependent `static_assert(false)` is ill-formed in C++20). Suite **1884/1884** Release (`-Werror`); the guard diagnostic verified by a throwaway compile, single-/two-arg bodies confirmed still well-formed. **Note:** the guard trades SFINAE-friendliness for a clear message (a both-arity call looks well-formed to `requires{…}` detection, then hard-errors on instantiation) — acceptable for a directly-called utility. **TSan** (data races on ThreadPool/TimedEvent) stays a separate future mode (incompatible with ASan). |
| ↳ ThreadPool hardening — full-class review | ✅ done | Owner-requested review pass (2026-07-02) over `ThreadPool.hpp/.cpp`; **9 findings fixed, no API change.** **Correctness:** (1) `parallelFor(grainSize=0)` on a range < 4×workers → zero effective grain → **division by zero**; grain 0 now clamped to 1. (2) `enqueueBatch()` checked `m_stop` **outside** the mutex (unlike `enqueueTask()`) → a concurrent shutdown could strand never-executed tasks in the queue (destructor doc promises a drain); check moved under the lock. (3) `worker()` decremented `m_pendingTasks` before incrementing `m_busyWorkers` → the lockless `isIdle()` could transiently report an idle pool with a task in flight; order inverted (worst case is now a conservative transient double-count). (4) `Task(F&&)`'s noexcept spec tested `is_nothrow_move_constructible` while an lvalue argument copy-constructs, and ignored the over-aligned heap path → could promise noexcept on a throwing path (std::terminate); now `is_nothrow_constructible_v<DecayedF, function_t>` + size AND alignment conditions. (5) `parallelForChunks()` ignored `enqueue()` failures → the completion count could never be reached (infinite wait on a shutdown race); failures are now compensated into the counter. **Efficiency (`parallelForChunks` rework):** the completion spin-wait (`yield` loop — one core at 100% until the slowest chunk) → **`std::atomic::wait`** (futex-style sleep; the last finishing task `notify_one`s, and the captured shared_ptr keeps the atomic alive during the notify — no dangling-notify UB). The helper capture was 56 B (> 48 B SBO) → **every helper Task heap-allocated, precisely on the pool's hottest path**; the chunk parameters moved into the shared state so the capture is shared_ptr + body ref = 24 B, **locked by `static_assert(sizeof(workerTask) <= SmallBufferSize)`** → exactly one allocation per parallelFor call. Helpers capped at `min(numWorkers, numChunks - 1)` (no wake-up without a chunk to run; the caller takes one share). `worker()`'s completion `notify_all` moved after the mutex release (woken waiters no longer collide with a held lock). **6 new tests** (crash or fail before the fixes): `ParallelFor{,Range}ZeroGrainSize`, `ParallelForFewerChunksThanWorkers`, `IsIdleNeverTrueWhileTaskInFlight` (2000-iteration stress), `ConstructorNoexceptSpecification` (static_asserts the old spec fails), `OverAlignedCallableFallsBackToHeap`. Suite **1890/1890** Release (`-Werror`); ThreadPool suite (58 tests) green under **ASan/UBSan**; cascade compiles+links. Speedup sanity: `ParallelPixmapDrawing` passes its 1.5× floor. **Measured A/B old-vs-new** (dedicated micro-bench, 3 alternating runs, secondary 28-thread host — ratios are the signal, absolute times are not): 20k small `parallelFor` calls (range 256, grain 16): **35.7 → 18.8 µs/call (×1.9)**, process CPU −46%; straggler scenario (one 20 ms chunk, caller waits): wall unchanged, **waiting CPU 2.08 s → 0.08 s (−96%)** — the old spin burned one full core for the whole wait. Owner re-baselines absolute times on the i9-14900K. Class `@version` refreshed 0.8.38 → 1.1.6. **Nested-livelock RESOLVED (owner-ruled, same day):** completion switched from per-TASK to per-CHUNK counting — a nested `parallelFor` from a task already running on the pool now completes on its calling thread even when the pool is saturated; helpers scheduled after the last chunk exit as no-ops (they claim an index past the last chunk, touching only the shared state their shared_ptr keeps alive; the never-dereferenced body reference may briefly dangle — accepted trade-off, documented in the driver). Bonus simplification: failed-enqueue compensation deleted (every chunk runs regardless, worst case all on the caller). New regression `NestedParallelForOnSaturatedPool` (2-worker pool, both workers run a nested parallelFor — hung forever before). Suite **1891/1891**. **Micro-pass (same day, owner-approved):** `<iostream>` evicted from the header (the empty-task `operator()` `std::cerr` fallback → `assert` in debug / safe no-op in release; one less include + `ios_base::Init` in every consumer TU); executed tasks destroyed **before** the completion signal (a `wait()` return now guarantees the task's captures are released — regression `TaskCapturesReleasedBeforeWaitReturns`, 500-iteration use_count stress); `m_isSmall` reset on move/clear so `isSmall()` never reports a stale true on an empty task (`IsSmallResetOnMoveAndClear`); `enqueueBatch` Doxygen now states the input range is left moved-from. Suite **1893/1893** Release; ThreadPool suite (60) green under ASan/UBSan. |

> Legend: ⬜ not started · 🟦 in progress · ✅ done · ⏸️ paused · 🚫 ruled out by owner

> [!NOTE]
> **Authorized freeze exceptions (owner-ruled).** Three additions landed on
> `develop` after the 2026-06-03 closure, each with the owner's explicit approval and each shipped
> with its test (so the "no fix without a test" rule holds). They are **not** freeze breaches:
> - **SHA-1 re-implementation** — `a569ae2` (2026-06-04). Owner-ruled necessary (see the Hash row).
> - **`Math::nextPowerOfTwo`** — `851f209` (2026-06-21). Unblocks diamond-square terrain, which
>   requires power-of-two grid sizes (snap-up instead of reject); `std::bit_ceil` based; test
>   `Math.nextPowerOfTwo`.
> - **ThreadPool `parallelFor` range overload** — `1f224bc` (2026-06-29, C. Lenhof). A chunk-based
>   `body(chunkStart, chunkEnd)` convenience sharing the existing fork-join driver; owner-authorized.
>   Backfilled with 9 tests + arity-guard hardening (`4816f9b`, `f67a30a` — see its tracker row).
>
> The feature freeze otherwise still stands: new features resume only once **Network
> production-grade** closes the plan.

---

## 7. How to use this document across sessions

1. Read §0 (north star) and §6 (where we are) first.
2. Phase 0 inventory drives everything — never start Axis A/B on a module before its
   intent row exists and (for B) the owner has ruled.
3. Doc-first: any change ships with its doc update **in the same session**; update this
   tracker (§6) and the affected module `AGENTS.md`.
4. The owner is the architect and rules all adjudication (Axis B) and all sequencing
   choices. The AI inventories, proposes, implements, and proves — it does not assume intent.