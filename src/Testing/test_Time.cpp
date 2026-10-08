/*
 * src/Testing/test_Time.cpp
 * This file is part of Emeraude-Base
 *
 * Copyright (C) 2010-2026 - Sébastien Léon Claude Christian Bémelmans "LondNoir" <londnoir@gmail.com>
 *
 * Emeraude-Base is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Emeraude-Base is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Emeraude-Base; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Complete project and additional information can be found at :
 * https://github.com/EmeraudeEngine/emeraude-base
 *
 * --- THIS IS AUTOMATICALLY GENERATED, DO NOT CHANGE ---
 */

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ratio>
#include <thread>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Thread.hpp"
#include "Time/Time.hpp"
#include "Time/EventTrait.hpp"
#include "Time/Elapsed/CPUTime.hpp"
#include "Time/Statistics/CPUTime.hpp"
#include "Time/Statistics/RealTime.hpp"

namespace EmEn::Base::Time
{
	namespace
	{
		/* EventTrait is a trait: its constructor and resetTimer() are protected.
		 * This subclass makes it instantiable and exposes resetTimer() for testing. */
		class TestableEventTrait final : public EventTrait<>
		{
			public:

				using EventTrait<>::resetTimer;
		};
	}

	/*
	 * Exercises the whole withTimer-based timer API. Before the fix, withTimer was
	 * declared const, so every mutating call (start/stop/pause/resume/setGranularity/
	 * reset) failed to compile — hidden because nothing ever instantiated EventTrait.
	 * State flags are set synchronously under the timer mutex, so the assertions are
	 * deterministic regardless of thread scheduling; the granularity is huge so the
	 * callback never fires during the test (no race, no flakiness).
	 */
	TEST(TimeEventTrait, fullTimerLifecycle)
	{
		TestableEventTrait events;

		std::atomic< int > fired{0};
		const auto callback = [&fired] (TimerID) {
			fired.fetch_add(1, std::memory_order_relaxed);

			return false;
		};

		const auto timerID = events.createTimer(callback, 3'600'000U /* 1 h */, false, false);
		ASSERT_NE(timerID, 0U);
		EXPECT_FALSE(events.isTimerStarted(timerID));

		/* Mutating ops return true when the timer is found (intent of @return bool). */
		EXPECT_TRUE(events.startTimer(timerID));
		EXPECT_TRUE(events.isTimerStarted(timerID));

		EXPECT_TRUE(events.pauseTimer(timerID));
		EXPECT_TRUE(events.isTimerPaused(timerID));

		EXPECT_TRUE(events.resumeTimer(timerID));
		EXPECT_TRUE(events.isTimerStarted(timerID));
		EXPECT_FALSE(events.isTimerPaused(timerID));

		EXPECT_TRUE(events.setTimerGranularity(timerID, 7'200'000U));
		events.resetTimer(timerID);              // the original compile-breaker (resetTop → reset)

		/* Unknown id → withTimer default → false / safe no-op. */
		EXPECT_FALSE(events.startTimer(999999));
		events.resetTimer(999999);

		EXPECT_TRUE(events.stopTimer(timerID));
		EXPECT_FALSE(events.isTimerStarted(timerID));

		events.destroyTimers();                  // joins the timer thread cleanly

		/* Huge granularity → the callback must never have fired. */
		EXPECT_EQ(fired.load(std::memory_order_relaxed), 0);
	}

	namespace
	{
		/**
		 * @brief Waits for a flag, at most a few seconds.
		 * @param flag The flag.
		 * @return bool Whether it was raised in time.
		 */
		bool
		waitFor (const std::atomic_bool & flag) noexcept
		{
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};

			while ( !flag.load() )
			{
				if ( std::chrono::steady_clock::now() > deadline )
				{
					return false;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds{1});
			}

			return true;
		}
	}

	/*
	 * A callback that uses the trait while another thread destroys the timers. destroyTimers() used to erase the timers
	 * under m_eventsAccess, and erasing joins each timer thread: the destroyer held the mutex waiting for the callback,
	 * the callback waited for the mutex — a deadlock (2026-09-30, found by reading). The timers are now destroyed
	 * outside the lock. A deadlock cannot be asserted from inside: past 5 s the test reports it and leaves the process.
	 */
	TEST(TimeEventTrait, aCallbackMayUseTheTraitWhileItsTimersAreDestroyed)
	{
		TestableEventTrait events;

		std::atomic_bool inCallback{false};
		std::atomic_bool callbackReturned{false};

		static_cast< void >(events.createTimer([&events, &inCallback, &callbackReturned] (TimerID self) {
			inCallback = true;

			/* Long enough for the destroyer to take the lock first. */
			std::this_thread::sleep_for(std::chrono::milliseconds{100});

			static_cast< void >(events.isTimerPaused(self));

			callbackReturned = true;

			return true;
		}, 1U, true, true));

		ASSERT_TRUE(waitFor(inCallback));

		std::atomic_bool destroyed{false};
		std::thread destroyer{[&events, &destroyed] {
			events.destroyTimers();

			destroyed = true;
		}};

		if ( !waitFor(destroyed) )
		{
			ADD_FAILURE() << "destroyTimers() deadlocked with a callback that uses the trait.";

			std::fflush(stdout);
			std::_Exit(EXIT_FAILURE);
		}

		destroyer.join();

		EXPECT_TRUE(callbackReturned.load());
	}

	/*
	 * A callback that destroys its OWN timer. Erasing it there destroyed the TimedEvent on its own thread, whose
	 * destructor joined that thread — std::thread::join() on the current thread throws, an abort under
	 * -fno-exceptions. Owner decision (2026-10-07): DEFERRED — the timer is taken out at once (it never fires again)
	 * and its thread ends when the callback returns; the trait joins it later, from another thread.
	 */
	TEST(TimeEventTrait, aCallbackMayDestroyItsOwnTimer)
	{
		TestableEventTrait events;

		std::atomic_int fired{0};
		std::atomic_bool callbackReturned{false};

		const auto timerID = events.createTimer([&events, &fired, &callbackReturned] (TimerID self) {
			fired.fetch_add(1);

			events.destroyTimer(self);

			callbackReturned = true;

			/* A REPEATING timer: only the destruction may stop it. */
			return false;
		}, 1U, false, true);

		ASSERT_NE(timerID, 0U);
		ASSERT_TRUE(waitFor(callbackReturned));

		/* Fifty periods later it fired once, and it is gone. */
		std::this_thread::sleep_for(std::chrono::milliseconds{50});

		EXPECT_EQ(fired.load(), 1);
		EXPECT_FALSE(events.isTimerStarted(timerID));
		EXPECT_FALSE(events.startTimer(timerID));

		/* Joins the retired thread (and nothing is left for the destructor). */
		events.destroyTimers();
	}

	/* A timer whose thread the system refuses (Base::Thread, owner policy 2026-10-07) is refused: createTimer() answers
	 * 0 and keeps nothing, startTimer() answers false. TimedEvent used to start a std::thread — an abort there. */
	TEST(TimeEventTrait, aTimerWhoseThreadCannotStartIsRefused)
	{
		TestableEventTrait events;

		Thread::failNextStartsForTesting(1);
		EXPECT_EQ(events.createTimer([] (TimerID) { return true; }, 1U, true, true), 0U);

		const auto timerID = events.createTimer([] (TimerID) { return false; }, 3'600'000U, false, false);
		ASSERT_NE(timerID, 0U);

		Thread::failNextStartsForTesting(1);
		EXPECT_FALSE(events.startTimer(timerID));
		EXPECT_FALSE(events.isTimerStarted(timerID));

		/* The next start works. */
		EXPECT_TRUE(events.startTimer(timerID));
		EXPECT_TRUE(events.isTimerStarted(timerID));

		events.destroyTimers();
	}

	namespace
	{
		/**
		 * @brief Deterministic CPUTime: scripts the CPU-clock source so the
		 * nanosecond duration / unit conversion can be asserted with no dependency
		 * on real timing (zero flakiness).
		 */
		class ScriptedCPUTime final : public Elapsed::CPUTime
		{
			public:

				explicit ScriptedCPUTime (std::vector< uint64_t > values) noexcept
					: m_values{std::move(values)}
				{

				}

			protected:

				[[nodiscard]]
				uint64_t
				currentCPUNanoseconds () const noexcept override
				{
					const std::size_t index = m_index < m_values.size() ? m_index : m_values.size() - 1;
					++m_index;

					return m_values[index];
				}

			private:

				std::vector< uint64_t > m_values;
				mutable std::size_t m_index{0};
		};
	}

	TEST(TimeElapsedCPUTime, deterministicNsConversion)
	{
		/* start() reads 5.0 s, stop() reads 5.1 s → delta = 100 ms.
		 * Guards the historical bug where raw clock() ticks were stored as nanoseconds. */
		ScriptedCPUTime cpuTime{{5'000'000'000ULL, 5'100'000'000ULL}};

		cpuTime.start();
		cpuTime.stop();

		EXPECT_EQ(cpuTime.duration(), 100'000'000ULL);
		EXPECT_DOUBLE_EQ(cpuTime.microseconds(), 100'000.0);
		EXPECT_DOUBLE_EQ(cpuTime.milliseconds(), 100.0);
		EXPECT_DOUBLE_EQ(cpuTime.seconds(), 0.1);
	}

	TEST(TimeElapsedCPUTime, realClockIsMonotonic)
	{
		const auto first = processCPUTimeNanoseconds();

		volatile double sink = 0.0;
		for ( int i = 0; i < 2'000'000; ++i )
		{
			sink += static_cast< double >(i) * 0.5;
		}
		(void)sink;

		const auto second = processCPUTimeNanoseconds();

		/* Process CPU time never decreases — safe lower bound, not timing-sensitive. */
		EXPECT_GE(second, first);
	}
}

namespace
{
	/** @brief A clock whose time the test sets: it can go backwards, as a non-monotonic clock does. */
	struct ScriptedStatisticsClock final
	{
		using rep = int64_t;
		using period = std::milli;
		using duration = std::chrono::duration< rep, period >;
		using time_point = std::chrono::time_point< ScriptedStatisticsClock >;

		static constexpr bool is_steady{false};

		static
		rep &
		current () noexcept
		{
			static rep value{0};

			return value;
		}

		static
		time_point
		now () noexcept
		{
			return time_point{duration{current()}};
		}
	};
}

/* 2026-10-08: CPUTime recorded nothing when CLOCKS_PER_SEC was not 1e3, 1e6 or 1e9. The conversion is now generic and
 * exact, without overflow. */
TEST(TimeStatisticsCPUTime, ticksToMillisecondsForAnyRate)
{
	using EmEn::Base::Time::Statistics::CPUTime;

	EXPECT_EQ(CPUTime::ticksToMilliseconds(1'500, 1'000), 1'500U);
	EXPECT_EQ(CPUTime::ticksToMilliseconds(1'500'000, 1'000'000), 1'500U);
	EXPECT_EQ(CPUTime::ticksToMilliseconds(1'500'000'000, 1'000'000'000), 1'500U);
	/* An unusual rate: 128 ticks per second. */
	EXPECT_EQ(CPUTime::ticksToMilliseconds(192, 128), 1'500U);
	EXPECT_EQ(CPUTime::ticksToMilliseconds(1, 128), 7U);
	EXPECT_EQ(CPUTime::ticksToMilliseconds(0, 1'000), 0U);
	EXPECT_EQ(CPUTime::ticksToMilliseconds(1'000, 0), 0U);

	/* No overflow at the top of the range. */
	constexpr uint64_t Huge{UINT64_MAX};
	constexpr uint64_t Rate{1'000'000'000};

	EXPECT_EQ(CPUTime::ticksToMilliseconds(Huge, Rate), ((Huge / Rate) * 1000) + (((Huge % Rate) * 1000) / Rate));
}

/* 2026-10-08 (owner decision): a sample that is not a measurement is dropped AND counted. */
TEST(TimeStatisticsRealTime, aBackwardsSampleIsDroppedAndCounted)
{
	ScriptedStatisticsClock::current() = 1'000;

	EmEn::Base::Time::Statistics::RealTime< ScriptedStatisticsClock > statistics;

	statistics.start();
	ScriptedStatisticsClock::current() = 900;
	statistics.stop();

	EXPECT_EQ(statistics.droppedSampleCount(), 1U);
	EXPECT_EQ(statistics.topCount(), 0U);

	statistics.start();
	ScriptedStatisticsClock::current() = 950;
	statistics.stop();

	EXPECT_EQ(statistics.droppedSampleCount(), 1U);
	EXPECT_EQ(statistics.topCount(), 1U);
	EXPECT_EQ(statistics.duration(), 50U);
}
