/*
 * src/ThreadPool.hpp
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
#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <concepts>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Mutex.hpp"
#include "Thread.hpp"
#include "ThreadSafety.hpp"

namespace EmEn::Base
{
	class ThreadPool;

	/**
	 * @class TaskHandle
	 * @brief Owns one task submitted with ThreadPool::submit(): the std::jthread model applied to a pool task
	 * (Ave Robustus II, owner decision D1, 2026-10-08).
	 *
	 * The task receives a std::stop_token. Destroying (or reassigning) the handle requests the task to stop and waits
	 * for THAT task only — never for the whole pool. When wait() returns after a task RAN, its callable and everything
	 * it captured are destroyed. The object that starts a job keeps its handle as a member: its own destruction then
	 * stops and waits for the job, so nothing else (a global flag, another class's drain) has to.
	 *
	 * A task stopped BEFORE it starts never runs (owner decisions 2026-10-15, the std::execution semantics: a stop before
	 * the start completes as "stopped"): the stop request REMOVES it from the pool's queue and destroys its callable on
	 * the stopping thread, without invoking it. So a wait after a stop depends on a RUNNING task only, never on the queue
	 * (nor on other owners' work queued before it): an owner destroyed on a pool worker cannot deadlock a saturated pool,
	 * and a shutdown never waits for unrelated queued work.
	 *
	 * @note Thread-safe: requestStop(), finished() and wait() may be called from any thread. A task waiting for (or
	 * destroying) its OWN handle would never return: a contract fault, traced and aborted in every build (owner decision
	 * D2, 2026-10-08).
	 */
	class TaskHandle final
	{
		public:

			/** @brief The state shared by a handle and its task. */
			struct State final
			{
				std::stop_source stopSource;
				std::mutex mutex;
				std::condition_variable finishedSignal;
				std::thread::id runningThread;
				/** @brief The pool the task was submitted to (set before the task is queued, never changed). */
				ThreadPool * pool{nullptr};
				bool started{false};
				bool finished{false};
			};

			/** @brief Constructs an empty handle (no task; valid() is false). */
			TaskHandle () noexcept = default;

			/**
			 * @brief Constructs a handle over a task's shared state (ThreadPool::submit()).
			 * @param state The shared state.
			 */
			explicit
			TaskHandle (std::shared_ptr< State > state) noexcept
				: m_state{std::move(state)}
			{

			}

			/** @brief Destructs the handle: requests the task to stop, then waits for it. */
			~TaskHandle ()
			{
				this->reset();
			}

			/** @brief Copy constructor (deleted: one owner per task). */
			TaskHandle (const TaskHandle & copy) = delete;

			/** @brief Copy assignment (deleted). */
			TaskHandle & operator= (const TaskHandle & copy) = delete;

			/**
			 * @brief Move constructor: the task changes owner, the source becomes empty.
			 * @param other The handle to take the task from.
			 */
			TaskHandle (TaskHandle && other) noexcept = default;

			/**
			 * @brief Move assignment: the task held before is stopped and waited for, then the other one is taken.
			 * @param other The handle to take the task from.
			 * @return TaskHandle &
			 */
			TaskHandle &
			operator= (TaskHandle && other) noexcept
			{
				if ( this != &other )
				{
					this->reset();

					m_state = std::move(other.m_state);
				}

				return *this;
			}

			/**
			 * @brief Returns whether the handle owns a task (false when empty, moved from, reset, or refused by the pool).
			 * @return bool
			 */
			[[nodiscard]]
			bool
			valid () const noexcept
			{
				return m_state != nullptr;
			}

			/**
			 * @brief Asks the task to stop (its std::stop_token reports it); a task still queued is removed from the pool's
			 * queue and destroyed here, never run. No effect on an empty handle.
			 * @pre The pool outlives the handle while its task may still be queued (a pool runs its whole queue before it
			 * is destroyed).
			 */
			void requestStop () noexcept;

			/**
			 * @brief Returns whether the task has finished, or will never run (stopped before it started). An empty handle
			 * reports true.
			 * @return bool
			 */
			[[nodiscard]]
			bool finished () const noexcept;

			/**
			 * @brief Blocks until the task has finished (or was removed from the queue by a stop) and its callable is
			 * destroyed. Returns at once on an empty handle.
			 * @note Without a stop, a task still queued is waited for: from a pool worker, on a saturated pool, that can
			 * deadlock — an owner STOPS first (reset(), the destructor).
			 * @pre Not called from the task's own thread (it would never return): a contract fault, aborted (decision D2).
			 */
			void wait () noexcept;

			/** @brief Requests the task to stop, waits for it and releases the handle (valid() becomes false). */
			void reset () noexcept;

		private:

			std::shared_ptr< State > m_state;
	};

	/**
	 * @class TaskRunner
	 * @brief The callable a ThreadPool runs for ThreadPool::submit(): runs the task with its stop token, destroys it, then
	 * marks the handle finished — unless the stop came before the start. A runner destroyed without running (refused by
	 * the pool, or removed from the queue by a stop) still marks the handle finished, so no wait() can hang on it.
	 * @tparam function_t The task's callable type, invocable with a std::stop_token.
	 */
	template< typename function_t >
	class TaskRunner final
	{
		public:

			/**
			 * @brief Constructs the runner.
			 * @param state The state shared with the handle.
			 * @param function The task.
			 */
			template< typename forwarded_t >
			TaskRunner (std::shared_ptr< TaskHandle::State > state, forwarded_t && function) noexcept
				: m_state{std::move(state)},
				m_function{std::in_place, std::forward< forwarded_t >(function)}
			{

			}

			/** @brief Destructs the runner: a task that never ran still releases its handle. */
			~TaskRunner ()
			{
				this->finish();
			}

			TaskRunner (const TaskRunner & copy) = delete;
			TaskRunner & operator= (const TaskRunner & copy) = delete;
			TaskRunner (TaskRunner && other) noexcept = default;
			TaskRunner & operator= (TaskRunner && other) noexcept = delete;

			/** @brief Runs the task, destroys it, then reports the handle finished. */
			void
			operator() () noexcept
			{
				if ( m_state == nullptr || !m_function.has_value() )
				{
					return;
				}

				bool started = false;

				{
					const std::scoped_lock lock{m_state->mutex};

					/* NOTE: Stopped before it started: it never runs (its waiter may already be gone). Decided under the
					 * state's lock, against which wait() reads `started`. */
					if ( !m_state->stopSource.stop_requested() )
					{
						m_state->started = true;
						m_state->runningThread = std::this_thread::get_id();

						started = true;
					}
				}

				if ( started )
				{
					(*m_function)(m_state->stopSource.get_token());
				}

				this->finish();
			}

		private:

			/** @brief Destroys the callable FIRST, then marks the handle finished (the waiter may destroy its owner next). */
			void
			finish () noexcept
			{
				m_function.reset();

				if ( m_state == nullptr )
				{
					return;
				}

				{
					/* NOTE: notified UNDER the lock (see PeerStore / SharingServer, 2026-10-08). */
					const std::scoped_lock lock{m_state->mutex};

					m_state->finished = true;
					m_state->runningThread = {};
					m_state->finishedSignal.notify_all();
				}

				m_state.reset();
			}

			std::shared_ptr< TaskHandle::State > m_state;
			std::optional< function_t > m_function;
	};

	/**
	 * @class TaskGroup
	 * @brief Owns every task submitted with ThreadPool::submit(TaskGroup &, …): the TaskHandle contract for an object
	 * that runs SEVERAL tasks at once (a resource container's loads, a network manager's transfers).
	 *
	 * The group's tasks share one std::stop_source. Destroying the group requests the stop and waits for EVERY task of the
	 * group — never for the rest of the pool; when wait() returns, the callable and the captures of every task that RAN
	 * are destroyed. Once a stop is requested the group admits no new task (submit() refuses it): the stop is final.
	 *
	 * A task stopped BEFORE it starts never runs (owner decisions 2026-10-15, as TaskHandle): the stop request removes the
	 * group's queued tasks from the pool's queue and destroys their callables on the stopping thread, without invoking
	 * them. After a stop, wait() waits for the RUNNING tasks only — never for the queue, nor for other owners' work queued
	 * before the group's.
	 *
	 * State of the art: the async scope of C++26 std::execution (P3149, `counting_scope`: a join that waits for the work
	 * spawned into the scope, a stop request forwarded to it) and the nurseries of N. J. Smith's Trio ("Notes on
	 * structured concurrency, or: Go statement considered harmful", 2018, https://vorpus.org/blog/) — no task outlives
	 * the scope that started it.
	 *
	 * @note Thread-safe: tasks may be submitted, and requestStop(), stopRequested(), pendingCount() and wait() called,
	 * from any thread. A task waiting for (or destroying) its OWN group would never return: a contract fault, traced and
	 * aborted in every build (owner decision D2, 2026-10-08).
	 * @note The owner declares its group AFTER every member the tasks use: the group is then destroyed (stopped, waited
	 * for) first.
	 */
	class TaskGroup final
	{
		public:

			/** @brief The state shared by a group and its tasks. */
			struct State final
			{
				/**
				 * @brief Counts one more task, unless a stop was requested.
				 * @param threadPool The pool the task goes to: a group serves ONE pool (the first submit sets it).
				 * @return bool False when the group is stopping or the pool is not the group's (the task must not run).
				 */
				[[nodiscard]]
				bool admit (ThreadPool & threadPool) noexcept;

				/**
				 * @brief Lets a dequeued task start, unless a stop was requested (it then never runs).
				 * @return bool True when the task may run.
				 */
				[[nodiscard]]
				bool begin () noexcept;

				/** @brief Counts one task less (a task that ran, or one that never will); the last one wakes the waiters. */
				void release () noexcept;

				std::stop_source stopSource;
				mutable Mutex mutex;
				std::condition_variable_any idleSignal;
				ThreadPool * pool EMEN_GUARDED_BY(mutex){nullptr};
				size_t pending EMEN_GUARDED_BY(mutex){0};
				bool stopping EMEN_GUARDED_BY(mutex){false};
			};

			/** @brief Constructs an empty group (no task, no stop requested). */
			TaskGroup () noexcept;

			/** @brief Destructs the group: requests the stop, then waits for every task of the group. */
			~TaskGroup ();

			/** @brief Copy constructor (deleted: the tasks hold the group's state, one owner). */
			TaskGroup (const TaskGroup & copy) = delete;

			/** @brief Move constructor (deleted: a group is a member of its owner, it never changes owner). */
			TaskGroup (TaskGroup && other) = delete;

			/** @brief Copy assignment (deleted). @return TaskGroup & */
			TaskGroup & operator= (const TaskGroup & copy) = delete;

			/** @brief Move assignment (deleted). @return TaskGroup & */
			TaskGroup & operator= (TaskGroup && other) = delete;

			/**
			 * @brief Asks every task of the group to stop (their std::stop_token reports it), removes the queued ones from
			 * the pool's queue (destroyed here, never run) and refuses any new task.
			 * @note Final: a stopped group never admits a task again.
			 * @pre The pool outlives the group while one of its tasks may still be queued.
			 */
			void requestStop () noexcept;

			/**
			 * @brief Returns whether a stop was requested.
			 * @return bool
			 */
			[[nodiscard]]
			bool stopRequested () const noexcept;

			/**
			 * @brief Returns the number of tasks submitted and not finished yet (queued or running).
			 * @note Stale as soon as it returns when tasks are submitted concurrently.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t pendingCount () const noexcept;

			/**
			 * @brief Blocks until every task of the group has finished and its callable is destroyed. Does not stop them;
			 * after a stop, only the RUNNING tasks are left to wait for (the queued ones were removed).
			 * @note Without a stop, queued tasks are waited for: from a pool worker, on a saturated pool, that can deadlock
			 * — an owner STOPS first (the destructor does).
			 * @pre Not called from one of the group's own tasks (it would never return): a contract fault, aborted
			 * (decision D2).
			 */
			void wait () noexcept;

			/**
			 * @brief wait(), refused from a pool worker: the strict form for an owner shutting down on the main thread.
			 * After it returns, no callable of the group exists any more (a discarded task's "never ran" guard has run).
			 * @pre Not called from a thread-pool worker: it would wait for the queue its own worker serves (a deadlock on
			 * a saturated pool). A contract fault, traced and aborted in every build.
			 */
			void waitForAll () noexcept;

		private:

			friend class ThreadPool;

			/**
			 * @brief Counts a new task for ThreadPool::submit().
			 * @param threadPool The pool the task goes to.
			 * @return std::shared_ptr< State > The state the task holds, or nullptr when the group is stopping (or serves
			 * another pool).
			 */
			[[nodiscard]]
			std::shared_ptr< State > admit (ThreadPool & threadPool) noexcept;

			std::shared_ptr< State > m_state;
	};

	/**
	 * @class TaskGroupRunScope
	 * @brief Marks, on the calling thread, that a task of a group runs: TaskGroup::wait() refuses to wait for a group
	 * whose task runs on the waiting thread (decision D2). Nested scopes chain (a pool without worker runs a task on the
	 * submitting thread, possibly from inside another task).
	 */
	class TaskGroupRunScope final
	{
		public:

			/**
			 * @brief Opens the scope.
			 * @param state The group whose task runs.
			 */
			explicit TaskGroupRunScope (const TaskGroup::State * state) noexcept;

			/** @brief Closes the scope. */
			~TaskGroupRunScope ();

			TaskGroupRunScope (const TaskGroupRunScope & copy) = delete;
			TaskGroupRunScope (TaskGroupRunScope && other) = delete;
			TaskGroupRunScope & operator= (const TaskGroupRunScope & copy) = delete;
			TaskGroupRunScope & operator= (TaskGroupRunScope && other) = delete;

			/**
			 * @brief Returns whether a task of this group runs on the calling thread.
			 * @param state The group.
			 * @return bool
			 */
			[[nodiscard]]
			static bool runsOnThisThread (const TaskGroup::State * state) noexcept;

		private:

			const TaskGroup::State * m_state;
			const TaskGroupRunScope * m_previous;
	};

	/**
	 * @class TaskGroupRunner
	 * @brief The callable a ThreadPool runs for ThreadPool::submit(TaskGroup &, …): runs the task with the group's stop
	 * token — unless the group stopped before the task started — destroys it, then releases its count. A runner destroyed
	 * without running (refused by the pool, or removed from the queue by a stop) still releases its count, so no wait()
	 * can hang on it.
	 * @tparam function_t The task's callable type, invocable with a std::stop_token.
	 */
	template< typename function_t >
	class TaskGroupRunner final
	{
		public:

			/**
			 * @brief Constructs the runner.
			 * @param state The group's state, already counting this task (TaskGroup::admit()).
			 * @param function The task.
			 */
			template< typename forwarded_t >
			TaskGroupRunner (std::shared_ptr< TaskGroup::State > state, forwarded_t && function) noexcept
				: m_state{std::move(state)},
				m_function{std::in_place, std::forward< forwarded_t >(function)}
			{

			}

			/** @brief Destructs the runner: a task that never ran still releases its count. */
			~TaskGroupRunner ()
			{
				this->finish();
			}

			TaskGroupRunner (const TaskGroupRunner & copy) = delete;
			TaskGroupRunner & operator= (const TaskGroupRunner & copy) = delete;
			TaskGroupRunner (TaskGroupRunner && other) noexcept = default;
			TaskGroupRunner & operator= (TaskGroupRunner && other) noexcept = delete;

			/** @brief Runs the task, destroys it, then releases its count. */
			void
			operator() () noexcept
			{
				if ( m_state == nullptr || !m_function.has_value() )
				{
					return;
				}

				/* NOTE: Stopped before it started: it never runs (its group's waiter may already be gone). */
				if ( !m_state->begin() )
				{
					this->finish();

					return;
				}

				/* NOTE: Open while the callable runs AND while its captures are destroyed: a capture whose release destroys
				 * the group's owner is a task waiting for its own group (D2). */
				const TaskGroupRunScope scope{m_state.get()};

				(*m_function)(m_state->stopSource.get_token());

				this->finish();
			}

		private:

			/** @brief Destroys the callable FIRST, then releases the count (the waiter may destroy the group's owner next). */
			void
			finish () noexcept
			{
				m_function.reset();

				if ( m_state == nullptr )
				{
					return;
				}

				m_state->release();
				m_state.reset();
			}

			std::shared_ptr< TaskGroup::State > m_state;
			std::optional< function_t > m_function;
	};

	/**
	 * @class ThreadPool
	 * @brief High-performance thread pool optimized for game engine workloads.
	 *
	 * ThreadPool provides efficient task-based parallelism with several optimizations
	 * designed for real-time applications and game engines. The implementation focuses
	 * on minimizing allocation overhead and simplicity for reliability.
	 *
	 * ## Key Features
	 *
	 * ### Small Buffer Optimization (SBO)
	 * Tasks up to 48 bytes are stored inline without heap allocation, covering most
	 * lambdas with small captures (approximately 6 pointers). Larger tasks automatically
	 * fall back to heap allocation. This eliminates memory allocation for the common case.
	 *
	 * ### Single Shared Queue
	 * Uses a single shared task queue protected by a mutex. This design provides
	 * simplicity and reliability while still achieving good performance for typical
	 * game engine workloads. The single-queue design eliminates complex synchronization
	 * issues that can arise with more sophisticated approaches.
	 *
	 * ### Template-Based Task Submission
	 * The template-based enqueue() avoids std::function overhead for known callable types,
	 * while still maintaining compatibility with std::function for type-erased callables.
	 * This eliminates virtual dispatch costs in performance-critical code.
	 *
	 * ### Data-Parallel Support
	 * The parallelFor() method provides an optimized fork-join pattern for data-parallel
	 * workloads. It distributes work automatically across available workers using atomic
	 * chunk distribution, and the calling thread participates in the work to avoid
	 * wasting resources. Blocks until completion, providing implicit synchronization.
	 *
	 * ### Batch Operations
	 * The enqueueBatch() method allows submitting multiple tasks under a single lock
	 * acquisition, reducing synchronization overhead for bulk task submission.
	 *
	 * ## Thread Safety
	 * All public methods are thread-safe and can be called concurrently from any thread.
	 * The pool uses condition variables for efficient waiting and notification.
	 *
	 * ## Usage Example
	 * @code
	 * // Create a thread pool with hardware thread count
	 * EmEn::Base::ThreadPool pool;
	 *
	 * // Fire-and-forget task submission
	 * pool.enqueue([] {
	 *	 std::cout << "Hello from worker thread\n";
	 * });
	 *
	 * // Task with result
	 * auto future = pool.enqueueWithResult([] {
	 *	 return 42;
	 * });
	 * int result = future.get(); // Blocks until task completes
	 *
	 * // Data-parallel loop
	 * std::vector<int> data(10000);
	 * pool.parallelFor(size_t{0}, data.size(), [&](size_t i) {
	 *	 data[i] = compute(i);
	 * });
	 * // parallelFor blocks until all iterations complete
	 *
	 * // Batch task submission
	 * std::vector<std::function<void()>> tasks;
	 * tasks.push_back([] { doWork1(); });
	 * tasks.push_back([] { doWork2(); });
	 * pool.enqueueBatch(tasks.begin(), tasks.end());
	 *
	 * // Wait for all tasks to finish
	 * pool.wait();
	 * @endcode
	 *
	 * ## Performance Considerations
	 * - Tasks smaller than 48 bytes avoid heap allocation entirely
	 * - For small workloads, parallelFor() falls back to sequential execution to avoid overhead
	 * - The single-queue design trades maximum throughput for reliability and simplicity
	 * - For typical game engine workloads, queue contention is negligible
	 *
	 * ## Implementation Details
	 * - Workers wait on a condition variable when the queue is empty
	 * - Task completion signals a separate condition variable for wait() callers
	 * - The destructor gracefully drains remaining tasks before joining workers
	 * - Move-only Task wrapper supports lambdas with move-only captures (e.g., std::unique_ptr)
	 *
	 * @note Thread-safe: All public methods can be called from any thread concurrently.
	 * @note Platform support: Tested on Linux (G++ 13.3+), macOS (AppleClang 17.0+), Windows (MSVC 2022+).
	 * @note Requires C++20 for concepts and std::invocable support.
	 * @see Task
	 * @version 1.1.6
	 */
	class ThreadPool final
	{
		public:

			/** @brief Small buffer size for inline task storage (48 bytes = 6 pointers). */
			static constexpr size_t SmallBufferSize{48};

			/**
			 * @class Task
			 * @brief Move-only callable wrapper with Small Buffer Optimization.
			 *
			 * Task is a type-erased callable wrapper designed to minimize heap allocations
			 * for small function objects. It uses Small Buffer Optimization (SBO) to store
			 * callables up to SmallBufferSize (48 bytes) inline, avoiding dynamic allocation
			 * entirely for the common case.
			 *
			 * This class serves as a C++20 alternative to std::move_only_function (which was
			 * introduced in C++23) and provides better performance characteristics than
			 * std::function for thread pool workloads by being move-only.
			 *
			 * ## Storage Strategy
			 * - **Small callables** (≤48 bytes, standard alignment): Stored in inline buffer
			 * - **Large callables** (>48 bytes or over-aligned): Heap-allocated via new/delete
			 *
			 * ## Type Erasure
			 * Uses function pointers for invoke, destroy, and move operations to achieve
			 * type erasure without virtual dispatch overhead.
			 *
			 * ## Move Semantics
			 * Task is move-only (non-copyable) to avoid expensive copy operations and to
			 * support move-only captured values in lambdas (e.g., std::unique_ptr).
			 *
			 * ## Usage Example
			 * @code
			 * // Small lambda (fits in SBO)
			 * ThreadPool::Task task1{[] { std::cout << "Hello\n"; }};
			 *
			 * // Lambda with small capture (fits in SBO)
			 * int x = 42;
			 * ThreadPool::Task task2{[x] { std::cout << x << "\n"; }};
			 *
			 * // Large capture (heap-allocated)
			 * std::array<int, 100> largeData{};
			 * ThreadPool::Task task3{[largeData] { ... }};
			 *
			 * // Move-only capture
			 * auto ptr = std::make_unique<int>(42);
			 * ThreadPool::Task task4{[p = std::move(ptr)] { std::cout << *p << "\n"; }};
			 *
			 * // Invoke the task
			 * task1(); // Calls the stored callable
			 * @endcode
			 *
			 * @note Move-only: Cannot be copied, only moved.
			 * @note Noexcept guarantee: Move operations are noexcept for small callables.
			 * @see ThreadPool
			 */
			class Task final
			{
				public:

					/**
					 * @brief Constructs an empty task.
					 * @post empty() returns true.
					 */
					Task () noexcept = default;

					/**
					 * @brief Constructs a task from any invocable object.
					 *
					 * Stores the callable using Small Buffer Optimization if it fits within
					 * SmallBufferSize (48 bytes) and has standard alignment. Larger or over-aligned
					 * callables are heap-allocated.
					 *
					 * @tparam function_t Callable type (lambda, function pointer, functor, etc.).
					 * @param callable The callable object to store. Forwarded to avoid unnecessary copies.
					 * @pre F must be invocable with no arguments: callable().
					 * @post empty() returns false, operator bool() returns true.
					 * @note Noexcept if the callable takes the small-buffer path (size AND
					 *	   alignment fit) and its construction from the forwarded argument
					 *	   (copy for lvalues, move for rvalues) cannot throw.
					 */
					template< typename function_t >
					requires std::invocable< function_t > && (!std::is_same_v< std::decay_t< function_t >, Task >)
					explicit
					Task (function_t && callable) noexcept(sizeof(std::decay_t< function_t >) <= SmallBufferSize && alignof(std::decay_t< function_t >) <= alignof(std::max_align_t) && std::is_nothrow_constructible_v< std::decay_t< function_t >, function_t >)
					{
						using DecayedF = std::decay_t< function_t >;

						if constexpr ( sizeof(DecayedF) <= SmallBufferSize && alignof(DecayedF) <= alignof(std::max_align_t) )
						{
							/* Small buffer: construct in-place. */
							::new (static_cast< void * >(m_storage)) DecayedF(std::forward< function_t >(callable));
							m_invoke = &invokeSmall< DecayedF >;
							m_destroy = &destroySmall< DecayedF >;
							m_move = &moveSmall< DecayedF >;
							m_isSmall = true;
						}
						else
						{
							/* Large buffer: heap allocate. */
							m_heapPtr = new DecayedF(std::forward< function_t >(callable));
							m_invoke = &invokeLarge< DecayedF >;
							m_destroy = &destroyLarge< DecayedF >;
							m_move = &moveLarge< DecayedF >;
							m_isSmall = false;
						}
					}

					/**
					 * @brief Deleted copy constructor.
					 * @note Task is move-only to support move-only captures and avoid expensive copies.
					 */
					Task (const Task &) = delete;

					/**
					 * @brief Deleted copy assignment operator.
					 * @note Task is move-only to support move-only captures and avoid expensive copies.
					 */
					Task & operator= (const Task &) = delete;

					/**
					 * @brief Move constructor.
					 * @param other Task to move from.
					 * @post other.empty() returns true after the move.
					 */
					Task (Task && other) noexcept
					{
						if ( other.m_invoke != nullptr )
						{
							other.m_move(&other, this);
							other.m_invoke = nullptr;
							other.m_destroy = nullptr;
							other.m_move = nullptr;
							other.m_isSmall = false;
						}

						m_owner = std::exchange(other.m_owner, nullptr);
					}

					/**
					 * @brief Move assignment operator.
					 *
					 * Destroys the current task (if any) and moves the contents of other
					 * into this task.
					 *
					 * @param other Task to move from.
					 * @return Reference to this task.
					 * @post other.empty() returns true after the move.
					 * @post If this == &other, no operation is performed (self-assignment safe).
					 */
					Task &
					operator= (Task && other) noexcept
					{
						if ( this != &other )
						{
							this->clear();

							if ( other.m_invoke != nullptr )
							{
								other.m_move(&other, this);
								other.m_invoke = nullptr;
								other.m_destroy = nullptr;
								other.m_move = nullptr;
								other.m_isSmall = false;
							}

							m_owner = std::exchange(other.m_owner, nullptr);
						}

						return *this;
					}

					/**
					 * @brief Destructor.
					 *
					 * Destroys the stored callable, invoking its destructor. For heap-allocated
					 * callables, releases the allocated memory.
					 */
					~Task () noexcept
					{
						this->clear();
					}

					/**
					 * @brief Invokes the stored callable.
					 *
					 * Executes the callable that was passed to the constructor. After invocation,
					 * the task remains valid and can be invoked again (though this is not typical
					 * usage in a thread pool context).
					 *
					 * @pre empty() must return false (task must contain a callable).
					 * @note Invoking an empty task asserts in debug builds and is a safe
					 *	   no-op in release builds.
					 * @note Any exceptions thrown by the callable propagate to the caller.
					 */
					void
					operator() ()
					{
						assert(m_invoke != nullptr && "Attempted to invoke an empty task!");

						if ( m_invoke != nullptr )
						{
							m_invoke(this);
						}
					}

					/**
					 * @brief Checks if the task contains a callable.
					 * @return True if the task contains a callable (not empty), false otherwise.
					 * @note Equivalent to !empty().
					 */
					[[nodiscard]]
					explicit
					operator bool () const noexcept
					{
						return m_invoke != nullptr;
					}

					/**
					 * @brief Checks if the task is empty.
					 * @return True if the task does not contain a callable, false otherwise.
					 * @note Empty tasks are created by the default constructor or after being moved from.
					 */
					[[nodiscard]]
					bool
					empty () const noexcept
					{
						return m_invoke == nullptr;
					}

					/**
					 * @brief Checks if the callable is stored inline using Small Buffer Optimization.
					 * @return True if using inline storage (callable ≤48 bytes), false if heap-allocated.
					 * @note Always false for an empty task (default-constructed, moved-from or cleared).
					 * @note Useful for debugging and performance analysis.
					 */
					[[nodiscard]]
					bool
					isSmall () const noexcept
					{
						return m_isSmall;
					}

					/**
					 * @brief Tags the task with the state of the TaskHandle / TaskGroup that owns it: their stop request
					 * removes it from the queue (ThreadPool::purgeQueued()). nullptr (the default) for an unowned task.
					 * @param owner The owner's state address, used as an identity only.
					 */
					void
					setOwner (const void * owner) noexcept
					{
						m_owner = owner;
					}

					/**
					 * @brief Returns the owner tag (setOwner()).
					 * @return const void *
					 */
					[[nodiscard]]
					const void *
					owner () const noexcept
					{
						return m_owner;
					}

				private:

					/**
					 * @brief Clears the stored callable.
					 */
					void
					clear () noexcept
					{
						if ( m_destroy != nullptr )
						{
							m_destroy(this);
							m_invoke = nullptr;
							m_destroy = nullptr;
							m_move = nullptr;
						}

						m_isSmall = false;
					}

					/* Type-erased function pointers. */
					template< typename function_t >
					static void
					invokeSmall (Task * self)
					{
						(*reinterpret_cast< function_t * >(self->m_storage))();
					}

					template< typename function_t >
					static void
					invokeLarge (Task * self)
					{
						(*static_cast< function_t * >(self->m_heapPtr))();
					}

					template< typename function_t >
					static void
					destroySmall (Task * self) noexcept
					{
						reinterpret_cast< function_t * >(self->m_storage)->~function_t();
					}

					template< typename function_t >
					static void
					destroyLarge (Task * self) noexcept
					{
						delete static_cast< function_t * >(self->m_heapPtr);
					}

					template< typename function_t >
					static void
					moveSmall (Task * from, Task * to) noexcept
					{
						::new (static_cast< void * >(to->m_storage)) function_t(std::move(*reinterpret_cast< function_t * >(from->m_storage)));
						reinterpret_cast< function_t * >(from->m_storage)->~function_t();
						to->m_invoke = from->m_invoke;
						to->m_destroy = from->m_destroy;
						to->m_move = from->m_move;
						to->m_isSmall = true;
					}

					template< typename function_t >
					static void
					moveLarge (Task * from, Task * to) noexcept
					{
						to->m_heapPtr = from->m_heapPtr;
						from->m_heapPtr = nullptr;
						to->m_invoke = from->m_invoke;
						to->m_destroy = from->m_destroy;
						to->m_move = from->m_move;
						to->m_isSmall = false;
					}

					/* Storage: either inline buffer or heap pointer. */
					union
					{
						alignas(std::max_align_t) std::byte m_storage[SmallBufferSize];
						void * m_heapPtr{nullptr};
					};

					void (*m_invoke)(Task *){nullptr};
					void (*m_destroy)(Task *) noexcept {nullptr};
					void (*m_move)(Task *, Task *) noexcept {nullptr};
					const void * m_owner{nullptr};
					bool m_isSmall{false};
			};

			/**
			 * @brief Constructs a thread pool with the specified number of worker threads.
			 *
			 * Creates and launches worker threads immediately. Each worker thread runs
			 * until the pool is destroyed. If threadCount is 0, defaults to at least 1 worker.
			 *
			 * @param threadCount Number of worker threads to create. Defaults to the number
			 *					of hardware threads available (std::thread::hardware_concurrency()).
			 * @post threadCount() returns the actual number of workers created.
			 * @note If threadCount is 0, it is automatically adjusted to 1.
			 * @note Worker threads are launched immediately and wait for tasks.
			 */
			explicit ThreadPool (size_t threadCount = std::thread::hardware_concurrency());

			/**
			 * @brief Deleted copy constructor.
			 * @note ThreadPool manages threads and cannot be copied.
			 */
			ThreadPool (const ThreadPool &) = delete;

			/**
			 * @brief Deleted move constructor.
			 * @note ThreadPool manages threads and cannot be moved.
			 */
			ThreadPool (ThreadPool &&) = delete;

			/**
			 * @brief Deleted copy assignment operator.
			 * @note ThreadPool manages threads and cannot be copied.
			 */
			ThreadPool & operator= (const ThreadPool &) = delete;

			/**
			 * @brief Deleted move assignment operator.
			 * @note ThreadPool manages threads and cannot be moved.
			 */
			ThreadPool & operator= (ThreadPool &&) = delete;

			/**
			 * @brief Destructs the thread pool, gracefully shutting down all workers.
			 *
			 * Signals all workers to stop, wakes them up, and waits for them to finish
			 * any in-progress tasks. Remaining queued tasks are drained by each worker
			 * before it exits.
			 *
			 * @note Blocks until all worker threads have terminated.
			 * @note Any tasks still queued when the destructor runs will be executed before shutdown.
			 */
			~ThreadPool () noexcept;

			/**
			 * @brief Returns the number of worker threads in the pool.
			 * @return The worker threads the system agreed to start at construction — 0 when it started none, the pool
			 * then running every task on the calling thread.
			 * @note Thread-safe: Does not change after construction.
			 */
			[[nodiscard]]
			size_t
			threadCount () const noexcept
			{
				return m_workers.size();
			}

			/**
			 * @brief Returns the number of workers currently executing tasks.
			 *
			 * Provides a snapshot of how many worker threads are actively running tasks
			 * at the moment of the call. Useful for monitoring pool utilization.
			 *
			 * @return The number of busy workers (range: 0 to threadCount()).
			 * @note Thread-safe: Uses atomic load with relaxed ordering.
			 * @note The value may be stale by the time it is used due to concurrent operations.
			 */
			[[nodiscard]]
			size_t
			busyWorkers () const noexcept
			{
				return m_busyWorkers.load(std::memory_order_relaxed);
			}

			/**
			 * @brief Returns the approximate number of pending tasks in the queue.
			 *
			 * Counts tasks that have been enqueued but not yet started executing. This
			 * does not include tasks currently being executed by workers.
			 *
			 * @return The approximate number of queued tasks waiting to be executed.
			 * @note Thread-safe: Uses atomic load with relaxed ordering.
			 * @note Due to concurrent operations, the value is approximate
			 *	   and may be stale by the time it is used.
			 */
			[[nodiscard]]
			size_t
			pendingTasks () const noexcept
			{
				return m_pendingTasks.load(std::memory_order_relaxed);
			}

			/**
			 * @brief Checks if all tasks are complete and all workers are idle.
			 *
			 * Returns true when there are no pending tasks in any queue and no workers
			 * are currently executing tasks. Useful for determining when the pool has
			 * finished all submitted work.
			 *
			 * @return True if pendingTasks() == 0 and busyWorkers() == 0, false otherwise.
			 * @note Thread-safe: Uses atomic loads with acquire ordering for memory visibility.
			 * @note The result may become stale immediately if tasks are enqueued concurrently.
			 */
			[[nodiscard]]
			bool
			isIdle () const noexcept
			{
				return m_pendingTasks.load(std::memory_order_acquire) == 0 && m_busyWorkers.load(std::memory_order_acquire) == 0;
			}

			/**
			 * @brief Adds a task to the pool for asynchronous execution (template version).
			 *
			 * This is the preferred method for submitting tasks. The template version avoids
			 * std::function overhead by storing the callable directly using the Task class
			 * with Small Buffer Optimization.
			 *
			 * @tparam function_t Callable type (lambda, function pointer, functor, etc.).
			 * @param callable The callable to execute. Must be invocable with no arguments.
			 * @return True if enqueued successfully, false if the pool is shutting down.
			 * @note Thread-safe: Can be called concurrently from multiple threads.
			 * @note No guarantee on execution order or timing.
			 * @note If enqueue returns false, the callable will not be executed.
			 *
			 * @code
			 * ThreadPool pool;
			 * bool success = pool.enqueue([] {
			 *	 std::cout << "Task executed\n";
			 * });
			 * @endcode
			 */
			template< typename function_t >
			requires std::invocable< function_t >
			bool
			enqueue (function_t && callable)
			{
				return this->enqueueTask(Task{std::forward< function_t >(callable)});
			}

			/**
			 * @brief Adds a task to the pool (std::function version, for compatibility).
			 *
			 * This overload accepts std::function for compatibility with code that requires
			 * type erasure. Prefer the template version when possible for better performance.
			 *
			 * @param task A std::function<void()> to execute.
			 * @return True if enqueued successfully, false if the pool is shutting down.
			 * @note Thread-safe: Can be called concurrently from multiple threads.
			 */
			bool
			enqueue (std::function< void() > task)
			{
				return this->enqueueTask(Task{std::move(task)});
			}

			/**
			 * @brief Submits a task the caller OWNS through the returned handle (Ave Robustus II, decision D1).
			 * @note The task is invoked with a std::stop_token: a long task checks it at a bounded interval and returns
			 * when a stop is requested. The handle's destructor requests the stop and waits for this task only. Use
			 * enqueue() for fire-and-forget work that captures values only; a task touching an object (`this`) is
			 * submitted, and the object keeps the handle as a member.
			 * @tparam function_t A callable invocable with a std::stop_token.
			 * @param callable The task.
			 * @return TaskHandle A valid handle, or an empty one (valid() false) when the pool refuses the task (stopping).
			 */
			template< typename function_t >
			requires std::invocable< function_t, std::stop_token >
			[[nodiscard]]
			TaskHandle
			submit (function_t && callable)
			{
				auto state = std::make_shared< TaskHandle::State >();
				state->pool = this;

				Task task{TaskRunner< std::decay_t< function_t > >{state, std::forward< function_t >(callable)}};
				task.setOwner(state.get());

				if ( !this->enqueueTask(std::move(task)) )
				{
					/* NOTE: The refused runner was destroyed and marked the state finished: nothing to wait for. */
					return {};
				}

				return TaskHandle{std::move(state)};
			}

			/**
			 * @brief Submits a task OWNED by a group (Ave Robustus II): the object that runs several tasks at once keeps
			 * one TaskGroup as a member, and its destruction stops and waits for all of them.
			 * @note The task is invoked with the group's std::stop_token: a long task checks it at a bounded interval and
			 * returns when a stop is requested.
			 * @tparam function_t A callable invocable with a std::stop_token.
			 * @param group The group that owns the task.
			 * @param callable The task.
			 * @return bool False when the group is stopping or the pool refuses the task (stopping): the callable never
			 * runs and is destroyed before this returns.
			 */
			template< typename function_t >
			requires std::invocable< function_t, std::stop_token >
			[[nodiscard]]
			bool
			submit (TaskGroup & group, function_t && callable)
			{
				auto state = group.admit(*this);

				if ( state == nullptr )
				{
					return false;
				}

				const auto * owner = state.get();
				Task task{TaskGroupRunner< std::decay_t< function_t > >{std::move(state), std::forward< function_t >(callable)}};
				task.setOwner(owner);

				/* NOTE: A refused runner is destroyed unrun and releases its count: nothing to wait for. */
				return this->enqueueTask(std::move(task));
			}

			/**
			 * @brief Adds multiple tasks to the pool in a single batch operation.
			 *
			 * More efficient than multiple individual enqueue() calls. This method adds
			 * all tasks under a single lock acquisition and uses notify_all() to wake
			 * all workers, reducing synchronization overhead.
			 *
			 * @tparam Iterator Forward iterator type that dereferences to a callable object.
			 * @param begin Iterator to the first task to enqueue.
			 * @param end Iterator past the last task (standard C++ end iterator).
			 * @return The number of tasks successfully enqueued (0 if pool is shutting down).
			 * @note Thread-safe: Can be called concurrently from multiple threads.
			 * @note All tasks in the range [begin, end) must be valid callables.
			 * @note The callables are MOVED out of the range: elements in [begin, end)
			 *	   are left in a valid but unspecified (moved-from) state after the call.
			 * @note Returns 0 immediately if begin == end or if the pool is stopped.
			 *
			 * @code
			 * ThreadPool pool;
			 * std::vector<std::function<void()>> tasks;
			 * tasks.push_back([] { std::cout << "Task 1\n"; });
			 * tasks.push_back([] { std::cout << "Task 2\n"; });
			 * tasks.push_back([] { std::cout << "Task 3\n"; });
			 * size_t enqueued = pool.enqueueBatch(tasks.begin(), tasks.end());
			 * @endcode
			 */
			template< typename Iterator >
			size_t
			enqueueBatch (Iterator begin, Iterator end)
			{
				if ( begin == end )
				{
					return 0;
				}

				size_t count = 0;

				/* No worker could start (Base::Thread::start() refused them all): the calling thread runs the tasks. */
				if ( m_workers.empty() )
				{
					for ( auto it = begin; it != end; ++it )
					{
						Task task{std::move(*it)};

						task();

						++count;
					}

					return count;
				}

				{
					const std::scoped_lock lock{m_mutex};

					/* NOTE: The stop flag is checked under the mutex (like enqueueTask()) so
					 * a concurrent shutdown cannot slip in between the check and the insertion,
					 * which would leave tasks in the queue that no worker will ever execute. */
					if ( m_stop.load(std::memory_order_acquire) )
					{
						return 0;
					}

					for ( auto it = begin; it != end; ++it )
					{
						m_tasks.emplace_back(std::move(*it));
						++count;
					}

					m_pendingTasks.fetch_add(count, std::memory_order_release);
				}

				m_condition.notify_all();

				return count;
			}

#ifdef __cpp_exceptions
			/**
			 * @brief Adds a task and returns a std::future for retrieving its result.
			 *
			 * Use this method when you need to retrieve the return value of a task or
			 * detect exceptions thrown during task execution. The returned future can be
			 * used to block until the task completes and retrieve its result.
			 *
			 * The future provides synchronization: calling future.get() will block until
			 * the task completes. If the task throws an exception, it will be propagated
			 * when get() is called.
			 *
			 * @tparam function_t Callable type with any return type (void or non-void).
			 * @param callable The callable to execute. Must be invocable with no arguments.
			 * @return std::future<ReturnType> where ReturnType is the callable's return type.
			 * @note Thread-safe: Can be called concurrently from multiple threads.
			 * @note The task is wrapped in exception-safe handling: exceptions are captured
			 *	   and rethrown when future.get() is called.
			 * @note Slightly more overhead than enqueue() due to std::promise/std::future.
			 * @note This method is only available when C++ exceptions are enabled.
			 *
			 * @code
			 * ThreadPool pool;
			 *
			 * // Task returning a value
			 * auto future = pool.enqueueWithResult([] {
			 *	 return 42;
			 * });
			 * int result = future.get(); // Blocks until task completes, returns 42
			 *
			 * // Task returning void
			 * auto voidFuture = pool.enqueueWithResult([] {
			 *	 std::cout << "Task completed\n";
			 * });
			 * voidFuture.get(); // Blocks until task completes
			 *
			 * // Exception handling
			 * auto exFuture = pool.enqueueWithResult([] {
			 *	 throw std::runtime_error("Error");
			 *	 return 0;
			 * });
			 * try {
			 *	 exFuture.get(); // Rethrows the exception
			 * } catch (const std::runtime_error& e) {
			 *	 std::cerr << e.what() << "\n";
			 * }
			 * @endcode
			 */
			template< typename function_t >
			requires std::invocable< function_t >
			[[nodiscard]]
			auto
			enqueueWithResult (function_t && callable) -> std::future< std::invoke_result_t< function_t > >
			{
				using ReturnType = std::invoke_result_t< function_t >;

				auto promise = std::make_shared< std::promise< ReturnType > >();
				auto future = promise->get_future();

				this->enqueue([p = std::move(promise), f = std::forward< function_t >(callable)] () mutable {
					try
					{
						if constexpr ( std::is_void_v< ReturnType > )
						{
							f();
							p->set_value();
						}
						else
						{
							p->set_value(f());
						}
					}
					catch ( ... )
					{
						p->set_exception(std::current_exception());
					}
				});

				return future;
			}
#endif

			/**
			 * @brief Executes a callable in parallel over a range of indices (fork-join pattern).
			 *
			 * This is the most efficient way to parallelize data-parallel workloads such as
			 * array processing, image manipulation, or batch computations. The method divides
			 * the iteration space into chunks, distributes them across worker threads,
			 * and blocks until all iterations complete.
			 *
			 * Key optimizations:
			 * - Single lambda instantiation shared across all workers
			 * - Automatic chunk sizing based on worker count and grain size
			 * - The calling thread participates in execution (no wasted resources)
			 * - Dynamic work distribution via atomic counter (natural load balancing)
			 * - Falls back to sequential execution for small workloads to avoid overhead
			 *
			 * @tparam index_t Unsigned integer type for indices (e.g., size_t, uint32_t, uint64_t).
			 * @tparam function_t Callable accepting a single index_t parameter: body(index).
			 * @param start First index to process (inclusive).
			 * @param end One-past-the-last index to process (exclusive).
			 * @param body Callable invoked for each index: body(index). Must be thread-safe
			 *			 if it accesses shared data.
			 * @param grainSize Minimum number of iterations per task chunk (default 1).
			 *				  Higher values reduce task creation overhead but may cause
			 *				  load imbalance. Automatically increased for very small workloads.
			 *				  A value of 0 is treated as 1.
			 *
			 * @pre start < end (if start >= end, returns immediately without executing body).
			 * @post All iterations in [start, end) have been executed exactly once.
			 * @note Blocks until all iterations complete (synchronous operation with implicit wait).
			 * @note Thread-safe: body must be thread-safe for concurrent execution.
			 * @note Overload selection: this overload is chosen when @a body is invocable with a
			 *	   single index_t and NOT with two. A callable invocable with BOTH one and two
			 *	   index_t arguments (e.g. a variadic generic lambda) matches neither this overload
			 *	   nor the range one, and is rejected at compile time by a guard overload with a
			 *	   clear diagnostic; give such a callable a fixed arity to disambiguate.
			 * @note Sequential fallback: if (end - start) <= grainSize, or the pool has 0 or 1
			 *	   worker threads, the whole range runs on the calling thread with no task
			 *	   enqueuing, to avoid parallelization overhead.
			 * @warning @a body must not throw. It runs on worker threads and on the calling thread;
			 *	   an exception escaping a worker-thread invocation calls std::terminate(), and one
			 *	   escaping the calling-thread invocation abandons the still-running worker tasks
			 *	   while their captured body is destroyed (undefined behaviour). Report failures
			 *	   through captured state, not exceptions.
			 *
			 * @code
			 * ThreadPool pool;
			 *
			 * // Process array elements
			 * std::vector<int> data(10000);
			 * pool.parallelFor(size_t{0}, data.size(), [&data](size_t i) {
			 *	 data[i] = compute(i);
			 * });
			 * // All elements processed when parallelFor returns
			 *
			 * // With custom grain size for better performance
			 * pool.parallelFor(size_t{0}, data.size(), [&data](size_t i) {
			 *	 data[i] = expensiveCompute(i);
			 * }, 100); // Process at least 100 elements per task
			 *
			 * // 2D loop example
			 * const size_t width = 1920, height = 1080;
			 * std::vector<Pixel> image(width * height);
			 * pool.parallelFor(size_t{0}, height, [&](size_t y) {
			 *	 for (size_t x = 0; x < width; ++x) {
			 *		 image[y * width + x] = renderPixel(x, y);
			 *	 }
			 * });
			 * @endcode
			 */
			template< typename index_t, typename function_t >
			requires std::unsigned_integral< index_t > && std::invocable< function_t, index_t > && ( !std::invocable< function_t, index_t, index_t > )
			void
			parallelFor (index_t start, index_t end, function_t && body, size_t grainSize = 1)
			{
				this->parallelForChunks(start, end, [&body] (index_t chunkStart, index_t chunkEnd) {
					for ( index_t index = chunkStart; index < chunkEnd; ++index )
					{
						body(index);
					}
				}, grainSize);
			}

			/**
			 * @brief Executes a callable in parallel over a range of indices, one sub-range per chunk (fork-join pattern).
			 *
			 * Range-based overload of parallelFor(). Instead of invoking the body once per
			 * index, this version invokes it once per chunk with the chunk's half-open
			 * bounds: body(chunkStart, chunkEnd). The body is responsible for iterating
			 * over [chunkStart, chunkEnd) itself. This is preferable when the body can
			 * amortize per-chunk setup (e.g. local accumulators, SIMD over a sub-range)
			 * or wants to avoid the per-index call overhead.
			 *
			 * Shares the same chunking, work-distribution and synchronization strategy as
			 * the per-index overload: the calling thread participates, chunks are handed
			 * out via an atomic counter, and the call blocks until every chunk completes.
			 *
			 * @tparam index_t Unsigned integer type for indices (e.g., size_t, uint32_t, uint64_t).
			 * @tparam function_t Callable accepting two index_t parameters: body(chunkStart, chunkEnd).
			 * @param start First index to process (inclusive).
			 * @param end Last index (exclusive).
			 * @param body Callable invoked for each chunk: body(chunkStart, chunkEnd), where
			 *			 chunkStart is inclusive and chunkEnd exclusive. Must be thread-safe
			 *			 if it accesses shared data.
			 * @param grainSize Minimum number of iterations per task chunk (default 1).
			 *				  Higher values reduce task creation overhead but may cause
			 *				  load imbalance. Automatically increased for very small workloads.
			 *				  A value of 0 is treated as 1.
			 *
			 * @pre start < end (if start >= end, returns immediately without executing body).
			 * @post Every index in [start, end) has been covered by exactly one chunk.
			 * @note Blocks until all chunks complete (synchronous operation with implicit wait).
			 * @note Thread-safe: body must be thread-safe for concurrent execution.
			 * @note Overload selection: this overload is chosen when @a body is invocable with two
			 *	   index_t arguments (chunkStart, chunkEnd) and NOT with a single one. A callable
			 *	   invocable with BOTH one and two index_t arguments (e.g. a variadic generic lambda)
			 *	   matches neither overload and is rejected at compile time by a guard overload with
			 *	   a clear diagnostic; give such a callable a fixed arity to disambiguate.
			 * @note Sequential fallback: if (end - start) <= grainSize, or the pool has 0 or 1
			 *	   worker threads, the whole range runs on the calling thread as a single
			 *	   body(start, end) call, to avoid parallelization overhead.
			 * @warning @a body must not throw. It runs on worker threads and on the calling thread;
			 *	   an exception escaping a worker-thread invocation calls std::terminate(), and one
			 *	   escaping the calling-thread invocation abandons the still-running worker tasks
			 *	   while their captured body is destroyed (undefined behaviour). Report failures
			 *	   through captured state, not exceptions.
			 *
			 * @code
			 * ThreadPool pool;
			 *
			 * // Sum elements with per-chunk local accumulators.
			 * std::vector< double > data(100000);
			 * std::atomic< double > total{0.0};
			 * pool.parallelFor(size_t{0}, data.size(), [&] (size_t begin, size_t end) {
			 *	 double local = 0.0;
			 *	 for ( size_t i = begin; i < end; ++i ) { local += data[i]; }
			 *	 total.fetch_add(local);
			 * });
			 * @endcode
			 */
			template< typename index_t, typename function_t >
			requires std::unsigned_integral< index_t > && std::invocable< function_t, index_t, index_t > && ( !std::invocable< function_t, index_t > )
			void
			parallelFor (index_t start, index_t end, function_t && body, size_t grainSize = 1)
			{
				this->parallelForChunks(start, end, std::forward< function_t >(body), grainSize);
			}

			/**
			 * @brief Guard overload: rejects a callable that is ambiguous between the two parallelFor() overloads.
			 *
			 * A callable invocable with BOTH a single index_t and two index_t arguments (e.g. a
			 * variadic generic lambda such as [](auto...){}) cannot be classified as per-index or
			 * range: choosing either would silently pick an iteration model the author may not have
			 * intended. This overload captures exactly that case and fails compilation with a clear
			 * message rather than letting overload resolution emit a cryptic diagnostic (or worse,
			 * silently select one model). It is never callable: give the body a fixed arity.
			 *
			 * @tparam index_t Unsigned integer index type.
			 * @tparam function_t Callable invocable with both one and two index_t arguments.
			 */
			template< typename index_t, typename function_t >
			requires std::unsigned_integral< index_t > && std::invocable< function_t, index_t > && std::invocable< function_t, index_t, index_t >
			void
			parallelFor (index_t /*start*/, index_t /*end*/, function_t && /*body*/, size_t /*grainSize*/ = 1)
			{
				static_assert(dependentFalse< function_t >,
					"ThreadPool::parallelFor: the body is invocable with BOTH one and two index_t "
					"arguments, which is ambiguous between the per-index overload (body(index)) and "
					"the range overload (body(chunkStart, chunkEnd)). Give the body a fixed arity "
					"(name its parameters instead of using an auto/variadic pack) to select one.");
			}

			/**
			 * @brief Blocks until all queued and in-progress tasks are finished.
			 *
			 * Waits until pendingTasks() == 0 and busyWorkers() == 0. This ensures
			 * all enqueued work has completed before returning.
			 *
			 * @post isIdle() returns true when wait() returns.
			 * @note Thread-safe: Can be called concurrently from multiple threads.
			 * @note Does not prevent new tasks from being enqueued concurrently.
			 *	   If other threads continue enqueuing, wait() may return while
			 *	   new tasks are being added.
			 * @note Calling wait() from a task executed by the pool will deadlock
			 *	   if the pool is saturated (all workers busy). Avoid this pattern.
			 *
			 * @code
			 * ThreadPool pool;
			 *
			 * // Enqueue some tasks
			 * for (int i = 0; i < 100; ++i) {
			 *	 pool.enqueue([i] {
			 *		 std::cout << "Task " << i << "\n";
			 *	 });
			 * }
			 *
			 * // Wait for all tasks to complete
			 * pool.wait();
			 * std::cout << "All tasks finished\n";
			 * @endcode
			 */
			void wait ();

		private:

			/**
			 * @brief Always-false value template, dependent on its type parameter.
			 *
			 * Used to defer a static_assert to template instantiation: a non-dependent
			 * static_assert(false, ...) inside an uninstantiated template is ill-formed in C++20,
			 * so the guard parallelFor() overload asserts on this instead.
			 */
			template< typename >
			static constexpr bool dependentFalse = false;

			/**
			 * @brief Internal: shared fork-join driver for both parallelFor() overloads.
			 *
			 * Handles range checking, the sequential fallback, chunk sizing, work
			 * distribution (atomic counter), calling-thread participation and the
			 * completion wait. @c chunkBody is invoked once per chunk with the
			 * half-open bounds [chunkStart, chunkEnd). The public per-index overload
			 * wraps it with a per-index loop; the public range overload forwards its
			 * body directly.
			 *
			 * The calling thread sleeps on the completion counter (std::atomic::wait,
			 * futex-style) instead of spinning, and helper tasks capture a single
			 * shared_ptr plus a reference so they fit Task's small buffer (enforced
			 * by a static_assert): a parallelFor call performs exactly one heap
			 * allocation (the shared state), regardless of the worker count.
			 *
			 * @tparam index_t Unsigned integer index type.
			 * @tparam chunk_function_t Callable accepting (index_t chunkStart, index_t chunkEnd).
			 * @param start First index to process (inclusive).
			 * @param end Last index (exclusive).
			 * @param chunkBody Per-chunk callable. Must be thread-safe for concurrent execution.
			 * @param grainSize Minimum number of iterations per task chunk. A value of 0 is treated as 1.
			 * @note Completion is counted per CHUNK, not per helper task, so calling this
			 *		 from a task already running on this pool is safe even when the pool is
			 *		 saturated: the nested call completes on its calling thread, and helpers
			 *		 that only get scheduled afterwards exit as no-ops (they claim an index
			 *		 past the last chunk and never touch the body).
			 * @note @c chunkBody must not throw. On a worker thread an escaping exception calls
			 *		 std::terminate(); on the calling thread it abandons the still-running worker
			 *		 tasks while their captured chunkBody is destroyed (undefined behaviour).
			 */
			template< typename index_t, typename chunk_function_t >
			requires std::unsigned_integral< index_t > && std::invocable< chunk_function_t, index_t, index_t >
			void
			parallelForChunks (index_t start, index_t end, chunk_function_t && chunkBody, size_t grainSize)
			{
				if ( start >= end )
				{
					return;
				}

				/* NOTE: A grain size of zero would produce a zero effective grain below
				 * (division by zero); treat it as the smallest meaningful value. */
				if ( grainSize == 0 )
				{
					grainSize = 1;
				}

				const auto totalIterations = static_cast< size_t >(end - start);

				/* For very small workloads, just run the whole range sequentially. */
				if ( totalIterations <= grainSize || m_workers.size() <= 1 )
				{
					chunkBody(start, end);

					return;
				}

				/* Calculate number of chunks based on worker count. */
				const size_t numWorkers = m_workers.size();
				const size_t effectiveGrain = std::max(grainSize, totalIterations / (numWorkers * 4));
				const size_t numChunks = (totalIterations + effectiveGrain - 1) / effectiveGrain;

				/* The calling thread takes one share of the work, so at most numChunks - 1
				 * helpers are useful: never enqueue tasks that would find no chunk to run.
				 * NOTE: numChunks >= 2 here (the sequential fallback handled the rest). */
				const size_t helperCount = std::min(numWorkers, numChunks - 1);

				/* Shared state, kept alive by the shared_ptr captured in each helper task.
				 * The chunk parameters live here (instead of in the lambda capture) so the
				 * helper capture stays within Task's small buffer (see static_assert below). */
				struct SharedState
				{
					std::atomic< size_t > nextChunk{0};
					std::atomic< size_t > completedChunks{0};
					index_t start{0};
					index_t end{0};
					size_t effectiveGrain{0};
					size_t numChunks{0};
				};

				auto state = std::make_shared< SharedState >();
				state->start = start;
				state->end = end;
				state->effectiveGrain = effectiveGrain;
				state->numChunks = numChunks;

				/* Create the worker task. The reference to chunkBody is only dereferenced
				 * for a claimed chunk; the caller cannot return while any chunk is in
				 * flight, so the reference is never used after it dies. */
				auto workerTask = [state, &chunkBody] ()
				{
					while ( true )
					{
						const size_t chunkIndex = state->nextChunk.fetch_add(1, std::memory_order_relaxed);

						if ( chunkIndex >= state->numChunks )
						{
							break;
						}

						const auto chunkStart = static_cast< index_t >(static_cast< size_t >(state->start) + (chunkIndex * state->effectiveGrain));
						const auto chunkEnd = static_cast< index_t >(std::min(static_cast< size_t >(chunkStart) + state->effectiveGrain, static_cast< size_t >(state->end)));

						chunkBody(chunkStart, chunkEnd);

						/* Count completion per CHUNK; the finisher of the last chunk wakes the
						 * calling thread. NOTE: The captured shared_ptr keeps the state alive
						 * during notify_one(), so the notification can never touch a destroyed
						 * atomic. */
						if ( state->completedChunks.fetch_add(1, std::memory_order_release) + 1 == state->numChunks )
						{
							state->completedChunks.notify_one();
						}
					}
				};

				/* The whole point of the SharedState indirection: helper tasks must not
				 * fall back to heap allocation on the pool's hottest code path. */
				static_assert(sizeof(workerTask) <= SmallBufferSize, "parallelFor worker task must fit the Task small buffer");

				/* Enqueue one task per helper. A rejected enqueue (pool shutting down)
				 * needs no accounting: completion is counted per chunk, and every chunk
				 * is guaranteed to run -- at worst all on the calling thread below. */
				for ( size_t index = 0; index < helperCount; ++index )
				{
					this->enqueue(workerTask);
				}

				/* The calling thread also participates in the work. */
				workerTask();

				/* Sleep until every chunk completed (futex-style, no CPU burn).
				 * Chunk-based completion makes nested calls on a saturated pool safe:
				 * helpers scheduled after the last chunk exit as no-ops, never touching
				 * chunkBody (only the shared state, which their shared_ptr keeps alive).
				 * No chunkBody invocation can still be in flight once the count is
				 * reached, so the body reference cannot be used after this returns. */
				size_t completed = state->completedChunks.load(std::memory_order_acquire);

				while ( completed < numChunks )
				{
					state->completedChunks.wait(completed, std::memory_order_acquire);

					completed = state->completedChunks.load(std::memory_order_acquire);
				}
			}

			friend class TaskHandle;
			friend class TaskGroup;

			/**
			 * @brief Removes every queued task tagged with this owner (Task::setOwner()) and destroys them on the calling
			 * thread, outside the pool's lock (their "never ran" guards run here). A task already dequeued is left to its
			 * worker. Called by TaskHandle / TaskGroup when a stop is requested.
			 * @param owner The owner's state address.
			 */
			void purgeQueued (const void * owner) noexcept;

			/**
			 * @brief Internal: Enqueues a Task object.
			 * @param task The task to enqueue.
			 * @return True if enqueued, false if pool is stopped.
			 */
			bool enqueueTask (Task && task);

			/**
			 * @brief Worker thread main loop.
			 */
			void worker ();

			std::vector< Thread > m_workers;
			std::deque< Task > m_tasks;
			std::mutex m_mutex;
			std::condition_variable m_condition;
			std::condition_variable m_completionCondition;
			std::atomic< size_t > m_pendingTasks{0};
			std::atomic< size_t > m_busyWorkers{0};
			std::atomic< bool > m_stop{false};
	};
}
