/*
 * src/Testing/test_Any.cpp
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
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "Any.hpp"

using namespace EmEn::Base;

namespace
{
	/** @brief A payload larger than the inline buffer: held on the heap. */
	struct LargePayload final
	{
		std::array< uint64_t, 8 > values{};
	};

	/** @brief Counts its live instances, to prove every held copy is destroyed exactly once. */
	struct Counted final
	{
		explicit
		Counted (int * liveCount) noexcept
			: live{liveCount}
		{
			++*live;
		}

		Counted (const Counted & copy) noexcept
			: live{copy.live}
		{
			++*live;
		}

		Counted (Counted && move) noexcept
			: live{move.live}
		{
			++*live;
		}

		Counted & operator= (const Counted & copy) noexcept = delete;
		Counted & operator= (Counted && move) noexcept = delete;

		~Counted ()
		{
			--*live;
		}

		int * live;
	};

	struct Base {};
	struct Derived final : Base {};
}

TEST(BaseAny, anEmptyValueHoldsNothing)
{
	const Any empty;

	EXPECT_FALSE(empty.hasValue());
	EXPECT_EQ(empty.typeHash(), 0U);
	EXPECT_EQ(anyCast< int >(&empty), nullptr);
	EXPECT_EQ(anyCast< int >(static_cast< const Any * >(nullptr)), nullptr);
}

TEST(BaseAny, aValueIsReadBackOnlyAsItsOwnType)
{
	const Any integer{42};

	ASSERT_TRUE(integer.hasValue());
	EXPECT_EQ(integer.typeHash(), typeHashOf< int >());

	const auto * value = anyCast< int >(&integer);
	ASSERT_NE(value, nullptr);
	EXPECT_EQ(*value, 42);

	/* No conversion, no promotion, no signedness change: another type is another type. */
	EXPECT_EQ(anyCast< long >(&integer), nullptr);
	EXPECT_EQ(anyCast< unsigned int >(&integer), nullptr);
	EXPECT_EQ(anyCast< float >(&integer), nullptr);
	EXPECT_EQ(anyCast< std::string >(&integer), nullptr);
}

TEST(BaseAny, cvAndReferencesAreTheSameType)
{
	/* The engine reads `const std::shared_ptr< T >` out of a payload built from `std::shared_ptr< T >`. */
	const Any shared{std::make_shared< int >(7)};

	EXPECT_EQ(typeHashOf< const std::shared_ptr< int > & >(), typeHashOf< std::shared_ptr< int > >());
	ASSERT_NE(anyCast< const std::shared_ptr< int > >(&shared), nullptr);
	EXPECT_EQ(**anyCast< const std::shared_ptr< int > >(&shared), 7);
}

TEST(BaseAny, aBaseClassDoesNotMatchADerivedPayload)
{
	/* Like std::any: the exact type only, no hierarchy walk. */
	const Any derived{Derived{}};

	EXPECT_NE(anyCast< Derived >(&derived), nullptr);
	EXPECT_EQ(anyCast< Base >(&derived), nullptr);
	EXPECT_NE(typeHashOf< Base >(), typeHashOf< Derived >());
}

TEST(BaseAny, smallAndLargeValuesSurviveCopiesAndMoves)
{
	Any small{std::string{"inline"}};
	LargePayload large;
	large.values[7] = 77;
	Any heap{large};

	Any smallCopy{small};
	Any heapCopy{heap};
	ASSERT_NE(anyCast< std::string >(&smallCopy), nullptr);
	EXPECT_EQ(*anyCast< std::string >(&smallCopy), "inline");
	ASSERT_NE(anyCast< LargePayload >(&heapCopy), nullptr);
	EXPECT_EQ(anyCast< LargePayload >(&heapCopy)->values[7], 77U);

	/* The copies are independent of the originals. */
	*anyCast< std::string >(&smallCopy) = "changed";
	EXPECT_EQ(*anyCast< std::string >(&small), "inline");

	Any smallMoved{std::move(small)};
	Any heapMoved{std::move(heap)};
	/* The moved-from state is part of the contract. */
	EXPECT_FALSE(small.hasValue());
	EXPECT_FALSE(heap.hasValue());
	EXPECT_EQ(*anyCast< std::string >(&smallMoved), "inline");
	EXPECT_EQ(anyCast< LargePayload >(&heapMoved)->values[7], 77U);

	/* Assignment replaces the held type. */
	smallMoved = heapMoved;
	EXPECT_EQ(anyCast< std::string >(&smallMoved), nullptr);
	ASSERT_NE(anyCast< LargePayload >(&smallMoved), nullptr);
	EXPECT_EQ(anyCast< LargePayload >(&smallMoved)->values[7], 77U);

	heapMoved = Any{3.5F};
	ASSERT_NE(anyCast< float >(&heapMoved), nullptr);
	EXPECT_EQ(*anyCast< float >(&heapMoved), 3.5F);
}

TEST(BaseAny, everyHeldCopyIsDestroyedExactlyOnce)
{
	int live = 0;

	{
		Any first{Counted{&live}};
		EXPECT_EQ(live, 1);

		Any second{first};
		EXPECT_EQ(live, 2);

		Any third{std::move(first)};
		EXPECT_EQ(live, 2);

		second = third;
		EXPECT_EQ(live, 2);

		third.reset();
		EXPECT_EQ(live, 1);

		const auto & self = second;
		second = self;
		EXPECT_EQ(live, 1);
	}

	EXPECT_EQ(live, 0);
}

TEST(BaseAny, aSharedPointerIsHeldInlineAndKeepsItsOwnership)
{
	auto pointer = std::make_shared< int >(5);

	{
		const Any payload{pointer};
		EXPECT_EQ(pointer.use_count(), 2);

		Any copy{payload};
		EXPECT_EQ(pointer.use_count(), 3);
		EXPECT_EQ(anyCast< std::shared_ptr< int > >(&copy)->get(), pointer.get());

		copy.reset();
		EXPECT_EQ(pointer.use_count(), 2);
	}

	EXPECT_EQ(pointer.use_count(), 1);
	static_assert(sizeof(std::shared_ptr< int >) <= Any::SmallBufferSize);
}
