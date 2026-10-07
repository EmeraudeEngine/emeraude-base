/*
 * src/Testing/test_Debug.cpp
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

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <chrono>
#include <cstdint>

/* Local inclusions. */
#include "Debug/Statistics.hpp"
#include "Time/Time.hpp"

using namespace EmEn::Base;

/* Ave robustus! (Axis B): the Debug ns timer was Linux-only (link errors elsewhere) and subtracted
 * tv_nsec only (wrong across a 1s boundary). It now delegates to Time::processCPUTimeNanoseconds(),
 * cross-platform, in nanoseconds — but NOT nanosecond-RESOLUTION everywhere: Windows advances it by
 * 15.625 ms quanta (Time::processCPUTimeResolutionNanoseconds()). A fixed 50-million-iteration loop
 * landed near one quantum there and failed 2 runs in 6 (2026-09-15). The work now lasts at least two
 * resolutions of CPU time, whatever the platform; no fixed loop length decides the verdict. */
TEST(DebugStatistics, timerMeasuresBusyWork)
{
	const auto resolution = Time::processCPUTimeResolutionNanoseconds();
	const auto start = Debug::begin_timer();
	const auto wallDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};

	/* Busy work (volatile sink so it is not optimised away) to accrue real process CPU time. */
	volatile uint64_t sink = 0;

	while ( Time::processCPUTimeNanoseconds() - start < 2 * resolution )
	{
		ASSERT_LT(std::chrono::steady_clock::now(), wallDeadline) << "No CPU time accrued in 10 s of busy work.";

		for ( uint64_t i = 0; i < 100000ULL; ++i )
		{
			sink = sink + (i * 2654435761ULL);
		}
	}

	const auto elapsed = Debug::terminate_timer(start);

	EXPECT_GE(elapsed, 2 * resolution);
}

TEST(DebugStatistics, processCPUTimeResolutionIsKnown)
{
	const auto resolution = Time::processCPUTimeResolutionNanoseconds();

	EXPECT_GT(resolution, 0U);
	/* No platform is coarser than its scheduler quantum (Windows: 15.625 ms). */
	EXPECT_LE(resolution, 15'625'000ULL);
}
