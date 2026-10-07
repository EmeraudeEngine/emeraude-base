/*
 * src/Network/HappyEyeballs.cpp
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

#include "HappyEyeballs.hpp"

/* STL inclusions. */
#include <cstddef>
#include <optional>

namespace EmEn::Base::Network
{
	namespace
	{
		/**
		 * @brief One connection race: the attempts, their sockets, the attempt-delay timer and the winner.
		 * @note Lives on the stack of connectFirstReachable(), which drains the io_context before it returns: every
		 * handler capturing `this` has run by then.
		 */
		class ConnectionRace final
		{
			public:

				/**
				 * @brief Constructs a race.
				 * @param ioContext The io_context the attempts run on.
				 * @param endpoints The endpoints, in the order to try them.
				 * @param attemptDelay The delay before the next attempt.
				 */
				ConnectionRace (asio::io_context & ioContext, std::span< const asio::ip::tcp::endpoint > endpoints, std::chrono::milliseconds attemptDelay) noexcept
					: m_ioContext(&ioContext),
					m_endpoints(endpoints),
					m_timer(ioContext),
					m_attemptDelay(attemptDelay)
				{
					/* ⚠️ Reserved once: a socket with a pending connect must not move. */
					m_sockets.reserve(endpoints.size());
				}

				/**
				 * @brief Starts the first attempt.
				 * @return void
				 */
				void
				start () noexcept
				{
					this->startNextAttempt();
				}

				/**
				 * @brief Ends the race on a timeout: the timer and every attempt but the winner are closed.
				 * @note The io_context must then be run until it drains.
				 * @return void
				 */
				void
				abort () noexcept
				{
					m_aborted = true;

					this->closeLosers();
				}

				/**
				 * @brief Hands the winning socket over, if any.
				 * @param socket The socket that receives the connection.
				 * @return bool
				 */
				[[nodiscard]]
				bool
				takeWinner (asio::ip::tcp::socket & socket) noexcept
				{
					if ( !m_winner.has_value() )
					{
						return false;
					}

					socket = std::move(m_sockets[*m_winner]);

					return true;
				}

				/**
				 * @brief Returns the error of the last attempt that failed.
				 * @return const asio::error_code &
				 */
				[[nodiscard]]
				const asio::error_code &
				lastError () const noexcept
				{
					return m_lastError;
				}

			private:

				/**
				 * @brief Starts the next endpoint's attempt and arms the attempt delay. An endpoint whose socket cannot
				 * be opened (its family unsupported here) fails at once and the next one is tried.
				 * @return void
				 */
				void
				startNextAttempt () noexcept
				{
					/* A delay expiry already queued when the race ended must not start anything. */
					if ( m_winner.has_value() || m_aborted )
					{
						return;
					}

					while ( m_nextAttempt < m_endpoints.size() )
					{
						const auto index = m_nextAttempt++;
						auto & socket = m_sockets.emplace_back(*m_ioContext);

						asio::error_code openError;

						socket.open(m_endpoints[index].protocol(), openError);

						if ( openError )
						{
							m_lastError = openError;

							continue;
						}

						socket.async_connect(m_endpoints[index], [this, index] (const asio::error_code & error) {
							this->onAttemptCompleted(index, error);
						});

						if ( m_nextAttempt < m_endpoints.size() )
						{
							/* The generation tells a stale expiry (already queued when the timer was re-armed) from the
							 * current one. */
							const auto generation = ++m_timerGeneration;

							m_timer.expires_after(m_attemptDelay);
							m_timer.async_wait([this, generation] (const asio::error_code & error) {
								if ( !error && generation == m_timerGeneration )
								{
									this->startNextAttempt();
								}
							});
						}

						return;
					}
				}

				/**
				 * @brief Completion of one attempt: the first success wins, a failure starts the next attempt at once.
				 * @param index The attempt's index.
				 * @param error The connect result.
				 * @return void
				 */
				void
				onAttemptCompleted (size_t index, const asio::error_code & error) noexcept
				{
					if ( m_winner.has_value() || m_aborted )
					{
						/* A loser already closed by closeLosers(), or an attempt closed by abort(): nothing starts any
						 * more. */
						return;
					}

					if ( !error )
					{
						m_winner = index;

						this->closeLosers();

						return;
					}

					m_lastError = error;

					/* RFC 8305 § 5: a failed attempt does not wait for the delay. The pending delay is cancelled either way:
					 * left armed after the last attempt, it would only hold the io_context up to its expiry. */
					++m_timerGeneration;

					m_timer.cancel();

					this->startNextAttempt();
				}

				/**
				 * @brief Cancels the attempt delay and closes every socket but the winner's.
				 * @return void
				 */
				void
				closeLosers () noexcept
				{
					/* No error_code overload under ASIO_NO_DEPRECATED; a timer cancel never fails (the service clears the
					 * error), as in HTTPServer / GracefulCloser. */
					m_timer.cancel();

					for ( size_t index = 0; index < m_sockets.size(); ++index )
					{
						if ( index != m_winner && m_sockets[index].is_open() )
						{
							asio::error_code closeError;

							m_sockets[index].close(closeError);
						}
					}
				}

				asio::io_context * m_ioContext;
				std::span< const asio::ip::tcp::endpoint > m_endpoints;
				std::vector< asio::ip::tcp::socket > m_sockets;
				asio::steady_timer m_timer;
				std::chrono::milliseconds m_attemptDelay;
				asio::error_code m_lastError{asio::error::host_not_found};
				std::optional< size_t > m_winner;
				size_t m_nextAttempt{0};
				size_t m_timerGeneration{0};
				bool m_aborted{false};
		};
	}

	std::vector< asio::ip::tcp::endpoint >
	interleaveAddressFamilies (std::span< const asio::ip::tcp::endpoint > endpoints) noexcept
	{
		std::vector< asio::ip::tcp::endpoint > ordered;
		ordered.reserve(endpoints.size());

		if ( endpoints.empty() )
		{
			return ordered;
		}

		const auto firstIsIPv6 = endpoints.front().address().is_v6();

		std::vector< asio::ip::tcp::endpoint > firstFamily;
		std::vector< asio::ip::tcp::endpoint > otherFamily;

		for ( const auto & endpoint : endpoints )
		{
			(endpoint.address().is_v6() == firstIsIPv6 ? firstFamily : otherFamily).push_back(endpoint);
		}

		for ( size_t index = 0; index < firstFamily.size() || index < otherFamily.size(); ++index )
		{
			if ( index < firstFamily.size() )
			{
				ordered.push_back(firstFamily[index]);
			}

			if ( index < otherFamily.size() )
			{
				ordered.push_back(otherFamily[index]);
			}
		}

		return ordered;
	}

	asio::error_code
	connectFirstReachable (asio::io_context & ioContext, std::span< const asio::ip::tcp::endpoint > endpoints, asio::ip::tcp::socket & socket, std::chrono::milliseconds timeout, std::chrono::milliseconds attemptDelay) noexcept
	{
		if ( endpoints.empty() || socket.is_open() )
		{
			return asio::error::invalid_argument;
		}

		ConnectionRace race{ioContext, endpoints, attemptDelay};

		ioContext.restart();

		race.start();

		ioContext.run_for(timeout);

		const auto timedOut = !ioContext.stopped();

		if ( timedOut )
		{
			/* Closing the attempts completes them with operation_aborted, then the context drains: no handler
			 * referencing the race survives this function. */
			race.abort();

			ioContext.run();
		}

		if ( race.takeWinner(socket) )
		{
			return {};
		}

		if ( timedOut )
		{
			return asio::error::timed_out;
		}

		return race.lastError();
	}
}
