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
#include <memory>
#include <thread>
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

	ASSERT_TRUE(first.start([&ran, payload = std::move(payload)] { ran = (*payload == 7); }));

	Thread second{std::move(first)};

	EXPECT_FALSE(first.joinable());
	EXPECT_TRUE(second.joinable());

	Thread third;
	third = std::move(second);

	EXPECT_FALSE(second.joinable());
	EXPECT_TRUE(third.joinable());

	third.join();

	EXPECT_TRUE(ran.load());
}

TEST(BaseThread, aDetachedThreadRunsOnItsOwn)
{
	auto ran = std::make_shared< std::atomic_bool >(false);
	Thread thread;

	ASSERT_TRUE(thread.start([ran] { *ran = true; }));
	thread.detach();

	EXPECT_FALSE(thread.joinable());
	EXPECT_TRUE(waitFor(*ran));
}

TEST(BaseThread, aThreadJoiningItselfIsDetachedInsteadOfAborting)
{
	std::atomic_bool joined{false};
	auto thread = std::make_unique< Thread >();
	auto * self = thread.get();

	ASSERT_TRUE(thread->start([self, &joined] {
		/* std::thread::join() here throws std::system_error (resource_deadlock_would_occur): an abort. */
		self->join();

		joined = true;
	}));

	ASSERT_TRUE(waitFor(joined));
	EXPECT_FALSE(thread->joinable());
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

		self->join();

		joined = true;
	}));

	ASSERT_TRUE(waitFor(joined));
	EXPECT_TRUE(sawItsObject.load());
	EXPECT_FALSE(thread->joinable());
}
