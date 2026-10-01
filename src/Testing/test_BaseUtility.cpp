/*
 * src/Testing/test_BaseUtility.cpp
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

/* STL inclusions. */
#include <cstdint>
#include <limits>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "BaseUtility.hpp"

using namespace EmEn::Base;

namespace
{
	/* Draws many values and returns true when every one is inside [min, max]. */
	template< typename number_t >
	bool
	allInRange (number_t min, number_t max, int draws = 20000)
	{
		for ( int draw = 0; draw < draws; ++draw )
		{
			const auto value = Utility::quickRandom< number_t >(min, max);

			if ( value < min || value > max )
			{
				return false;
			}
		}

		return true;
	}
}

/* 2026-10-01 (engine triad 12): a rand() truncated to a signed 8 or 16-bit type went negative, so 46 % of the values of
 * quickRandom< int8_t >(0, 10) were below 0; a wide int32 range overflowed `1 + max - min` (signed overflow). */
TEST(BaseUtility, QuickRandomIntegerStaysInRange)
{
	ASSERT_TRUE(allInRange< int8_t >(0, 10));
	ASSERT_TRUE(allInRange< int8_t >(-128, 127));
	ASSERT_TRUE(allInRange< uint8_t >(0, 10));
	ASSERT_TRUE(allInRange< int16_t >(0, 10));
	ASSERT_TRUE(allInRange< int16_t >(-1000, 1000));
	ASSERT_TRUE(allInRange< uint16_t >(100, 200));
	ASSERT_TRUE(allInRange< int32_t >(0, 10));
	ASSERT_TRUE(allInRange< int32_t >(-2000000000, 2000000000));
	ASSERT_TRUE(allInRange< int32_t >(std::numeric_limits< int32_t >::min(), std::numeric_limits< int32_t >::max()));
	ASSERT_TRUE(allInRange< uint32_t >(0, std::numeric_limits< uint32_t >::max()));
	ASSERT_TRUE(allInRange< int64_t >(std::numeric_limits< int64_t >::min(), std::numeric_limits< int64_t >::max()));
	ASSERT_TRUE(allInRange< uint64_t >(5, 9));
}

TEST(BaseUtility, QuickRandomIntegerBounds)
{
	/* A single-value range, swapped bounds, and both ends reachable on a small range. */
	ASSERT_EQ(Utility::quickRandom< int16_t >(7, 7), 7);
	ASSERT_TRUE(allInRange< int8_t >(0, 3) && Utility::quickRandom< int8_t >(3, 0) <= 3);

	bool seenMin = false;
	bool seenMax = false;

	for ( int draw = 0; draw < 2000; ++draw )
	{
		const auto value = Utility::quickRandom< int8_t >(-2, 2);

		seenMin = seenMin || value == -2;
		seenMax = seenMax || value == 2;
	}

	ASSERT_TRUE(seenMin);
	ASSERT_TRUE(seenMax);
}
