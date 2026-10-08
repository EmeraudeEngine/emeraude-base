/*
 * src/Thread.hpp
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

/* Project configuration. */
#include "emeraude_platform.hpp"

/* STL inclusions. */
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#if !IS_WINDOWS
	#include <pthread.h>
#endif

namespace EmEn::Base
{
	/**
	 * @brief A thread started WITHOUT EVER THROWING.
	 * @note std::thread's constructor throws std::system_error when the system cannot start a thread (resource
	 * exhaustion, a thread limit): under -fno-exceptions that is an abort. Thread::start() answers false instead, so each
	 * caller decides what a refused start means (refuse its feature, run synchronously…). Built on pthread_create()
	 * (POSIX) and _beginthreadex() (Windows), with their default stack sizes, like std::thread.
	 * @note RAII: a started thread is JOINED by the destructor (and by a move assignment), unless detach() was called.
	 * A join (or a destruction) from the thread itself is a contract fault: traced and ABORTED in every build (owner
	 * decision D2, 2026-10-08) — the owner must never be destroyed or stopped from its own thread.
	 */
	class Thread final
	{
		public:

			/**
			 * @brief Constructs an idle thread object (nothing runs until start()).
			 */
			Thread () noexcept = default;

			/**
			 * @brief Copy constructor (deleted).
			 * @param copy A reference to the copied instance.
			 */
			Thread (const Thread & copy) noexcept = delete;

			/**
			 * @brief Move constructor: the running thread changes owner.
			 * @param other A reference to the moved instance, idle afterwards.
			 */
			Thread (Thread && other) noexcept;

			/**
			 * @brief Copy assignment (deleted).
			 * @param copy A reference to the copied instance.
			 * @return Thread &
			 */
			Thread & operator= (const Thread & copy) noexcept = delete;

			/**
			 * @brief Move assignment: joins this object's own thread first, then takes the other one's.
			 * @param other A reference to the moved instance, idle afterwards.
			 * @return Thread &
			 */
			Thread & operator= (Thread && other) noexcept;

			/**
			 * @brief Joins the thread, if one runs and it was not detached.
			 */
			~Thread ();

			/**
			 * @brief Starts a thread running a callable.
			 * @note An object that already owns a joinable thread refuses (join() or detach() it first).
			 * @tparam callable_t A callable taking no argument; its return value is ignored.
			 * @param callable The callable, moved (or copied) into the new thread.
			 * @return bool False when the system refused to start a thread (traced): the callable never runs.
			 */
			template< typename callable_t >
			requires std::is_invocable_v< std::decay_t< callable_t > & >
			[[nodiscard]]
			bool
			start (callable_t && callable) noexcept
			{
				return this->startTask(std::make_unique< Task< std::decay_t< callable_t > > >(std::forward< callable_t >(callable)));
			}

			/**
			 * @brief Returns whether a started thread is owned (neither joined nor detached).
			 * @return bool
			 */
			[[nodiscard]]
			bool
			joinable () const noexcept
			{
				return m_joinable;
			}

			/**
			 * @brief Waits for the thread to end. Nothing happens on an idle object.
			 * @pre Not called from the thread itself (directly, or through the destructor / a move assignment): that is a
			 * contract fault, traced and aborted in every build (owner decision D2).
			 */
			void join () noexcept;

			/**
			 * @brief Lets the thread run on its own: this object no longer owns it.
			 */
			void detach () noexcept;

			/**
			 * @brief Returns whether the calling thread is the one this object owns.
			 * @return bool
			 */
			[[nodiscard]]
			bool isCurrentThread () const noexcept;

			/**
			 * @brief TEST SEAM: makes the next start() calls of the process fail as if the system had refused them.
			 * @param count How many starts fail.
			 */
			static void failNextStartsForTesting (uint32_t count) noexcept;

			/**
			 * @brief TEST SEAM: makes the next start() of the process wait after the system started the thread and
			 * before it records it — the window in which the new thread must not observe this object yet.
			 * @param milliseconds The delay.
			 */
			static void delayNextPublicationForTesting (uint32_t milliseconds) noexcept;

		private:

			/** @brief The type-erased callable a new thread runs (move-only callables included). */
			struct TaskBase
			{
				TaskBase () noexcept = default;
				TaskBase (const TaskBase & copy) noexcept = delete;
				TaskBase (TaskBase && copy) noexcept = delete;
				TaskBase & operator= (const TaskBase & copy) noexcept = delete;
				TaskBase & operator= (TaskBase && copy) noexcept = delete;
				virtual ~TaskBase () = default;

				virtual void run () noexcept = 0;

				/** @brief Set by start() once it recorded the thread in its object: the new thread waits for it before
				 * running the callable, so whatever start() wrote happens-before the callable (a self-join included). */
				std::atomic_bool m_published{false};
			};

			/** @brief The callable of one start(). */
			template< typename function_t >
			struct Task final : TaskBase
			{
				template< typename argument_t >
				requires (!std::is_same_v< std::decay_t< argument_t >, Task >)
				explicit
				Task (argument_t && argument) noexcept
					: function{std::forward< argument_t >(argument)}
				{

				}

				void
				run () noexcept override
				{
					static_cast< void >(std::invoke(function));
				}

				function_t function;
			};

			/**
			 * @brief Starts the platform thread running a task, which it then owns.
			 * @param task The task.
			 * @return bool
			 */
			[[nodiscard]]
			bool startTask (std::unique_ptr< TaskBase > task) noexcept;

#if IS_WINDOWS
			/**
			 * @brief The Windows entry point (_beginthreadex()): runs and deletes the task.
			 * @param argument The task, owned.
			 * @return unsigned
			 */
			static unsigned __stdcall entryPoint (void * argument) noexcept;

			void * m_handle{nullptr};
			unsigned long m_threadID{0};
#else
			/**
			 * @brief The POSIX entry point (pthread_create()): runs and deletes the task.
			 * @param argument The task, owned.
			 * @return void *
			 */
			static void * entryPoint (void * argument) noexcept;

			pthread_t m_handle{};
#endif
			bool m_joinable{false};
	};
}
