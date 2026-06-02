/*
 * src/Testing/test_MathOrientedCuboid.cpp
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

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "Math/CartesianFrame.hpp"
#include "Math/OrientedCuboid.hpp"
#include "Math/Space3D/AACuboid.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base::Math;

/* Ave robustus! (Axis B — correction marker): OrientedCuboid::width/height/depth used to be stored
 * separately from the (transformed) vertices (the `FIXME: Extract these from vertices!`). They are
 * now derived from the vertices, so they cannot desync from the geometry. For an identity frame
 * they equal the source cuboid extents; a rigid transform (pure translation here) preserves them. */
TEST(MathOrientedCuboid, extentsDerivedFromVertices)
{
	const Space3D::AACuboid< float > cuboid{Vector< 3, float >{1.0F, 2.0F, 3.0F}, Vector< 3, float >{-1.0F, -2.0F, -3.0F}};
	ASSERT_FLOAT_EQ(cuboid.width(), 2.0F);
	ASSERT_FLOAT_EQ(cuboid.height(), 4.0F);
	ASSERT_FLOAT_EQ(cuboid.depth(), 6.0F);

	const CartesianFrame< float > identity{};
	const OrientedCuboid< float > box{cuboid, identity};
	EXPECT_FLOAT_EQ(box.width(), 2.0F);
	EXPECT_FLOAT_EQ(box.height(), 4.0F);
	EXPECT_FLOAT_EQ(box.depth(), 6.0F);

	const CartesianFrame< float > translated{Vector< 3, float >{10.0F, -5.0F, 2.0F}};
	const OrientedCuboid< float > movedBox{cuboid, translated};
	EXPECT_FLOAT_EQ(movedBox.width(), 2.0F);
	EXPECT_FLOAT_EQ(movedBox.height(), 4.0F);
	EXPECT_FLOAT_EQ(movedBox.depth(), 6.0F);
}
