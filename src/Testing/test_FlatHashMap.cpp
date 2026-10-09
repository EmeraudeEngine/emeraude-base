/*
 * src/Testing/test_FlatHashMap.cpp
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
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "FlatHashMap.hpp"

using namespace EmEn::Base;

namespace
{
	struct Point final
	{
		int64_t x{0};
		int64_t y{0};

		bool operator== (const Point & other) const noexcept = default;
	};

	struct PointHash final
	{
		[[nodiscard]]
		uint64_t
		operator() (const Point & point) const noexcept
		{
			return mixHash(static_cast< uint64_t >(point.x) ^ mixHash(static_cast< uint64_t >(point.y)));
		}
	};
}

/* The splitmix64 finalizer: golden values, the same on every platform (they fix the layout and the forEach() order). */
TEST(FlatHashMap, mixHashIsTheSplitmix64Finalizer)
{
	EXPECT_EQ(mixHash(0), UINT64_C(0));
	EXPECT_EQ(mixHash(1), UINT64_C(0x5692161D100B05E5));
	EXPECT_EQ(mixHash(2), UINT64_C(0xDBD238973A2B148A));
	EXPECT_EQ(mixHash(UINT64_MAX), UINT64_C(0xB4D055FCF2CBBD7B));
}

TEST(FlatHashMap, insertsFindsAndKeepsTheFirstValue)
{
	FlatHashMap< uint64_t, int > map;

	EXPECT_TRUE(map.empty());
	EXPECT_EQ(map.find(7), nullptr);

	const auto [first, inserted] = map.tryEmplace(7, 70);

	ASSERT_NE(first, nullptr);
	EXPECT_TRUE(inserted);
	EXPECT_EQ(*first, 70);

	const auto [again, insertedAgain] = map.tryEmplace(7, 99);

	EXPECT_FALSE(insertedAgain);
	EXPECT_EQ(again, first);
	EXPECT_EQ(*again, 70);
	EXPECT_EQ(map.size(), 1U);
	EXPECT_TRUE(map.contains(7));
	EXPECT_FALSE(map.contains(8));

	map[8] += 5;
	map[8] += 5;

	ASSERT_NE(map.find(8), nullptr);
	EXPECT_EQ(*map.find(8), 10);
}

/* Growth past the reserved count keeps every entry; a reserved map never reallocates. */
TEST(FlatHashMap, growsAndAgreesWithUnorderedMap)
{
	FlatHashMap< uint32_t, uint32_t > grown;
	std::unordered_map< uint32_t, uint32_t > reference;

	for ( uint32_t index = 0; index < 100000; ++index )
	{
		const auto key = (index * 2654435761U) % 50021U;

		static_cast< void >(grown.tryEmplace(key, index));
		reference.try_emplace(key, index);
	}

	EXPECT_EQ(grown.size(), reference.size());
	EXPECT_EQ(grown.slotCount() & (grown.slotCount() - 1), 0U);
	EXPECT_GE(grown.slotCount(), grown.size() * 2);

	for ( const auto & [key, value] : reference )
	{
		const auto * found = grown.find(key);

		ASSERT_NE(found, nullptr) << key;
		EXPECT_EQ(*found, value) << key;
	}

	FlatHashMap< uint32_t, uint32_t > reserved{100000};
	const auto slots = reserved.slotCount();

	for ( uint32_t index = 0; index < 100000; ++index )
	{
		static_cast< void >(reserved.tryEmplace(index, index));
	}

	EXPECT_EQ(reserved.slotCount(), slots);
	EXPECT_EQ(reserved.size(), 100000U);
}

/* forEach() visits each entry once; two maps fed the same insertions walk in the same order. */
TEST(FlatHashMap, forEachIsCompleteAndDeterministic)
{
	FlatHashMap< uint64_t, uint64_t > first;
	FlatHashMap< uint64_t, uint64_t > second;

	for ( uint64_t key = 1; key <= 1000; ++key )
	{
		static_cast< void >(first.tryEmplace(key * 7919U, key));
		static_cast< void >(second.tryEmplace(key * 7919U, key));
	}

	std::vector< uint64_t > orderFirst;
	std::vector< uint64_t > orderSecond;
	uint64_t sum = 0;

	first.forEach([&] (const uint64_t & key, uint64_t & value) {
		orderFirst.push_back(key);
		sum += value;
	});

	std::as_const(second).forEach([&orderSecond] (const uint64_t & key, const uint64_t &) {
		orderSecond.push_back(key);
	});

	EXPECT_EQ(orderFirst.size(), 1000U);
	EXPECT_EQ(sum, 500500U);
	EXPECT_EQ(orderFirst, orderSecond);
}

/* forEachUntil() stops when the function says so, and reports it. */
TEST(FlatHashMap, forEachUntilStops)
{
	FlatHashMap< uint32_t, uint32_t > map{64};

	for ( uint32_t key = 0; key < 50; ++key )
	{
		static_cast< void >(map.tryEmplace(key, key));
	}

	size_t visited = 0;

	EXPECT_FALSE(map.forEachUntil([&visited] (const uint32_t &, uint32_t &) {
		++visited;

		return visited < 10;
	}));
	EXPECT_EQ(visited, 10U);

	visited = 0;

	EXPECT_TRUE(map.forEachUntil([&visited] (const uint32_t &, uint32_t & value) {
		++visited;
		value += 1;

		return true;
	}));
	EXPECT_EQ(visited, 50U);
	ASSERT_NE(map.find(7), nullptr);
	EXPECT_EQ(*map.find(7), 8U);
}

TEST(FlatHashMap, customKeyAndClear)
{
	FlatHashMap< Point, size_t, PointHash > map{4};

	for ( int64_t x = -20; x <= 20; ++x )
	{
		for ( int64_t y = -20; y <= 20; ++y )
		{
			static_cast< void >(map.tryEmplace(Point{.x = x, .y = y}, map.size()));
		}
	}

	EXPECT_EQ(map.size(), 41U * 41U);
	ASSERT_NE(map.find(Point{.x = -20, .y = -20}), nullptr);
	EXPECT_EQ(*map.find(Point{.x = -20, .y = -20}), 0U);
	EXPECT_EQ(map.find(Point{.x = 21, .y = 0}), nullptr);

	const auto slots = map.slotCount();

	map.clear();

	EXPECT_TRUE(map.empty());
	EXPECT_EQ(map.slotCount(), slots);
	EXPECT_EQ(map.find(Point{.x = 0, .y = 0}), nullptr);
}
