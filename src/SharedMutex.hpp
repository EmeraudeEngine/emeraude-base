/*
 * src/SharedMutex.hpp
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
#include <shared_mutex>

/* Local inclusions. */
#include "ThreadSafety.hpp"

namespace EmEn::Base
{
	/**
	 * @brief A reader-writer mutex the Clang thread-safety analysis knows (decision D12, Ave Robustus II P-W): a member it
	 * protects is EMEN_GUARDED_BY(it); a read needs at least a SharedMutexReadLock, a write a SharedMutexWriteLock — the
	 * analysis tells the two apart (a write under a read lock is a compile-time warning).
	 * @note A std::shared_mutex inside, same cost. Lock it only through its guards (CP.20); the members are public
	 * because the guards call them.
	 * @note Not copyable nor movable, like std::shared_mutex.
	 */
	class EMEN_CAPABILITY("shared mutex") SharedMutex final
	{
		public:

			/** @brief Constructs an unlocked mutex. */
			SharedMutex () noexcept = default;

			/** @brief Copy constructor. */
			SharedMutex (const SharedMutex & copy) noexcept = delete;

			/** @brief Move constructor. */
			SharedMutex (SharedMutex && copy) noexcept = delete;

			/** @brief Copy assignment. @return SharedMutex & */
			SharedMutex & operator= (const SharedMutex & copy) noexcept = delete;

			/** @brief Move assignment. @return SharedMutex & */
			SharedMutex & operator= (SharedMutex && copy) noexcept = delete;

			/** @brief Destructs the mutex (it must not be held). */
			~SharedMutex () = default;

			/**
			 * @brief Acquires the mutex exclusively (a writer). Through a SharedMutexWriteLock only.
			 * @note A system error (a programming fault) terminates the process: this build cannot throw.
			 */
			void
			lock () noexcept EMEN_ACQUIRE()
			{
				m_mutex.lock();
			}

			/** @brief Releases the exclusive hold. Through a SharedMutexWriteLock only. */
			void
			unlock () noexcept EMEN_RELEASE()
			{
				m_mutex.unlock();
			}

			/** @brief Acquires the mutex shared (a reader). Through a SharedMutexReadLock only. */
			void
			lock_shared () noexcept EMEN_ACQUIRE_SHARED()
			{
				m_mutex.lock_shared();
			}

			/** @brief Releases a shared hold. Through a SharedMutexReadLock only. */
			void
			unlock_shared () noexcept EMEN_RELEASE_SHARED()
			{
				m_mutex.unlock_shared();
			}

		private:

			std::shared_mutex m_mutex;
	};

	/** @brief The exclusive (writer) RAII guard of a SharedMutex: acquired by the constructor, released by the destructor. */
	class EMEN_SCOPED_CAPABILITY SharedMutexWriteLock final
	{
		public:

			/**
			 * @brief Acquires the mutex exclusively.
			 * @param mutex A reference to the mutex. It outlives the guard.
			 */
			explicit
			SharedMutexWriteLock (SharedMutex & mutex) noexcept EMEN_ACQUIRE(mutex)
				: m_mutex{mutex}
			{
				m_mutex.lock();
			}

			/** @brief Copy constructor. */
			SharedMutexWriteLock (const SharedMutexWriteLock & copy) noexcept = delete;

			/** @brief Move constructor. */
			SharedMutexWriteLock (SharedMutexWriteLock && copy) noexcept = delete;

			/** @brief Copy assignment. @return SharedMutexWriteLock & */
			SharedMutexWriteLock & operator= (const SharedMutexWriteLock & copy) noexcept = delete;

			/** @brief Move assignment. @return SharedMutexWriteLock & */
			SharedMutexWriteLock & operator= (SharedMutexWriteLock && copy) noexcept = delete;

			/** @brief Releases the mutex. */
			~SharedMutexWriteLock () EMEN_RELEASE()
			{
				m_mutex.unlock();
			}

		private:

			SharedMutex & m_mutex;
	};

	/** @brief The shared (reader) RAII guard of a SharedMutex: acquired by the constructor, released by the destructor. */
	class EMEN_SCOPED_CAPABILITY SharedMutexReadLock final
	{
		public:

			/**
			 * @brief Acquires the mutex shared.
			 * @param mutex A reference to the mutex. It outlives the guard.
			 */
			explicit
			SharedMutexReadLock (SharedMutex & mutex) noexcept EMEN_ACQUIRE_SHARED(mutex)
				: m_mutex{mutex}
			{
				m_mutex.lock_shared();
			}

			/** @brief Copy constructor. */
			SharedMutexReadLock (const SharedMutexReadLock & copy) noexcept = delete;

			/** @brief Move constructor. */
			SharedMutexReadLock (SharedMutexReadLock && copy) noexcept = delete;

			/** @brief Copy assignment. @return SharedMutexReadLock & */
			SharedMutexReadLock & operator= (const SharedMutexReadLock & copy) noexcept = delete;

			/** @brief Move assignment. @return SharedMutexReadLock & */
			SharedMutexReadLock & operator= (SharedMutexReadLock && copy) noexcept = delete;

			/** @brief Releases the shared hold. */
			~SharedMutexReadLock () EMEN_RELEASE()
			{
				m_mutex.unlock_shared();
			}

		private:

			SharedMutex & m_mutex;
	};
}
