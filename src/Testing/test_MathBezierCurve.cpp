/*
 * src/Testing/test_MathBezierCurve.cpp
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
#include <algorithm>
#include <cstddef>
#include <vector>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "Math/BezierCurve.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base::Math;

namespace
{
	using V3 = Vector< 3, float >;

	/* projet-alpha basic-scenery's White flying light: five points spread over +-2000 m. */
	std::vector< V3 >
	flightPoints ()
	{
		return {{-2000.0F, 8.0F, -1800.0F}, {-1000.0F, 8.0F, 950.0F}, {0.0F, 8.0F, -1500.0F}, {1000.0F, 8.0F, 500.0F}, {2000.0F, 8.0F, -1600.0F}};
	}

	std::vector< V3 >
	synthesize (const std::vector< V3 > & points, bool closed, size_t segments)
	{
		BezierCurve< 3, float > curve;

		for ( const auto & point : points )
		{
			curve.addPoint(point);
		}

		if ( closed )
		{
			curve.close();
		}

		std::vector< V3 > samples;

		EXPECT_TRUE(curve.synthesize(segments, [&samples] (float /*time*/, const V3 & position) {
			samples.push_back(position);

			return true;
		}));

		return samples;
	}

	/* The largest distance between two consecutive samples. */
	float
	largestStep (const std::vector< V3 > & samples)
	{
		float largest = 0.0F;

		for ( size_t index = 1; index < samples.size(); ++index )
		{
			largest = std::max(largest, (samples[index] - samples[index - 1]).length());
		}

		return largest;
	}
}

/* 2026-09-26: segment i was (P[i], P[i+1], P[i+2]), so segment i ended at P[i+2] while segment i+1 started at P[i+1] — a
 * jump of ~2 200-2 650 units at t = 1/3 and 2/3 on these points. The midpoint chain is continuous: with 6000 samples no
 * step may exceed a few units. */
TEST(MathBezierCurve, anOpenCurveIsContinuousAndEndsOnItsEndPoints)
{
	const auto points = flightPoints();
	const auto samples = synthesize(points, false, 6000);

	ASSERT_EQ(samples.size(), 6001U);
	EXPECT_EQ(samples.front(), points.front());
	EXPECT_EQ(samples.back(), points.back());
	EXPECT_LT(largestStep(samples), 5.0F);
}

/* A closed curve used to stop short of its start (a 141-unit gap on game-logic's smoke circuit): the midpoint chain
 * wraps around and ends exactly where it began, continuously. */
TEST(MathBezierCurve, aClosedCurveClosesAndIsContinuous)
{
	const std::vector< V3 > circuit{{-100.0F, 2.0F, 0.0F}, {0.0F, 2.5F, 100.0F}, {100.0F, 2.0F, 0.0F}, {0.0F, 1.5F, -100.0F}};
	const auto samples = synthesize(circuit, true, 4000);

	ASSERT_EQ(samples.size(), 4001U);
	EXPECT_LT((samples.back() - samples.front()).length(), 1.0e-3F);
	EXPECT_LT(largestStep(samples), 0.5F);
}

/* Three points make ONE quadratic segment: the curve starts at P0, ends at P2, and at t = 0.5 it is the quadratic's
 * midpoint (P0 + 2 P1 + P2) / 4. */
TEST(MathBezierCurve, threePointsAreOneQuadraticSegment)
{
	const std::vector< V3 > points{{0.0F, 0.0F, 0.0F}, {10.0F, 10.0F, 0.0F}, {20.0F, 0.0F, 0.0F}};
	const auto samples = synthesize(points, false, 2);

	ASSERT_EQ(samples.size(), 3U);
	EXPECT_EQ(samples[0], points[0]);
	EXPECT_EQ(samples[1], (V3{10.0F, 5.0F, 0.0F}));
	EXPECT_EQ(samples[2], points[2]);
}
