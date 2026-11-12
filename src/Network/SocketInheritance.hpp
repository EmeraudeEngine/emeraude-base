/*
 * src/Network/SocketInheritance.hpp
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

namespace EmEn::Base::Network
{
	/**
	 * @brief Makes a native socket handle private to this process: a process started from here (a dialog, a desktop
	 * command, a tool) never inherits it. Without it, the child keeps the port bound or the connection open after this
	 * process ended (measured 2026-11-11: a dialog held a server's ports after the application was killed).
	 * @note POSIX: FD_CLOEXEC. Windows: the handle loses HANDLE_FLAG_INHERIT. asio opens its sockets without either.
	 * @param nativeHandle The native handle (a descriptor on POSIX, a SOCKET on Windows), widened.
	 * @return bool False when the system refused the change (the socket stays inheritable; the caller reports it).
	 */
	[[nodiscard]]
	bool preventHandleInheritance (std::uintptr_t nativeHandle) noexcept;

	/**
	 * @brief Makes an open asio socket or acceptor private to this process (preventHandleInheritance()).
	 * @note Call it right after the socket is opened (basic_socket::open(), an accepted socket, an acceptor's open()).
	 * @tparam socket_t An asio socket or acceptor type.
	 * @param socket The open socket.
	 * @return bool False when the socket is not open or the system refused the change.
	 */
	template< typename socket_t >
	[[nodiscard]]
	bool
	preventSocketInheritance (socket_t & socket) noexcept
	{
		if ( !socket.is_open() )
		{
			return false;
		}

		return preventHandleInheritance(static_cast< std::uintptr_t >(socket.native_handle()));
	}
}
