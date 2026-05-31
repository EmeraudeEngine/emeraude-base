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
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Time/Time.hpp"
#include "Time/EventTrait.hpp"
#include "Time/Elapsed/CPUTime.hpp"

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