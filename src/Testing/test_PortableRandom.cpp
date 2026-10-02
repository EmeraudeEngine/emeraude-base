/*
 * src/Testing/test_PortableRandom.cpp
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


/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>

/* Local inclusions. */
#include "Algorithms/PerlinNoise.hpp"
#include "PortableRandom.hpp"
#include "Randomizer.hpp"

using namespace EmEn::Base;

/* ⚠️ The GOLDEN values below were drawn on Linux (libstdc++) on 2026-10-02 and must be the same on every platform:
 * that is the point of PortableRandom. A failure on one OS is a portability defect, never a value to re-record. */

TEST(PortableRandom, Mt19937IsTheStandardSequence)
{
	/* The premise: the C++ standard fixes this value ([rand.predef]). */
	std::mt19937 generator;

	for ( int index = 1; index < 10000; ++index )
	{
		static_cast< void >(generator());
	}

	EXPECT_EQ(generator(), 4123659995U);
}

TEST(PortableRandom, IntegersAreGolden)
{
	std::mt19937 generator{20261002U};
	constexpr std::array< int, 12 > Golden{5, 6, 1, 2, 1, 6, 3, 4, 5, 4, 4, 3};

	for ( const auto expected : Golden )
	{
		EXPECT_EQ(PortableRandom::uniformInteger< int >(generator, 1, 6), expected);
	}

	std::mt19937 wide{20261002U};
	constexpr std::array< int64_t, 4 > GoldenWide{-429435754411LL, 152560350066LL, -126432423020LL, -383400680456LL};

	for ( const auto expected : GoldenWide )
	{
		EXPECT_EQ(PortableRandom::uniformInteger< int64_t >(wide, -1000000000000LL, 1000000000000LL), expected);
	}

	std::mt19937_64 generator64{42U};
	constexpr std::array< uint32_t, 8 > Golden64{7, 7, 9, 9, 0, 2, 5, 4};

	for ( const auto expected : Golden64 )
	{
		EXPECT_EQ(PortableRandom::uniformInteger< uint32_t >(generator64, 0U, 9U), expected);
	}
}

TEST(PortableRandom, RealsAreGolden)
{
	std::mt19937 generator{20261002U};
	constexpr std::array< float, 6 > Golden{0x1.7e2ad4p-1F, 0x1.263188p-1F, 0x1.400b2p-2F, 0x1.ef6c3ep-1F, 0x1.cede78p-1F, 0x1.f6f9fep-1F};

	for ( const auto expected : Golden )
	{
		EXPECT_EQ(PortableRandom::uniformReal< float >(generator, 0.0F, 1.0F), expected);
	}

	std::mt19937 doubles{20261002U};
	constexpr std::array< double, 4 > GoldenDouble{0x1.3b6b14dbef7bp+1, -0x1.dfe4288a5c584p+0, 0x1.0296182e970fcp+2, 0x1.accad72f97264p+0};

	for ( const auto expected : GoldenDouble )
	{
		EXPECT_EQ(PortableRandom::uniformReal< double >(doubles, -5.0, 5.0), expected);
	}
}

TEST(PortableRandom, ShuffleIsGolden)
{
	std::mt19937 generator{7U};
	std::array< int, 16 > values{};

	std::iota(values.begin(), values.end(), 0);
	PortableRandom::shuffle(values, generator);

	constexpr std::array< int, 16 > Golden{8, 2, 6, 0, 1, 9, 3, 12, 5, 11, 10, 14, 4, 13, 7, 15};

	EXPECT_EQ(values, Golden);
}

TEST(PortableRandom, PerlinNoiseIsTheSameWorld)
{
	/* citadel's seed. The permutation is exact (integers); the float evaluation may move by an ulp where a compiler
	 * fuses a multiply-add (clang on arm64), never by the tenths a different permutation gives. */
	Algorithms::PerlinNoise< float > noise{1307};

	EXPECT_NEAR(noise.generate(150.0F / 180.0F, 150.0F / 180.0F, 0.0F), 0x1.175b12p-1F, 1.0e-5F);
	EXPECT_NEAR(noise.generate(-200.0F / 45.0F, 120.0F / 45.0F, 0.5F), 0x1.6a6056p-2F, 1.0e-5F);
	EXPECT_NEAR(noise.generate(0.3F, 0.7F, 1.0F), 0x1.2c954cp-1F, 1.0e-5F);
}

TEST(PortableRandom, IntegerBoundsAndUniformity)
{
	std::mt19937 generator{1U};
	std::array< int, 6 > counts{};
	constexpr int Draws{60000};

	for ( int index = 0; index < Draws; ++index )
	{
		const auto face = PortableRandom::uniformInteger< int >(generator, 1, 6);

		ASSERT_GE(face, 1);
		ASSERT_LE(face, 6);

		++counts[static_cast< size_t >(face - 1)];
	}

	/* 10000 expected per face; 5 % is beyond 5 standard deviations (≈ 91). */
	constexpr double Expected{static_cast< double >(Draws) / 6.0};

	for ( const auto count : counts )
	{
		EXPECT_NEAR(static_cast< double >(count), Expected, Expected * 0.05);
	}

	/* Reversed bounds are swapped; equal bounds give the bound; 8-bit and full ranges. */
	EXPECT_EQ(PortableRandom::uniformInteger< int >(generator, 4, 4), 4);

	for ( int index = 0; index < 100; ++index )
	{
		const auto reversed = PortableRandom::uniformInteger< int >(generator, 9, -3);

		EXPECT_GE(reversed, -3);
		EXPECT_LE(reversed, 9);

		const auto byte = PortableRandom::uniformInteger< int8_t >(generator, std::numeric_limits< int8_t >::min(), std::numeric_limits< int8_t >::max());

		EXPECT_GE(byte, std::numeric_limits< int8_t >::min());

		static_cast< void >(PortableRandom::uniformInteger< uint32_t >(generator, 0U, std::numeric_limits< uint32_t >::max()));
		static_cast< void >(PortableRandom::uniformInteger< int64_t >(generator, std::numeric_limits< int64_t >::min(), std::numeric_limits< int64_t >::max()));
	}
}

TEST(PortableRandom, RealBoundsAndEdges)
{
	std::mt19937 generator{3U};

	for ( int index = 0; index < 10000; ++index )
	{
		const auto value = PortableRandom::uniformReal< float >(generator, -2.0F, 3.0F);

		ASSERT_GE(value, -2.0F);
		ASSERT_LT(value, 3.0F);

		/* A width a float barely resolves: still below the maximum. */
		const auto narrow = PortableRandom::uniformReal< float >(generator, 1.0F, std::nextafter(1.0F, 2.0F));

		ASSERT_GE(narrow, 1.0F);
		ASSERT_LT(narrow, std::nextafter(1.0F, 2.0F));
	}

	EXPECT_EQ(PortableRandom::uniformReal< float >(generator, 2.5F, 2.5F), 2.5F);
	/* A non-finite bound gives the minimum back, untouched (documented): a NaN stays a NaN. */
	EXPECT_TRUE(std::isnan(PortableRandom::uniformReal< float >(generator, std::numeric_limits< float >::quiet_NaN(), 1.0F)));
	EXPECT_EQ(PortableRandom::uniformReal< double >(generator, -std::numeric_limits< double >::infinity(), 1.0), -std::numeric_limits< double >::infinity());

	/* The whole float range (its width overflows): finite and inside. */
	const auto huge = PortableRandom::uniformReal< float >(generator, -std::numeric_limits< float >::max(), std::numeric_limits< float >::max());

	EXPECT_TRUE(std::isfinite(huge));
}

TEST(PortableRandom, RandomizerFollowsThePortableStream)
{
	Randomizer< int > dice{20261002U};
	constexpr std::array< int, 12 > Golden{5, 6, 1, 2, 1, 6, 3, 4, 5, 4, 4, 3};

	for ( const auto expected : Golden )
	{
		EXPECT_EQ(dice.value(1, 6), expected);
	}

	Randomizer< float > unit{20261002U};

	EXPECT_EQ(unit.value(0.0F, 1.0F), 0x1.7e2ad4p-1F);
}

TEST(PortableRandom, DistributionObjectsFollowTheFunctions)
{
	/* The drop-in types for a seeded call site: the same stream as the functions. */
	std::mt19937 generator{20261002U};
	const PortableRandom::UniformInteger< int > dice{1, 6};
	constexpr std::array< int, 12 > Golden{5, 6, 1, 2, 1, 6, 3, 4, 5, 4, 4, 3};

	for ( const auto expected : Golden )
	{
		EXPECT_EQ(dice(generator), expected);
	}

	std::mt19937 reals{20261002U};
	const PortableRandom::UniformReal< float > unit{0.0F, 1.0F};

	EXPECT_EQ(unit(reals), 0x1.7e2ad4p-1F);
	EXPECT_EQ(unit(reals), 0x1.263188p-1F);
}
