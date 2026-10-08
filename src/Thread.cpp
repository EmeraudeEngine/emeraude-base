/*
 * src/Thread.cpp
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


#include "Thread.hpp"

/* STL inclusions. */
#include <atomic>
#include <cerrno>
#include <chrono>
#include <string>
#include <thread>

/* Local inclusions. */
#include "Logging/Logging.hpp"

#if IS_WINDOWS
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif

	#include <Windows.h>
	#include <process.h>
#endif

namespace
{
	/**
	 * @brief Returns how many of the next starts fail (Thread::failNextStartsForTesting()).
	 * @return std::atomic< uint32_t > &
	 */
	[[nodiscard]]
	std::atomic< uint32_t > &
	failingStarts () noexcept
	{
		static std::atomic< uint32_t > counter{0};

		return counter;
	}

	/**
	 * @brief Returns the delay of the next publication, in milliseconds (Thread::delayNextPublicationForTesting()).
	 * @return std::atomic< uint32_t > &
	 */
	[[nodiscard]]
	std::atomic< uint32_t > &
	publicationDelay () noexcept
	{
		static std::atomic< uint32_t > milliseconds{0};

		return milliseconds;
	}

	/**
	 * @brief Holds a new thread until start() has recorded it in its Thread object.
	 * @note A yield loop, not atomic::wait(): wait() needs a notify after the store, and the notifying thread would
	 * then touch the task the waiter may already have deleted. The wait lasts the few instructions start() runs after
	 * the system call.
	 * @param published The task's publication flag.
	 */
	void
	waitUntilSet (const std::atomic_bool & published) noexcept
	{
		while ( !published.load(std::memory_order_acquire) )
		{
			std::this_thread::yield();
		}
	}

	/**
	 * @brief Consumes one forced failure, if any is pending.
	 * @return bool
	 */
	[[nodiscard]]
	bool
	consumeForcedFailure () noexcept
	{
		auto & counter = failingStarts();
		auto pending = counter.load();

		while ( pending > 0 )
		{
			if ( counter.compare_exchange_weak(pending, pending - 1) )
			{
				return true;
			}
		}

		return false;
	}
}

namespace EmEn::Base
{
	Thread::Thread (Thread && other) noexcept
		: m_handle{other.m_handle},
#if IS_WINDOWS
		m_threadID{other.m_threadID},
#endif
		m_joinable{other.m_joinable}
	{
		other.m_handle = {};
#if IS_WINDOWS
		other.m_threadID = 0;
#endif
		other.m_joinable = false;
	}

	Thread &
	Thread::operator= (Thread && other) noexcept
	{
		if ( this != &other )
		{
			this->join();

			m_handle = other.m_handle;
#if IS_WINDOWS
			m_threadID = other.m_threadID;
			other.m_threadID = 0;
#endif
			m_joinable = other.m_joinable;

			other.m_handle = {};
			other.m_joinable = false;
		}

		return *this;
	}

	Thread::~Thread ()
	{
		this->join();
	}

	void
	Thread::failNextStartsForTesting (uint32_t count) noexcept
	{
		failingStarts().store(count);
	}

	void
	Thread::delayNextPublicationForTesting (uint32_t milliseconds) noexcept
	{
		publicationDelay().store(milliseconds);
	}

	bool
	Thread::startTask (std::unique_ptr< TaskBase > task) noexcept
	{
		if ( m_joinable )
		{
			Logging::error("Thread", "Thread::start(), this object already owns a running thread (join or detach it first) !");

			return false;
		}

		if ( task == nullptr ) [[unlikely]]
		{
			return false;
		}

		if ( consumeForcedFailure() )
		{
			Logging::error("Thread", "Thread::start(), the system refused to start a thread (forced by the test seam) !");

			return false;
		}

#if IS_WINDOWS
		unsigned threadID = 0;
		const auto handle = _beginthreadex(nullptr, 0, &Thread::entryPoint, task.get(), 0, &threadID);

		if ( handle == 0 )
		{
			const auto error = errno;

			Logging::error("Thread", std::string{"Thread::start(), the system refused to start a thread (errno "} + std::to_string(error) + ") !");

			return false;
		}

		m_handle = reinterpret_cast< void * >(handle);
		m_threadID = threadID;
#else
		const auto error = pthread_create(&m_handle, nullptr, &Thread::entryPoint, task.get());

		if ( error != 0 )
		{
			Logging::error("Thread", std::string{"Thread::start(), the system refused to start a thread (error "} + std::to_string(error) + ") !");

			m_handle = {};

			return false;
		}
#endif

		if ( const auto delay = publicationDelay().exchange(0); delay > 0 )
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{delay});
		}

		m_joinable = true;

		/* The new thread owns the task now (Thread::entryPoint() deletes it). ⚠️ Publishing is the LAST access to the
		 * task: once the flag is seen, the new thread may run and delete it (hence no notify after the store). */
		task.release()->m_published.store(true, std::memory_order_release);

		return true;
	}

	void
	Thread::join () noexcept
	{
		if ( !m_joinable )
		{
			return;
		}

		if ( this->isCurrentThread() )
		{
			Logging::error("Thread", "Thread::join(), a thread cannot join itself: it is detached instead !");

			this->detach();

			return;
		}

#if IS_WINDOWS
		WaitForSingleObject(m_handle, INFINITE);
		CloseHandle(m_handle);

		m_handle = nullptr;
		m_threadID = 0;
#else
		pthread_join(m_handle, nullptr);

		m_handle = {};
#endif

		m_joinable = false;
	}

	void
	Thread::detach () noexcept
	{
		if ( !m_joinable )
		{
			return;
		}

#if IS_WINDOWS
		CloseHandle(m_handle);

		m_handle = nullptr;
		m_threadID = 0;
#else
		pthread_detach(m_handle);

		m_handle = {};
#endif

		m_joinable = false;
	}

	bool
	Thread::isCurrentThread () const noexcept
	{
		if ( !m_joinable )
		{
			return false;
		}

#if IS_WINDOWS
		return GetCurrentThreadId() == m_threadID;
#else
		return pthread_equal(m_handle, pthread_self()) != 0;
#endif
	}

#if IS_WINDOWS
	unsigned __stdcall
	Thread::entryPoint (void * argument) noexcept
	{
		const std::unique_ptr< TaskBase > task{static_cast< TaskBase * >(argument)};

		waitUntilSet(task->m_published);

		task->run();

		return 0;
	}
#else
	void *
	Thread::entryPoint (void * argument) noexcept
	{
		const std::unique_ptr< TaskBase > task{static_cast< TaskBase * >(argument)};

		waitUntilSet(task->m_published);

		task->run();

		return nullptr;
	}
#endif
}
