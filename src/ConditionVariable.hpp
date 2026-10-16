/*
 * src/ConditionVariable.hpp
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
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <utility>

/* Local inclusions. */
#include "Mutex.hpp"

namespace EmEn::Base
{
	/**
	 * @brief A condition variable that waits on a Mutex held through a MutexLock (decision D12, Ave Robustus II P-W).
	 * @note The cost of a std::condition_variable: it waits on the std::mutex inside the Mutex itself, where
	 * std::condition_variable_any adds an internal mutex and a shared pointer to every wait.
	 * @note The predicate runs with the mutex held: declare it EMEN_REQUIRES(theMutex) so the analysis checks what it
	 * reads (the analysis treats the wait itself as opaque).
	 * @note No std::stop_token wait: C++20 offers it on std::condition_variable_any only. A wait that a stop request must
	 * wake keeps std::condition_variable_any, over the MutexLock (BasicLockable).
	 * @note Not copyable nor movable, like std::condition_variable.
	 */
	class ConditionVariable final
	{
		public:

			/** @brief Constructs a condition variable. */
			ConditionVariable () noexcept = default;

			/** @brief Copy constructor. */
			ConditionVariable (const ConditionVariable & copy) noexcept = delete;

			/** @brief Move constructor. */
			ConditionVariable (ConditionVariable && copy) noexcept = delete;

			/** @brief Copy assignment. @return ConditionVariable & */
			ConditionVariable & operator= (const ConditionVariable & copy) noexcept = delete;

			/** @brief Move assignment. @return ConditionVariable & */
			ConditionVariable & operator= (ConditionVariable && copy) noexcept = delete;

			/** @brief Destructs the condition variable (no thread may still wait on it). */
			~ConditionVariable () = default;

			/** @brief Wakes one waiting thread. */
			void
			notifyOne () noexcept
			{
				m_condition.notify_one();
			}

			/** @brief Wakes every waiting thread. */
			void
			notifyAll () noexcept
			{
				m_condition.notify_all();
			}

			/**
			 * @brief Releases the lock and waits until the predicate holds; returns with the lock held again.
			 * @tparam predicate_t A callable returning bool, run with the mutex held.
			 * @param lock A reference to the guard holding the mutex.
			 * @param predicate The condition to wait for.
			 */
			template< typename predicate_t >
			void
			wait (MutexLock & lock, predicate_t predicate) noexcept
			{
				/* NOTE: The guard holds the std::mutex: the unique_lock adopts it for the wait and gives it back without
				 * unlocking (release()), so the guard still owns it afterwards. */
				std::unique_lock< std::mutex > native{lock.m_mutex.m_mutex, std::adopt_lock};

				m_condition.wait(native, std::move(predicate));

				static_cast< void >(native.release());
			}

			/**
			 * @brief Releases the lock and waits until the predicate holds or the timeout expires; returns with the lock
			 * held again.
			 * @tparam rep_t The duration's representation.
			 * @tparam period_t The duration's period.
			 * @tparam predicate_t A callable returning bool, run with the mutex held.
			 * @param lock A reference to the guard holding the mutex.
			 * @param timeout The longest wait.
			 * @param predicate The condition to wait for.
			 * @return bool The predicate's value at return: false means the timeout expired first.
			 */
			template< typename rep_t, typename period_t, typename predicate_t >
			[[nodiscard]]
			bool
			waitFor (MutexLock & lock, const std::chrono::duration< rep_t, period_t > & timeout, predicate_t predicate) noexcept
			{
				std::unique_lock< std::mutex > native{lock.m_mutex.m_mutex, std::adopt_lock};

				const auto satisfied = m_condition.wait_for(native, timeout, std::move(predicate));

				static_cast< void >(native.release());

				return satisfied;
			}

			/**
			 * @brief Releases the lock and waits until the predicate holds or the deadline passes; returns with the lock
			 * held again.
			 * @tparam clock_t The deadline's clock.
			 * @tparam duration_t The deadline's duration.
			 * @tparam predicate_t A callable returning bool, run with the mutex held.
			 * @param lock A reference to the guard holding the mutex.
			 * @param deadline The time point to stop waiting at.
			 * @param predicate The condition to wait for.
			 * @return bool The predicate's value at return: false means the deadline passed first.
			 */
			template< typename clock_t, typename duration_t, typename predicate_t >
			[[nodiscard]]
			bool
			waitUntil (MutexLock & lock, const std::chrono::time_point< clock_t, duration_t > & deadline, predicate_t predicate) noexcept
			{
				std::unique_lock< std::mutex > native{lock.m_mutex.m_mutex, std::adopt_lock};

				const auto satisfied = m_condition.wait_until(native, deadline, std::move(predicate));

				static_cast< void >(native.release());

				return satisfied;
			}

		private:

			std::condition_variable m_condition;
	};
}
