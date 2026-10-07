/*
 * src/Network/HappyEyeballs.hpp
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
#include <span>
#include <vector>

/* Third-party inclusions.
 * NOTE: the no-exceptions hook MUST be included before any asio header. */
#include "Network/asio_throw_exception.hpp"
#include "asio.hpp"

namespace EmEn::Base::Network
{
	/** @brief The delay before the next connection attempt starts while the previous one has not answered (RFC 8305
	 * § 8: 250 ms recommended, 100 ms minimum, 2 s maximum). */
	constexpr std::chrono::milliseconds ConnectionAttemptDelay{250};

	/**
	 * @brief Orders the resolved endpoints for a connection race (RFC 8305 § 4): the address families alternate,
	 * starting with the family of the first endpoint; the resolver's order is kept inside each family.
	 * @note getaddrinfo() already sorts the answer by RFC 6724 (destination address selection): only the interleaving
	 * is added here, so that an unreachable family costs one attempt delay instead of every one of its addresses.
	 * @param endpoints The resolved endpoints, in the resolver's order.
	 * @return std::vector< asio::ip::tcp::endpoint >
	 */
	[[nodiscard]]
	std::vector< asio::ip::tcp::endpoint > interleaveAddressFamilies (std::span< const asio::ip::tcp::endpoint > endpoints) noexcept;

	/**
	 * @brief Connects a TCP socket to the first endpoint that answers — Happy Eyeballs v2 (RFC 8305 § 5).
	 * @note The attempts start one after the other, the next one when the previous has not connected within
	 * `attemptDelay` or as soon as it failed; the first connected socket wins and every other attempt is closed. A
	 * sequential connect pays the whole failure of each unreachable address first (a refused connect costs ~2 s on
	 * Windows, an unanswered one the system's SYN timeout).
	 * @note Blocks the calling thread: it runs `ioContext` (restarted first) until the race ends or `timeout` expires,
	 * then drains it. Nothing else may be pending on `ioContext`.
	 * @pre `socket` belongs to `ioContext` and is closed.
	 * @param ioContext The io_context the attempts run on.
	 * @param endpoints The endpoints, in the order to try them (see interleaveAddressFamilies()).
	 * @param socket The socket that receives the winning connection.
	 * @param timeout The budget of the whole race.
	 * @param attemptDelay The delay before the next attempt. Default ConnectionAttemptDelay.
	 * @return asio::error_code Success; asio::error::timed_out; asio::error::invalid_argument for an empty list or an
	 * open socket; otherwise the error of the last attempt that failed.
	 */
	[[nodiscard]]
	asio::error_code connectFirstReachable (asio::io_context & ioContext, std::span< const asio::ip::tcp::endpoint > endpoints, asio::ip::tcp::socket & socket, std::chrono::milliseconds timeout, std::chrono::milliseconds attemptDelay = ConnectionAttemptDelay) noexcept;
}
