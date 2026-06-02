# Benchmarks — emeraude-base (`EmeraudeBaseBenchmarks`)

> Measurement harness for the **"Ave robustus!"** plan, phase **A.5 (Performance)**.
> The plan's rule: *performance is benchmark-gated — no optimization lands without a
> before/after measurement proving the gain.* This is **not** a correctness suite (that is
> `EmeraudeBaseUnitTests`); it never runs in the `ctest` pass.
> Plan: [`../../docs/plans/ave-robustus.md`](../../docs/plans/ave-robustus.md) (§A.5).

## Framework & wiring

- **Google Benchmark**, pulled test-only via `FetchContent` (like GoogleTest), pinned to a tag.
- Gated by the CMake option **`EMERAUDE_ENABLE_BENCHMARKS`** (OFF by default) → the normal
  cascade build never builds it.
- Built into a **separate `EmeraudeBaseBenchmarks` executable** (Google Benchmark provides its
  own `main`). Sources live in `src/Benchmarking/`, listed in
  `cmake/PrepareBaseSourceFiles.cmake` (`EMERAUDE_BASE_BENCHMARK_SOURCES`).

## Build & run

Use a dedicated, git-ignored **Release** build dir (Debug numbers are meaningless; Google
Benchmark itself warns when built without optimization):

```bash
cd /mnt/bunker/studio/dev/ln-isle/projet-alpha/dependencies/emeraude-engine/dependencies/emeraude-base

cmake -S . -B .claude-build-bench -DCMAKE_BUILD_TYPE=Release -DEMERAUDE_ENABLE_BENCHMARKS=On
cmake --build .claude-build-bench --target EmeraudeBaseBenchmarks -j$(nproc)

cd .claude-build-bench/Release
./EmeraudeBaseBenchmarks --benchmark_repetitions=5 --benchmark_report_aggregates_only=true --benchmark_min_time=0.3s
# filter: --benchmark_filter=cubic
```

> The host has CPU frequency scaling enabled, so Google Benchmark prints a noise warning. In
> practice the coefficient of variation stays < 0.5 % on the resize benchmarks, so the medians
> are trustworthy; pin the governor to `performance` if you want the warning gone.

## How a perf change is recorded

Each optimization ships its **before/after** numbers (median of ≥5 repetitions, host noted).
The parallelizable `Processor::resize` filters take an optional non-owning `ThreadPool *`
(default `nullptr` → serial); each benchmark runs both the serial and the pooled path so the
speedup is measured, not assumed. The *correctness* of the parallel path (parallel output ==
serial output, race-free) is locked by a unit test in `EmeraudeBaseUnitTests`, not here.

## Results log

Host: i9-14900K (32 threads), Release, median of 5 repetitions.

### `Processor::resize` — all three filters parallelized 2026-06-02

Median of 8 repetitions, no concurrent load. Downscale = 1920×1080 → 960×540,
Upscale = 1200×800 → 2400×1600.

| Filter | Workload | Serial | ThreadPool | Speedup |
|--------|----------|-------:|-----------:|--------:|
| Nearest | Downscale | 1.16 ms | 0.149 ms | ×7.8 |
| Nearest | Upscale | 8.55 ms | 1.00 ms | ×8.6 |
| Linear | Downscale | 4.92 ms | 0.401 ms | ×12.3 |
| Linear | Upscale | 36.1 ms | 2.97 ms | ×12.2 |
| Cubic | Downscale | 70.3 ms | 6.03 ms | ×11.7 |
| Cubic | Upscale | 519 ms | 43.1 ms | ×12.0 |

Nearest's lower ceiling is expected — it is a pure pixel copy (memory-bandwidth bound),
not arithmetic-bound like Linear/Cubic. The parallel-path coefficient of variation rises on
the sub-millisecond workloads (thread-dispatch overhead dominates); the medians stay stable.

> **Measurement hygiene:** run the benchmark with **no concurrent build/load** — a parallel
> `-j$(nproc)` cascade build running alongside inflated the serial (single-thread) medians ~3×.

Next A.5 targets: `mirrorY` block-copy and the `Pixmap` whole-buffer-copy marker.