/*
 * src/Testing/test_AlgorithmsWorleyNoise.cpp
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
#include "Algorithms/WorleyNoise.hpp"

using namespace EmEn::Base::Algorithms;

/* The whole point of the class: a texture baked over one period tiles with no seam. The offsets are exactly
 * representable, so the comparison can be exact. */
TEST(AlgorithmsWorleyNoise, tilesOverItsPeriod)
{
	const WorleyNoise< float > noise{42, 8};

	for ( const float offset : {0.25F, 0.5F, 3.75F, 7.125F} )
	{
		EXPECT_FLOAT_EQ(noise.generate(offset, 0.5F, 0.25F), noise.generate(offset + 8.0F, 0.5F, 0.25F)) << "offset " << offset;
		EXPECT_FLOAT_EQ(noise.generate(0.5F, offset, 0.25F), noise.generate(0.5F, offset + 8.0F, 0.25F)) << "offset " << offset;
		EXPECT_FLOAT_EQ(noise.generate(0.5F, 0.25F, offset), noise.generate(0.5F, 0.25F, offset + 8.0F)) << "offset " << offset;
		/* West of the origin too: the modulo must stay positive. */
		EXPECT_FLOAT_EQ(noise.generate(-offset, 0.5F, 0.25F), noise.generate(8.0F - offset, 0.5F, 0.25F)) << "offset " << offset;
	}
}

/* The fractal sum keeps the period of the lattice: every octave's period is a multiple of it. */
TEST(AlgorithmsWorleyNoise, billowsTileOverThePeriod)
{
	const WorleyNoise< float > noise{7, 4};

	EXPECT_FLOAT_EQ(noise.generateBillows(0.75F, 1.5F, 2.25F, 3), noise.generateBillows(4.75F, 1.5F, 2.25F, 3));
	EXPECT_FLOAT_EQ(noise.generateBillows(0.75F, 1.5F, 2.25F, 3), noise.generateBillows(0.75F, 5.5F, 2.25F, 3));
}

/* F1 is a distance: continuous, and 1-Lipschitz in cell units. */
TEST(AlgorithmsWorleyNoise, isOneLipschitz)
{
	const WorleyNoise< float > noise{3, 16};
	constexpr auto Step{1.0F / 256.0F};

	for ( int index = -300; index <= 300; ++index )
	{
		const auto x = static_cast< float >(index) * 0.0371F;
		const auto here = noise.generate(x, 0.43F, 0.71F);
		const auto next = noise.generate(x + Step, 0.43F, 0.71F);

		EXPECT_LE(std::abs(next - here), Step * 1.001F) << "x " << x;
	}
}

/* Both bases stay in the unit range. */
TEST(AlgorithmsWorleyNoise, staysInTheUnitRange)
{
	const WorleyNoise< float > noise{11, 8};

	for ( int index = -200; index <= 200; ++index )
	{
		const auto value = noise.generate(static_cast< float >(index) * 0.173F, static_cast< float >(index) * -0.291F, 0.5F);
		const auto billows = noise.generateBillows(static_cast< float >(index) * 0.173F, 0.5F, static_cast< float >(index) * 0.057F, 4);

		EXPECT_GE(value, 0.0F);
		EXPECT_LE(value, 1.0F);
		EXPECT_GE(billows, 0.0F);
		EXPECT_LE(billows, 1.0F);
	}
}

/* Same seed, same layout; another seed, another layout. */
TEST(AlgorithmsWorleyNoise, isDeterministicPerSeed)
{
	const WorleyNoise< float > first{123, 8};
	const WorleyNoise< float > same{123, 8};
	const WorleyNoise< float > other{124, 8};

	auto differs = false;

	for ( int index = 0; index < 64; ++index )
	{
		const auto x = static_cast< float >(index) * 0.125F;

		EXPECT_FLOAT_EQ(first.generate(x, 1.5F, 2.5F), same.generate(x, 1.5F, 2.5F));

		differs = differs || first.generate(x, 1.5F, 2.5F) != other.generate(x, 1.5F, 2.5F);
	}

	EXPECT_TRUE(differs);
}
