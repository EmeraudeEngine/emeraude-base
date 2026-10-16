/*
 * src/ScopedRegistration.hpp
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
#include <functional>
#include <utility>

namespace EmEn::Base
{
	/**
	 * @brief A registration held as an object: its destruction unregisters (Ave Robustus II P4, owner decision
	 * 2026-10-16: one generic token in base, the model of TaskHandle).
	 *
	 * A registry (a listener list, a seat table, a callback slot) that hands out a registration returns this token
	 * instead of expecting a matching remove call. The token stores the registry's undo as a callable and runs it ONCE:
	 * at destruction, at reset(), or when a move assignment replaces it. A moved-from token is empty and does nothing.
	 * The registrant keeps the token as a member, so its own destruction ends the registration: nothing else has to
	 * remember it.
	 *
	 * @pre The registry outlives every token it handed out, or its undo callable tolerates the registry's end (for
	 * instance through a weak reference). A token outliving its registry without that is a contract fault of the
	 * registry's owner.
	 * @note The undo runs on the thread that destroys or resets the token: a registry used from several threads makes
	 * its undo thread-safe. The undo must not throw (no exception in the cascade) and must not destroy the token itself.
	 * Storing the callable may allocate (a cold path: registrations are not per frame).
	 */
	class ScopedRegistration final
	{
		public:

			/** @brief Constructs an empty token (nothing registered; active() is false). */
			ScopedRegistration () noexcept = default;

			/**
			 * @brief Constructs a token that runs @p unregister once when it ends.
			 * @param unregister The registry's undo. An empty callable gives an empty token.
			 */
			explicit
			ScopedRegistration (std::function< void () > unregister) noexcept
				: m_unregister{std::move(unregister)}
			{

			}

			/** @brief Destructs the token: unregisters. */
			~ScopedRegistration ()
			{
				this->reset();
			}

			/** @brief Copy constructor (deleted: one owner per registration). */
			ScopedRegistration (const ScopedRegistration & copy) = delete;

			/** @brief Copy assignment (deleted). */
			ScopedRegistration & operator= (const ScopedRegistration & copy) = delete;

			/**
			 * @brief Move constructor: the registration changes owner, the source becomes empty.
			 * @param other The token to take the registration from.
			 */
			ScopedRegistration (ScopedRegistration && other) noexcept
				: m_unregister{std::exchange(other.m_unregister, nullptr)}
			{

			}

			/**
			 * @brief Move assignment: the registration held before is undone, then the other one is taken.
			 * @param other The token to take the registration from.
			 * @return ScopedRegistration &
			 */
			ScopedRegistration &
			operator= (ScopedRegistration && other) noexcept
			{
				if ( this != &other )
				{
					this->reset();

					m_unregister = std::exchange(other.m_unregister, nullptr);
				}

				return *this;
			}

			/**
			 * @brief Returns whether the token still holds a registration.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			active () const noexcept
			{
				return static_cast< bool >(m_unregister);
			}

			/**
			 * @brief Unregisters now (the undo runs once); the token becomes empty. No effect on an empty token.
			 * @note The undo is taken out of the token BEFORE it runs: an undo that ends up resetting this token again
			 * finds it empty.
			 */
			void
			reset () noexcept
			{
				if ( !m_unregister )
				{
					return;
				}

				const auto unregister = std::exchange(m_unregister, nullptr);

				unregister();
			}

		private:

			std::function< void () > m_unregister;
	};
}
