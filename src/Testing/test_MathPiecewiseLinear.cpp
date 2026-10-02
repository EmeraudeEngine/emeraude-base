/*
 * src/Testing/test_MathPiecewiseLinear.cpp
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
#include <limits>

/* Local inclusions. */
#include "Math/PiecewiseLinear.hpp"

using namespace EmEn::Base::Math;

TEST(MathPiecewiseLinear, EmptyAnswersZero)
{
	const PiecewiseLinear< float > curve;

	EXPECT_TRUE(curve.empty());
	EXPECT_EQ(curve.value(3.0F), 0.0F);
}

TEST(MathPiecewiseLinear, InterpolatesAndClampsBeyondTheEnds)
{
	/* A torque curve: 0.8 at idle, 1.0 at two thirds of the range, 0.8 at the top. */
	PiecewiseLinear< float > curve;

	ASSERT_TRUE(curve.addPoint(0.0F, 0.8F));
	ASSERT_TRUE(curve.addPoint(0.66F, 1.0F));
	ASSERT_TRUE(curve.addPoint(1.0F, 0.8F));

	EXPECT_FLOAT_EQ(curve.value(0.0F), 0.8F);
	EXPECT_FLOAT_EQ(curve.value(0.33F), 0.9F);
	EXPECT_FLOAT_EQ(curve.value(0.66F), 1.0F);
	EXPECT_NEAR(curve.value(0.83F), 0.9F, 1.0e-5F);
	EXPECT_FLOAT_EQ(curve.value(-5.0F), 0.8F);
	EXPECT_FLOAT_EQ(curve.value(7.0F), 0.8F);
	EXPECT_FLOAT_EQ(curve.value(std::numeric_limits< float >::quiet_NaN()), 0.8F);
}

TEST(MathPiecewiseLinear, OnePointIsConstant)
{
	PiecewiseLinear< float > curve;

	ASSERT_TRUE(curve.addPoint(2.0F, 1.5F));
	EXPECT_FLOAT_EQ(curve.value(-10.0F), 1.5F);
	EXPECT_FLOAT_EQ(curve.value(10.0F), 1.5F);
}

TEST(MathPiecewiseLinear, RefusesInvalidPoints)
{
	PiecewiseLinear< float, 3 > curve;

	ASSERT_TRUE(curve.addPoint(0.0F, 0.0F));
	EXPECT_FALSE(curve.addPoint(0.0F, 1.0F));
	EXPECT_FALSE(curve.addPoint(-1.0F, 1.0F));
	EXPECT_FALSE(curve.addPoint(std::numeric_limits< float >::infinity(), 1.0F));
	EXPECT_FALSE(curve.addPoint(1.0F, std::numeric_limits< float >::quiet_NaN()));
	ASSERT_TRUE(curve.addPoint(1.0F, 1.0F));
	ASSERT_TRUE(curve.addPoint(2.0F, 0.5F));
	EXPECT_FALSE(curve.addPoint(3.0F, 0.0F));
	EXPECT_EQ(curve.points().size(), 3U);

	curve.clear();
	EXPECT_TRUE(curve.empty());
}
