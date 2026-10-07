---
id: event-trait-join-under-lock
title: EventTrait destroys (joins) its timers while holding the mutex a timer callback may take
status: in-progress
priority: high
scope: Time (EventTrait, TimedEvent)
opened: 2026-09-30
tags: [threads, deadlock, lifetime, robustus]
---

# EventTrait destroys (joins) its timers while holding the mutex a timer callback may take

## Why

Found on 2026-09-30 while fixing a use-after-free in projet-alpha (a scene timer that outlived its act).
`EventTrait::destroyTimer()` and `destroyTimers()` erase the `TimedEvent`s under `m_eventsAccess`, and
`TimedEvent::~TimedEvent()` joins the timer thread. So:

- a callback that calls ANY `EventTrait` method taking `m_eventsAccess` (`createTimer`, `pauseTimer`,
  `destroyTimer`, `isTimerPaused`…) while another thread destroys the timers deadlocks: the destroyer holds
  the mutex and waits for the join, the callback waits for the mutex;
- a callback that destroys ITS OWN timer self-joins (`std::thread::join` on the current thread =
  `resource_deadlock_would_occur`, an abort under `-fno-exceptions`).

No caller does either today (the only engine-side user, projet-alpha's `LightenMarbles`, does not touch the
timer API from its callback). Nothing has deadlocked yet.

## Owner decision (2026-10-07)

Destroy outside the lock; a callback that destroys its OWN timer is DEFERRED (the timer is marked, its thread exits after the callback returns — no self-join).

## What remains

- Destroy outside the lock: move the doomed events out under `m_eventsAccess` (`m_events.extract(id)` / a swap
  with an empty map), release the lock, then let them die.
- Decide the self-destroy contract: refuse it (Debug assert + an error trace) or defer it (mark, and let the
  timer thread exit after the callback returns). Returning the callback's stop value already covers "stop
  me".
- A unit test: a callback that calls `isTimerPaused()` while the main thread calls `destroyTimers()`.

## References

- `src/Time/EventTrait.hpp` (`destroyTimer`, `destroyTimers`), `src/Time/TimedEvent.hpp` (destructor).
- projet-alpha `docs/caution-points.md` § Scene Building (the act / scene timer order).
