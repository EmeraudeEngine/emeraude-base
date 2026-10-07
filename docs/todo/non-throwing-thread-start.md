---
id: non-throwing-thread-start
title: A thread that cannot start aborts the process (std::thread's constructor throws)
status: in-progress
priority: high
scope: a new base thread helper (Threading / RAII), then every std::thread construction of the cascade
opened: 2026-10-01
tags: [threads, exceptions, robustness, cascade]
---

# A thread that cannot start aborts the process (std::thread's constructor throws)

## Why

`std::thread`'s constructor throws `std::system_error` when the system cannot start a thread (resource exhaustion, a
thread limit). The cascade builds with `-fno-exceptions` (MSVC: `/EHs- /EHc-`, `_HAS_EXCEPTIONS=0`), so a failure to
start is an abort. Ave Robustus forbids the throwing std calls, but the base offers no non-throwing way to start a
thread. The engine triad 14 (2026-10-01) confirmed the cascade-wide lead of section 2.

The sites, about ten:
- engine `Audio/ExternalInput.cpp` (×2), `Audio/Recorder.cpp`, `Audio/TrackMixer.cpp`;
- `Console/MCP/Server.cpp`, `Console/RemoteListener.cpp`;
- `PlatformSpecific/Helpers.linux.cpp` `executeCommandPumpingEvents()`: its comment claims that `joinable()` catches an
  unstartable thread, which is wrong (the constructor has already thrown);
- `PlatformSpecific/Desktop/Notification.windows.cpp`: a detached thread that also calls `DestroyWindow` on a window
  created by another thread, which fails and leaks the message-only window; a fixed `uID = 1` makes a second notification
  within 6 s fail `NIM_ADD`. It has no caller today.

The owner chose a base helper and a dedicated pass over a change inside section 14.

## Owner decision (2026-10-07)

An RAII `Base::Thread`: `[[nodiscard]] bool start(callable)`, join on destruction or an explicit `detach()`, on
`pthread_create` (POSIX) and `_beginthreadex` (Windows); a failed start is a value. What a refused start means, per
site category:
1. A feature thread (audio input, audio / video recorders, track mixer, remote console, HTTP / MCP server, timers):
   the feature is refused (false + trace); `ExternalInput::start()` and `TimedEvent::start()` return bool,
   `RemoteListener` reports whether it runs.
2. The Tracer's logger thread: synchronous writing, no log lost.
3. `executeCommandPumpingEvents()`: the synchronous path its comment already intends.
4. `ThreadPool`: keeps the workers that started; with none, runs the tasks on the calling thread.
5. `Core`'s logic and rendering threads: a clean start-up failure (an error, a non-zero exit code).

## What remains

Done (2026-10-07, Linux): `Base::Thread` and every site migrated with the policy above — base `ThreadPool`,
`TimedEvent` / `EventTrait`, `HTTPServer`; engine `Core` (logic, rendering), `Tracer` (synchronous fallback),
`Audio::ExternalInput` (×2, `start()` now bool), `Audio::Recorder`, `TrackMixer`, `RemoteListener`,
`Graphics::Recorder` (×2), `Helpers.linux.cpp`, `ParticlesEmitter` (a timed emission whose timer cannot start is
refused); projet-alpha `Application.cpp` (Ctrl+O reports a refused recording). `Notification.windows.cpp` no longer
has a thread: its icon is removed by a `SetTimer()` on the window's own thread, with one id per notification.
Proven on Linux: base 2394 / 2394 Release and ASan + UBSan (tests `BaseThread.*`, `ThreadPool.*NoWorker*`,
`…keepsTheWorkersThatStarted`, `TimeEventTrait.aTimerWhoseThreadCannotStartIsRefused`), `animation-debug` started
and shut down cleanly, MCP conformance 1881 / 0, the log file complete (header, 341 entries, footer).

- [ ] **Windows**: build (`Notification.windows.cpp` was written blind: `SetTimer()` with a `TIMERPROC`, `noexcept`
      conversion), then two notifications within 6 s both show and both icons go away; base suite green.
- [ ] **macOS**: build and base suite green.
- [ ] Not exercised at run time anywhere: the Tracer's synchronous fallback and the engine sites' refusals (the engine
      has no failure seam; the base tests cover the mechanism). Then delete this item.

## References

- `projet-alpha/docs/plans/ave-robustus.md` (no throwing std call).
- projet-alpha `docs/plans/triad-engine-pass-report.md` (per-section record) § 2 (the lead) and § 14.
