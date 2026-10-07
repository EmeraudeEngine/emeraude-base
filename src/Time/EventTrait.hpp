/*
 * src/Time/EventTrait.hpp
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
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <ranges>
#include <ratio>
#include <tuple>
#include <utility>
#include <vector>

/* Local inclusions for usages. */
#include "TimedEvent.hpp"
#include "Types.hpp"

namespace EmEn::Base::Time
{
	/**
	 * @brief Adds the ability to fire timed events to a class.
	 * @tparam rep_t The time thick precision. Default unsigned int.
	 * @tparam period_t The unity of time. Default microseconds.
	 */
	template< typename rep_t = uint32_t, typename period_t = std::milli >
	requires (std::is_arithmetic_v< rep_t >)
	class EventTrait
	{
		public:

			/**
			 * @brief Copy constructor.
			 * @param copy A reference to the copied instance.
			 */
			EventTrait (const EventTrait & copy) noexcept = default;

			/**
			 * @brief Move constructor.
			 * @param copy A reference to the copied instance.
			 */
			EventTrait (EventTrait && copy) noexcept = default;

			/**
			 * @brief Copy assignment.
			 * @param copy A reference to the copied instance.
			 * @return EventTrait &
			 */
			EventTrait & operator= (const EventTrait & copy) noexcept = default;

			/**
			 * @brief Move assignment.
			 * @param copy A reference to the copied instance.
			 * @return EventTrait &
			 */
			EventTrait & operator= (EventTrait && copy) noexcept = default;

			/**
			 * @brief Destructs the timed events interface.
			 * @warning It joins every timer thread: never destroy the owner from one of its own timers' callbacks.
			 */
			virtual ~EventTrait () = default;

			/**
			 * @brief Starts a timer. This will register a cyclic (or a unique) call to timeEvent() with an ID given by this function.
			 * @note If the function return true, the event will be stopped.
			 * @param callable The function triggered after the timeout.
			 * @param granularity This is the interval, or the timeout respecting period_t (Time unit).
			 * @param once If set to true, your timer will fire only once and will be killed automatically.
			 * @param autostart If set to true, the timer will execute automatically.
			 * @return TimerID
			 */
			TimerID
			createTimer (const std::function< bool (TimerID) > & callable, rep_t granularity, bool once = false, bool autostart = false) noexcept
			{
				/* Declared before the lock, destroyed after it: the retired events' destructors join their threads. */
				std::vector< EventNode > reaped;

				const std::lock_guard< std::mutex > lock{m_eventsAccess};

				this->takeRetiredEvents(reaped);

				const auto timerID = m_lastTimerID.fetch_add(1, std::memory_order_relaxed);

				const auto result = m_events.emplace(
					std::piecewise_construct,
					std::forward_as_tuple(timerID),
					std::forward_as_tuple(callable, granularity, once)
				);

				if ( !result.second )
				{
					std::cerr << "EventTrait::createTimer(), unable to create a timer !" "\n";

					return 0;
				}

				auto & timer = result.first->second;
				timer.setTimerID(timerID);

				if ( autostart )
				{
					timer.start();
				}

				return timerID;
			}

			/**
			 * @brief Executes a single event using EventTrait::createTimer().
			 * @param callable The function triggered after the timeout.
			 * @param timeout The delay before firing the event respecting the period_t (Time unit).
			 * @return TimerID
			 */
			TimerID
			fire (const std::function< bool (TimerID) > & callable, rep_t timeout) noexcept
			{
				return this->createTimer(callable, timeout, true, true);
			}

			/**
			 * @brief Sets a new granularity to the timer.
			 * @param timerID The ID of your timer.
			 * @param granularity The interval duration respecting period_t (Time unit).
			 * @return bool
			 */
			bool
			setTimerGranularity (TimerID timerID, rep_t granularity) noexcept
			{
				return this->withTimer(timerID, [granularity] (auto * timer) {
					timer->setGranularity(granularity);

					return true;
				});
			}

			/**
			 * @brief Starts an existing timer.
			 * @param timerID The ID of your timer.
			 * @return bool
			 */
			bool
			startTimer (TimerID timerID) noexcept
			{
				return this->withTimer(timerID, [] (auto * timer) {
					timer->start();

					return true;
				});
			}

			/**
			 * @brief Pauses a timer without destroy it.
			 * @param timerID The ID of your timer.
			 * @return bool
			 */
			bool
			stopTimer (TimerID timerID) noexcept
			{
				return this->withTimer(timerID, [] (auto * timer) {
					timer->stop();

					return true;
				});
			}

			/**
			 * @brief Returns whether the timer is started.
			 * @param timerID The ID of your timer.
			 * @return bool
			 */
			bool
			isTimerStarted (TimerID timerID) noexcept
			{
				return this->withTimer(timerID, [] (auto * timer) {
					return timer->isStarted();
				});
			}

			/**
			 * @brief Pauses a timer without destroy it.
			 * @param timerID The ID of your timer.
			 * @return bool
			 */
			bool
			pauseTimer (TimerID timerID) noexcept
			{
				return this->withTimer(timerID, [] (auto * timer) {
					timer->pause();

					return true;
				});
			}

			/**
			 * @brief Starts an existing timer.
			 * @param timerID The ID of your timer.
			 * @return bool
			 */
			bool
			resumeTimer (TimerID timerID) noexcept
			{
				return this->withTimer(timerID, [] (auto * timer) {
					timer->resume();

					return true;
				});
			}

			/**
			 * @brief Restarts all previously active timers.
			 * @return void
			 */
			void
			startTimers () noexcept
			{
				const std::lock_guard< std::mutex > lock{m_eventsAccess};

				for ( auto & event : std::ranges::views::values(m_events) )
				{
					event.start();
				}
			}

			/**
			 * @brief Pauses every active timers.
			 * @return void
			 */
			void
			stopTimers () noexcept
			{
				const std::lock_guard< std::mutex > lock{m_eventsAccess};

				for ( auto & event : std::ranges::views::values(m_events) )
				{
					event.stop();
				}
			}

			/**
			 * @brief Pauses every active timers.
			 * @return void
			 */
			void
			pauseTimers () noexcept
			{
				const std::lock_guard< std::mutex > lock{m_eventsAccess};

				for ( auto & event : std::ranges::views::values(m_events) )
				{
					event.pause();
				}
			}

			/**
			 * @brief Restarts all previously active timers.
			 * @return void
			 */
			void
			resumeTimers () noexcept
			{
				const std::lock_guard< std::mutex > lock{m_eventsAccess};

				for ( auto & event : std::ranges::views::values(m_events) )
				{
					event.resume();
				}
			}

			/**
			 * @brief Returns whether the timer is paused.
			 * @param timerID The ID of your timer.
			 * @return bool
			 */
			bool
			isTimerPaused (TimerID timerID) noexcept
			{
				return this->withTimer(timerID, [] (auto * timer) {
					return timer->isPaused();
				});
			}

			/**
			 * @brief This function is used to kill a cyclic timer, or a timeout before it fire.
			 * @note The timer is destroyed OUTSIDE the trait's lock: its destructor joins the timer thread, and a callback
			 * running meanwhile may call the trait (it found the lock held and deadlocked, until 2026-10-07).
			 * @note Called from the timer's OWN callback, the destruction is deferred: the timer is taken out at once (it
			 * never fires again), its thread ends when the callback returns, and the trait joins it later from another
			 * thread (the next create / destroy, or its destructor). A self-join used to abort.
			 * @param timerID The ID of your timer.
			 * @return void
			 */
			void
			destroyTimer (TimerID timerID) noexcept
			{
				/* Declared before the lock, destroyed after it: their destructors join the timer threads. */
				EventNode doomed;
				std::vector< EventNode > reaped;

				{
					const std::scoped_lock lock{m_eventsAccess};

					this->takeRetiredEvents(reaped);

					doomed = m_events.extract(timerID);

					if ( !doomed.empty() && doomed.mapped().isRunningOnThisThread() )
					{
						this->retire(std::move(doomed));
					}
				}
			}

			/**
			 * @brief This function kill every timer.
			 * @note Same contract as destroyTimer(): the timers are joined outside the lock, and the caller's own timer
			 * (a callback calling it) is retired instead of self-joined.
			 * @return void
			 */
			void
			destroyTimers () noexcept
			{
				/* Declared before the lock, destroyed after it: their destructors join the timer threads. */
				std::map< TimerID, TimedEvent< rep_t, period_t > > doomed;
				std::vector< EventNode > reaped;

				{
					const std::scoped_lock lock{m_eventsAccess};

					this->takeRetiredEvents(reaped);

					for ( auto eventIt = m_events.begin(); eventIt != m_events.end(); )
					{
						const auto current = eventIt;

						++eventIt;

						if ( current->second.isRunningOnThisThread() )
						{
							this->retire(m_events.extract(current));
						}
					}

					doomed.swap(m_events);
				}
			}

		protected:

			/**
			 * @brief Constructs a timed events interface.
			 */
			EventTrait () noexcept = default;

			/**
			 * @brief Executes a function with a specific timer.
			 * @tparam function_t The type of function.
			 * @param timerID The timer ID.
			 * @param function A function.
			 * @return std::invoke_result_t< function_t, TimedEvent< rep_t, period_t > * >
			 */
			template< typename function_t >
			std::invoke_result_t< function_t, TimedEvent< rep_t, period_t > * >
			withTimer (TimerID timerID, function_t function) noexcept
			{
				const std::lock_guard< std::mutex > lock{m_eventsAccess};

				const auto timerIt = m_events.find(timerID);

				if ( timerIt == m_events.end() )
				{
					using ReturnType = std::invoke_result_t< function_t, TimedEvent< rep_t, period_t > * >;

					if constexpr ( !std::is_void_v< ReturnType > )
					{
						/* NOTE: Return a default value. */
						return ReturnType{};
					}
					else
					{
						return;
					}
				}

				return function(&timerIt->second);
			}

			/**
			 * @brief Resets a timer.
			 * @param timerID The timer ID.
			 * @return void
			 */
			void
			resetTimer (TimerID timerID) noexcept
			{
				this->withTimer(timerID, [] (auto * timer) {
					timer->reset();
				});
			}

		private:

			using EventNode = typename std::map< TimerID, TimedEvent< rep_t, period_t > >::node_type;

			/**
			 * @brief Retires an event destroyed from its own callback: its thread is asked to end, and it is kept until
			 * another thread joins it. Call it under m_eventsAccess.
			 * @param event The event's node, not empty.
			 * @return void
			 */
			void
			retire (EventNode event) noexcept
			{
				event.mapped().requestExit();

				m_retiredEvents.push_back(std::move(event));
			}

			/**
			 * @brief Moves out the retired events the calling thread may join (not its own). Call it under m_eventsAccess,
			 * and let the output die after releasing it.
			 * @param reaped The output.
			 * @return void
			 */
			void
			takeRetiredEvents (std::vector< EventNode > & reaped) noexcept
			{
				for ( auto & event : m_retiredEvents )
				{
					if ( !event.mapped().isRunningOnThisThread() )
					{
						reaped.push_back(std::move(event));
					}
				}

				/* A moved-from node handle is empty. */
				std::erase_if(m_retiredEvents, [] (const EventNode & event) {
					return event.empty();
				});
			}

			std::atomic< TimerID > m_lastTimerID{1};
			std::map< TimerID, TimedEvent< rep_t, period_t > > m_events;
			/** @brief The events destroyed from their own callback, waiting for another thread to join them. */
			std::vector< EventNode > m_retiredEvents;
			mutable std::mutex m_eventsAccess;
	};
}
