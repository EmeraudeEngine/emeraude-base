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
#include "ConditionVariable.hpp"
#include "Mutex.hpp"
#include "SharedMutex.hpp"

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

namespace
{
	/** @brief A value published under a Mutex and awaited through a ConditionVariable. */
	class GuardedMailbox final
	{
		public:

			void
			post (std::size_t value) noexcept
			{
				{
					const MutexLock lock{m_access};

					m_value = value;
				}

				m_changed.notifyAll();
			}

			[[nodiscard]]
			std::size_t
			waitAtLeast (std::size_t threshold) noexcept
			{
				MutexLock lock{m_access};

				m_changed.wait(lock, [this, threshold] () EMEN_REQUIRES(m_access) {
					return m_value >= threshold;
				});

				/* NOTE: The guard holds the mutex again: this read is checked by the analysis like any other. */
				return m_value;
			}

			[[nodiscard]]
			bool
			waitAtLeastFor (std::size_t threshold, std::chrono::milliseconds timeout) noexcept
			{
				MutexLock lock{m_access};

				return m_changed.waitFor(lock, timeout, [this, threshold] () EMEN_REQUIRES(m_access) {
					return m_value >= threshold;
				});
			}

			[[nodiscard]]
			bool
			waitAtLeastUntil (std::size_t threshold, std::chrono::steady_clock::time_point deadline) noexcept
			{
				MutexLock lock{m_access};

				return m_changed.waitUntil(lock, deadline, [this, threshold] () EMEN_REQUIRES(m_access) {
					return m_value >= threshold;
				});
			}

			[[nodiscard]]
			Mutex &
			access () noexcept EMEN_RETURN_CAPABILITY(m_access)
			{
				return m_access;
			}

		private:

			Mutex m_access;
			ConditionVariable m_changed;
			std::size_t m_value EMEN_GUARDED_BY(m_access){0};
	};

	/** @brief A table read by many readers at once and written by one writer at a time. */
	class GuardedTable final
	{
		public:

			void
			write (std::size_t value) noexcept
			{
				const SharedMutexWriteLock lock{m_access};

				m_value = value;
				++m_writes;
			}

			[[nodiscard]]
			std::size_t
			writes () const noexcept
			{
				const SharedMutexReadLock lock{m_access};

				return m_writes;
			}

			/** @brief Reads under a shared lock and reports how many readers were inside together, at most. */
			void
			readWhileCounting (std::atomic< int > & inside, std::atomic< int > & maximum, std::atomic_bool & release) const noexcept
			{
				const SharedMutexReadLock lock{m_access};

				const auto now = inside.fetch_add(1) + 1;

				auto seen = maximum.load();

				while ( seen < now && !maximum.compare_exchange_weak(seen, now) )
				{
					/* NOTE: compare_exchange_weak() reloaded `seen`: compare again. */
				}

				while ( !release.load() )
				{
					std::this_thread::yield();
				}

				static_cast< void >(m_value);

				inside.fetch_sub(1);
			}

		private:

			mutable SharedMutex m_access;
			std::size_t m_value EMEN_GUARDED_BY(m_access){0};
			std::size_t m_writes EMEN_GUARDED_BY(m_access){0};
	};
}

TEST(Mutex, aGuardAdoptsASuccessfulTryLockAndReleasesIt)
{
	Mutex access;

	{
		if ( !access.try_lock() )
		{
			FAIL() << "a free mutex refused try_lock()";
		}

		const MutexLock lock{access, std::adopt_lock};

		/* NOTE: Held by the adopted guard: another thread cannot take it. */
		std::atomic_bool acquiredElsewhere{false};

		std::thread other{[&access, &acquiredElsewhere] {
			if ( access.try_lock() )
			{
				acquiredElsewhere = true;

				access.unlock();
			}
		}};

		other.join();

		ASSERT_FALSE(acquiredElsewhere.load());
	}

	/* NOTE: The guard's end released it. */
	bool acquiredAfter = false;

	if ( access.try_lock() )
	{
		acquiredAfter = true;

		access.unlock();
	}

	ASSERT_TRUE(acquiredAfter);
}

TEST(ConditionVariable, waitReturnsWhenThePredicateHoldsWithTheMutexHeldAgain)
{
	GuardedMailbox mailbox;

	std::thread producer{[&mailbox] {
		std::this_thread::sleep_for(std::chrono::milliseconds{20});

		mailbox.post(3);
	}};

	ASSERT_EQ(mailbox.waitAtLeast(3), 3U);

	producer.join();

	/* NOTE: The wait gave the mutex back to the guard, and the guard's end released it: it is free now. */
	bool acquired = false;

	if ( mailbox.access().try_lock() )
	{
		acquired = true;

		mailbox.access().unlock();
	}

	ASSERT_TRUE(acquired);
}

TEST(ConditionVariable, waitReturnsAtOnceWhenThePredicateAlreadyHolds)
{
	GuardedMailbox mailbox;
	mailbox.post(5);

	ASSERT_EQ(mailbox.waitAtLeast(1), 5U);
}

TEST(ConditionVariable, waitForReportsTheTimeoutAndTheSuccess)
{
	GuardedMailbox mailbox;

	const auto start = std::chrono::steady_clock::now();

	ASSERT_FALSE(mailbox.waitAtLeastFor(1, std::chrono::milliseconds{30}));
	ASSERT_GE(std::chrono::steady_clock::now() - start, std::chrono::milliseconds{30});

	std::thread producer{[&mailbox] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});

		mailbox.post(1);
	}};

	ASSERT_TRUE(mailbox.waitAtLeastFor(1, std::chrono::seconds{10}));

	producer.join();
}

TEST(ConditionVariable, waitUntilReportsTheDeadlineAndTheSuccess)
{
	GuardedMailbox mailbox;

	ASSERT_FALSE(mailbox.waitAtLeastUntil(1, std::chrono::steady_clock::now() + std::chrono::milliseconds{30}));

	mailbox.post(2);

	ASSERT_TRUE(mailbox.waitAtLeastUntil(2, std::chrono::steady_clock::now() + std::chrono::milliseconds{30}));
}

TEST(SharedMutex, readersHoldTheLockTogether)
{
	constexpr int ReaderCount{4};

	const GuardedTable table;
	std::atomic< int > inside{0};
	std::atomic< int > maximum{0};
	std::atomic_bool release{false};

	std::vector< std::thread > readers;
	readers.reserve(ReaderCount);

	for ( int index = 0; index < ReaderCount; ++index )
	{
		readers.emplace_back([&] {
			table.readWhileCounting(inside, maximum, release);
		});
	}

	/* NOTE: Every reader enters before any leaves (they wait for the release inside the lock). */
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};

	while ( maximum.load() < ReaderCount && std::chrono::steady_clock::now() < deadline )
	{
		std::this_thread::yield();
	}

	release = true;

	for ( auto & reader : readers )
	{
		reader.join();
	}

	ASSERT_EQ(maximum.load(), ReaderCount);
}

TEST(SharedMutex, aWriterExcludesTheReadersAndTheOtherWriters)
{
	constexpr std::size_t WriterCount{8};
	constexpr std::size_t WritesPerWriter{5000};

	GuardedTable table;
	std::vector< std::thread > writers;
	writers.reserve(WriterCount);

	for ( std::size_t index = 0; index < WriterCount; ++index )
	{
		writers.emplace_back([&table, index] {
			for ( std::size_t step = 0; step < WritesPerWriter; ++step )
			{
				table.write((index * WritesPerWriter) + step);

				static_cast< void >(table.writes());
			}
		});
	}

	for ( auto & writer : writers )
	{
		writer.join();
	}

	ASSERT_EQ(table.writes(), WriterCount * WritesPerWriter);
}
