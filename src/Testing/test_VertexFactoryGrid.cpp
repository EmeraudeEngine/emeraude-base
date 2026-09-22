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
