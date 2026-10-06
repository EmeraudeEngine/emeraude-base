/*
 * src/Network/GracefulCloser.hpp
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
#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <vector>

/* Third-party inclusions. */
#include "asio.hpp"
#include "Network/asio_throw_exception.hpp"

namespace EmEn::Base::Network
{
	/**
	 * @brief Closes TCP sockets GRACEFULLY: the last bytes a server wrote (a refusal, a "Connection: close" answer) reach
	 * the client even when it is still sending.
	 * @note Closing a socket whose receive buffer holds unread bytes sends a RST instead of a FIN (RFC 1122 § 4.2.2.13),
	 * and on Windows a RST discards what the client had received but not yet read — the 503 of a server at its
	 * connection cap was lost 3 % of the time there (2026-10-06). So: shutdown(send) — the FIN follows what was written —
	 * then read and discard what the client still sends until it closes, then close(). BOUNDED, a hostile client must not
	 * hold the socket: a timeout, a byte budget, and a cap on the sockets lingering at once (beyond it, an immediate close:
	 * a RST accepted under a flood). Owner decision 2026-10-06: 1 s, 64 KiB, the server's connection cap.
	 * Threading: every call on the io_context thread that runs the sockets; the drains run there.
	 */
	class GracefulCloser final
	{
		public:

			/** @brief The longest a socket lingers. */
			static constexpr std::chrono::milliseconds DefaultTimeout{1000};

			/** @brief The most bytes read and discarded before giving up (then an immediate close). */
			static constexpr size_t DefaultMaxDrainBytes{65536};

			/**
			 * @brief Constructs a closer.
			 * @param maxLingering The most sockets lingering at once (0 = every close is immediate).
			 * @param timeout The longest a socket lingers. Default DefaultTimeout.
			 * @param maxDrainBytes The most bytes discarded per socket. Default DefaultMaxDrainBytes.
			 */
			explicit GracefulCloser (size_t maxLingering, std::chrono::milliseconds timeout = DefaultTimeout, size_t maxDrainBytes = DefaultMaxDrainBytes) noexcept;

			/**
			 * @brief Closes a socket gracefully (or at once, beyond the lingering cap).
			 * @pre Called on the thread running the socket's io_context; no operation of the caller is pending on it.
			 * @param socket The socket, taken.
			 * @return void
			 */
			void close (asio::ip::tcp::socket socket) noexcept;

			/**
			 * @brief Closes a shared socket gracefully (or at once, beyond the lingering cap).
			 * @pre As close(asio::ip::tcp::socket): the other owners only read is_open() from now on.
			 * @param socket The socket. Null is ignored.
			 * @return void
			 */
			void close (const std::shared_ptr< asio::ip::tcp::socket > & socket) noexcept;

			/**
			 * @brief Closes every lingering socket at once (the server stops).
			 * @pre Called on the thread running the sockets' io_context.
			 * @return void
			 */
			void abortAll () noexcept;

			/**
			 * @brief Returns the sockets lingering now.
			 * @pre Called on the thread running the sockets' io_context.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t lingering () noexcept;

		private:

			class Lingering;

			/**
			 * @brief Forgets the finished lingering sockets.
			 * @return void
			 */
			void prune () noexcept;

			std::vector< std::weak_ptr< Lingering > > m_lingering;
			std::chrono::milliseconds m_timeout;
			size_t m_maxLingering;
			size_t m_maxDrainBytes;
	};
}
