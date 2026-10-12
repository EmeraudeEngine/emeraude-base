/*
 * src/Testing/test_Mutex.cpp
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
#include <condition_variable>
#include <cstddef>
#include <thread>
#include <vector>

/* Local inclusions. */
#include "Mutex.hpp"

using namespace EmEn::Base;

namespace
{
	/** @brief A counter whose value the mutex protects: the shape every annotated class takes. */
	class GuardedCounter final
	{
		public:

			void
			increment () noexcept
			{
				const MutexLock lock{m_access};

				++m_value;
			}

			[[nodiscard]]
			std::size_t
			value () const noexcept
			{
				const MutexLock lock{m_access};

				return m_value;
			}

			void
			publish (std::size_t value) noexcept
			{
				{
					const MutexLock lock{m_access};

					m_value = value;
				}

				m_changed.notify_all();
			}

			[[nodiscard]]
			std::size_t
			waitAtLeast (std::size_t threshold) noexcept
			{
				MutexLock lock{m_access};

				m_changed.wait(lock, [this, threshold] () EMEN_REQUIRES(m_access) {
					return m_value >= threshold;
				});

				return m_value;
			}

			[[nodiscard]]
			Mutex &
			access () noexcept EMEN_RETURN_CAPABILITY(m_access)
			{
				return m_access;
			}

		private:

			mutable Mutex m_access;
			std::condition_variable_any m_changed;
			std::size_t m_value EMEN_GUARDED_BY(m_access){0};
	};
}

TEST(Mutex, guardedIncrementsFromManyThreadsAreAllCounted)
{
	constexpr std::size_t ThreadCount{8};
	constexpr std::size_t IncrementsPerThread{20000};

	GuardedCounter counter;
	std::vector< std::thread > threads;
	threads.reserve(ThreadCount);

	for ( std::size_t index = 0; index < ThreadCount; ++index )
	{
		threads.emplace_back([&counter] {
			for ( std::size_t step = 0; step < IncrementsPerThread; ++step )
			{
				counter.increment();
			}
		});
	}

	for ( auto & thread : threads )
	{
		thread.join();
	}

	ASSERT_EQ(counter.value(), ThreadCount * IncrementsPerThread);
}

TEST(Mutex, tryLockFailsWhileAnotherThreadHoldsIt)
{
	GuardedCounter counter;
	std::atomic_bool held{false};
	std::atomic_bool release{false};

	std::thread holder{[&] {
		const MutexLock lock{counter.access()};

		held = true;

		while ( !release )
		{
			std::this_thread::yield();
		}
	}};

	while ( !held )
	{
		std::this_thread::yield();
	}

	/* NOTE: The analysis follows a try_lock() in a condition, never one hidden in an assertion macro. */
	bool acquiredWhileHeld = false;

	if ( counter.access().try_lock() )
	{
		acquiredWhileHeld = true;

		counter.access().unlock();
	}

	ASSERT_FALSE(acquiredWhileHeld);

	release = true;
	holder.join();

	bool acquiredWhenFree = false;

	if ( counter.access().try_lock() )
	{
		acquiredWhenFree = true;

		counter.access().unlock();
	}

	ASSERT_TRUE(acquiredWhenFree);
}

TEST(Mutex, conditionVariableAnyWaitsOnTheGuard)
{
	GuardedCounter counter;

	std::thread producer{[&counter] {
		std::this_thread::sleep_for(std::chrono::milliseconds{20});

		counter.publish(3);
	}};

	ASSERT_EQ(counter.waitAtLeast(3), 3U);

	producer.join();
}
