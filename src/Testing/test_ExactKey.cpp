/*
 * src/Testing/test_ExactKey.cpp
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
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "ExactKey.hpp"

namespace EmEn::Base
{
	namespace
	{
		enum class Pass : uint8_t
		{
			Csm = 3,
			SpotShadow = 7
		};
	}

	TEST(ExactKey, SameInputsSameKey)
	{
		auto a = ExactKey{"Generator"}.add(Pass::Csm).add(std::string_view{"CityBuildings6"}).add(8U);
		auto b = ExactKey{"Generator"}.add(Pass::Csm).add(std::string_view{"CityBuildings6"}).add(8U);

		EXPECT_EQ(a, b);
		EXPECT_EQ(a.hash(), b.hash());
	}

	TEST(ExactKey, OneDifferentInputDifferentKey)
	{
		const auto csm = ExactKey{"Generator"}.add(Pass::Csm).add(8U);
		const auto spot = ExactKey{"Generator"}.add(Pass::SpotShadow).add(8U);
		const auto otherLayer = ExactKey{"Generator"}.add(Pass::Csm).add(9U);
		const auto otherDomain = ExactKey{"Other"}.add(Pass::Csm).add(8U);

		EXPECT_NE(csm, spot);
		EXPECT_NE(csm, otherLayer);
		EXPECT_NE(csm, otherDomain);
	}

	TEST(ExactKey, TextBoundariesAreUnambiguous)
	{
		/* The length in front of each text: "ab" + "c" is not "a" + "bc", nor "abc". */
		const auto abC = ExactKey{}.add(std::string_view{"ab"}).add(std::string_view{"c"});
		const auto aBc = ExactKey{}.add(std::string_view{"a"}).add(std::string_view{"bc"});
		const auto abc = ExactKey{}.add(std::string_view{"abc"});

		EXPECT_NE(abC, aBc);
		EXPECT_NE(abC, abc);
		EXPECT_NE(aBc, abc);
	}

	TEST(ExactKey, FixedWidthValues)
	{
		/* Every value takes eight bytes, whatever its type: two small values are not one larger one. */
		const auto twoBytes = ExactKey{}.add(static_cast< uint8_t >(1)).add(static_cast< uint8_t >(2));
		const auto oneWord = ExactKey{}.add(static_cast< uint16_t >(0x0201));

		EXPECT_NE(twoBytes, oneWord);
		EXPECT_EQ(ExactKey{}.add(true).size(), 8U);
		EXPECT_EQ(ExactKey{}.add(1.0F).size(), 8U);
		EXPECT_EQ(ExactKey{}.add(std::string_view{"abc"}).size(), 11U);
	}

	TEST(ExactKey, FloatsByTheirBits)
	{
		EXPECT_EQ(ExactKey{}.add(-4.0F), ExactKey{}.add(-4.0F));
		EXPECT_NE(ExactKey{}.add(-4.0F), ExactKey{}.add(-4.5F));
		/* By the bits: the two zeros are two keys. */
		EXPECT_NE(ExactKey{}.add(0.0F), ExactKey{}.add(-0.0F));
		EXPECT_NE(ExactKey{}.add(1.0F), ExactKey{}.add(1.0));
	}

	TEST(ExactKey, PointersByTheirAddress)
	{
		int first = 0;
		int second = 0;

		EXPECT_EQ(ExactKey{}.add(static_cast< const void * >(&first)), ExactKey{}.add(static_cast< const void * >(&first)));
		EXPECT_NE(ExactKey{}.add(static_cast< const void * >(&first)), ExactKey{}.add(static_cast< const void * >(&second)));
		EXPECT_NE(ExactKey{}.add(static_cast< const void * >(nullptr)), ExactKey{}.add(static_cast< const void * >(&first)));
	}

	TEST(ExactKey, ServesTheRightObjectEvenWhenTheHashesMeet)
	{
		/* The map compares the keys, not the hashes: a collision of hash() never serves one object for another. With
		 * a single bucket every key collides, yet each finds its own value. */
		std::unordered_map< ExactKey, int > cache;
		cache.max_load_factor(1000.0F);
		cache.rehash(1);

		cache.emplace(ExactKey{"G"}.add(Pass::Csm), 1);
		cache.emplace(ExactKey{"G"}.add(Pass::SpotShadow), 2);

		ASSERT_EQ(cache.size(), 2U);

		const auto csm = cache.find(ExactKey{"G"}.add(Pass::Csm));
		const auto spot = cache.find(ExactKey{"G"}.add(Pass::SpotShadow));

		ASSERT_NE(csm, cache.end());
		ASSERT_NE(spot, cache.end());
		EXPECT_EQ(csm->second, 1);
		EXPECT_EQ(spot->second, 2);
	}
}
