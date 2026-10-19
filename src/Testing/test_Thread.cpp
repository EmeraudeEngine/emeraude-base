/*
 * src/Testing/test_Thread.cpp
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
#include <compare>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

/* Local inclusions. */
#include "Thread.hpp"

namespace
{
	using EmEn::Base::Thread;

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

/* std::thread's constructor throws when the system cannot start a thread — an abort under -fno-exceptions. Thread
 * answers the failure as a value (base item non-throwing-thread-start, owner decision 2026-10-07). */
TEST(BaseThread, startsRunsAndJoins)
{
	std::atomic_bool ran{false};
	Thread thread;

	EXPECT_FALSE(thread.joinable());
	ASSERT_TRUE(thread.start([&ran] { ran = true; }));
	EXPECT_TRUE(thread.joinable());

	thread.join();

	EXPECT_FALSE(thread.joinable());
	EXPECT_TRUE(ran.load());
}

TEST(BaseThread, theDestructorJoins)
{
	std::atomic_bool ran{false};

	{
		Thread thread;

		ASSERT_TRUE(thread.start([&ran] {
			std::this_thread::sleep_for(std::chrono::milliseconds{20});

			ran = true;
		}));
	}

	EXPECT_TRUE(ran.load());
}

TEST(BaseThread, aRefusedStartIsAValueAndTheCallableNeverRuns)
{
	std::atomic_bool ran{false};
	Thread thread;

	Thread::failNextStartsForTesting(1);

	EXPECT_FALSE(thread.start([&ran] { ran = true; }));
	EXPECT_FALSE(thread.joinable());

	/* Only one start was refused. */
	ASSERT_TRUE(thread.start([] {}));
	thread.join();

	EXPECT_FALSE(ran.load());
}

TEST(BaseThread, aSecondStartOnARunningThreadIsRefused)
{
	std::atomic_bool release{false};
	Thread thread;

	ASSERT_TRUE(thread.start([&release] { static_cast< void >(waitFor(release)); }));
	EXPECT_FALSE(thread.start([] {}));

	release = true;
	thread.join();
}

TEST(BaseThread, moveOnlyCallablesAndMovesTransferTheThread)
{
	std::atomic_bool ran{false};
	auto payload = std::make_unique< int >(7);
	Thread first;

	ASSERT_TRUE(first.start([&ran, ownedPayload = std::move(payload)] { ran = (*ownedPayload == 7); }));

	Thread second{std::move(first)};

	/* cppcheck-suppress accessMoved ; the test asserts the moved-from state, part of the documented contract. */
	EXPECT_FALSE(first.joinable());
	EXPECT_TRUE(second.joinable());

	Thread third;
	third = std::move(second);

	/* cppcheck-suppress accessMoved ; the test asserts the moved-from state, part of the documented contract. */
	EXPECT_FALSE(second.joinable());
	EXPECT_TRUE(third.joinable());

	third.join();

	EXPECT_TRUE(ran.load());
}

/* Ave Robustus II § 4.2 (owner, 2026-10-15): Thread follows the std::jthread model — a body invocable with a
 * std::stop_token receives the thread's token; the destructor and the move assignment request the stop, then join. */
TEST(BaseThread, aStopRequestedBeforeTheBodyChecksIsSeenAtItsFirstCheck)
{
	std::atomic_bool gate{false};
	std::atomic_bool sawTheStop{false};
	Thread thread;

	ASSERT_TRUE(thread.start([&gate, &sawTheStop] (const std::stop_token & token) {
		static_cast< void >(waitFor(gate));

		sawTheStop = token.stop_requested();
	}));

	EXPECT_TRUE(thread.stopToken().stop_possible());
	EXPECT_FALSE(thread.stopToken().stop_requested());
	EXPECT_TRUE(thread.requestStop());
	/* A second request changes nothing. */
	EXPECT_FALSE(thread.requestStop());

	gate = true;
	thread.join();

	EXPECT_TRUE(sawTheStop.load());
}

TEST(BaseThread, aStopWakesAConditionVariableWaitWithoutANotify)
{
	std::atomic_bool waiting{false};
	std::atomic_bool wokenByTheStop{false};
	Thread thread;

	/* cppcheck-suppress localMutex ; the wait only needs a lock to hold: nothing else shares it, the stop alone ends the wait. */
	ASSERT_TRUE(thread.start([&waiting, &wokenByTheStop] (const std::stop_token & token) {
		std::mutex mutex;
		std::condition_variable_any condition;
		std::unique_lock lock{mutex};

		waiting = true;

		/* The predicate is never true: only the stop can end the wait (it then returns the predicate, false). */
		wokenByTheStop = !condition.wait(lock, token, [] { return false; }) && token.stop_requested();
	}));

	ASSERT_TRUE(waitFor(waiting));
	std::this_thread::sleep_for(std::chrono::milliseconds{20});

	const auto start = std::chrono::steady_clock::now();

	EXPECT_TRUE(thread.requestStop());
	thread.join();

	EXPECT_TRUE(wokenByTheStop.load());
	EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds{2});
}

TEST(BaseThread, theDestructorRequestsTheStopThenJoins)
{
	std::atomic_bool looping{false};
	std::atomic_bool returned{false};

	{
		Thread thread;

		ASSERT_TRUE(thread.start([&looping, &returned] (const std::stop_token & token) {
			while ( !token.stop_requested() )
			{
				looping = true;

				std::this_thread::sleep_for(std::chrono::milliseconds{1});
			}

			returned = true;
		}));

		ASSERT_TRUE(waitFor(looping));
	}

	EXPECT_TRUE(returned.load());
}

TEST(BaseThread, aMoveAssignmentOverARunningThreadStopsAndJoinsIt)
{
	std::atomic_bool firstLooping{false};
	std::atomic_bool firstReturned{false};
	std::atomic_bool secondLooping{false};
	std::atomic_bool secondReturned{false};

	const auto loopUntilStopped = [] (std::atomic_bool & looping, std::atomic_bool & returned) {
		return [&looping, &returned] (const std::stop_token & token) {
			while ( !token.stop_requested() )
			{
				looping = true;

				std::this_thread::sleep_for(std::chrono::milliseconds{1});
			}

			returned = true;
		};
	};

	Thread first;
	Thread second;

	ASSERT_TRUE(first.start(loopUntilStopped(firstLooping, firstReturned)));
	ASSERT_TRUE(second.start(loopUntilStopped(secondLooping, secondReturned)));
	ASSERT_TRUE(waitFor(firstLooping));
	ASSERT_TRUE(waitFor(secondLooping));

	first = std::move(second);

	/* The first body was stopped and joined by the assignment; the second one runs on, now owned by `first`. */
	EXPECT_TRUE(firstReturned.load());
	EXPECT_FALSE(secondReturned.load());
	/* cppcheck-suppress accessMoved ; the test asserts the moved-from state, part of the documented contract. */
	EXPECT_FALSE(second.joinable());
	EXPECT_FALSE(second.stopToken().stop_possible());
	ASSERT_TRUE(first.joinable());

	/* The stop state moved with the thread. */
	EXPECT_TRUE(first.requestStop());
	first.join();

	EXPECT_TRUE(secondReturned.load());
}

TEST(BaseThread, aRestartedThreadGetsAFreshStopState)
{
	Thread thread;

	/* An idle object has no stop state: nothing to request. */
	EXPECT_FALSE(thread.stopToken().stop_possible());
	EXPECT_FALSE(thread.requestStop());

	ASSERT_TRUE(thread.start([] (const std::stop_token & token) {
		while ( !token.stop_requested() )
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{1});
		}
	}));

	EXPECT_TRUE(thread.requestStop());
	thread.join();

	std::atomic_bool stoppedAtStart{true};

	ASSERT_TRUE(thread.start([&stoppedAtStart] (const std::stop_token & token) { stoppedAtStart = token.stop_requested(); }));
	thread.join();

	EXPECT_FALSE(stoppedAtStart.load());
}

TEST(BaseThread, aBodyTakingNoTokenStillRunsAndTheDestructorStillJoinsIt)
{
	/* The form of the engine owners until their migration (engine item jobs-owned-by-their-starter). */
	std::atomic_bool ran{false};

	{
		Thread thread;

		ASSERT_TRUE(thread.start([&ran] {
			std::this_thread::sleep_for(std::chrono::milliseconds{20});

			ran = true;
		}));

		EXPECT_TRUE(thread.stopToken().stop_possible());
	}

	EXPECT_TRUE(ran.load());
}

/* Owner decision D2 (2026-10-08): a self-join is a contract fault — its owner is destroyed or stopped from its own thread,
 * which then runs on a dead object. It used to be detached silently (a hidden use-after-free); it aborts now, in every
 * build. */
TEST(BaseThreadDeathTest, aThreadJoiningItselfAborts)
{
	EXPECT_DEATH({
		Thread thread;
		std::atomic_bool started{false};
		auto * self = &thread;

		if ( thread.start([self, &started] {
			while ( !started.load() )
			{
				std::this_thread::yield();
			}

			self->join();
		}) )
		{
			started = true;
			/* The destructor would join the aborted thread: the process never gets there. */
			std::this_thread::sleep_for(std::chrono::seconds{10});
		}
	}, "joins itself");
}

TEST(BaseThread, aNewThreadSeesItsObjectAlreadyRecorded)
{
	/* macOS peer, 2026-10-07: under load the new thread could run before start() recorded it (m_joinable, the
	 * handle), so its self-join saw an idle object, and start() then marked it joinable. The seam holds start()
	 * inside that window. */
	std::atomic_bool joined{false};
	std::atomic_bool sawItsObject{false};
	auto thread = std::make_unique< Thread >();
	auto * self = thread.get();

	Thread::delayNextPublicationForTesting(100);

	ASSERT_TRUE(thread->start([self, &joined, &sawItsObject] {
		sawItsObject = self->joinable() && self->isCurrentThread();

		joined = true;
	}));

	ASSERT_TRUE(waitFor(joined));
	thread->join();
	EXPECT_TRUE(sawItsObject.load());
	EXPECT_FALSE(thread->joinable());
}
