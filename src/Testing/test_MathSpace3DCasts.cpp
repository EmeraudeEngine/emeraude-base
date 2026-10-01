/*
 * src/Testing/test_MathSpace3DCasts.cpp
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
#include "Math/Space3D/Casts/ShapeCast.hpp"
#include "Math/Space3D/OrientedBox.hpp"
#include "Math/Space3D/Sphere.hpp"
#include "Math/Space3D/Triangle.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base::Math;
using namespace EmEn::Base::Math::Space3D;

namespace
{
	using Vec3 = Vector< 3, float >;

	constexpr float Tolerance{1.0e-3F};

	OrientedBox< float >
	axisBox (const Vec3 & center, const Vec3 & halfExtents)
	{
		return OrientedBox< float >{center, {Vec3{1.0F, 0.0F, 0.0F}, Vec3{0.0F, 1.0F, 0.0F}, Vec3{0.0F, 0.0F, 1.0F}}, halfExtents};
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
				return {this->uniform(low, high), this->uniform(low, high), this->uniform(low, high)};
			}

		private:

			uint32_t m_state;
	};
}

TEST(MathSpace3DCasts, rayDownOntoTheGround)
{
	CastHit< float > hit;

	ASSERT_TRUE(castRay(Vec3{1.0F, 5.0F, 2.0F}, Vec3{0.0F, -10.0F, 0.0F}, ground(), hit));
	EXPECT_NEAR(hit.fraction(), 0.5F, Tolerance);
	expectVector(hit.point(), {1.0F, 0.0F, 2.0F});
	expectVector(hit.normal(), {0.0F, 1.0F, 0.0F});
	EXPECT_FALSE(hit.startedInside());
}

TEST(MathSpace3DCasts, rayMissesOrFallsShort)
{
	CastHit< float > hit;

	/* Above the ground, parallel to it. */
	EXPECT_FALSE(castRay(Vec3{0.0F, 1.0F, 0.0F}, Vec3{10.0F, 0.0F, 0.0F}, ground(), hit));
	/* Pointing down but too short to reach it. */
	EXPECT_FALSE(castRay(Vec3{0.0F, 5.0F, 0.0F}, Vec3{0.0F, -4.0F, 0.0F}, ground(), hit));
	/* Pointing away. */
	EXPECT_FALSE(castRay(Vec3{0.0F, 5.0F, 0.0F}, Vec3{0.0F, 4.0F, 0.0F}, ground(), hit));
}

TEST(MathSpace3DCasts, rayStartingInsideIsReported)
{
	CastHit< float > hit;

	ASSERT_TRUE(castRay(Vec3{0.0F, -0.2F, 0.0F}, Vec3{0.0F, -1.0F, 0.0F}, ground(), hit));
	EXPECT_EQ(hit.fraction(), 0.0F);
	EXPECT_TRUE(hit.startedInside());
}

TEST(MathSpace3DCasts, sphereDownOntoTheGround)
{
	CastHit< float > hit;

	ASSERT_TRUE(castSphere(Sphere< float >{0.5F, Vec3{1.0F, 5.0F, 2.0F}}, Vec3{0.0F, -10.0F, 0.0F}, ground(), hit));
	EXPECT_NEAR(hit.fraction(), 0.45F, Tolerance);
	expectVector(hit.point(), {1.0F, 0.0F, 2.0F});
	expectVector(hit.normal(), {0.0F, 1.0F, 0.0F});
}

TEST(MathSpace3DCasts, touchingAndLeavingIsNoContact)
{
	/* A sphere resting on the ground (touching) cast upwards: it leaves, nothing blocks it. Cast down: blocked at 0. */
	CastHit< float > hit;
	const Sphere< float > resting{0.5F, Vec3{0.0F, 0.5F, 0.0F}};

	EXPECT_FALSE(castSphere(resting, Vec3{0.0F, 1.0F, 0.0F}, ground(), hit));
	ASSERT_TRUE(castSphere(resting, Vec3{0.0F, -1.0F, 0.0F}, ground(), hit));
	EXPECT_NEAR(hit.fraction(), 0.0F, Tolerance);
	EXPECT_FALSE(hit.startedInside());
}

TEST(MathSpace3DCasts, capsuleDownOntoTheGround)
{
	/* A standing capsule, feet sphere centre at y = 1 (bottom at 0.5). */
	CastHit< float > hit;

	ASSERT_TRUE(castCapsule(Capsule< float >{Vec3{0.0F, 1.0F, 0.0F}, Vec3{0.0F, 2.0F, 0.0F}, 0.5F}, Vec3{0.0F, -10.0F, 0.0F}, ground(), hit));
	EXPECT_NEAR(hit.fraction(), 0.05F, Tolerance);
	expectVector(hit.normal(), {0.0F, 1.0F, 0.0F});
}

TEST(MathSpace3DCasts, capsuleWalkingIntoAStepHitsItsTopEdge)
{
	/* A citadel step (0.29 m high, its face at x = 1.5) and a walking capsule whose lowest point is at y = 0.2: the
	 * lower hemisphere (centre y = 0.6, radius 0.4) meets the step's TOP EDGE (y = 0.29), where its horizontal radius is
	 * √(0.4² − 0.31²). The normal points from that edge to the hemisphere centre. */
	const auto step = axisBox({2.0F, 0.145F, 0.0F}, {0.5F, 0.145F, 1.0F});
	const Capsule< float > capsule{Vec3{0.0F, 0.6F, 0.0F}, Vec3{0.0F, 1.6F, 0.0F}, 0.4F};
	const float reachAtEdge = std::sqrt((0.4F * 0.4F) - (0.31F * 0.31F));
	const float expectedX = 1.5F - reachAtEdge;

	CastHit< float > hit;

	ASSERT_TRUE(castCapsule(capsule, Vec3{3.0F, 0.0F, 0.0F}, step, hit));
	EXPECT_NEAR(hit.fraction(), expectedX / 3.0F, Tolerance);
	expectVector(hit.point(), {1.5F, 0.29F, 0.0F});
	expectVector(hit.normal(), Vec3{-reachAtEdge, 0.31F, 0.0F}.normalized());
}

TEST(MathSpace3DCasts, sphereOntoASlopedTriangle)
{
	/* A 30° slope through the origin (rising towards +X), and a sphere dropped onto it. */
	const float angle = std::numbers::pi_v< float > / 6.0F;
	const Vec3 up{-std::sin(angle), std::cos(angle), 0.0F};
	const Triangle< float > slope{
		Vec3{-3.0F * std::cos(angle), -3.0F * std::sin(angle), 3.0F},
		Vec3{3.0F * std::cos(angle), 3.0F * std::sin(angle), 3.0F},
		Vec3{0.0F, 0.0F, -3.0F}
	};
	const Sphere< float > sphere{0.5F, Vec3{0.0F, 3.0F, 0.0F}};

	CastHit< float > hit;

	ASSERT_TRUE(castSphere(sphere, Vec3{0.0F, -10.0F, 0.0F}, slope, hit));
	/* Touching when the centre is one radius from the plane: height above the plane along Y = radius / cos 30°. */
	const float travel = 3.0F - (0.5F / std::cos(angle));

	EXPECT_NEAR(hit.fraction(), travel / 10.0F, Tolerance);
	/* The triangle's winding normal is −up here: the normal must point back towards the sphere, i.e. up. */
	expectVector(hit.normal(), up);
}

TEST(MathSpace3DCasts, sphereAgainstSphereAndCapsule)
{
	CastHit< float > hit;

	ASSERT_TRUE(castSphere(Sphere< float >{0.5F, Vec3{0.0F, 0.0F, 0.0F}}, Vec3{10.0F, 0.0F, 0.0F}, Sphere< float >{0.5F, Vec3{5.0F, 0.0F, 0.0F}}, hit));
	EXPECT_NEAR(hit.fraction(), 0.4F, Tolerance);
	expectVector(hit.point(), {4.5F, 0.0F, 0.0F});
	expectVector(hit.normal(), {-1.0F, 0.0F, 0.0F});

	ASSERT_TRUE(castSphere(Sphere< float >{0.5F, Vec3{0.0F, 0.0F, 0.0F}}, Vec3{10.0F, 0.0F, 0.0F}, Capsule< float >{Vec3{5.0F, -1.0F, 0.0F}, Vec3{5.0F, 1.0F, 0.0F}, 0.5F}, hit));
	EXPECT_NEAR(hit.fraction(), 0.4F, Tolerance);
	expectVector(hit.normal(), {-1.0F, 0.0F, 0.0F});
}

TEST(MathSpace3DCasts, capsuleAgainstCapsule)
{
	/* Two standing capsules, one moving at the other. */
	CastHit< float > hit;

	ASSERT_TRUE(castCapsule(Capsule< float >{Vec3{0.0F, 0.0F, 0.0F}, Vec3{0.0F, 1.0F, 0.0F}, 0.3F}, Vec3{0.0F, 0.0F, -4.0F}, Capsule< float >{Vec3{0.0F, 0.5F, -3.0F}, Vec3{0.0F, 2.0F, -3.0F}, 0.3F}, hit));
	EXPECT_NEAR(hit.fraction(), (3.0F - 0.6F) / 4.0F, Tolerance);
	expectVector(hit.normal(), {0.0F, 0.0F, 1.0F});
}

TEST(MathSpace3DCasts, degenerateTriangleIsNeverHit)
{
	const Triangle< float > collinear{Vec3{-1.0F, 0.0F, 0.0F}, Vec3{0.0F, 0.0F, 0.0F}, Vec3{1.0F, 0.0F, 0.0F}};

	CastHit< float > hit;

	EXPECT_FALSE(castSphere(Sphere< float >{0.5F, Vec3{0.0F, 3.0F, 0.0F}}, Vec3{0.0F, -10.0F, 0.0F}, collinear, hit));
}

TEST(MathSpace3DCasts, randomSphereCastsAreConservativeAndExactAgainstBoxes)
{
	/* For every cast: no contact anywhere before the reported fraction, and contact AT it (within 1 mm); a reported
	 * miss means no contact on the whole motion. Checked by sampling the motion against the exact point-box distance. */
	Draw draw{0x5CA7U};
	size_t hits = 0;

	for ( int trial = 0; trial < 1000; ++trial )
	{
		const auto box = OrientedBox< float >{draw.vector(-1.0F, 1.0F), {
			Vec3{1.0F, 0.0F, 0.0F}, Vec3{0.0F, 1.0F, 0.0F}, Vec3{0.0F, 0.0F, 1.0F}
		}, Vec3{draw.uniform(0.2F, 1.0F), draw.uniform(0.2F, 1.0F), draw.uniform(0.2F, 1.0F)}};
		const Sphere< float > sphere{draw.uniform(0.1F, 0.5F), draw.vector(-4.0F, 4.0F)};
		const Vec3 motion = (box.center() - sphere.position()) * draw.uniform(0.5F, 2.0F) + draw.vector(-1.0F, 1.0F);

		const auto gapAt = [&] (float fraction) {
			const Vec3 center = sphere.position() + (motion * fraction);
			Vec3 clamped = box.center();

			for ( size_t index = 0; index < 3; ++index )
			{
				const float local = Vec3::dotProduct(center - box.center(), box.axis(index));

				clamped += box.axis(index) * std::clamp(local, -box.halfExtent(index), box.halfExtent(index));
			}

			return (center - clamped).length() - sphere.radius();
		};

		if ( gapAt(0.0F) <= 0.0F )
		{
			continue;
		}

		CastHit< float > hit;
		const bool contact = castSphere(sphere, motion, box, hit);
		const float stop = contact ? hit.fraction() : 1.0F;

		for ( int step = 0; step < 400; ++step )
		{
			const float fraction = stop * (static_cast< float >(step) / 400.0F);

			EXPECT_GT(gapAt(fraction), -1.0e-4F) << "trial " << trial << " fraction " << fraction;
		}

		if ( contact )
		{
			++hits;

			EXPECT_LT(std::abs(gapAt(hit.fraction())), 1.0e-3F) << "trial " << trial;
			EXPECT_NEAR(hit.normal().length(), 1.0F, 1.0e-3F);
		}
	}

	EXPECT_GT(hits, 300U);
}

TEST(MathSpace3DCasts, randomCapsuleCastsAreConservativeAndExactAgainstTriangles)
{
	Draw draw{0x5CA8U};
	size_t hits = 0;

	for ( int trial = 0; trial < 1000; ++trial )
	{
		const Triangle< float > triangle{draw.vector(-1.0F, 1.0F), draw.vector(-1.0F, 1.0F), draw.vector(-1.0F, 1.0F)};
		Vec3 faceNormal;

		if ( !TriangleDetail::unitNormal(triangle, faceNormal) )
		{
			continue;
		}

		const Vec3 start = draw.vector(-3.0F, 3.0F);
		const Vec3 end = start + (draw.vector(-1.0F, 1.0F) * 0.7F);
		const Capsule< float > capsule{start, end, draw.uniform(0.1F, 0.5F)};
		const Vec3 aim = (triangle.pointA() + triangle.pointB() + triangle.pointC()) * (1.0F / 3.0F);
		const Vec3 motion = (aim - ((start + end) * 0.5F)) * draw.uniform(0.5F, 2.0F) + draw.vector(-0.5F, 0.5F);

		const auto gapAt = [&] (float fraction) {
			const Vec3 offset = motion * fraction;
			const auto closest = CapsuleTriangleDetail::closestOfSegmentAndTriangle(start + offset, end + offset, triangle, faceNormal);

			return std::sqrt(closest.distanceSquared) - capsule.radius();
		};

		if ( gapAt(0.0F) <= 0.0F )
		{
			continue;
		}

		CastHit< float > hit;
		const bool contact = castCapsule(capsule, motion, triangle, hit);
		const float stop = contact ? hit.fraction() : 1.0F;

		for ( int step = 0; step < 400; ++step )
		{
			const float fraction = stop * (static_cast< float >(step) / 400.0F);

			EXPECT_GT(gapAt(fraction), -1.0e-4F) << "trial " << trial << " fraction " << fraction;
		}

		if ( contact )
		{
			++hits;

			EXPECT_LT(std::abs(gapAt(hit.fraction())), 1.0e-3F) << "trial " << trial;
		}
	}

	EXPECT_GT(hits, 300U);
}
