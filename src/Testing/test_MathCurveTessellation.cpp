/*
 * src/Testing/test_MathCurveTessellation.cpp
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

/* STL inclusions. */
#include <cmath>
#include <span>
#include <vector>

/* Local inclusions. */
#include "Math/BSpline.hpp"
#include "Math/CurveTessellation.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base::Math;

namespace
{
	using V3 = Vector< 3, double >;

	/** @brief The distance from a point to a polyline. */
	double
	distanceToPolyline (const V3 & point, const std::vector< V3 > & polyline) noexcept
	{
		double best = 1.0E300;

		for ( size_t index = 0; index + 1 < polyline.size(); ++index )
		{
			best = std::min(best, CurveTessellation::distanceToSegment(point, polyline[index], polyline[index + 1]));
		}

		return best;
	}

	/** @brief A cubic Bézier evaluated by its Bernstein form (independent of the de Casteljau code under test). */
	V3
	bernstein (const V3 & p0, const V3 & p1, const V3 & p2, const V3 & p3, double t) noexcept
	{
		const double u = 1.0 - t;

		return p0 * (u * u * u) + p1 * (3.0 * u * u * t) + p2 * (3.0 * u * t * t) + p3 * (t * t * t);
	}

	/** @brief A uniform cubic B-spline span evaluated by its BASIS functions (independent of the Bézier conversion). */
	V3
	bSplineBasis (const V3 & q0, const V3 & q1, const V3 & q2, const V3 & q3, double t) noexcept
	{
		const double t2 = t * t;
		const double t3 = t2 * t;

		return (q0 * (1.0 - 3.0 * t + 3.0 * t2 - t3) + q1 * (4.0 - 6.0 * t2 + 3.0 * t3) + q2 * (1.0 + 3.0 * t + 3.0 * t2 - 3.0 * t3) + q3 * t3) / 6.0;
	}

	constexpr double Tolerance{0.01};
	/** @brief Dense sampling may land a hair beyond the bound through rounding. */
	constexpr double Slack{1.0E-9};
}

/* A straight cubic (control points on the chord) is ONE segment: the tessellation is adaptive, not uniform. */
TEST(MathCurveTessellation, aStraightCubicIsOneSegment)
{
	std::vector< V3 > polyline{V3{0, 0, 0}};

	CurveTessellation::appendCubic(V3{0, 0, 0}, V3{1, 0, 0}, V3{2, 0, 0}, V3{3, 0, 0}, Tolerance, polyline);

	ASSERT_EQ(polyline.size(), 2U);
	EXPECT_NEAR(polyline.back()[X], 3.0, 1.0E-12);
}

/* Every point of the true curve lies within the tolerance of the polyline, and the ends are exact. */
TEST(MathCurveTessellation, aCubicStaysWithinTheTolerance)
{
	const V3 p0{0, 0, 0};
	const V3 p1{0, 4, 0};
	const V3 p2{6, 4, 2};
	const V3 p3{6, 0, 0};

	std::vector< V3 > polyline{p0};
	CurveTessellation::appendCubic(p0, p1, p2, p3, Tolerance, polyline);

	EXPECT_GT(polyline.size(), 8U) << "a bent cubic needs several segments";
	EXPECT_LT(polyline.size(), 200U) << "and not thousands at 1 cm on a 6 m curve";
	EXPECT_NEAR((polyline.back() - p3).length(), 0.0, 1.0E-12);

	double worst = 0.0;

	for ( int sample = 0; sample <= 2000; ++sample )
	{
		worst = std::max(worst, distanceToPolyline(bernstein(p0, p1, p2, p3, sample / 2000.0), polyline));
	}

	EXPECT_LE(worst, Tolerance + Slack);
}

/* The Bézier path reads Math::BSpline's handles as OFFSETS from the anchor, and follows each span's type. */
TEST(MathCurveTessellation, aBezierPathFollowsTheSplineTypes)
{
	BSpline< 3, double > path{8, CurveType::BezierCubic};
	path.addPoint(V3{0, 0, 0}, V3{0, 0, 0}, V3{0, 3, 0});
	path.addPoint(V3{4, 0, 0}, V3{0, 3, 0}, V3{0, -3, 0}).setCurveType(CurveType::None);
	path.addPoint(V3{8, 0, 0});

	const auto polyline = CurveTessellation::bezierPath(path, Tolerance);

	ASSERT_GE(polyline.size(), 4U);
	EXPECT_NEAR((polyline.front() - V3{0, 0, 0}).length(), 0.0, 1.0E-12);
	EXPECT_NEAR((polyline.back() - V3{8, 0, 0}).length(), 0.0, 1.0E-12);

	/* The first span bulges up to 3/4 of the handle height (the Bernstein maximum of (0,0)(0,3)(4,3)(4,0)). */
	double highest = 0.0;

	for ( const auto & point : polyline )
	{
		highest = std::max(highest, point[Y]);
	}

	EXPECT_NEAR(highest, 2.25, Tolerance);

	/* The second span is straight: the last two points are its ends. */
	EXPECT_NEAR((polyline[polyline.size() - 2] - V3{4, 0, 0}).length(), 0.0, 1.0E-12);
}

/* The Bézier conversion of a uniform cubic B-spline is checked against the B-spline BASIS, and the open curve is
 * clamped to its end control points. */
TEST(MathCurveTessellation, aUniformBSplineMatchesItsBasis)
{
	const std::vector< V3 > controls{V3{0, 0, 0}, V3{2, 3, 0}, V3{4, -1, 1}, V3{6, 2, 0}, V3{8, 0, 0}};

	const auto polyline = CurveTessellation::uniformBSpline(std::span< const V3 >{controls}, Tolerance);

	ASSERT_GE(polyline.size(), 4U);
	EXPECT_NEAR((polyline.front() - controls.front()).length(), 0.0, 1.0E-12);
	EXPECT_NEAR((polyline.back() - controls.back()).length(), 0.0, 1.0E-12);

	/* The padded sequence the tessellator uses, evaluated by the basis. */
	std::vector< V3 > sequence{controls.front(), controls.front()};
	sequence.insert(sequence.end(), controls.begin(), controls.end());
	sequence.emplace_back(controls.back());
	sequence.emplace_back(controls.back());

	double worst = 0.0;

	for ( size_t span = 0; span + 3 < sequence.size(); ++span )
	{
		for ( int sample = 0; sample <= 400; ++sample )
		{
			const auto point = bSplineBasis(sequence[span], sequence[span + 1], sequence[span + 2], sequence[span + 3], sample / 400.0);

			worst = std::max(worst, distanceToPolyline(point, polyline));
		}
	}

	EXPECT_LE(worst, Tolerance + Slack);
}

/* A closed B-spline ends where it starts. */
TEST(MathCurveTessellation, aClosedUniformBSplineCloses)
{
	const std::vector< V3 > controls{V3{0, 0, 0}, V3{4, 0, 0}, V3{4, 4, 0}, V3{0, 4, 0}};

	const auto polyline = CurveTessellation::uniformBSpline(std::span< const V3 >{controls}, Tolerance, true);

	ASSERT_GE(polyline.size(), 8U);
	EXPECT_NEAR((polyline.front() - polyline.back()).length(), 0.0, 1.0E-12);
}

/* Catmull-Rom passes THROUGH every point, in order, and stays smooth between them. */
TEST(MathCurveTessellation, aCatmullRomPassesThroughEveryPoint)
{
	const std::vector< V3 > points{V3{0, 0, 0}, V3{1, 2, 0}, V3{5, 2, 1}, V3{6, 0, 0}, V3{9, 1, 0}};

	for ( const double alpha : {0.0, 0.5, 1.0} )
	{
		const auto polyline = CurveTessellation::catmullRom(std::span< const V3 >{points}, Tolerance, alpha);

		size_t cursor = 0;

		for ( const auto & point : points )
		{
			bool found = false;

			for ( ; cursor < polyline.size(); ++cursor )
			{
				if ( (polyline[cursor] - point).length() < 1.0E-12 )
				{
					found = true;

					break;
				}
			}

			EXPECT_TRUE(found) << "alpha " << alpha << ": the curve misses a point, or visits them out of order";
		}

		/* No kink: consecutive segments turn by less than 30 degrees at 1 cm on these metre-scale spans. */
		for ( size_t index = 1; index + 1 < polyline.size(); ++index )
		{
			const auto in = (polyline[index] - polyline[index - 1]).normalized();
			const auto out = (polyline[index + 1] - polyline[index]).normalized();

			EXPECT_GT(V3::dotProduct(in, out), std::cos(30.0 * 3.14159265358979 / 180.0)) << "alpha " << alpha << " at " << index;
		}
	}
}

/* The CENTRIPETAL variant does not overshoot into a loop where the uniform one does (Yuksel et al. 2011, figure 1:
 * two close points between two far ones). */
TEST(MathCurveTessellation, theCentripetalCatmullRomDoesNotLoop)
{
	const std::vector< V3 > points{V3{0, 0, 0}, V3{10, 10, 0}, V3{10.5, 10, 0}, V3{20, 0, 0}};

	const auto maximumX = [&points] (double alpha) {
		const auto polyline = CurveTessellation::catmullRom(std::span< const V3 >{points}, Tolerance, alpha);

		/* Within the middle span only: between the two close points. */
		double furthest = -1.0E300;
		bool inside = false;

		for ( const auto & point : polyline )
		{
			if ( (point - points[1]).length() < 1.0E-12 )
			{
				inside = true;
			}

			if ( inside )
			{
				furthest = std::max(furthest, std::abs(point[Y] - 10.0));
			}

			if ( inside && (point - points[2]).length() < 1.0E-12 )
			{
				break;
			}
		}

		return furthest;
	};

	/* Uniform bulges far away from the 0.5 m segment; centripetal stays close to it. */
	EXPECT_GT(maximumX(0.0), 1.0);
	EXPECT_LT(maximumX(0.5), 0.5);
}

/* Coincident consecutive points are dropped instead of dividing by zero. */
TEST(MathCurveTessellation, duplicatePointsAreDropped)
{
	const std::vector< V3 > points{V3{0, 0, 0}, V3{0, 0, 0}, V3{3, 1, 0}, V3{3, 1, 0}, V3{6, 0, 0}};

	const auto polyline = CurveTessellation::catmullRom(std::span< const V3 >{points}, Tolerance);

	for ( const auto & point : polyline )
	{
		EXPECT_TRUE(std::isfinite(point[X]) && std::isfinite(point[Y]) && std::isfinite(point[Z]));
	}

	EXPECT_NEAR((polyline.back() - V3{6, 0, 0}).length(), 0.0, 1.0E-12);
	EXPECT_EQ(CurveTessellation::polyline(std::span< const V3 >{points}).size(), 3U);
}

/* The engine instantiates FLOAT (Scenes::Component::Path): every kind compiles and stays within the tolerance there
 * too (the float instantiation is also what MSVC's narrowing warnings see). */
TEST(MathCurveTessellation, everyKindWorksInFloat)
{
	using F3 = Vector< 3, float >;

	const std::vector< F3 > points{F3{0, 0, 0}, F3{1, 2, 0}, F3{3, 2, 1}, F3{4, 0, 0}};

	BSpline< 3, float > path{4, CurveType::BezierCubic};
	path.addPoint(F3{0, 0, 0}, F3{0, 1, 0});
	path.addPoint(F3{4, 0, 0}, F3{0, 1, 0});

	for ( const auto & polyline : {
		CurveTessellation::polyline(std::span< const F3 >{points}),
		CurveTessellation::bezierPath(path),
		CurveTessellation::uniformBSpline(std::span< const F3 >{points}),
		CurveTessellation::catmullRom(std::span< const F3 >{points})
	} )
	{
		ASSERT_GE(polyline.size(), 2U);
		EXPECT_NEAR(polyline.back()[X], 4.0F, 1.0E-5F);
	}
}
