/*
 * src/Testing/test_AlgorithmsPerlinNoise.cpp
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

#include <gtest/gtest.h>

/* STL inclusions. */
#include <cmath>

/* Local inclusions. */
#include "Algorithms/PerlinNoise.hpp"

using namespace EmEn::Base::Algorithms;

/* A negative coordinate used to be converted straight to an unsigned lattice index: undefined behaviour,
 * and in practice a lattice cell unrelated to its neighbour across zero. The noise is periodic over 256
 * cells, so a point west of the origin must read exactly like the same point one period east. */
TEST(AlgorithmsPerlinNoise, negativeCoordinatesWrapOntoThePeriod)
{
	PerlinNoise< float > noise{42};

	for ( const float offset : {0.25F, 0.5F, 3.75F, 17.125F} )
	{
		EXPECT_FLOAT_EQ(noise.generate(-offset, 0.5F, 0.25F), noise.generate(256.0F - offset, 0.5F, 0.25F)) << "offset " << offset;
		EXPECT_FLOAT_EQ(noise.generate(0.5F, -offset, 0.25F), noise.generate(0.5F, 256.0F - offset, 0.25F)) << "offset " << offset;
	}
}

/* Perlin noise is continuous: across the origin, one step of 1/1024 cell must not jump. */
TEST(AlgorithmsPerlinNoise, isContinuousAcrossZero)
{
	PerlinNoise< float > noise{7};

	const auto left = noise.generate(-1.0F / 1024.0F, 0.37F, 0.61F);
	const auto right = noise.generate(1.0F / 1024.0F, 0.37F, 0.61F);

	EXPECT_LT(std::abs(left - right), 0.01F);
}

/* The output is remapped to [0, 1]. */
TEST(AlgorithmsPerlinNoise, staysInTheUnitRange)
{
	PerlinNoise< float > noise{3};

	for ( int i = -200; i <= 200; ++i )
	{
		const auto value = noise.generate(static_cast< float >(i) * 0.173F, static_cast< float >(i) * -0.291F, 0.5F);

		EXPECT_GE(value, 0.0F);
		EXPECT_LE(value, 1.0F);
	}
}
