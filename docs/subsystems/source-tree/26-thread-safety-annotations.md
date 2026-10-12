## Thread-safety annotations — Base::Mutex, Base::MutexLock and the EMEN_* attributes (D12, 2026-10-12)

`src/ThreadSafety.hpp` (the attribute macros) and `src/Mutex.hpp` (`EmEn::Base::Mutex`, `EmEn::Base::MutexLock`).
Owner decision D12 (Ave Robustus II, 2026-10-08): every mutex-protected member of the cascade is annotated, and Clang's
thread-safety analysis proves the locking protocol at compile time.

### Why
A data race is undefined behaviour that no test reliably reproduces. Clang's analysis turns "this member is read
without its lock" into a compile error: `-Wthread-safety` is in the paranoid warning set (Clang / AppleClang), so with
`-Werror` an unannotated access never builds. GCC and MSVC compile the same code without the check; the Linux pre-push
`clang -fsyntax-only` pass and the macOS build run it.

### What it is
- `ThreadSafety.hpp`: `EMEN_CAPABILITY`, `EMEN_SCOPED_CAPABILITY`, `EMEN_GUARDED_BY`, `EMEN_PT_GUARDED_BY`,
  `EMEN_REQUIRES`, `EMEN_EXCLUDES`, `EMEN_ACQUIRE`, `EMEN_RELEASE`, `EMEN_TRY_ACQUIRE`, `EMEN_RETURN_CAPABILITY` — the
  attribute set of Clang's reference `mutex.h` (https://clang.llvm.org/docs/ThreadSafetyAnalysis.html). Empty on every
  other compiler.
- `Base::Mutex`: a `std::mutex` inside, same size and cost, declared as a capability. Not copyable nor movable.
  `lock()` / `try_lock()` / `unlock()` are public only because `MutexLock` and `std::condition_variable_any` call them.
- `Base::MutexLock`: the scoped guard (`std::scoped_lock`'s role, annotated). It is BasicLockable, so a
  `std::condition_variable_any` waits on it directly.

### How to use it
```cpp
class Something final
{
	public:
		void set (int value) noexcept
		{
			const Base::MutexLock lock{m_access};

			m_value = value;
		}

	private:
		mutable Base::Mutex m_access;
		std::condition_variable_any m_changed;
		int m_value EMEN_GUARDED_BY(m_access){0};
};
```
- Every member the mutex protects: `EMEN_GUARDED_BY(m_access)` (`EMEN_PT_GUARDED_BY` for the pointee of a pointer).
- A private helper called with the lock held: `EMEN_REQUIRES(m_access)`; one that takes the lock itself and must not be
  called with it held: `EMEN_EXCLUDES(m_access)`.
- Lock only through `MutexLock` (CP.20). A `try_lock()` is followed by the analysis only inside an `if` condition, never
  hidden in an assertion macro.
- A notification, a signal or a callback is emitted AFTER the guard's scope closes (CP.31/32; the cascade rule "never
  emit while holding a mutex a handler may take").

### ⚠️ Traps
- A `std::mutex` is NOT annotated in libstdc++: `EMEN_GUARDED_BY(aStdMutex)` is silently useless. Annotated state takes
  a `Base::Mutex`.
- The predicate of `condition_variable_any::wait(lock, predicate)` runs with the mutex held, but the analysis cannot see
  through the wait: declare the lambda `[this] () EMEN_REQUIRES(m_access) { ... }`, or every guarded read in it is a
  warning.
- Verified 2026-10-12: a probe that reads a guarded member without the lock, calls a `REQUIRES` function unlocked and
  releases a mutex it never took gives the three expected diagnostics under `clang -Wthread-safety`;
  `src/Testing/test_Mutex.cpp` (3 tests: 8 threads × 20 000 guarded increments, `try_lock()` while held, a
  `condition_variable_any` wait on the guard) builds with 0 warning and passes under GCC, ASan/UBSan and TSan.

### The ThreadSanitizer build — `EMERAUDE_ENABLE_TSAN`
The analysis proves the annotated protocol; TSan finds the races in what is not annotated (atomics misuse, a lock-free
handoff, a forgotten member). `EMERAUDE_ENABLE_TSAN` (default OFF, GCC / Clang only, exclusive with
`EMERAUDE_ENABLE_SANITIZERS`: the configure step refuses both) adds `-fsanitize=thread -fno-omit-frame-pointer -g -O1`
to `EMERAUDE_COMPILE_OPTIONS` and `-fsanitize=thread` to the link, so it reaches the engine and the applications built
on top; googletest is instrumented too. Run it with `OMP_NUM_THREADS=1`: the system's libgomp is not instrumented and
every VertexFactory OpenMP loop reads as a race otherwise (3374 false reports in one run). First full run, 2026-10-12:
2510 tests, 0 report, after it found the `TimedEvent` sleep (`docs/caution-points.md` § Time). Procedure: projet-alpha
`docs/unit-testing.md` § "ThreadSanitizer".

### The annotated classes
`Time::TimedEvent` (base) and, in the engine, `Audio::TrackMixer` and `Notifier` (2026-10-12). A class that gains a
mutex takes `Base::Mutex` and annotates its members in the same change.
