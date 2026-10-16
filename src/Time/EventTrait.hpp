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
#include "ScopedRegistration.hpp"
#include "TimedEvent.hpp"
#include "Types.hpp"

namespace EmEn::Base::Time
{
	/**
	 * @brief A timer owned by its holder (Ave Robustus II P4, owner decision 2026-10-16): its end destroys the timer —
	 * stops it and joins its thread (EventTrait::destroyTimer()).
	 *
	 * EventTrait is a base class: without a handle, its timers were joined only in its own destructor, AFTER the members
	 * of the derived class, which a callback capturing `this` still used. The holder declares the handle after the state
	 * its callback uses, so the timer is stopped before that state goes.
	 * @pre The EventTrait that made the timer outlives the handle.
	 * @note Move-only; a moved-from or reset handle is empty (valid() false, id() 0).
	 */
	class TimerHandle final
	{
		public:

			/** @brief Constructs an empty handle (no timer). */
			TimerHandle () noexcept = default;

			/**
			 * @brief Constructs a handle over a timer just created.
			 * @param timerID The timer ID.
			 * @param registration The undo destroying the timer.
			 */
			TimerHandle (TimerID timerID, ScopedRegistration registration) noexcept
				: m_registration{std::move(registration)},
				m_timerID{timerID}
			{

			}

			/** @brief Copy constructor (deleted: one owner per timer). */
			TimerHandle (const TimerHandle & copy) = delete;

			/** @brief Copy assignment (deleted). */
			TimerHandle & operator= (const TimerHandle & copy) = delete;

			/**
			 * @brief Move constructor: the timer changes owner, the source becomes empty.
			 * @param other The handle to take the timer from.
			 */
			TimerHandle (TimerHandle && other) noexcept
				: m_registration{std::move(other.m_registration)},
				m_timerID{std::exchange(other.m_timerID, 0)}
			{

			}

			/**
			 * @brief Move assignment: the timer held before is destroyed, then the other one is taken.
			 * @param other The handle to take the timer from.
			 * @return TimerHandle &
			 */
			TimerHandle &
			operator= (TimerHandle && other) noexcept
			{
				if ( this != &other )
				{
					m_registration = std::move(other.m_registration);
					m_timerID = std::exchange(other.m_timerID, 0);
				}

				return *this;
			}

			/** @brief Destructs the handle: destroys the timer. */
			~TimerHandle () = default;

			/**
			 * @brief Returns whether the handle owns a timer.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			valid () const noexcept
			{
				return m_registration.active();
			}

			/**
			 * @brief Returns the timer ID (for EventTrait's start / stop / pause calls), 0 when empty.
			 * @return TimerID
			 */
			[[nodiscard]]
			TimerID
			id () const noexcept
			{
				return m_timerID;
			}

			/**
			 * @brief Destroys the timer now (stops it, joins its thread; deferred when called from its own callback).
			 */
			void
			reset () noexcept
			{
				m_registration.reset();
				m_timerID = 0;
			}

		private:

			ScopedRegistration m_registration;
			TimerID m_timerID{0};
	};

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
			 * @pre Not called from one of its own timers' callbacks (a callback destroying its owner, or releasing the last
			 * shared_ptr to it): the timer would join itself — a contract fault, ABORTED in every build (owner decision
			 * D2, 2026-10-08). There is no safe alternative: nothing would be left to join the timer later, and the
			 * callback would go on running on a destroyed owner. destroyTimer() / destroyTimers() from a callback stay
			 * legal (the timer is retired and joined later).
			 */
			virtual ~EventTrait () = default;

			/**
			 * @brief Starts a timer. This will register a cyclic (or a unique) call to timeEvent() with an ID given by this function.
			 * @note If the function return true, the event will be stopped.
			 * @param callable The function triggered after the timeout.
			 * @param granularity This is the interval, or the timeout respecting period_t (Time unit).
			 * @param once If set to true, your timer will fire only once and will be killed automatically.
			 * @param autostart If set to true, the timer will execute automatically.
			 * @return TimerHandle The timer, owned by the caller: its end destroys it. Empty when refused.
			 */
			[[nodiscard]]
			TimerHandle
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

					return {};
				}

				auto & timer = result.first->second;
				timer.setTimerID(timerID);

				/* A timer whose thread the system refuses is no timer: refused (Base::Thread traced why). It never ran,
				 * so erasing it here, under the lock, joins nothing. */
				if ( autostart && !timer.start() )
				{
					m_events.erase(result.first);

					return {};
				}

				return TimerHandle{timerID, ScopedRegistration{[this, timerID] () {
					this->destroyTimer(timerID);
				}}};
			}

			/**
			 * @brief Executes a single event using EventTrait::createTimer().
			 * @param callable The function triggered after the timeout.
			 * @param timeout The delay before firing the event respecting the period_t (Time unit).
			 * @return TimerHandle The timer, owned by the caller (its end cancels it if it has not fired yet).
			 */
			[[nodiscard]]
			TimerHandle
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
					return timer->start();
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
			 */
			void
			startTimers () noexcept
			{
				const std::lock_guard< std::mutex > lock{m_eventsAccess};

				for ( auto & event : std::ranges::views::values(m_events) )
				{
					/* A refused start leaves that timer stopped (Base::Thread traced it); the others still start. */
					static_cast< void >(event.start());
				}
			}

			/**
			 * @brief Pauses every active timers.
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
