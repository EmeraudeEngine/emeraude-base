/*
 * src/Debug/Statistics.cpp
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

/* Project configuration. */
#include "emeraude_platform.hpp"

#if IS_LINUX

extern "C"
{
	#include "Statistics.hpp"

	void
	begin_timer (timespec * start_time) noexcept
	{
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, start_time);
	}

	long
	terminate_timer (timespec start_time) noexcept
	{
		timespec end_time{};

		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);

		return end_time.tv_nsec - start_time.tv_nsec;
	}
}

#endif
