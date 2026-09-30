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
#include "Math/CurveShape.hpp"
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

/* Every original point is kept (a corner stays sharp), no piece is longer than length / N, and a segment exactly N
 * steps long is cut into exactly N pieces (no extra piece from rounding). */
TEST(MathCurveTessellation, subdividedKeepsCornersAndBoundsTheSteps)
{
	const std::vector< V3 > corner{V3{0, 0, 0}, V3{3, 0, 0}, V3{3, 1, 0}};

	const auto result = CurveTessellation::subdivided(std::span< const V3 >{corner}, 8);

	ASSERT_EQ(result.size(), 9U);
	EXPECT_NEAR((result[6] - V3{3, 0, 0}).length(), 0.0, 1.0E-12);
	EXPECT_NEAR((result.back() - V3{3, 1, 0}).length(), 0.0, 1.0E-12);

	for ( size_t index = 1; index < result.size(); ++index )
	{
		EXPECT_LE((result[index] - result[index - 1]).length(), 0.5 + 1.0E-12);
	}

	const std::vector< V3 > line{V3{0, 0, 0}, V3{10, 0, 0}};

	EXPECT_EQ(CurveTessellation::subdivided(std::span< const V3 >{line}, 96).size(), 97U);
	EXPECT_EQ(CurveTessellation::subdivided(std::span< const V3 >{line}, 1).size(), 2U);
	EXPECT_EQ(CurveTessellation::subdivided(std::span< const V3 >{corner}, 1).size(), 3U);
}

/* A straight line keeps its first normal, cross(tangent, +Y) — the beam's historical arc axes. */
TEST(MathCurveTessellation, rotationMinimizingNormalsOfALineAreConstant)
{
	const std::vector< V3 > line{V3{0, 0, 0}, V3{0, 0, 1}, V3{0, 0, 2}, V3{0, 0, 5}};

	const auto normals = CurveTessellation::rotationMinimizingNormals(std::span< const V3 >{line});

	ASSERT_EQ(normals.size(), line.size());

	const auto expected = V3::crossProduct(V3{0, 0, 1}, V3::positiveY()).normalized();

	for ( const auto & normal : normals )
	{
		EXPECT_NEAR((normal - expected).length(), 0.0, 1.0E-12);
	}
}

/* A PLANAR curve's rotation minimizing frame does not turn out of its plane: the normal of a circle in XY, starting
 * along Z, stays along Z all the way round. On a helix the normals stay unit and perpendicular to the tangent. */
TEST(MathCurveTessellation, rotationMinimizingNormalsDoNotTwist)
{
	std::vector< V3 > circle;
	std::vector< V3 > helix;

	for ( int index = 0; index <= 64; ++index )
	{
		const double angle = static_cast< double >(index) * 0.0981747704246810387;

		circle.emplace_back(std::sin(angle), 1.0 - std::cos(angle), 0.0);
		helix.emplace_back(std::cos(angle), std::sin(angle), static_cast< double >(index) * 0.05);
	}

	const auto circleNormals = CurveTessellation::rotationMinimizingNormals(std::span< const V3 >{circle});

	ASSERT_EQ(circleNormals.size(), circle.size());

	for ( const auto & normal : circleNormals )
	{
		EXPECT_NEAR(std::abs(normal[Z]), 1.0, 1.0E-9);
	}

	const auto helixNormals = CurveTessellation::rotationMinimizingNormals(std::span< const V3 >{helix});

	ASSERT_EQ(helixNormals.size(), helix.size());

	for ( size_t index = 1; index + 1 < helix.size(); ++index )
	{
		const auto tangent = (helix[index + 1] - helix[index - 1]).normalized();

		EXPECT_NEAR(helixNormals[index].length(), 1.0, 1.0E-9);
		EXPECT_NEAR(V3::dotProduct(helixNormals[index], tangent), 0.0, 1.0E-9);
	}
}

/* The description tessellates as the free functions do, and moving the end anchors of a Bézier path keeps the handles. */
TEST(MathCurveTessellation, curveShapeMovesItsEndsAndTessellates)
{
	const std::vector< V3 > points{V3{0, 0, 0}, V3{1, 2, 0}, V3{3, 2, 1}, V3{4, 0, 0}};

	CurveShape< double > shape;
	shape.setCatmullRom(std::span< const V3 >{points});

	EXPECT_EQ(shape.kind(), CurveKind::CatmullRom);
	EXPECT_EQ(shape.tessellate(Tolerance).size(), CurveTessellation::catmullRom(std::span< const V3 >{points}, Tolerance).size());

	shape.setLastPoint(V3{5, 0, 0});

	EXPECT_NEAR((shape.tessellate(Tolerance).back() - V3{5, 0, 0}).length(), 0.0, 1.0E-12);

	BSpline< 3, double > path{4, CurveType::BezierCubic};
	path.addPoint(V3{0, 0, 0}, V3{0, 1, 0});
	path.addPoint(V3{4, 0, 0}, V3{0, 1, 0});

	shape.setBezierPath(path);
	shape.setFirstPoint(V3{-1, 0, 0});

	ASSERT_EQ(shape.bezierPath().points().size(), 2U);
	EXPECT_NEAR((shape.bezierPath().points().front().position() - V3{-1, 0, 0}).length(), 0.0, 1.0E-12);
	EXPECT_NEAR((shape.bezierPath().points().front().handleOut() - path.points().front().handleOut()).length(), 0.0, 1.0E-12);
	EXPECT_EQ(shape.bezierPath().points().front().curveType(), CurveType::BezierCubic);
	EXPECT_NEAR((shape.tessellate(Tolerance).front() - V3{-1, 0, 0}).length(), 0.0, 1.0E-12);
	EXPECT_STREQ(to_cstring(CurveKind::UniformBSpline), "UniformBSpline");
}

/* The frame is rotation minimizing, not merely perpendicular: on a helix (curvature 1 / (1 + c²), torsion
 * c / (1 + c²)) the exact RMF turns against the Frenet frame at minus the torsion, θ(s) = θ(0) − τ s. Measured over
 * two turns at 64 points per turn: 7e-7 rad for the double reflection, 1.5e-5 for a single reflection followed by a
 * projection (the mutation this tolerance was chosen to catch). */
TEST(MathCurveTessellation, rotationMinimizingNormalsFollowTheTorsion)
{
	constexpr double C{0.3};
	constexpr double Step{0.0981747704246810387};
	const double speed = std::sqrt(1.0 + C * C);
	const double torsion = C / (1.0 + C * C);

	std::vector< V3 > helix;

	for ( int index = 0; index <= 128; ++index )
	{
		const double a = static_cast< double >(index) * Step;

		helix.emplace_back(std::cos(a), std::sin(a), C * a);
	}

	const auto normals = CurveTessellation::rotationMinimizingNormals(std::span< const V3 >{helix});

	ASSERT_EQ(normals.size(), helix.size());

	const auto frenetAngle = [&] (size_t index) noexcept {
		const double a = static_cast< double >(index) * Step;
		const V3 tangent = V3{-std::sin(a), std::cos(a), C} / speed;
		const V3 normal{-std::cos(a), -std::sin(a), 0.0};
		const auto binormal = V3::crossProduct(tangent, normal);

		return std::atan2(V3::dotProduct(normals[index], binormal), V3::dotProduct(normals[index], normal));
	};

	const double start = frenetAngle(1);
	double previous = start;
	double unwrapped = start;

	/* The end points' tangents are one-sided: compare the interior. */
	for ( size_t index = 2; index + 1 < helix.size(); ++index )
	{
		double angle = frenetAngle(index);

		while ( angle - previous > 3.14159265358979 ) { angle -= 6.28318530717959; }
		while ( angle - previous < -3.14159265358979 ) { angle += 6.28318530717959; }

		previous = angle;
		unwrapped = angle;

		const double arcLength = static_cast< double >(index - 1) * Step * speed;

		EXPECT_NEAR(unwrapped - start, -torsion * arcLength, 3.0E-6) << "at point " << index;
	}
}
