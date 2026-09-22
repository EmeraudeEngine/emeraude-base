/*
 * src/Testing/test_VertexFactoryGrid.cpp
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
#include "VertexFactory/Grid.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::VertexFactory;

/* A 1024 m grid of 1 m cells, streamed through a 256-cell window: the window's centre may travel
 * from -384 to +384 m; beyond that the window would leave the grid and is held at the border. */

TEST(VertexFactoryGrid, SubGridCenterSnapsToACellCornerInsideTheGrid)
{
	Grid< float > grid;
	ASSERT_TRUE(grid.initializeByGridSize(1024.0F, 1024U));

	const auto center = grid.subGridCenter({100.4F, -37.6F}, 256U);

	/* floor((100.4 + 512) / 1) = 612 → 612 - 512 = 100 ; floor((-37.6 + 512)) = 474 → -38. */
	ASSERT_FLOAT_EQ(center[0], 100.0F);
	ASSERT_FLOAT_EQ(center[1], -38.0F);
}

TEST(VertexFactoryGrid, SubGridCenterIsHeldAtTheBorderWindow)
{
	Grid< float > grid;
	ASSERT_TRUE(grid.initializeByGridSize(1024.0F, 1024U));

	/* Far outside on both axes: the window sits against the +X / -Z corner of the grid. */
	const auto center = grid.subGridCenter({5000.0F, -5000.0F}, 256U);

	ASSERT_FLOAT_EQ(center[0], 384.0F);
	ASSERT_FLOAT_EQ(center[1], -384.0F);

	/* And two requests beyond the border give the SAME centre: nothing to re-extract. */
	ASSERT_EQ(grid.subGridCenter({9000.0F, -9000.0F}, 256U), center);
}

TEST(VertexFactoryGrid, SubGridCenterMatchesTheExtractedSubGrid)
{
	Grid< float > grid;
	ASSERT_TRUE(grid.initializeByGridSize(1024.0F, 1024U));

	const Math::Vector< 2, float > request{300.0F, 450.0F};
	const auto center = grid.subGridCenter(request, 256U);
	const auto window = grid.subGrid(request, 256U);

	/* The extracted window's middle point is the announced centre (in X and Z), with the grid's heights (0 here). */
	const auto middle = window.position(128U, 128U);

	ASSERT_FLOAT_EQ(middle[Math::X], center[0]);
	ASSERT_FLOAT_EQ(middle[Math::Z], center[1]);
}

TEST(VertexFactoryGrid, SubGridCenterSnapsToAMultipleOfTheGivenCells)
{
	Grid< float > grid;
	ASSERT_TRUE(grid.initializeByGridSize(1024.0F, 1024U));

	/* A 256-cell window snapped to 128 cells: centres are multiples of 128 (in grid index space, i.e.
	 * -512 + k × 128 in world units), held inside [-384, +384]. */
	ASSERT_EQ(grid.subGridCenter({100.0F, -37.0F}, 256U, 128U), (Math::Vector< 2, float >{128.0F, 0.0F}));
	ASSERT_EQ(grid.subGridCenter({70.0F, -70.0F}, 256U, 128U), (Math::Vector< 2, float >{128.0F, -128.0F})); /* 582 is nearer 640 than 512 */
	ASSERT_EQ(grid.subGridCenter({50.0F, -70.0F}, 256U, 128U), (Math::Vector< 2, float >{0.0F, -128.0F}));
	ASSERT_EQ(grid.subGridCenter({5000.0F, -5000.0F}, 256U, 128U), (Math::Vector< 2, float >{384.0F, -384.0F}));

	/* The extraction agrees with the announced centre. */
	const auto window = grid.subGrid({100.0F, -37.0F}, 256U, 128U);
	const auto middle = window.position(128U, 128U);

	ASSERT_FLOAT_EQ(middle[Math::X], 128.0F);
	ASSERT_FLOAT_EQ(middle[Math::Z], 0.0F);
}

TEST(VertexFactoryGrid, ASubGridKeepsTheTextureCoordinatesOfItsParent)
{
	Grid< float > grid;
	ASSERT_TRUE(grid.initializeByGridSize(1024.0F, 1024U));
	grid.setUVMultiplier(100.0F); /* 100 tiles over the grid: 0.09765625 tile per cell, so an offset is never a whole tile. */

	const auto window = grid.subGrid({300.0F, -200.0F}, 256U);

	/* The same world point, addressed in the parent and in the window, has the same UV. */
	const auto parentUV = grid.textureCoordinates2D(700U, 400U);
	const auto windowUV = window.textureCoordinates2D(700U - (812U - 128U), 400U - (312U - 128U));

	ASSERT_FLOAT_EQ(windowUV[0], parentUV[0]);
	ASSERT_FLOAT_EQ(windowUV[1], parentUV[1]);
}

TEST(VertexFactoryGrid, ACoarsenedGridCoincidesWithItsParentAtTheSharedPoints)
{
	Grid< float > grid;
	ASSERT_TRUE(grid.initializeByGridSize(1024.0F, 1024U));
	grid.setUVMultiplier(64.0F);
	grid.applyPerlinNoise(64.0F, 50.0F);

	const auto coarse = grid.coarsened(32U);

	ASSERT_TRUE(coarse.isValid());
	ASSERT_EQ(coarse.squaredQuadCount(), 32U);
	ASSERT_FLOAT_EQ(coarse.quadSize(), 32.0F);

	for ( uint32_t y = 0; y <= 32U; y += 8U )
	{
		for ( uint32_t x = 0; x <= 32U; x += 8U )
		{
			ASSERT_EQ(coarse.position(x, y), grid.position(x * 32U, y * 32U));
			ASSERT_EQ(coarse.textureCoordinates2D(x, y), grid.textureCoordinates2D(x * 32U, y * 32U));
		}
	}

	/* A step that does not divide the cell count gives an invalid grid, not a wrong one. */
	ASSERT_FALSE(grid.coarsened(48U).isValid());
}

TEST(VertexFactoryGrid, ASubGridBoundingBoxIsWhereTheWindowIs)
{
	Grid< float > grid;
	ASSERT_TRUE(grid.initializeByGridSize(1024.0F, 1024U));

	const auto window = grid.subGrid({300.0F, -200.0F}, 256U);
	const auto & box = window.boundingBox();

	/* Centre (300, -200), half-size 128: the box spans [172, 428] × [-328, -72], not [-128, 128]². */
	ASSERT_FLOAT_EQ(box.minimum(Math::X), 172.0F);
	ASSERT_FLOAT_EQ(box.maximum(Math::X), 428.0F);
	ASSERT_FLOAT_EQ(box.minimum(Math::Z), -328.0F);
	ASSERT_FLOAT_EQ(box.maximum(Math::Z), -72.0F);

	/* And the corner points of the window lie on that box. */
	ASSERT_FLOAT_EQ(window.position(0U, 0U)[Math::X], box.minimum(Math::X));
	ASSERT_FLOAT_EQ(window.position(256U, 256U)[Math::Z], box.maximum(Math::Z));
}

TEST(VertexFactoryGrid, AHalvedTentGridIsTheTentMeanOfItsParent)
{
	Grid< float > grid;
	ASSERT_TRUE(grid.initializeByGridSize(512.0F, 256U));
	grid.applyPerlinNoise(16.0F, 40.0F);

	const auto half = grid.halvedTent();

	ASSERT_TRUE(half.isValid());
	ASSERT_EQ(half.squaredQuadCount(), 128U);
	ASSERT_FLOAT_EQ(half.quadSize(), 4.0F);

	/* Every point sits on the parent's point (2x, 2y) and holds the 1-2-1 × 1-2-1 mean around it. */
	for ( uint32_t y = 1; y < 128U; y += 7U )
	{
		for ( uint32_t x = 1; x < 128U; x += 5U )
		{
			ASSERT_EQ(half.position(x, y)[Math::X], grid.position(x * 2U, y * 2U)[Math::X]);
			ASSERT_EQ(half.position(x, y)[Math::Z], grid.position(x * 2U, y * 2U)[Math::Z]);

			float expected = 0.0F;

			for ( int dy = -1; dy <= 1; ++dy )
			{
				for ( int dx = -1; dx <= 1; ++dx )
				{
					const float weight = (dx == 0 ? 2.0F : 1.0F) * (dy == 0 ? 2.0F : 1.0F) / 16.0F;

					expected += weight * grid.getHeightAt((x * 2U) + dx, (y * 2U) + dy);
				}
			}

			ASSERT_NEAR(half.getHeightAt(x, y), expected, 1.0e-4F);
		}
	}

	/* An odd cell count cannot be halved: invalid, not wrong. */
	Grid< float > odd;
	ASSERT_TRUE(odd.initializeByGridSize(96.0F, 3U));
	ASSERT_FALSE(odd.halvedTent().isValid());
}

TEST(VertexFactoryGrid, AHalvedTentGridIsSmootherThanAPointSampledOne)
{
	/* Relief at 3 cells per period: finer than what a grid of twice the cell can carry (that period is
	 * 1.5 of its cells, under the 2 of Nyquist). The point
	 * sample (coarsened) folds it back as a false coarse relief; the tent attenuates it. The measure
	 * is the mean absolute second difference, the curvature a lighting normal would see. */
	Grid< float > grid;
	ASSERT_TRUE(grid.initializeByGridSize(1024.0F, 1024U));
	grid.applyPerlinNoise(1024.0F / 3.0F, 10.0F); /* size = periods over the WHOLE grid: 3 cells per period. */

	const auto curvature = [] (const Grid< float > & source) {
		const auto last = source.squaredQuadCount();
		double sum = 0.0;
		uint32_t count = 0;

		for ( uint32_t y = 1; y < last; ++y )
		{
			for ( uint32_t x = 1; x < last; ++x )
			{
				const auto h = source.getHeightAt(x, y);

				sum += std::abs(source.getHeightAt(x - 1U, y) - (2.0F * h) + source.getHeightAt(x + 1U, y));
				++count;
			}
		}

		return sum / count;
	};

	const auto pointSampled = curvature(grid.coarsened(2U));
	const auto filtered = curvature(grid.halvedTent());

	ASSERT_GT(pointSampled, 0.0);
	ASSERT_LT(filtered, pointSampled * 0.75);
}
