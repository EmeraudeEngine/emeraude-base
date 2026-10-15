/*
 * src/ThreadPool.cpp
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

#include "ThreadPool.hpp"

/* Project configuration. */
#include "emeraude_base_config.hpp"

/* STL inclusions. */
#include <cstdlib>
#include <iostream>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "Time/Elapsed/PrintScopeRealTime.hpp"

namespace EmEn::Base
{
	void
	TaskHandle::requestStop () noexcept
	{
		if ( m_state != nullptr )
		{
			m_state->stopSource.request_stop();
		}
	}

	bool
	TaskHandle::finished () const noexcept
	{
		if ( m_state == nullptr )
		{
			return true;
		}

		const std::scoped_lock lock{m_state->mutex};

		return m_state->finished || ( !m_state->started && m_state->stopSource.stop_requested() );
	}

	void
	TaskHandle::wait () noexcept
	{
		if ( m_state == nullptr )
		{
			return;
		}

		std::unique_lock< std::mutex > lock{m_state->mutex};

		if ( !m_state->finished && m_state->runningThread == std::this_thread::get_id() )
		{
			/* NOTE: The task waits for its own end (or destroys its own handle): it would never come, and its owner is going
			 * away under it. A CONTRACT FAULT, aborted in every build (owner decision D2, 2026-10-08). */
			Logging::fatal("TaskHandle", "wait(), contract violation: a task waits for (or destroys) its own handle ! Aborting.");

			std::abort();
		}

		/* NOTE: A task stopped before it started will never run: no wait for the queue (owner decision 2026-10-15). The
		 * runner decides under this lock, after the stop request: it sees the stop too. */
		m_state->finishedSignal.wait(lock, [this] () {
			return m_state->finished || ( !m_state->started && m_state->stopSource.stop_requested() );
		});
	}

	void
	TaskHandle::reset () noexcept
	{
		if ( m_state == nullptr )
		{
			return;
		}

		this->requestStop();
		this->wait();

		m_state.reset();
	}

	namespace
	{
		/**
		 * @brief Returns the innermost TaskGroupRunScope of the calling thread (nullptr: no group task runs on it).
		 * @return const TaskGroupRunScope *&
		 */
		const TaskGroupRunScope * &
		innermostGroupScope () noexcept
		{
			/* NOTE: Per thread by nature (which group task runs HERE): no other thread ever reads it. */
			thread_local const TaskGroupRunScope * scope{nullptr};

			return scope;
		}

		/**
		 * @brief Returns whether the calling thread is a worker of a ThreadPool (TaskGroup::waitForAll() refuses it).
		 * @return bool &
		 */
		bool &
		isPoolWorker () noexcept
		{
			thread_local bool worker{false};

			return worker;
		}
	}

	TaskGroupRunScope::TaskGroupRunScope (const TaskGroup::State * state) noexcept
		: m_state{state},
		m_previous{innermostGroupScope()}
	{
		innermostGroupScope() = this;
	}

	TaskGroupRunScope::~TaskGroupRunScope ()
	{
		innermostGroupScope() = m_previous;
	}

	bool
	TaskGroupRunScope::runsOnThisThread (const TaskGroup::State * state) noexcept
	{
		/* NOTE: Bounded by the nesting depth of inline task runs on this thread (one scope per frame). */
		for ( const auto * scope = innermostGroupScope(); scope != nullptr; scope = scope->m_previous )
		{
			if ( scope->m_state == state )
			{
				return true;
			}
		}

		return false;
	}

	bool
	TaskGroup::State::admit () noexcept
	{
		const MutexLock lock{mutex};

		if ( stopping )
		{
			return false;
		}

		++pending;

		return true;
	}

	bool
	TaskGroup::State::begin () noexcept
	{
		const MutexLock lock{mutex};

		if ( stopping )
		{
			return false;
		}

		++running;

		return true;
	}

	void
	TaskGroup::State::release (bool ran) noexcept
	{
		bool wake = false;

		{
			const MutexLock lock{mutex};

			assert(pending > 0);
			assert(!ran || running > 0);

			--pending;

			if ( ran )
			{
				--running;
			}

			wake = this->idle();
		}

		/* NOTE: After the guard (CP.31): the runner still holds the state, so the waiter cannot destroy it under us. */
		if ( wake )
		{
			idleSignal.notify_all();
		}
	}

	TaskGroup::TaskGroup () noexcept
		: m_state{std::make_shared< State >()}
	{

	}

	TaskGroup::~TaskGroup ()
	{
		this->requestStop();
		this->wait();
	}

	void
	TaskGroup::requestStop () noexcept
	{
		{
			const MutexLock lock{m_state->mutex};

			m_state->stopping = true;
		}

		/* NOTE: A plain wait() in progress re-evaluates: after a stop it waits for the running tasks only. */
		m_state->idleSignal.notify_all();

		/* NOTE: Outside the guard: request_stop() runs the tasks' std::stop_callback synchronously (a
		 * condition_variable_any wait registers one), and calling out under a lock is forbidden. A task admitted before
		 * the flag sees the stop through its token; none is admitted after it. */
		m_state->stopSource.request_stop();
	}

	bool
	TaskGroup::stopRequested () const noexcept
	{
		return m_state->stopSource.stop_requested();
	}

	size_t
	TaskGroup::pendingCount () const noexcept
	{
		const MutexLock lock{m_state->mutex};

		return m_state->pending;
	}

	void
	TaskGroup::wait () noexcept
	{
		if ( TaskGroupRunScope::runsOnThisThread(m_state.get()) )
		{
			/* NOTE: A task of the group waits for its own group (or destroys its owner): it would never return, and the
			 * owner is going away under it. A CONTRACT FAULT, aborted in every build (owner decision D2, 2026-10-08). */
			Logging::fatal("TaskGroup", "wait(), contract violation: a task waits for (or destroys) its own group ! Aborting.");

			std::abort();
		}

		auto & state = *m_state;
		MutexLock lock{state.mutex};

		state.idleSignal.wait(lock, [&state] () EMEN_REQUIRES(state.mutex) {
			return state.idle();
		});
	}

	void
	TaskGroup::waitForAll () noexcept
	{
		if ( isPoolWorker() || TaskGroupRunScope::runsOnThisThread(m_state.get()) )
		{
			/* NOTE: A worker waiting for the queue it serves can deadlock a saturated pool; a task of the group waits for
			 * itself. Both are CONTRACT FAULTS, aborted in every build (D2, owner decision 2026-10-15). */
			Logging::fatal("TaskGroup", "waitForAll(), contract violation: called from a thread-pool worker (or a task of its own group) ! Aborting.");

			std::abort();
		}

		auto & state = *m_state;
		MutexLock lock{state.mutex};

		state.idleSignal.wait(lock, [&state] () EMEN_REQUIRES(state.mutex) {
			return state.pending == 0;
		});
	}

	std::shared_ptr< TaskGroup::State >
	TaskGroup::admit () noexcept
	{
		if ( !m_state->admit() )
		{
			return nullptr;
		}

		return m_state;
	}

	ThreadPool::ThreadPool (size_t threadCount)
	{
		/* Ensure at least one worker. */
		if ( threadCount == 0 )
		{
			threadCount = 1;
		}

		m_workers.reserve(threadCount);

		/* A worker the system refuses to start is left out (Base::Thread::start() traces it): the pool runs with the
		 * workers it got, and with none it runs every task on the calling thread (enqueueTask(), enqueueBatch()). */
		for ( size_t index = 0; index < threadCount; ++index )
		{
			Thread worker;

			if ( worker.start([this] { this->worker(); }) )
			{
				m_workers.emplace_back(std::move(worker));
			}
		}

		if ( m_workers.size() < threadCount )
		{
			std::cerr << "[ThreadPool] Only " << m_workers.size() << " of " << threadCount << " worker threads could start" << (m_workers.empty() ? ": the tasks run on the calling thread." : ".") << "\n";
		}

		if constexpr ( ThreadPoolDebugEnabled )
		{
			std::cout << "[ThreadPool-debug] " << m_workers.size() << " threads spawned in the pool." "\n";
		}
	}

	ThreadPool::~ThreadPool () noexcept
	{
		if constexpr ( ThreadPoolDebugEnabled )
		{
			std::cout << "[ThreadPool-debug] Cleaning the thread pool..." "\n";
		}

		/* Signal all workers to stop. */
		{
			const std::scoped_lock lock{m_mutex};

			m_stop.store(true, std::memory_order_release);
		}

		/* Wake all workers so they can see the stop flag. */
		m_condition.notify_all();

		if constexpr ( ThreadPoolDebugEnabled )
		{
			std::cout << "[ThreadPool-debug] Stopped and waiting for all threads to quit..." "\n";
		}

		for ( auto & worker : m_workers )
		{
			worker.join();
		}

		if constexpr ( ThreadPoolDebugEnabled )
		{
			std::cout << "[ThreadPool-debug] Thread pool terminated." "\n";
		}
	}

	bool
	ThreadPool::enqueueTask (Task && task)
	{
		/* No worker could start (Base::Thread::start() refused them all): the calling thread runs the task. */
		if ( m_workers.empty() )
		{
			if ( m_stop.load(std::memory_order_acquire) )
			{
				return false;
			}

			task();

			return true;
		}

		{
			const std::scoped_lock lock{m_mutex};

			if ( m_stop.load(std::memory_order_acquire) )
			{
				if constexpr ( ThreadPoolDebugEnabled )
				{
					std::cerr << "[ThreadPool-debug] Enqueue on a stopped thread pool!" "\n";
				}

				return false;
			}

			m_tasks.emplace_back(std::move(task));
			m_pendingTasks.fetch_add(1, std::memory_order_release);
		}

		if constexpr ( ThreadPoolDebugEnabled )
		{
			std::cout << "[ThreadPool-debug] New task added to queue." "\n";
		}

		m_condition.notify_one();

		return true;
	}

	void
	ThreadPool::wait ()
	{
		if constexpr ( ThreadPoolDebugEnabled )
		{
			std::cout <<
				"[ThreadPool-debug] Waiting for " << m_busyWorkers.load(std::memory_order_relaxed) << " workers to finish "
				"(" << m_pendingTasks.load(std::memory_order_relaxed) << " tasks left)..." "\n";
		}

		std::unique_lock< std::mutex > lock{m_mutex};

		m_completionCondition.wait(lock, [this] {
			return m_pendingTasks.load(std::memory_order_acquire) == 0 && m_busyWorkers.load(std::memory_order_acquire) == 0;
		});

		if constexpr ( ThreadPoolDebugEnabled )
		{
			std::cout << "[ThreadPool-debug] All tasks completed." "\n";
		}
	}

	void
	ThreadPool::worker ()
	{
		isPoolWorker() = true;

		while ( true )
		{
			Task task;

			{
				std::unique_lock< std::mutex > lock{m_mutex};

				/* Wait until there's work or stop signal. */
				m_condition.wait(lock, [this] {
					return m_stop.load(std::memory_order_acquire) || !m_tasks.empty();
				});

				/* Check stop condition - but finish remaining tasks first. */
				if ( m_stop.load(std::memory_order_acquire) && m_tasks.empty() )
				{
					return;
				}

				/* Get a task from the queue. */
				if ( !m_tasks.empty() )
				{
					task = std::move(m_tasks.front());

					m_tasks.pop_front();

					/* NOTE: Increment the busy count BEFORE decrementing the pending count.
					 * isIdle() reads both atomics without holding the mutex: the reverse
					 * order would expose a transient "pending == 0 && busy == 0" state
					 * while a task is about to execute. This order only exposes a
					 * conservative transient double-count. */
					m_busyWorkers.fetch_add(1, std::memory_order_release);
					m_pendingTasks.fetch_sub(1, std::memory_order_release);
				}
				else
				{
					/* Spurious wakeup or stop without tasks. */
					continue;
				}
			}

			/* Execute task outside any locks. */
			if constexpr ( ThreadPoolDebugEnabled )
			{
				std::cout <<
					"[ThreadPool-debug] Worker running task... "
					"(" << m_busyWorkers.load(std::memory_order_relaxed) << " busy, " << m_pendingTasks.load(std::memory_order_relaxed) << " pending)" "\n";

				{
					const Time::Elapsed::PrintScopeRealTime stat{"[ThreadPool-debug] Task finished"};

					task();
				}

				std::cout << "[ThreadPool-debug] Worker finished task." "\n";
			}
			else
			{
				task();
			}

			/* Destroy the task (and its captured resources) BEFORE signaling
			 * completion: once wait() reports the pool idle, no capture may still
			 * be alive on a worker. */
			task = {};

			/* Decrement busy count and signal completion.
			 * NOTE: The notification is issued after releasing the mutex, so woken
			 * waiters do not immediately block on a still-held lock. */
			{
				const std::scoped_lock lock{m_mutex};

				m_busyWorkers.fetch_sub(1, std::memory_order_release);
			}

			m_completionCondition.notify_all();
		}
	}
}
