/*
 * src/Testing/test_MathSpace3DConvexDistance.cpp
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
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

/* Local inclusions. */
#include "Math/Space3D/Capsule.hpp"
#include "Math/Space3D/Casts/ConvexDistance.hpp"
#include "Math/Space3D/Casts/ShapeCast.hpp"
#include "Math/Space3D/Contacts/BoxBox.hpp"
#include "Math/Space3D/OrientedBox.hpp"
#include "Math/Space3D/Sphere.hpp"
#include "Math/Space3D/Triangle.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base::Math;
using namespace EmEn::Base::Math::Space3D;

namespace
{
	using Vec3 = Vector< 3, float >;
	using Polytope = ConvexPolytope< float >;

	constexpr float Tolerance{1.0e-3F};

	OrientedBox< float >
	axisBox (const Vec3 & center, const Vec3 & halfExtents)
	{
		return OrientedBox< float >{center, {Vec3{1.0F, 0.0F, 0.0F}, Vec3{0.0F, 1.0F, 0.0F}, Vec3{0.0F, 0.0F, 1.0F}}, halfExtents};
	}

	/** A box turned by an angle about Z. */
	OrientedBox< float >
	boxAboutZ (const Vec3 & center, const Vec3 & halfExtents, float angle)
	{
		const auto c = std::cos(angle);
		const auto s = std::sin(angle);

		return OrientedBox< float >{center, {Vec3{c, s, 0.0F}, Vec3{-s, c, 0.0F}, Vec3{0.0F, 0.0F, 1.0F}}, halfExtents};
	}

	/** The ground: a 20 × 1 × 20 slab whose top face is the plane Y = 0. */
	OrientedBox< float >
	ground ()
	{
		return axisBox({0.0F, -0.5F, 0.0F}, {10.0F, 0.5F, 10.0F});
	}

	void
	expectVector (const Vec3 & actual, const Vec3 & expected, float tolerance = Tolerance)
	{
		EXPECT_NEAR(actual[X], expected[X], tolerance);
		EXPECT_NEAR(actual[Y], expected[Y], tolerance);
		EXPECT_NEAR(actual[Z], expected[Z], tolerance);
	}

	/** A 32-bit LCG (Numerical Recipes constants): the same draws on every platform. */
	class Draw final
	{
		public:

			explicit Draw (uint32_t seed) noexcept : m_state{seed} {}

			float
			uniform (float low, float high) noexcept
			{
				m_state = (m_state * 1664525U) + 1013904223U;

				return low + ((high - low) * (static_cast< float >(m_state >> 8U) / 16777216.0F));
			}

			Vec3
			vector (float low, float high) noexcept
			{
				const auto x = this->uniform(low, high);
				const auto y = this->uniform(low, high);
				const auto z = this->uniform(low, high);

				return {x, y, z};
			}

		private:

			uint32_t m_state;
	};

	/** A random orthonormal frame (two random vectors, Gram-Schmidt). */
	OrientedBox< float >
	randomBox (Draw & draw, float spread)
	{
		Vec3 first = draw.vector(-1.0F, 1.0F);
		Vec3 second = draw.vector(-1.0F, 1.0F);

		if ( first.lengthSquared() < 1.0e-3F )
		{
			first = {1.0F, 0.0F, 0.0F};
		}

		first.normalize();
		second -= first * Vec3::dotProduct(second, first);

		if ( second.lengthSquared() < 1.0e-3F )
		{
			second = Vec3::crossProduct(first, Vec3{0.0F, 0.0F, 1.0F});

			if ( second.lengthSquared() < 1.0e-3F )
			{
				second = Vec3::crossProduct(first, Vec3{0.0F, 1.0F, 0.0F});
			}
		}

		second.normalize();

		const auto third = Vec3::crossProduct(first, second);

		return OrientedBox< float >{draw.vector(-spread, spread), {first, second, third}, draw.vector(0.1F, 1.0F)};
	}
}

/* ===== GJK distance ===== */

TEST(MathSpace3DConvexDistance, PointToPoint)
{
	const auto closest = closestPoints(Polytope{Vec3{0.0F, 0.0F, 0.0F}}, Polytope{Vec3{3.0F, 4.0F, 0.0F}});

	ASSERT_FALSE(closest.overlapping);
	EXPECT_NEAR(closest.distance, 5.0F, Tolerance);
}

TEST(MathSpace3DConvexDistance, SeparatedBoxes)
{
	/* Two unit boxes 2 m apart along X: the gap between their faces is 1 m. */
	const auto closest = closestPoints(Polytope{axisBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F})}, Polytope{axisBox({2.0F, 0.3F, 0.0F}, {0.5F, 0.5F, 0.5F})});

	ASSERT_FALSE(closest.overlapping);
	EXPECT_NEAR(closest.distance, 1.0F, Tolerance);
	EXPECT_NEAR(closest.onA[X], 0.5F, Tolerance);
	EXPECT_NEAR(closest.onB[X], 1.5F, Tolerance);
}

TEST(MathSpace3DConvexDistance, TurnedBoxCornerAboveGround)
{
	/* A unit cube turned 45° about Z, centre 2 m above the ground: its lowest edge is 2 − √2 / 2 above it. */
	const auto closest = closestPoints(Polytope{boxAboutZ({0.0F, 2.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, std::numbers::pi_v< float > / 4.0F)}, Polytope{ground()});

	ASSERT_FALSE(closest.overlapping);
	EXPECT_NEAR(closest.distance, 2.0F - (std::numbers::sqrt2_v< float > * 0.5F), Tolerance);
}

TEST(MathSpace3DConvexDistance, OverlappingBoxes)
{
	const auto closest = closestPoints(Polytope{axisBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F})}, Polytope{boxAboutZ({0.6F, 0.2F, 0.1F}, {0.5F, 0.5F, 0.5F}, 0.4F)});

	EXPECT_TRUE(closest.overlapping);
}

TEST(MathSpace3DConvexDistance, EmptyPolytopeIsRefused)
{
	EXPECT_TRUE(closestPoints(Polytope{}, Polytope{Vec3{1.0F, 0.0F, 0.0F}}).overlapping);
}

TEST(MathSpace3DConvexDistance, SegmentBoxMatchesTheExactClosestPoints)
{
	Draw draw{20261002U};
	uint32_t separated = 0;

	for ( uint32_t trial = 0; trial < 400; ++trial )
	{
		const auto box = randomBox(draw, 0.5F);
		const auto start = draw.vector(-3.0F, 3.0F);
		const auto end = draw.vector(-3.0F, 3.0F);
		const auto exact = ShapeCastDetail::coreClosest(start, end, box);
		const auto expected = (exact.onTarget - exact.onCaster).length();
		const auto gjk = closestPoints(Polytope{start, end}, Polytope{box});

		if ( expected < 1.0e-3F )
		{
			continue;
		}

		++separated;
		ASSERT_FALSE(gjk.overlapping) << "trial " << trial;
		EXPECT_NEAR(gjk.distance, expected, 2.0e-3F) << "trial " << trial;
	}

	EXPECT_GT(separated, 200U);
}

TEST(MathSpace3DConvexDistance, SegmentTriangleMatchesTheExactClosestPoints)
{
	Draw draw{7U};
	uint32_t separated = 0;

	for ( uint32_t trial = 0; trial < 400; ++trial )
	{
		const Triangle< float > triangle{draw.vector(-1.5F, 1.5F), draw.vector(-1.5F, 1.5F), draw.vector(-1.5F, 1.5F)};
		const auto start = draw.vector(-3.0F, 3.0F);
		const auto end = draw.vector(-3.0F, 3.0F);
		const auto exact = ShapeCastDetail::coreClosest(start, end, triangle);

		if ( !exact.valid )
		{
			continue;
		}

		const auto expected = (exact.onTarget - exact.onCaster).length();

		if ( expected < 1.0e-3F )
		{
			continue;
		}

		++separated;

		const auto gjk = closestPoints(Polytope{start, end}, Polytope{triangle});

		ASSERT_FALSE(gjk.overlapping) << "trial " << trial;
		EXPECT_NEAR(gjk.distance, expected, 2.0e-3F) << "trial " << trial;
	}

	EXPECT_GT(separated, 200U);
}

TEST(MathSpace3DConvexDistance, BoxBoxOverlapAgreesWithSAT)
{
	/* 3000 random turned boxes near each other: GJK says "overlapping" exactly when the separating axis test finds a
	 * contact (a 1 mm band around touching left out). A flat tetrahedron once answered "inside" 0.1 m from the origin. */
	Draw draw{31337U};
	uint32_t overlaps = 0;
	uint32_t separations = 0;

	for ( uint32_t trial = 0; trial < 3000; ++trial )
	{
		const auto boxA = randomBox(draw, 1.2F);
		const auto boxB = randomBox(draw, 1.2F);
		const auto gjk = closestPoints(Polytope{boxA}, Polytope{boxB});
		ContactManifold< float > manifold;
		const bool sat = computeContactManifold(boxA, boxB, manifold);

		if ( sat && manifold.maximumDepth() > 1.0e-3F )
		{
			++overlaps;
			EXPECT_TRUE(gjk.overlapping) << "trial " << trial;
		}
		else if ( !sat && !gjk.overlapping )
		{
			++separations;
		}
		else if ( !sat )
		{
			ADD_FAILURE() << "GJK overlap where SAT finds none, trial " << trial;
		}
	}

	EXPECT_GT(overlaps, 500U);
	EXPECT_GT(separations, 500U);
}

/* ===== Box casts ===== */

TEST(MathSpace3DConvexDistance, BoxCastOntoGround)
{
	/* A unit cube, its centre 2 m up, moved 3 m down: its bottom (1.5 m up) meets the ground at half the motion. */
	CastHit< float > hit;

	ASSERT_TRUE(castBox(axisBox({0.0F, 2.0F, 0.0F}, {0.5F, 0.5F, 0.5F}), Vec3{0.0F, -3.0F, 0.0F}, ground(), hit));
	EXPECT_NEAR(hit.fraction(), 0.5F, Tolerance);
	expectVector(hit.normal(), {0.0F, 1.0F, 0.0F});
	EXPECT_NEAR(hit.point()[Y], 0.0F, Tolerance);
	EXPECT_FALSE(hit.startedInside());
}

TEST(MathSpace3DConvexDistance, FaceAgainstFaceNormalIsExactFarFromTheOrigin)
{
	/* A 0.2 m box shot at a 0.2 m wall 89 m from the origin: face against face, the normal must be EXACTLY the wall's
	 * (it leaned 0.0024 rad, and a bounced box drifted over the wall). */
	const OrientedBox< float > wall = axisBox({0.0F, 2.0F, -90.0F}, {0.1F, 2.0F, 2.0F});
	CastHit< float > hit;

	ASSERT_TRUE(castBox(axisBox({-1.0F, 1.0F, -89.0F}, {0.1F, 0.1F, 0.1F}), Vec3{1.3333F, 0.0F, 0.0F}, wall, hit));
	EXPECT_NEAR(hit.fraction(), 0.6F, Tolerance);
	EXPECT_EQ(hit.normal()[X], -1.0F);
	EXPECT_EQ(hit.normal()[Y], 0.0F);
	EXPECT_EQ(hit.normal()[Z], 0.0F);
}

TEST(MathSpace3DConvexDistance, TurnedBoxCastMeetsWithItsEdge)
{
	/* Turned 45° about Z: its lowest edge, not its inscribed sphere, meets the ground — earlier than a face would. */
	CastHit< float > hit;
	const auto lowest = 2.0F - (std::numbers::sqrt2_v< float > * 0.5F);

	ASSERT_TRUE(castBox(boxAboutZ({0.0F, 2.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, std::numbers::pi_v< float > / 4.0F), Vec3{0.0F, -3.0F, 0.0F}, ground(), hit));
	EXPECT_NEAR(hit.fraction(), lowest / 3.0F, Tolerance);
	EXPECT_NEAR(hit.normal()[Y], 1.0F, Tolerance);
}

TEST(MathSpace3DConvexDistance, BoxCastOntoTriangle)
{
	const Triangle< float > floor{{-5.0F, 0.0F, -5.0F}, {0.0F, 0.0F, 5.0F}, {5.0F, 0.0F, -5.0F}};
	CastHit< float > hit;

	ASSERT_TRUE(castBox(axisBox({0.0F, 2.0F, 0.0F}, {0.5F, 0.5F, 0.5F}), Vec3{0.0F, -3.0F, 0.0F}, floor, hit));
	EXPECT_NEAR(hit.fraction(), 0.5F, Tolerance);
	EXPECT_NEAR(std::abs(hit.normal()[Y]), 1.0F, Tolerance);
}

TEST(MathSpace3DConvexDistance, BoxCastOntoSphereAndCapsule)
{
	/* The box's face at x 0.5 meets the sphere's surface at x 2.5 after 2 m of a 4 m motion. */
	CastHit< float > hit;

	ASSERT_TRUE(castBox(axisBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}), Vec3{4.0F, 0.0F, 0.0F}, Sphere< float >{0.5F, Vec3{3.0F, 0.0F, 0.0F}}, hit));
	EXPECT_NEAR(hit.fraction(), 0.5F, Tolerance);
	expectVector(hit.normal(), {-1.0F, 0.0F, 0.0F});
	EXPECT_NEAR(hit.point()[X], 2.5F, Tolerance);

	ASSERT_TRUE(castBox(axisBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}), Vec3{4.0F, 0.0F, 0.0F}, Capsule< float >{Vec3{3.0F, -2.0F, 0.0F}, Vec3{3.0F, 2.0F, 0.0F}, 0.5F}, hit));
	EXPECT_NEAR(hit.fraction(), 0.5F, Tolerance);
}

TEST(MathSpace3DConvexDistance, BoxCastStartedInsideAndLeaving)
{
	CastHit< float > hit;

	/* Overlapping the ground at the start. */
	ASSERT_TRUE(castBox(axisBox({0.0F, 0.2F, 0.0F}, {0.5F, 0.5F, 0.5F}), Vec3{1.0F, 0.0F, 0.0F}, ground(), hit));
	EXPECT_TRUE(hit.startedInside());
	EXPECT_NEAR(hit.fraction(), 0.0F, Tolerance);

	/* Resting on it and leaving it: no contact. */
	EXPECT_FALSE(castBox(axisBox({0.0F, 0.5F, 0.0F}, {0.5F, 0.5F, 0.5F}), Vec3{0.0F, 1.0F, 0.0F}, ground(), hit));

	/* Moving away from a distant target: no contact. */
	EXPECT_FALSE(castBox(axisBox({0.0F, 3.0F, 0.0F}, {0.5F, 0.5F, 0.5F}), Vec3{0.0F, 1.0F, 0.0F}, ground(), hit));
}

TEST(MathSpace3DConvexDistance, BoxCastMatchesAFineMarch)
{
	/* Random turned boxes cast at random turned boxes: the first contact equals a 1/2000 march of the distance. */
	Draw draw{99U};
	uint32_t hits = 0;

	for ( uint32_t trial = 0; trial < 120; ++trial )
	{
		const auto caster = randomBox(draw, 0.2F);
		auto target = randomBox(draw, 0.2F);

		target = OrientedBox< float >{target.center() + Vec3{4.0F, 0.0F, 0.0F}, target.axes(), target.halfExtents()};

		const Vec3 motion = Vec3{6.0F, 0.0F, 0.0F} + draw.vector(-1.0F, 1.0F);
		CastHit< float > hit;
		const bool met = castBox(caster, motion, target, hit);

		float marched = 2.0F;

		for ( int step = 0; step <= 2000; ++step )
		{
			const auto fraction = static_cast< float >(step) / 2000.0F;
			const OrientedBox< float > moved{caster.center() + (motion * fraction), caster.axes(), caster.halfExtents()};
			const auto closest = closestPoints(Polytope{moved}, Polytope{target});

			if ( closest.overlapping || closest.distance <= 1.0e-3F )
			{
				marched = fraction;

				break;
			}
		}

		if ( marched > 1.0F )
		{
			EXPECT_FALSE(met) << "trial " << trial;

			continue;
		}

		++hits;
		ASSERT_TRUE(met) << "trial " << trial;
		EXPECT_NEAR(hit.fraction(), marched, 2.0e-3F) << "trial " << trial;
	}

	EXPECT_GT(hits, 60U);
}
