---
id: non-throwing-thread-start
title: A thread that cannot start aborts the process (std::thread's constructor throws)
status: open
priority: unranked
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

## What remains

- [ ] Design the helper with the owner: an RAII `Thread` (join on destruction, or an explicit detach) with
  `[[nodiscard]] bool start(callable)`, implemented on `pthread_create` (POSIX) and `_beginthreadex` (Windows), and
  reporting the failure as a value. Unit tests: a start, a join, the destructor's join, and a refused start (inject the
  failure through a test hook, or a `RLIMIT_NPROC` sandbox on Linux).
- [ ] Migrate every site above. Each caller decides what a failed start means: refuse the feature with an error, or
  fall back to the synchronous path, as `executeCommandPumpingEvents()` already intends.
- [ ] Fix `Notification.windows.cpp` with it: remove the icon from a timer on the window's own thread, and use a
  distinct `uID` per notification.
- [ ] Re-test on the three OS; `std::thread{` / `std::thread(` absent from the cascade (`/usr/bin/grep -rn`).

## References

- `projet-alpha/docs/plans/ave-robustus.md` (no throwing std call).
- projet-alpha `docs/plans/triad-engine-pass-report.md` (per-section record) § 2 (the lead) and § 14.
