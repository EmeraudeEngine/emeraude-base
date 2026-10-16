/*
 * src/Mutex.hpp
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

#pragma once

/* STL inclusions. */
#include <mutex>

/* Local inclusions. */
#include "ThreadSafety.hpp"

namespace EmEn::Base
{
	class ConditionVariable;

	/**
	 * @brief A mutex the Clang thread-safety analysis knows (decision D12): annotated members name it in
	 * EMEN_GUARDED_BY(), and every access outside a MutexLock is a compile-time warning (an error under -Werror).
	 * @note A std::mutex inside, same cost. Lock it only through a MutexLock (CP.20: no manual lock() / unlock() in user
	 * code); lock() / unlock() are public because MutexLock and std::condition_variable_any call them.
	 * @note Not copyable nor movable, like std::mutex.
	 */
	class EMEN_CAPABILITY("mutex") Mutex final
	{
		public:

			/** @brief Constructs an unlocked mutex. */
			Mutex () noexcept = default;

			/** @brief Copy constructor. */
			Mutex (const Mutex & copy) noexcept = delete;

			/** @brief Move constructor. */
			Mutex (Mutex && copy) noexcept = delete;

			/** @brief Copy assignment. @return Mutex & */
			Mutex & operator= (const Mutex & copy) noexcept = delete;

			/** @brief Move assignment. @return Mutex & */
			Mutex & operator= (Mutex && copy) noexcept = delete;

			/** @brief Destructs the mutex (it must not be held). */
			~Mutex () = default;

			/**
			 * @brief Acquires the mutex, waiting for it. Through a MutexLock only.
			 * @note std::mutex::lock() reports a system error by exception, which this build cannot throw: such an error
			 * (EDEADLK / EINVAL, a programming fault) terminates the process, as any std::mutex here.
			 */
			void
			lock () noexcept EMEN_ACQUIRE()
			{
				m_mutex.lock();
			}

			/**
			 * @brief Acquires the mutex if it is free.
			 * @return bool True when acquired.
			 */
			[[nodiscard]]
			bool
			try_lock () noexcept EMEN_TRY_ACQUIRE(true)
			{
				return m_mutex.try_lock();
			}

			/** @brief Releases the mutex. Through a MutexLock only. */
			void
			unlock () noexcept EMEN_RELEASE()
			{
				m_mutex.unlock();
			}

		private:

			/* NOTE: ConditionVariable waits on the std::mutex itself (the cost of a std::condition_variable). */
			friend class ConditionVariable;

			std::mutex m_mutex;
	};

	/**
	 * @brief The RAII guard of a Mutex (std::scoped_lock's role, annotated): acquired by the constructor, released by the
	 * destructor.
	 * @note ConditionVariable waits on it at the cost of a std::condition_variable (ConditionVariable.hpp). It is also
	 * BasicLockable: std::condition_variable_any waits on it, which a std::stop_token wait needs (wait(lock, predicate)
	 * releases and reacquires the mutex around the wait; the analysis treats the wait as opaque, the predicate reads
	 * under the lock).
	 */
	class EMEN_SCOPED_CAPABILITY MutexLock final
	{
		public:

			/**
			 * @brief Acquires the mutex.
			 * @param mutex A reference to the mutex. It outlives the guard.
			 */
			explicit
			MutexLock (Mutex & mutex) noexcept EMEN_ACQUIRE(mutex)
				: m_mutex{mutex}
			{
				m_mutex.lock();
			}

			/** @brief Copy constructor. */
			MutexLock (const MutexLock & copy) noexcept = delete;

			/** @brief Move constructor. */
			MutexLock (MutexLock && copy) noexcept = delete;

			/** @brief Copy assignment. @return MutexLock & */
			MutexLock & operator= (const MutexLock & copy) noexcept = delete;

			/** @brief Move assignment. @return MutexLock & */
			MutexLock & operator= (MutexLock && copy) noexcept = delete;

			/** @brief Releases the mutex. */
			~MutexLock () EMEN_RELEASE()
			{
				m_mutex.unlock();
			}

			/** @brief Reacquires the mutex (BasicLockable, for std::condition_variable_any). */
			void
			lock () noexcept EMEN_ACQUIRE()
			{
				m_mutex.lock();
			}

			/** @brief Releases the mutex (BasicLockable, for std::condition_variable_any). */
			void
			unlock () noexcept EMEN_RELEASE()
			{
				m_mutex.unlock();
			}

		private:

			friend class ConditionVariable;

			Mutex & m_mutex;
	};
}
