## Development Patterns

### Math Usage Throughout Engine
```cpp
// Types are templates on dimension/precision: Vector< 3, float >, Matrix< 4, float >,
// Quaternion< float >. Float aliases exist: Vector3F, Vector4F, Matrix4F, ...

// Graphics uses Math
const auto projectionMatrix = Matrix4F::perspectiveProjection(fov, aspectRatio, zNear, zFar);
const Vector3F cameraPos = camera.cartesianFrame().position();

// Physics uses Math
const Vector3F force = acceleration * mass;
const Quaternion< float > rotation{angleX, angleY, angleZ}; // Euler angles, radians (setFromEulerAngles).

// Audio uses Math
const Vector3F soundPos = emitter.cartesianFrame().position();
const float distance = (listenerPos - soundPos).length();

// The engine and every consumer are unified via EmEn::Base::Math.
```

### Quaternion Rotation Matrix Conventions (CRITICAL)

Two coexisting conventions in `Quaternion.hpp`:
- **`rotationMatrix()`** — Stores row-major rotation data in column-major array (effectively R^T). Used by legacy code.
- **`toRotationMatrix4()`** — Standard column-major 4x4 rotation matrix. Compatible with `Quaternion(Matrix4)` constructor and `CartesianFrame::getModelMatrix()`.

When doing Quaternion ↔ Matrix4 roundtrips, always use `toRotationMatrix4()` + `Quaternion(Matrix4)`. Using `rotationMatrix()` in a roundtrip will produce the transposed rotation.

⚠️ **`conjugate()` MUTATES, `conjugated()` returns a copy** — same `inverse()` / `inversed()` pattern as `Vector`. `rotatedVector()` called the mutating one until Aug 2026, so it did not compile on a `const` quaternion (i.e. on every realistic caller) and would have mutated an operand of its own expression under unspecified evaluation order. It had therefore **never been called anywhere in the cascade**, and `MathQuaternion.RotatedVector` — the test named after it — quietly exercised `operator*` instead, with a comment saying so. Both are fixed; the test now calls the function it is named after and cross-checks the two paths agree. **A test that avoids its subject to keep compiling is reporting a defect, not working around one.**

### Observer/Observable Pattern
```cpp
// Observable in Resources
class TextureResource : public ResourceTrait, public Observable {
    void finishLoading() {
        // ...
        notifyObservers(Event::Loaded);
    }
};

// Observer in Scene
class Scene : public Observer {
    void onNotify(Observable* source, Event event) override {
        if (event == Event::Loaded) {
            // React to loading
        }
    }
};
```

### ThreadPool for Async Tasks
```cpp
// Global or local pool
ThreadPool pool(std::thread::hardware_concurrency());

// Fire-and-forget submission. The callable takes NO arguments (capture instead);
// returns false if the pool is shutting down.
pool.enqueue([](){ loadHeavyResource(); });

// Result retrieval goes through enqueueWithResult (std::future; only available
// in exceptions-enabled builds -- guarded by __cpp_exceptions).
auto future = pool.enqueueWithResult([]() { return 21 * 2; });
int result = future.get();  // 42

// Block until every queued and in-progress task finished.
pool.wait();

// Data-parallel fork-join: parallelFor. The calling thread participates and the
// call blocks until every chunk completes. Two overloads, selected by body arity:

// (1) per-index — body(index), invoked once per element:
pool.parallelFor(size_t{0}, data.size(), [&](size_t i) { data[i] = compute(i); });

// (2) range — body(chunkStart, chunkEnd), invoked once per chunk (amortizes
//     per-chunk setup: local accumulators, SIMD over a sub-range, reductions):
std::atomic<size_t> total{0};
pool.parallelFor(size_t{0}, data.size(), [&](size_t begin, size_t end) {
    size_t local = 0;
    for ( size_t i = begin; i < end; ++i ) { local += data[i]; }
    total.fetch_add(local);
});

// Optional grainSize (min iterations per chunk); small ranges or single-thread
// pools fall back to one serial body(start, end) call.
pool.parallelFor(size_t{0}, n, body, /*grainSize=*/256);
```

**`parallelFor` contract (enforced — see `src/Testing/test_ThreadPool.cpp`):**
- **The body must NOT throw.** It runs on worker threads too: a throw on a worker calls
  `std::terminate()`; a throw on the calling thread abandons the still-running workers whose
  captured body is destroyed (UB). Report failures through captured state, not exceptions.
- **Body arity is disambiguated at compile time.** The per-index (`invocable<F,I> && !invocable<F,I,I>`)
  and range (`invocable<F,I,I> && !invocable<F,I>`) overloads are mutually exclusive; a body
  invocable with BOTH one and two `index_t` args (e.g. `[](../../../src/auto...){}`) hits a guard overload that
  fails with a clear `static_assert` — give it a fixed arity.
- **A grainSize of 0 is treated as 1** (used to be a division by zero on ranges smaller
  than 4x the worker count).
- **The completion wait sleeps, it does not spin** (`std::atomic::wait`, futex-style): the
  calling thread yields its core until the last helper task notifies. One heap allocation
  per parallelFor call (the shared state); helper tasks are guaranteed to fit Task's small
  buffer by a `static_assert` (capture = shared_ptr + body reference, 24 bytes).
- Helpers are capped at `min(numWorkers, numChunks - 1)` -- no worker is woken without a
  chunk to run (the calling thread takes one share).
- **Nested parallelFor from a pool task is safe, even on a saturated pool.** Completion is
  counted per CHUNK, not per helper task: the nested call finishes on its calling thread and
  helpers that only get scheduled afterwards exit as no-ops. (`wait()` from a pool task can
  still deadlock -- that note is about `wait()`, not `parallelFor`.)
- Covered end-to-end: per-index tests + 9 range-overload cases (empty/reversed range, exact-once
  coverage, chunk-tiling, reduction, offset, grain size, both sequential fallbacks) + the
  2026-07-02 hardening regressions (zero grain on both overloads, fewer chunks than workers,
  `isIdle()` in-flight stress, `Task` noexcept-specification static_asserts, over-aligned
  callable heap fallback).

### TokenFormatter for Case Conversion
```cpp
// Static methods for direct conversion
std::string snaked = TokenFormatter::toSnakeCase("myVariableName");  // "my_variable_name"
std::string pascal = TokenFormatter::toPascalCase("my_variable_name");  // "MyVariableName"

// Instance for multiple conversions (parses once, converts many times)
TokenFormatter formatter("XMLParser");
formatter.toSnakeCase();       // "xml_parser"
formatter.toCamelCase();       // "xmlParser"
formatter.toKebabCase();       // "xml-parser"
formatter.toTitleCase();       // "Xml Parser"
formatter.toScreamingSnake();  // "XML_PARSER"

// Style detection
CaseStyle style = TokenFormatter::detect("myVar");  // CaseStyle::CamelCase
std::string_view name = TokenFormatter::styleName(style);  // "camelCase"

// Supported styles: CamelCase, PascalCase, SnakeCase, ScreamingSnake,
// KebabCase, TrainCase, FlatCase, UpperFlatCase, LowerSpaced, UpperSpaced, TitleCase
```

**Zero-allocation design:**
- Internal buffer (128 chars max) stores token copy
- Words stored as `std::string_view` into buffer (max 8 words)
- Only output methods allocate (with `reserve()`)
- See: `TokenFormatter.hpp:MaxWords`, `TokenFormatter.hpp:MaxTokenLength`

### INIParser for INI Files
```cpp
// Read configuration file
INIParser parser;
if (parser.read("config.ini")) {
    // Access section (creates if not exists)
    auto& graphics = parser.section("Graphics");

    // Read variables with type conversion
    int width = graphics.variable("width").asInteger();
    bool fullscreen = graphics.variable("fullscreen").asBoolean();
    float gamma = graphics.variable("gamma").asFloat();
    const std::string& path = graphics.variable("path").asString();

    // Check if variable exists
    if (graphics.variable("vsync").isUndefined()) {
        // Variable not found
    }
}

// Write configuration
INIParser config;
config.section("main").addVariable("version", INIVariable{"1.0"});
config.section("Graphics").addVariable("width", INIVariable{1920});
config.section("Graphics").addVariable("fullscreen", INIVariable{true});
config.write("output.ini");
```

**INI file format:**
```ini
[SectionName]
key = value
another_key = 123

# Comment lines (ignored)
@ Header lines (ignored)
```

> Line type is decided by the **first non-whitespace character**: a line *starting* with
> `[`/`#`/`@` is a section/comment/header; any other line containing `=` is a definition.
> So a key may legitimately contain `[`, `#` or `@` (e.g. `arr[0] = 5`) — markers only act
> as such at the start of a line. Values are taken verbatim after the first `=` (no inline
> comment stripping).

**Code references:**
- `INIParser.hpp:INIVariable` - Variable with type conversions (bool, int, float, double, string)
- `INIParser.hpp:INISection` - Named variable collection
- `INIParser.hpp:INIParser` - Main parser with sections
- Uses `std::filesystem::path` for file operations (C++17+)
- Uses `std::string_view` for read-only parameters

### Adding a New Module (Rules)
```cpp
// FORBIDDEN: Depending on the engine (or any consumer above this library)
#include "Scenes/Node.hpp"  // NO! emeraude-base never includes engine headers.

// CORRECT: Agnostic and generic (EmEn::Base namespace, engine types from Math/)
class MyUtility
{
	public:

		// Works without knowing Scenes/Physics/Graphics.
		[[nodiscard]]
		static Math::Vector< 3, float > interpolate (const Math::Vector< 3, float > & a, const Math::Vector< 3, float > & b, float factor) noexcept;
};

// The engine (and any standalone consumer) includes base headers, never the reverse.
```
