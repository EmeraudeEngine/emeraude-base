/*
 * src/Time/Time.cpp
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

#include "Time.hpp"

/* Project configuration. */
#include "emeraude_platform.hpp"

/* STL / system inclusions (platform-specific). */
#if IS_WINDOWS
#include <windows.h>
#else
#include <ctime>
#endif

namespace EmEn::Base::Time
{
	uint64_t
	processCPUTimeNanoseconds () noexcept
	{
#if IS_WINDOWS
		FILETIME creationTime{};
		FILETIME exitTime{};
		FILETIME kernelTime{};
		FILETIME userTime{};

		if ( GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime) == 0 )
		{
			return 0;
		}

		ULARGE_INTEGER kernel{};
		ULARGE_INTEGER user{};

		kernel.LowPart = kernelTime.dwLowDateTime;
		kernel.HighPart = kernelTime.dwHighDateTime;
		user.LowPart = userTime.dwLowDateTime;
		user.HighPart = userTime.dwHighDateTime;

		/* GetProcessTimes reports kernel + user CPU time in 100-nanosecond units. */
		return (static_cast< uint64_t >(kernel.QuadPart) + static_cast< uint64_t >(user.QuadPart)) * 100ULL;
#else
		struct timespec timeSpec{};

		if ( clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timeSpec) != 0 )
		{
			return 0;
		}

		return (static_cast< uint64_t >(timeSpec.tv_sec) * 1'000'000'000ULL) + static_cast< uint64_t >(timeSpec.tv_nsec);
#endif
	}
}