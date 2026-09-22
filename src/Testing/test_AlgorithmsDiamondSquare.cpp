/*
 * src/Testing/test_AlgorithmsDiamondSquare.cpp
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
#include <algorithm>
#include <cmath>
#include <cstddef>

/* Local inclusions. */
#include "Algorithms/DiamondSquare.hpp"

using namespace EmEn::Base::Algorithms;

namespace
{
	/**
	 * @brief Mean absolute height difference between points `step` apart along X: the roughness
	 * of the relief at ONE scale.
	 */
	float
	meanStep (const DiamondSquare< float > & generator, size_t size, size_t step) noexcept
	{
		double sum = 0.0;
		size_t count = 0;

		for ( size_t y = 0; y < size; ++y )
		{
			for ( size_t x = 0; x + step < size; ++x )
			{
				sum += std::abs(generator.value(x + step, y) - generator.value(x, y));
				++count;
			}
		}

		return count > 0 ? static_cast< float >(sum / static_cast< double >(count)) : 0.0F;
	}

	/**
	 * @brief Mean |h(x-1) - 2 h(x) + h(x+1)| along X: the curvature at the VERTEX frequency.
	 * @note A relief interpolated from coarser levels has almost none; white noise deposited on the
	 * grid has a lot. This — not the slope, which the coarse relief carries too — is what a lit grid
	 * shades as a lattice.
	 */
	float
	meanCurvature (const DiamondSquare< float > & generator, size_t size) noexcept
	{
		double sum = 0.0;
		size_t count = 0;

		for ( size_t y = 0; y < size; ++y )
		{
			for ( size_t x = 1; x + 1 < size; ++x )
			{
				sum += std::abs(generator.value(x - 1, y) - 2.0F * generator.value(x, y) + generator.value(x + 1, y));
				++count;
			}
		}

		return count > 0 ? static_cast< float >(sum / static_cast< double >(count)) : 0.0F;
	}
}

TEST(AlgorithmsDiamondSquare, RejectsASizeThatIsNotAPowerOfTwoPlusOne)
{
	DiamondSquare< float > generator{1, false};

	ASSERT_FALSE(generator.generate(2, 0.5F));
	ASSERT_FALSE(generator.generate(6, 0.5F));
	ASSERT_FALSE(generator.generate(100, 0.5F));
	ASSERT_TRUE(generator.generate(5, 0.5F));
	ASSERT_TRUE(generator.generate(257, 0.5F));
}

TEST(AlgorithmsDiamondSquare, NormalisesToTheUnitRange)
{
	DiamondSquare< float > generator{7, false};

	ASSERT_TRUE(generator.generate(65, 1.0F));

	const auto [minimum, maximum] = std::minmax_element(generator.data().begin(), generator.data().end());

	ASSERT_NEAR(*minimum, -1.0F, 1e-5F);
	ASSERT_NEAR(*maximum, 1.0F, 1e-5F);
}

TEST(AlgorithmsDiamondSquare, TheSameSeedGivesTheSameRelief)
{
	DiamondSquare< float > first{42, false};
	DiamondSquare< float > second{42, false};

	ASSERT_TRUE(first.generate(33, 0.8F, 1.0F));
	ASSERT_TRUE(second.generate(33, 0.8F, 1.0F));

	ASSERT_EQ(first.data(), second.data());
}

TEST(AlgorithmsDiamondSquare, AHigherHurstExponentDampsTheFinestLevels)
{
	constexpr size_t Size{257};

	/* The same seed, the same roughness: only the per-level decay changes. */
	DiamondSquare< float > brownian{3, false};
	DiamondSquare< float > damped{3, false};
	DiamondSquare< float > moreDamped{3, false};

	ASSERT_TRUE(brownian.generate(Size, 1.0F, 1.0F));
	ASSERT_TRUE(damped.generate(Size, 1.0F, 1.5F));
	ASSERT_TRUE(moreDamped.generate(Size, 1.0F, 2.0F));

	/* The vertex-frequency curvature against the coarse relief (32 cells): the lattice ratio.
	 * The output is normalised, so the coarse relief is the same order for all three and the ratio
	 * isolates the finest levels. */
	const auto latticeRatio = [] (const DiamondSquare< float > & generator) {
		return meanCurvature(generator, Size) / meanStep(generator, Size, 32);
	};

	const auto brownianRatio = latticeRatio(brownian);
	const auto dampedRatio = latticeRatio(damped);
	const auto moreDampedRatio = latticeRatio(moreDamped);

	ASSERT_GT(brownianRatio, dampedRatio);
	ASSERT_GT(dampedRatio, moreDampedRatio);
	/* Not cosmetic either. Level k (of 8) has amplitude 2^-kH relative to the first and contributes a
	 * curvature at step 1 of order amplitude / 4^k... at H = 1 the finest level dominates (sum ≈ 2 ×
	 * finest), at H = 2 every level weighs the same (sum ≈ 8 × finest) while the finest is 2^7 times
	 * smaller: an analytical ratio near 30. A bound of 4 leaves room for the seed. */
	ASSERT_GT(brownianRatio / moreDampedRatio, 4.0F);
}

TEST(AlgorithmsDiamondSquare, AZeroHurstExponentKeepsEveryLevelAtTheSameAmplitude)
{
	/* H = 0: no decay, every level displaces by the first level's amplitude — the roughest relief
	 * the generator can make. The fine/coarse ratio must then exceed the Brownian one. */
	constexpr size_t Size{129};

	DiamondSquare< float > white{11, false};
	DiamondSquare< float > brownian{11, false};

	ASSERT_TRUE(white.generate(Size, 1.0F, 0.0F));
	ASSERT_TRUE(brownian.generate(Size, 1.0F, 1.0F));

	ASSERT_GT(meanStep(white, Size, 1) / meanStep(white, Size, 16), meanStep(brownian, Size, 1) / meanStep(brownian, Size, 16));
}
