/*
 * src/Testing/test_MathBSpline.cpp
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

/* STL inclusions. */
#include <array>
#include <cstddef>
#include <vector>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "Math/BSpline.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::Math;

namespace
{
	/* The shape of projet-alpha basic-scenery's flying lights: five points with a handle each, the default segment count
	 * stamped on every point, the LAST one included. */
	constexpr size_t Segments{32};

	struct Sample
	{
		float time;
		Vector< 3, float > position;
	};

	std::vector< Sample >
	synthesizeFivePoints (CurveType curveType, bool constant)
	{
		BSpline< 3, float > spline{Segments, curveType};
		spline.addPoint({-2000.0F, 8.0F, -1800.0F}, {2000.0F, 8.0F, 0.0F});
		spline.addPoint({-1000.0F, 8.0F, 950.0F}, {2000.0F, 8.0F, 0.0F});
		spline.addPoint({0.0F, 8.0F, -1500.0F}, {2000.0F, 8.0F, 0.0F});
		spline.addPoint({1000.0F, 8.0F, 500.0F}, {2000.0F, 8.0F, 0.0F});
		spline.addPoint({2000.0F, 8.0F, -1600.0F}, {2000.0F, 8.0F, 0.0F});

		std::vector< Sample > samples;

		const auto synthesized = spline.synthesize([&samples] (float time, const Vector< 3, float > & position) {
			samples.push_back({time, position});

			return true;
		}, constant);

		EXPECT_TRUE(synthesized);

		return samples;
	}
}

/* 2026-09-25: the last point took the curve branch whenever it had more than one segment and read m_points[index + 1],
 * one past the end — undefined behaviour on every basic-scenery launch, corrupting the final keyframes of three of its
 * flying lights (Red, Green, Blue). ⚠️ The Bezier types' FIRST sample is not checked here: their segments run backwards
 * (item vector-lerp-runs-backwards), and that check belongs with the fix. The last point is the TERMINAL sample:
 * segments × (n - 1) curve samples, then that point, at t = 1. */
TEST(MathBSpline, lastPointIsTheTerminalSampleForEveryCurveType)
{
	constexpr std::array< CurveType, 3 > curveTypes{CurveType::None, CurveType::BezierQuadratic, CurveType::BezierCubic};

	for ( const auto curveType : curveTypes )
	{
		for ( const auto constant : {false, true} )
		{
			const auto samples = synthesizeFivePoints(curveType, constant);

			ASSERT_EQ(samples.size(), (Segments * 4) + 1) << "curve type " << static_cast< int >(curveType) << ", constant " << constant;

			EXPECT_EQ(samples.back().position, (Vector< 3, float >{2000.0F, 8.0F, -1600.0F}));
			EXPECT_NEAR(samples.back().time, 1.0F, 1.0e-5F);
			EXPECT_FLOAT_EQ(samples.front().time, 0.0F);

			for ( size_t index = 1; index < samples.size(); ++index )
			{
				EXPECT_GT(samples[index].time, samples[index - 1].time) << "sample " << index;
			}
		}
	}
}
