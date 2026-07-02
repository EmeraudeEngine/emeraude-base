# Testing System

Context for developing **emeraude-base** (`EmEn::Base`) unit tests.

## Module Overview

Unit tests for the emeraude-base foundation library, using **Google Test**.
Suite target: **`EmeraudeBaseUnitTests`**, gated by the CMake option
**`EMERAUDE_ENABLE_TESTS`** (OFF by default — a normal library build never builds tests).

- **Governance ("Ave robustus!"): no fix without a test.** Every correction ships with a
  unit test that fails before the fix and passes after. Plan:
  [`docs/plans/ave-robustus.md`](../../docs/plans/ave-robustus.md).
- **CTest integration**: `ctest` aggregates the suite and sets the working directory to
  `resources/` so fixture assets resolve (see `Constants.hpp`).
- **Sanitizer gate**: the suite also builds under ASan+UBSan via
  **`EMERAUDE_ENABLE_SANITIZERS`** (Debug, separate build dir). A fix is "proven" when the
  suite is green in Release AND under the sanitizers. **TSan** (data races — ThreadPool,
  TimedEvent) is a planned separate mode, incompatible with ASan.
- **No CI yet**: both gates are run manually.

## MANDATORY Test Conventions

**File organization** (flat directory, one file per tested concept):
- Naming: `test_<Concept>.cpp` — e.g. `test_MathVector.cpp`, `test_ThreadPool.cpp`.
- **A new test file must be added to the explicit source list in
  [`cmake/PrepareBaseSourceFiles.cmake`](../../cmake/PrepareBaseSourceFiles.cmake)**
  (there is NO glob), then re-run the CMake configure step.

**Test organization**:
- Test naming: `TEST(Suite, Behavior)` — e.g. `TEST(ThreadPool, ParallelForZeroGrainSize)`.
- Group variants and edge cases of one behavior inside the same `TEST()`.
- **Regression tests state their history**: open with a comment explaining what failed
  before the fix (see `test_ThreadPool.cpp` for examples). This is how the suite doubles
  as institutional memory.
- Code style matches the library: tabs, braces on every block, `std::memory_order`
  spelled out in concurrency tests.

**Support files**:
- `main.cpp` — plain `InitGoogleTest` + `RUN_ALL_TESTS` entry point.
- `Constants.hpp` — shared fixture paths and dimensions (assets under `resources/`;
  some sizes shrink when heavy-fixture builds are disabled).
- `TinySoundFontImpl.cpp` — vendored TinySoundFont implementation TU for Wave tests.

## Test Inventory (by domain)

Authoritative list: `ls src/Testing/` (or the CMake source list). Grouped view:

| Domain | Files (`test_*.cpp`) |
|--------|----------------------|
| Math | `MathBasics`, `MathVector`, `MathMatrix`, `MathQuaternion`, `MathCartesianFrame`, `MathSpace2D`, `MathSpace3D`, `MathTransformConversions`, `MathOrientedCuboid`, `LineFormula`, `AnimationCubicSpline` |
| PixelFactory | `PixelFactoryColor`, `PixelFactoryPixmap`, `PixelFactoryPixmapFormat`, `PixelFactoryProcessor`, `PixelFactoryTextPixmap`, `PixelFactoryFileFormats` |
| VertexFactory | `VertexFactoryFileFormats`, `VertexFactoryShapeBuilder`, `MD5AnimParser` |
| WaveFactory | `WaveFactoryFileFormats` |
| Concurrency / core | `ThreadPool`, `Time`, `ObserverPattern`, `NodeTrait`, `StaticVector`, `Variant`, `String`, `TokenFormatter` |
| IO / formats | `IO`, `ZipArchive`, `Compression`, `INIParser`, `FastJSON`, `Hash` |
| Platform / infra | `Platform`, `Debug`, `Logging`, `Version` |

Higher-level systems (Resources, Physics, Graphics, Scenes, Saphir) belong to the
**engine** repository, not to this foundation library.

## Development Commands

```bash
# From the emeraude-base repo root, in a dedicated git-ignored build dir
# (.gitignore covers cmake-build-* and .claude-build-*):

# 1. Configure once (Release, tests ON)
cmake -S . -B .claude-build-release -DCMAKE_BUILD_TYPE=Release -DEMERAUDE_ENABLE_TESTS=On

# 2. Build only the test target
cmake --build .claude-build-release --target EmeraudeBaseUnitTests -j$(nproc)

# 3. Run the suite (ctest sets the resources/ working dir for fixtures)
cd .claude-build-release && ctest --output-on-failure -j$(nproc)

# Run a single suite while iterating (from resources/ so fixture paths resolve):
cd resources && ../.claude-build-release/Release/EmeraudeBaseUnitTests --gtest_filter='ThreadPool.*'

# Sanitizer gate (ASan+UBSan, Debug, own build dir):
cmake -S . -B .claude-build-san -DCMAKE_BUILD_TYPE=Debug -DEMERAUDE_ENABLE_TESTS=On -DEMERAUDE_ENABLE_SANITIZERS=On
cmake --build .claude-build-san --target EmeraudeBaseUnitTests -j$(nproc)
cd .claude-build-san && ctest --output-on-failure -j$(nproc)
```

## Development Patterns

### Creating a New Test

1. Create `src/Testing/test_<Concept>.cpp` (license header + STL / third-party / local
   include blocks, like every other file).
2. Add it to `cmake/PrepareBaseSourceFiles.cmake`, re-run the configure step.
3. One `TEST(Suite, Behavior)` per behavior; edge cases (zero, negative, empty range,
   overflow, invalid input) are mandatory, either grouped or as `Suite, Behavior_EdgeCase`.
4. For a bugfix: write the test FIRST, watch it fail, fix, watch it pass (Release + sanitizers).

### Google Test Assertion Types

```cpp
EXPECT_EQ(a, b);   EXPECT_NE(a, b);            // equality
EXPECT_LT/LE/GT/GE(a, b);                      // ordering
EXPECT_FLOAT_EQ(a, b);  EXPECT_DOUBLE_EQ(a, b);// floats (never EXPECT_EQ on floats)
EXPECT_NEAR(a, b, eps);                        // |a - b| <= eps
EXPECT_TRUE(cond);      EXPECT_FALSE(cond);
ASSERT_*(...);   // same checks, but STOP the test on failure (use when continuing is meaningless)
```

### Tests with Fixtures (setup/teardown)

```cpp
class MyFixture : public ::testing::Test
{
	protected:

		void SetUp () override { /* runs before each TEST_F */ }
		void TearDown () override { /* runs after each TEST_F */ }
};

TEST_F(MyFixture, Behavior) { /* ... */ }
```

## Critical Points

- **The base must stay 100% tested** — it is the foundation the engine trusts blindly.
- **One test = one behavior**: clarity and isolation.
- **Edge cases mandatory**: boundary values, zero, negative, empty, overflow.
- **EXPECT vs ASSERT**: EXPECT continues after failure, ASSERT stops the test.
- **Float comparison**: `EXPECT_FLOAT_EQ` / `EXPECT_NEAR`, never `EXPECT_EQ`.
- **Fast tests** (< 1 s ideally). Deliberate exceptions exist (LZMA compression,
  `ThreadPool.ParallelPixmapDrawing`) — do not add new heavy tests without cause, and
  never assert on absolute timings (hosts differ); assert on ratios with a safety margin.
- **Reproducible**: no free randomness — RNGs use fixed seeds (e.g. `std::mt19937{42}`).
- **Verification is manual** (no CI yet): Release suite + ASan/UBSan run, both green,
  before any change is considered done.

## Detailed Documentation

- [`../AGENTS.md`](../AGENTS.md) — library-wide context, module map, patterns.
- [`docs/plans/ave-robustus.md`](../../docs/plans/ave-robustus.md) — robustness plan,
  per-fix history, known-green baseline.
- Google Test / CTest documentation — assertions, fixtures, CMake integration.
