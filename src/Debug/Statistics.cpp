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
 */

#include "Statistics.hpp"

/* Local inclusions. */
#include "Time/Time.hpp"

namespace EmEn::Base::Debug
{
	uint64_t
	begin_timer () noexcept
	{
		return Time::processCPUTimeNanoseconds();
	}

	uint64_t
	terminate_timer (uint64_t start_time) noexcept
	{
		const auto now = Time::processCPUTimeNanoseconds();

		/* Process CPU time is monotonic; guard the rare query-failure case (returns 0). */
		return now >= start_time ? now - start_time : 0;
	}
}
