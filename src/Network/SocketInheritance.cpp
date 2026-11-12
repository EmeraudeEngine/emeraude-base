/*
 * src/Network/SocketInheritance.cpp
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

#include "SocketInheritance.hpp"

/* Project configuration. */
#include "emeraude_platform.hpp"

/* Third-party inclusions. */
#if IS_WINDOWS
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>
#else
	#include <fcntl.h>
#endif

namespace EmEn::Base::Network
{
	bool
	preventHandleInheritance (std::uintptr_t nativeHandle) noexcept
	{
#if IS_WINDOWS
		/* NOTE: A SOCKET is a kernel handle: SetHandleInformation() is the documented way to drop its inheritance. */
		return SetHandleInformation(reinterpret_cast< HANDLE >(nativeHandle), HANDLE_FLAG_INHERIT, 0) != 0;
#else
		const auto descriptor = static_cast< int >(nativeHandle);

		/* NOTE: fcntl() is the POSIX interface for descriptor flags (a C variadic call, the only one there is). */
		const int flags = ::fcntl(descriptor, F_GETFD);

		if ( flags < 0 )
		{
			return false;
		}

		return (flags & FD_CLOEXEC) != 0 || ::fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) == 0;
#endif
	}
}
