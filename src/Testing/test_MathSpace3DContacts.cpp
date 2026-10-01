/*
 * src/Testing/test_MathSpace3DContacts.cpp
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
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

/* Local inclusions. */
#include "Math/CartesianFrame.hpp"
#include "Math/Space3D/AACuboid.hpp"
#include "Math/Space3D/Capsule.hpp"
#include "Math/Space3D/Contacts/BoxBox.hpp"
#include "Math/Space3D/Contacts/BoxTriangle.hpp"
#include "Math/Space3D/Contacts/CapsuleBox.hpp"
#include "Math/Space3D/Contacts/CapsuleTriangle.hpp"
#include "Math/Space3D/Contacts/ContactManifold.hpp"
#include "Math/Space3D/Contacts/RoundShapes.hpp"
#include "Math/Space3D/Contacts/SphereBox.hpp"
#include "Math/Space3D/Contacts/SphereTriangle.hpp"
#include "Math/Space3D/OrientedBox.hpp"
#include "Math/Space3D/Sphere.hpp"
#include "Math/Space3D/Triangle.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base::Math;
using namespace EmEn::Base::Math::Space3D;

namespace
{
	using Vec3 = Vector< 3, float >;

	constexpr float Tolerance{1.0e-4F};

	/** Rotates a vector about a unit axis (Rodrigues). */
	Vec3
	rotated (const Vec3 & vector, const Vec3 & axis, float angle)
	{
		const float cosine = std::cos(angle);
		const float sine = std::sin(angle);

		return (vector * cosine) + (Vec3::crossProduct(axis, vector) * sine) + (axis * (Vec3::dotProduct(axis, vector) * (1.0F - cosine)));
	}

	/** A box whose axes are the world axes turned about one axis. */
	OrientedBox< float >
	makeBox (const Vec3 & center, const Vec3 & halfExtents, const Vec3 & rotationAxis = {0.0F, 1.0F, 0.0F}, float angle = 0.0F)
	{
		return OrientedBox< float >{center, {
			rotated({1.0F, 0.0F, 0.0F}, rotationAxis, angle),
			rotated({0.0F, 1.0F, 0.0F}, rotationAxis, angle),
			rotated({0.0F, 0.0F, 1.0F}, rotationAxis, angle)
		}, halfExtents};
	}

	/** The ground: a 10 × 1 × 10 slab whose top face is the plane Y = 0. */
	OrientedBox< float >
	ground ()
	{
		return makeBox({0.0F, -0.5F, 0.0F}, {5.0F, 0.5F, 5.0F});
	}

	std::vector< uint32_t >
	sortedFeatureIds (const ContactManifold< float > & manifold)
	{
		std::vector< uint32_t > ids;

		for ( const auto & point : manifold.points() )
		{
			ids.push_back(point.featureId());
		}

		std::ranges::sort(ids);

		return ids;
	}

	void
	expectNormal (const ContactManifold< float > & manifold, const Vec3 & expected)
	{
		EXPECT_NEAR(manifold.normal()[X], expected[X], Tolerance);
		EXPECT_NEAR(manifold.normal()[Y], expected[Y], Tolerance);
		EXPECT_NEAR(manifold.normal()[Z], expected[Z], Tolerance);
		EXPECT_NEAR(manifold.normal().length(), 1.0F, Tolerance);
	}
}

TEST(MathSpace3DContacts, orientedBoxFromCuboidAppliesFrame)
{
	const AACuboid< float > localBox{Vec3{1.0F, 2.0F, 3.0F}, Vec3{-1.0F, 0.0F, -1.0F}};

	CartesianFrame< float > frame{Vec3{10.0F, 0.0F, -5.0F}};
	frame.setScalingFactor(2.0F, 1.0F, 0.5F);

	const auto box = OrientedBox< float >::fromCuboid(localBox, frame);

	ASSERT_TRUE(box.isValid());
	/* Local centre (0, 1, 1) scaled by (2, 1, 0.5) then moved by the frame. */
	EXPECT_NEAR(box.center()[X], 10.0F, Tolerance);
	EXPECT_NEAR(box.center()[Y], 1.0F, Tolerance);
	EXPECT_NEAR(box.center()[Z], -4.5F, Tolerance);
	EXPECT_NEAR(box.halfExtent(0), 2.0F, Tolerance);
	EXPECT_NEAR(box.halfExtent(1), 1.0F, Tolerance);
	EXPECT_NEAR(box.halfExtent(2), 1.0F, Tolerance);
}

TEST(MathSpace3DContacts, orientedBoxRejectsInvalidData)
{
	EXPECT_FALSE(makeBox({0.0F, 0.0F, 0.0F}, {-1.0F, 1.0F, 1.0F}).isValid());
	EXPECT_FALSE(makeBox({std::nanf(""), 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}).isValid());
	EXPECT_FALSE((OrientedBox< float >{Vec3{}, {Vec3{2.0F, 0.0F, 0.0F}, Vec3{0.0F, 1.0F, 0.0F}, Vec3{0.0F, 0.0F, 1.0F}}, Vec3{1.0F, 1.0F, 1.0F}}).isValid());
	EXPECT_TRUE(makeBox({0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 1.0F}).isValid());
}

TEST(MathSpace3DContacts, separatedBoxesGiveNoContact)
{
	ContactManifold< float > manifold;

	EXPECT_FALSE(computeContactManifold(ground(), makeBox({0.0F, 0.6F, 0.0F}, {0.5F, 0.5F, 0.5F}), manifold));
	EXPECT_TRUE(manifold.empty());
	EXPECT_FALSE(computeContactManifold(makeBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}), makeBox({3.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}), manifold));
}

TEST(MathSpace3DContacts, boxRestingFlatGivesFourPoints)
{
	const auto box = makeBox({0.0F, 0.49F, 0.0F}, {0.5F, 0.5F, 0.5F});

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(ground(), box, manifold));
	ASSERT_EQ(manifold.points().size(), 4U);
	/* From A (the ground) to B (the box): up. */
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.01F, Tolerance);
		EXPECT_NEAR(std::abs(point.position()[X]), 0.5F, Tolerance);
		EXPECT_NEAR(std::abs(point.position()[Z]), 0.5F, Tolerance);
		/* Halfway between the box's bottom (-0.01) and the ground's top (0). */
		EXPECT_NEAR(point.position()[Y], -0.005F, Tolerance);
	}

	EXPECT_NEAR(manifold.maximumDepth(), 0.01F, Tolerance);
}

TEST(MathSpace3DContacts, swappingTheBoxesNegatesTheNormal)
{
	const auto box = makeBox({0.0F, 0.49F, 0.0F}, {0.5F, 0.5F, 0.5F});

	ContactManifold< float > forward;
	ContactManifold< float > backward;

	ASSERT_TRUE(computeContactManifold(ground(), box, forward));
	ASSERT_TRUE(computeContactManifold(box, ground(), backward));
	ASSERT_EQ(backward.points().size(), 4U);
	expectNormal(backward, {0.0F, -1.0F, 0.0F});

	for ( const auto & point : backward.points() )
	{
		EXPECT_NEAR(point.depth(), 0.01F, Tolerance);
	}
}

TEST(MathSpace3DContacts, boxOnAnEdgeGivesTwoPoints)
{
	const float halfDiagonal = 0.5F * std::numbers::sqrt2_v< float >;
	const auto box = makeBox({0.0F, halfDiagonal - 0.01F, 0.0F}, {0.5F, 0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}, std::numbers::pi_v< float > * 0.25F);

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(ground(), box, manifold));
	ASSERT_EQ(manifold.points().size(), 2U);
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.01F, Tolerance);
		EXPECT_NEAR(point.position()[X], 0.0F, Tolerance);
		EXPECT_NEAR(std::abs(point.position()[Z]), 0.5F, Tolerance);
	}
}

TEST(MathSpace3DContacts, boxOnACornerGivesOnePoint)
{
	/* Turn a cube so that its (-1, -1, -1) diagonal points straight down: 45° about Z, then atan(1/√2) about X. */
	auto box = makeBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F});
	const float tilt = std::atan(1.0F / std::numbers::sqrt2_v< float >);
	std::array< Vec3, 3 > axes = box.axes();

	for ( auto & axis : axes )
	{
		axis = rotated(rotated(axis, {0.0F, 0.0F, 1.0F}, std::numbers::pi_v< float > * 0.25F), {1.0F, 0.0F, 0.0F}, tilt);
	}

	box = OrientedBox< float >{Vec3{}, axes, {0.5F, 0.5F, 0.5F}};

	float lowest = 0.0F;

	for ( size_t index = 0; index < 8; ++index )
	{
		lowest = std::min(lowest, box.corner(index)[Y]);
	}

	box.setCenter({0.0F, -lowest - 0.01F, 0.0F});

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(ground(), box, manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.01F, Tolerance);
}

TEST(MathSpace3DContacts, yawedBoxRestingFlatKeepsItsCorners)
{
	const auto box = makeBox({1.0F, 0.48F, -2.0F}, {0.5F, 0.5F, 0.5F}, {0.0F, 1.0F, 0.0F}, std::numbers::pi_v< float > * 0.25F);

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(ground(), box, manifold));
	ASSERT_EQ(manifold.points().size(), 4U);
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});

	const float halfDiagonal = 0.5F * std::numbers::sqrt2_v< float >;

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.02F, Tolerance);
		/* The corners of a 45° square lie on the axes through its centre, at the half diagonal. */
		const float distance = std::hypot(point.position()[X] - 1.0F, point.position()[Z] + 2.0F);

		EXPECT_NEAR(distance, halfDiagonal, Tolerance);
	}
}

TEST(MathSpace3DContacts, octagonalOverlapIsReducedToFourPoints)
{
	/* Two equal cubes stacked, the top one turned 45° about Y: its bottom face clipped by the top face of the
	 * other one is an octagon (8 points) — the manifold keeps 4 of them. */
	const auto bottom = makeBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F});
	const auto top = makeBox({0.0F, 0.99F, 0.0F}, {0.5F, 0.5F, 0.5F}, {0.0F, 1.0F, 0.0F}, std::numbers::pi_v< float > * 0.25F);

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(bottom, top, manifold));
	ASSERT_EQ(manifold.points().size(), 4U);
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.01F, Tolerance);
		/* Every kept point lies on the octagon: inside both squares, on the border of one. */
		EXPECT_LE(std::abs(point.position()[X]), 0.5F + Tolerance);
		EXPECT_LE(std::abs(point.position()[Z]), 0.5F + Tolerance);
	}

	/* The four points span the overlap: their bounding box is at least 0.6 m wide on both axes. */
	float minX = 1.0F;
	float maxX = -1.0F;
	float minZ = 1.0F;
	float maxZ = -1.0F;

	for ( const auto & point : manifold.points() )
	{
		minX = std::min(minX, point.position()[X]);
		maxX = std::max(maxX, point.position()[X]);
		minZ = std::min(minZ, point.position()[Z]);
		maxZ = std::max(maxZ, point.position()[Z]);
	}

	EXPECT_GE(maxX - minX, 0.6F);
	EXPECT_GE(maxZ - minZ, 0.6F);
}

TEST(MathSpace3DContacts, featureIdsAreStableUnderASmallMove)
{
	ContactManifold< float > before;
	ContactManifold< float > after;

	ASSERT_TRUE(computeContactManifold(ground(), makeBox({0.0F, 0.49F, 0.0F}, {0.5F, 0.5F, 0.5F}), before));
	ASSERT_TRUE(computeContactManifold(ground(), makeBox({0.001F, 0.4895F, -0.0005F}, {0.5F, 0.5F, 0.5F}, {0.0F, 1.0F, 0.0F}, 0.002F), after));

	const auto idsBefore = sortedFeatureIds(before);
	const auto idsAfter = sortedFeatureIds(after);

	ASSERT_EQ(idsBefore.size(), 4U);
	EXPECT_EQ(idsBefore, idsAfter);
	/* Four distinct points. */
	EXPECT_EQ(std::ranges::adjacent_find(idsBefore), idsBefore.end());
}

TEST(MathSpace3DContacts, crossedEdgesGiveOneEdgePoint)
{
	/* A long bar along Z, its top edge up (turned 45° about Z), and above it a long bar along X with its bottom edge
	 * down (turned 45° about X): the two edges cross at the origin's vertical. */
	const float halfDiagonal = 0.5F * std::numbers::sqrt2_v< float >;
	const auto lower = makeBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 3.0F}, {0.0F, 0.0F, 1.0F}, std::numbers::pi_v< float > * 0.25F);
	const auto upper = makeBox({0.0F, (2.0F * halfDiagonal) - 0.02F, 0.0F}, {3.0F, 0.5F, 0.5F}, {1.0F, 0.0F, 0.0F}, std::numbers::pi_v< float > * 0.25F);

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(lower, upper, manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});

	const auto & point = manifold.points()[0];

	EXPECT_NE(point.featureId() & 0x80000000U, 0U);
	EXPECT_NEAR(point.depth(), 0.02F, Tolerance);
	EXPECT_NEAR(point.position()[X], 0.0F, Tolerance);
	EXPECT_NEAR(point.position()[Y], halfDiagonal - 0.01F, Tolerance);
	EXPECT_NEAR(point.position()[Z], 0.0F, Tolerance);
}

TEST(MathSpace3DContacts, deepPenetrationStaysUsable)
{
	/* The small box's centre sits inside the large one. */
	const auto large = makeBox({0.0F, 0.0F, 0.0F}, {2.0F, 2.0F, 2.0F});
	const auto small = makeBox({0.3F, 1.5F, 0.2F}, {0.5F, 0.5F, 0.5F}, {0.0F, 1.0F, 0.0F}, 0.3F);

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(large, small, manifold));
	ASSERT_FALSE(manifold.empty());
	/* The least penetrating axis is +Y: 2 + 0.5 - 1.5 = 1 m. */
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});
	EXPECT_NEAR(manifold.maximumDepth(), 1.0F, Tolerance);

	for ( const auto & point : manifold.points() )
	{
		EXPECT_TRUE(std::isfinite(point.position()[X]) && std::isfinite(point.position()[Y]) && std::isfinite(point.position()[Z]));
		EXPECT_GT(point.depth(), 0.0F);
	}
}

TEST(MathSpace3DContacts, flatBoxOnTheGround)
{
	/* A zero-thickness box (a 1 m square) lying 1 cm into the ground. */
	const auto square = makeBox({0.0F, -0.01F, 0.0F}, {0.5F, 0.0F, 0.5F});

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(ground(), square, manifold));
	ASSERT_EQ(manifold.points().size(), 4U);
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});
	EXPECT_NEAR(manifold.maximumDepth(), 0.01F, Tolerance);
}

TEST(MathSpace3DContacts, resultIsDeterministic)
{
	const auto boxA = makeBox({0.1F, 0.2F, 0.3F}, {0.7F, 0.4F, 0.9F}, Vec3{1.0F, 2.0F, 3.0F}.normalized(), 0.7F);
	const auto boxB = makeBox({0.5F, 0.9F, -0.2F}, {0.6F, 0.5F, 0.4F}, Vec3{-2.0F, 1.0F, 0.5F}.normalized(), 1.3F);

	ContactManifold< float > first;
	ContactManifold< float > second;

	ASSERT_TRUE(computeContactManifold(boxA, boxB, first));
	ASSERT_TRUE(computeContactManifold(boxA, boxB, second));
	ASSERT_EQ(first.points().size(), second.points().size());

	for ( size_t index = 0; index < first.points().size(); ++index )
	{
		EXPECT_EQ(first.points()[index].featureId(), second.points()[index].featureId());
		EXPECT_EQ(first.points()[index].depth(), second.points()[index].depth());
		EXPECT_EQ(first.points()[index].position(), second.points()[index].position());
	}
}

TEST(MathSpace3DContacts, manifoldRefusesAFifthPoint)
{
	ContactManifold< float > manifold;

	for ( uint32_t index = 0; index < 4; ++index )
	{
		EXPECT_TRUE(manifold.addPoint({Vec3{}, 0.1F, index}));
	}

	EXPECT_FALSE(manifold.addPoint({Vec3{}, 0.1F, 4U}));
	EXPECT_EQ(manifold.points().size(), 4U);

	manifold.setNormal({0.0F, 1.0F, 0.0F});
	manifold.flip();
	EXPECT_FLOAT_EQ(manifold.normal()[Y], -1.0F);

	manifold.clear();
	EXPECT_TRUE(manifold.empty());
}

/* ===== Sphere ↔ box ===== */

TEST(MathSpace3DContacts, sphereOnTheGroundGivesOnePointDown)
{
	const Sphere< float > sphere{0.5F, Vec3{1.0F, 0.4F, 2.0F}};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(sphere, ground(), manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	/* From the sphere (A) to the ground (B): down. */
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});

	const auto & point = manifold.points()[0];

	EXPECT_NEAR(point.depth(), 0.1F, Tolerance);
	EXPECT_NEAR(point.position()[X], 1.0F, Tolerance);
	/* Halfway between the sphere's bottom (-0.1) and the ground's top (0). */
	EXPECT_NEAR(point.position()[Y], -0.05F, Tolerance);
	EXPECT_NEAR(point.position()[Z], 2.0F, Tolerance);

	ContactManifold< float > swapped;

	ASSERT_TRUE(computeContactManifold(ground(), sphere, swapped));
	expectNormal(swapped, {0.0F, 1.0F, 0.0F});
	EXPECT_EQ(swapped.points()[0].featureId(), point.featureId());
}

TEST(MathSpace3DContacts, sphereAgainstABoxCorner)
{
	const auto box = makeBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F});
	const Sphere< float > sphere{0.6F, Vec3{0.8F, 0.8F, 0.8F}};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(sphere, box, manifold));
	ASSERT_EQ(manifold.points().size(), 1U);

	constexpr float inverseRoot3 = std::numbers::inv_sqrt3_v< float >;

	expectNormal(manifold, {-inverseRoot3, -inverseRoot3, -inverseRoot3});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.6F - (0.3F * std::numbers::sqrt3_v< float >), Tolerance);
}

TEST(MathSpace3DContacts, sphereSeparatedFromABoxGivesNoContact)
{
	ContactManifold< float > manifold;

	EXPECT_FALSE(computeContactManifold(Sphere< float >{0.5F, Vec3{0.0F, 0.6F, 0.0F}}, ground(), manifold));
	EXPECT_TRUE(manifold.empty());
	/* Near the corner: inside the corner's bounding slabs, yet farther than the radius from the corner. */
	EXPECT_FALSE(computeContactManifold(Sphere< float >{0.5F, Vec3{0.85F, 0.85F, 0.85F}}, makeBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}), manifold));
}

TEST(MathSpace3DContacts, sphereCentreInsideTheBoxLeavesByTheNearestFace)
{
	const auto box = makeBox({0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F});
	const Sphere< float > sphere{0.2F, Vec3{0.1F, 0.9F, -0.2F}};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(sphere, box, manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	/* It leaves upwards (+Y is the nearest face), so the normal from the sphere to the box points down. */
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.3F, Tolerance);
	EXPECT_NE(manifold.points()[0].featureId() & 0x100U, 0U);
}

TEST(MathSpace3DContacts, sphereAgainstARotatedBoxEdge)
{
	/* A cube turned 45° about Y: its vertical edge points along +X, at x = 0.5 √2. */
	const auto box = makeBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, {0.0F, 1.0F, 0.0F}, std::numbers::pi_v< float > * 0.25F);
	const float edgeX = 0.5F * std::numbers::sqrt2_v< float >;
	const Sphere< float > sphere{0.3F, Vec3{edgeX + 0.29F, 0.1F, 0.0F}};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(sphere, box, manifold));
	expectNormal(manifold, {-1.0F, 0.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.01F, Tolerance);
	EXPECT_NEAR(manifold.points()[0].position()[X], edgeX - 0.005F, Tolerance);
}

/* ===== Capsule ↔ box ===== */

TEST(MathSpace3DContacts, capsuleLyingOnTheGroundGivesTwoPoints)
{
	const Capsule< float > capsule{Vec3{-1.0F, 0.4F, 0.0F}, Vec3{1.0F, 0.4F, 0.0F}, 0.5F};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(capsule, ground(), manifold));
	ASSERT_EQ(manifold.points().size(), 2U);
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.1F, Tolerance);
		EXPECT_NEAR(std::abs(point.position()[X]), 1.0F, Tolerance);
		EXPECT_NEAR(point.position()[Y], -0.05F, Tolerance);
	}

	ContactManifold< float > swapped;

	ASSERT_TRUE(computeContactManifold(ground(), capsule, swapped));
	ASSERT_EQ(swapped.points().size(), 2U);
	expectNormal(swapped, {0.0F, 1.0F, 0.0F});
}

TEST(MathSpace3DContacts, capsuleOverhangingTheGroundIsClippedToIt)
{
	/* From x = 4 to x = 6 over a ground ending at x = 5. */
	const Capsule< float > capsule{Vec3{4.0F, 0.4F, 0.0F}, Vec3{6.0F, 0.4F, 0.0F}, 0.5F};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(capsule, ground(), manifold));
	ASSERT_EQ(manifold.points().size(), 2U);

	float minX = 10.0F;
	float maxX = -10.0F;

	for ( const auto & point : manifold.points() )
	{
		minX = std::min(minX, point.position()[X]);
		maxX = std::max(maxX, point.position()[X]);
	}

	EXPECT_NEAR(minX, 4.0F, Tolerance);
	EXPECT_NEAR(maxX, 5.0F, Tolerance);
}

TEST(MathSpace3DContacts, standingCapsuleGivesOnePoint)
{
	const Capsule< float > capsule{Vec3{0.3F, 0.4F, -0.2F}, Vec3{0.3F, 2.0F, -0.2F}, 0.5F};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(capsule, ground(), manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.1F, Tolerance);
	EXPECT_NEAR(manifold.points()[0].position()[X], 0.3F, Tolerance);
	EXPECT_NEAR(manifold.points()[0].position()[Z], -0.2F, Tolerance);
}

TEST(MathSpace3DContacts, tiltedCapsuleTouchesWithItsLowerEnd)
{
	const Capsule< float > capsule{Vec3{0.0F, 0.3F, 0.0F}, Vec3{1.0F, 1.3F, 0.0F}, 0.5F};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(capsule, ground(), manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.2F, Tolerance);
	EXPECT_NEAR(manifold.points()[0].position()[X], 0.0F, Tolerance);
}

TEST(MathSpace3DContacts, capsuleSeparatedFromABoxGivesNoContact)
{
	ContactManifold< float > manifold;

	EXPECT_FALSE(computeContactManifold(Capsule< float >{Vec3{-1.0F, 0.6F, 0.0F}, Vec3{1.0F, 0.6F, 0.0F}, 0.5F}, ground(), manifold));
	EXPECT_TRUE(manifold.empty());
}

TEST(MathSpace3DContacts, capsuleCrossingABoxUsesTheNearestFace)
{
	/* The segment runs THROUGH the cube, 0.1 under its top face: deep contact, the top face clipped to 2 points. */
	const auto box = makeBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F});
	const Capsule< float > capsule{Vec3{-2.0F, 0.4F, 0.0F}, Vec3{2.0F, 0.4F, 0.0F}, 0.2F};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(capsule, box, manifold));
	ASSERT_EQ(manifold.points().size(), 2U);
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.3F, Tolerance);
		EXPECT_NEAR(std::abs(point.position()[X]), 0.5F, Tolerance);
		EXPECT_NE(point.featureId() & 0x4000U, 0U);
	}
}

TEST(MathSpace3DContacts, capsuleAcrossARotatedBoxEdge)
{
	/* A cube turned 45° about Z: its top edge runs along Z at y = 0.5 √2. A thin capsule along X crosses it 0.057 lower. */
	const auto box = makeBox({0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}, std::numbers::pi_v< float > * 0.25F);
	const float edgeY = 0.5F * std::numbers::sqrt2_v< float >;
	const Capsule< float > capsule{Vec3{-1.0F, 0.65F, 0.0F}, Vec3{1.0F, 0.65F, 0.0F}, 0.1F};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(capsule, box, manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});

	const auto & point = manifold.points()[0];

	EXPECT_NE(point.featureId() & 0x8000U, 0U);
	EXPECT_NEAR(point.depth(), edgeY - 0.65F + 0.1F, Tolerance);
	EXPECT_NEAR(point.position()[X], 0.0F, Tolerance);
	EXPECT_NEAR(point.position()[Z], 0.0F, Tolerance);
}

TEST(MathSpace3DContacts, degenerateCapsuleActsAsASphere)
{
	const Capsule< float > capsule{Vec3{1.0F, 0.4F, 2.0F}, Vec3{1.0F, 0.4F, 2.0F}, 0.5F};
	const Sphere< float > sphere{0.5F, Vec3{1.0F, 0.4F, 2.0F}};

	ContactManifold< float > fromCapsule;
	ContactManifold< float > fromSphere;

	ASSERT_TRUE(computeContactManifold(capsule, ground(), fromCapsule));
	ASSERT_TRUE(computeContactManifold(sphere, ground(), fromSphere));
	ASSERT_EQ(fromCapsule.points().size(), 1U);
	EXPECT_NEAR(fromCapsule.points()[0].depth(), fromSphere.points()[0].depth(), Tolerance);
	EXPECT_NEAR(fromCapsule.points()[0].position()[Y], fromSphere.points()[0].position()[Y], Tolerance);
}

TEST(MathSpace3DContacts, capsuleOnARotatedBoxFaceMatchesTheExactDistance)
{
	/* A box tilted 30° about Z and a capsule above it at an arbitrary angle: the depth must equal radius minus the
	 * exact segment-box distance, checked against a dense sampling of the segment. */
	const auto box = makeBox({0.0F, 0.0F, 0.0F}, {1.0F, 0.3F, 0.8F}, {0.0F, 0.0F, 1.0F}, std::numbers::pi_v< float > / 6.0F);
	const Vec3 start{-0.9F, 0.6F, 0.4F};
	const Vec3 end{0.8F, 1.2F, -0.5F};
	const Capsule< float > capsule{start, end, 0.6F};

	float exact = 1.0e9F;

	for ( int step = 0; step <= 20000; ++step )
	{
		const Vec3 point = start + ((end - start) * (static_cast< float >(step) / 20000.0F));
		Vec3 clamped = box.center();

		for ( size_t index = 0; index < 3; ++index )
		{
			const float local = Vec3::dotProduct(point - box.center(), box.axis(index));

			clamped += box.axis(index) * std::clamp(local, -box.halfExtent(index), box.halfExtent(index));
		}

		exact = std::min(exact, (point - clamped).length());
	}

	ASSERT_GT(exact, 0.0F);
	ASSERT_LT(exact, 0.6F);

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(capsule, box, manifold));
	EXPECT_NEAR(manifold.maximumDepth(), 0.6F - exact, 1.0e-3F);
}

/* ===== Randomised properties (fixed seed, a local generator: the same draws on every platform) ===== */

namespace
{
	/** A 32-bit linear congruential generator (Numerical Recipes constants): reproducible everywhere. */
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
			unitVector () noexcept
			{
				const Vec3 vector{this->uniform(-1.0F, 1.0F), this->uniform(-1.0F, 1.0F), this->uniform(-1.0F, 1.0F) + 0.01F};

				return vector.normalized();
			}

			OrientedBox< float >
			box () noexcept
			{
				return makeBox(
					{this->uniform(-1.0F, 1.0F), this->uniform(-1.0F, 1.0F), this->uniform(-1.0F, 1.0F)},
					{this->uniform(0.1F, 1.0F), this->uniform(0.1F, 1.0F), this->uniform(0.1F, 1.0F)},
					this->unitVector(),
					this->uniform(0.0F, std::numbers::pi_v< float >)
				);
			}

		private:

			uint32_t m_state;
	};

	/** The distance of a point to a box (clamping in the box frame). */
	float
	pointBoxDistance (const Vec3 & point, const OrientedBox< float > & box)
	{
		Vec3 clamped = box.center();

		for ( size_t index = 0; index < 3; ++index )
		{
			const float local = Vec3::dotProduct(point - box.center(), box.axis(index));

			clamped += box.axis(index) * std::clamp(local, -box.halfExtent(index), box.halfExtent(index));
		}

		return (point - clamped).length();
	}

	void
	expectWellFormed (const ContactManifold< float > & manifold)
	{
		ASSERT_FALSE(manifold.empty());
		EXPECT_NEAR(manifold.normal().length(), 1.0F, 1.0e-3F);

		for ( const auto & point : manifold.points() )
		{
			EXPECT_TRUE(std::isfinite(point.position()[X]) && std::isfinite(point.position()[Y]) && std::isfinite(point.position()[Z]));
			EXPECT_TRUE(std::isfinite(point.depth()));
			EXPECT_GE(point.depth(), -1.0e-4F);
		}
	}
}

TEST(MathSpace3DContacts, randomBoxPairsAreWellFormed)
{
	Draw draw{0xC0FFEEU};
	size_t contacts = 0;

	for ( int trial = 0; trial < 2000; ++trial )
	{
		const auto boxA = draw.box();
		const auto boxB = draw.box();

		ContactManifold< float > manifold;

		if ( computeContactManifold(boxA, boxB, manifold) )
		{
			++contacts;

			expectWellFormed(manifold);
			/* Moving B along the normal by the deepest depth (plus a margin) must separate the boxes. */
			const auto movedB = OrientedBox< float >{boxB.center() + (manifold.normal() * (manifold.maximumDepth() + 1.0e-3F)), boxB.axes(), boxB.halfExtents()};
			ContactManifold< float > after;

			EXPECT_FALSE(computeContactManifold(boxA, movedB, after)) << "trial " << trial;
		}
	}

	/* The draw must exercise the code: most random pairs at this density overlap. */
	EXPECT_GT(contacts, 500U);
}

TEST(MathSpace3DContacts, randomSpheresMatchTheExactDistance)
{
	Draw draw{0x5EEDU};
	size_t shallow = 0;

	for ( int trial = 0; trial < 2000; ++trial )
	{
		const auto box = draw.box();
		const Sphere< float > sphere{draw.uniform(0.1F, 1.0F), Vec3{draw.uniform(-2.0F, 2.0F), draw.uniform(-2.0F, 2.0F), draw.uniform(-2.0F, 2.0F)}};
		const float distance = pointBoxDistance(sphere.position(), box);

		ContactManifold< float > manifold;
		const bool contact = computeContactManifold(sphere, box, manifold);

		if ( distance > sphere.radius() + 1.0e-4F )
		{
			EXPECT_FALSE(contact) << "trial " << trial;
		}
		else if ( distance > 1.0e-3F && distance < sphere.radius() - 1.0e-4F )
		{
			++shallow;

			ASSERT_TRUE(contact) << "trial " << trial;
			expectWellFormed(manifold);
			EXPECT_NEAR(manifold.maximumDepth(), sphere.radius() - distance, 1.0e-4F) << "trial " << trial;
		}
		else if ( contact )
		{
			expectWellFormed(manifold);
		}
	}

	EXPECT_GT(shallow, 200U);
}

TEST(MathSpace3DContacts, randomCapsulesMatchTheExactDistance)
{
	Draw draw{0xCA75U};
	size_t shallow = 0;

	for ( int trial = 0; trial < 2000; ++trial )
	{
		const auto box = draw.box();
		const Vec3 start{draw.uniform(-2.0F, 2.0F), draw.uniform(-2.0F, 2.0F), draw.uniform(-2.0F, 2.0F)};
		const Vec3 end = start + (draw.unitVector() * draw.uniform(0.0F, 1.5F));
		const Capsule< float > capsule{start, end, draw.uniform(0.1F, 0.8F)};

		/* The exact segment-box distance, sampled densely. */
		float distance = 1.0e9F;

		for ( int step = 0; step <= 2000; ++step )
		{
			distance = std::min(distance, pointBoxDistance(start + ((end - start) * (static_cast< float >(step) / 2000.0F)), box));
		}

		ContactManifold< float > manifold;
		const bool contact = computeContactManifold(capsule, box, manifold);

		if ( distance > capsule.radius() + 1.0e-3F )
		{
			EXPECT_FALSE(contact) << "trial " << trial;
		}
		else if ( distance > 1.0e-2F && distance < capsule.radius() - 1.0e-3F )
		{
			++shallow;

			ASSERT_TRUE(contact) << "trial " << trial;
			expectWellFormed(manifold);
			/* The deepest point is the closest one: radius minus the exact distance (sampling error ≤ 1 mm here). */
			EXPECT_NEAR(manifold.maximumDepth(), capsule.radius() - distance, 1.0e-3F) << "trial " << trial;
		}
		else if ( contact )
		{
			expectWellFormed(manifold);
		}
	}

	EXPECT_GT(shallow, 200U);
}

/* ===== Sphere / capsule ↔ triangle (two-sided) ===== */

namespace
{
	/** A triangle in the plane Y = 0, winding normal +Y: A(-1, 0, 1), B(1, 0, 1), C(0, 0, -1). */
	Triangle< float >
	floorTriangle ()
	{
		return Triangle< float >{Vec3{-1.0F, 0.0F, 1.0F}, Vec3{1.0F, 0.0F, 1.0F}, Vec3{0.0F, 0.0F, -1.0F}};
	}
}

TEST(MathSpace3DContacts, sphereAboveATriangleFace)
{
	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(Sphere< float >{0.5F, Vec3{0.0F, 0.4F, 0.0F}}, floorTriangle(), manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.1F, Tolerance);
	EXPECT_NEAR(manifold.points()[0].position()[Y], -0.05F, Tolerance);
	EXPECT_EQ(manifold.points()[0].featureId(), static_cast< uint32_t >(TriangleDetail::Region::Face));

	ContactManifold< float > swapped;

	ASSERT_TRUE(computeContactManifold(floorTriangle(), Sphere< float >{0.5F, Vec3{0.0F, 0.4F, 0.0F}}, swapped));
	expectNormal(swapped, {0.0F, 1.0F, 0.0F});
}

TEST(MathSpace3DContacts, sphereUnderATriangleIsPushedDown)
{
	/* Two-sided: from under the triangle, the sphere leaves downwards. */
	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(Sphere< float >{0.5F, Vec3{0.0F, -0.4F, 0.0F}}, floorTriangle(), manifold));
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.1F, Tolerance);
}

TEST(MathSpace3DContacts, sphereAgainstATriangleVertexAndEdge)
{
	ContactManifold< float > manifold;

	/* Near vertex A (-1, 0, 1). */
	const Vec3 nearVertex{-1.3F, 0.1F, 1.3F};
	const Vec3 offset = nearVertex - Vec3{-1.0F, 0.0F, 1.0F};

	ASSERT_TRUE(computeContactManifold(Sphere< float >{0.5F, nearVertex}, floorTriangle(), manifold));
	EXPECT_NEAR(manifold.points()[0].depth(), 0.5F - offset.length(), Tolerance);
	expectNormal(manifold, -offset.normalized());
	EXPECT_EQ(manifold.points()[0].featureId(), static_cast< uint32_t >(TriangleDetail::Region::VertexA));

	/* Beyond edge AB (z = 1). */
	ASSERT_TRUE(computeContactManifold(Sphere< float >{0.5F, Vec3{0.0F, 0.2F, 1.3F}}, floorTriangle(), manifold));
	EXPECT_NEAR(manifold.points()[0].depth(), 0.5F - std::hypot(0.2F, 0.3F), Tolerance);
	EXPECT_EQ(manifold.points()[0].featureId(), static_cast< uint32_t >(TriangleDetail::Region::EdgeAB));
}

TEST(MathSpace3DContacts, sphereAndTriangleSpecialCases)
{
	ContactManifold< float > manifold;

	EXPECT_FALSE(computeContactManifold(Sphere< float >{0.5F, Vec3{0.0F, 0.6F, 0.0F}}, floorTriangle(), manifold));

	/* A collinear triangle has no plane: no contact. */
	const Triangle< float > collinear{Vec3{0.0F, 0.0F, 0.0F}, Vec3{1.0F, 0.0F, 0.0F}, Vec3{2.0F, 0.0F, 0.0F}};

	EXPECT_FALSE(computeContactManifold(Sphere< float >{0.5F, Vec3{1.0F, 0.1F, 0.0F}}, collinear, manifold));

	/* A centre ON the triangle leaves along the winding normal. */
	ASSERT_TRUE(computeContactManifold(Sphere< float >{0.3F, Vec3{0.0F, 0.0F, 0.0F}}, floorTriangle(), manifold));
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.3F, Tolerance);
	EXPECT_EQ(manifold.points()[0].featureId(), 0x107U);
}

TEST(MathSpace3DContacts, capsuleLyingOnATriangleGivesTwoPoints)
{
	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(Capsule< float >{Vec3{-0.3F, 0.4F, 0.2F}, Vec3{0.3F, 0.4F, 0.2F}, 0.5F}, floorTriangle(), manifold));
	ASSERT_EQ(manifold.points().size(), 2U);
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.1F, Tolerance);
		EXPECT_NEAR(std::abs(point.position()[X]), 0.3F, Tolerance);
	}
}

TEST(MathSpace3DContacts, capsuleOverhangingATriangleIsClippedToItsPrism)
{
	/* At z = 0.2 the triangle spans x in [-0.6, 0.6]. */
	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(Capsule< float >{Vec3{-2.0F, 0.4F, 0.2F}, Vec3{2.0F, 0.4F, 0.2F}, 0.5F}, floorTriangle(), manifold));
	ASSERT_EQ(manifold.points().size(), 2U);

	float minX = 10.0F;
	float maxX = -10.0F;

	for ( const auto & point : manifold.points() )
	{
		minX = std::min(minX, point.position()[X]);
		maxX = std::max(maxX, point.position()[X]);
	}

	EXPECT_NEAR(minX, -0.6F, Tolerance);
	EXPECT_NEAR(maxX, 0.6F, Tolerance);
}

TEST(MathSpace3DContacts, capsuleStandingOnATriangle)
{
	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(Capsule< float >{Vec3{0.1F, 0.4F, 0.0F}, Vec3{0.1F, 2.0F, 0.0F}, 0.5F}, floorTriangle(), manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.1F, Tolerance);
	EXPECT_EQ(manifold.points()[0].featureId(), 0x2000U | static_cast< uint32_t >(TriangleDetail::Region::Face));
}

TEST(MathSpace3DContacts, piercingCapsuleIsPushedTowardsItsCentre)
{
	/* The former overlap test pushed a piercing capsule by its radius whatever its side: this one leaves towards the
	 * side holding its centre, by the depth of its end below the plane plus the radius. */
	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(Capsule< float >{Vec3{0.0F, -0.1F, 0.0F}, Vec3{0.0F, 1.5F, 0.0F}, 0.2F}, floorTriangle(), manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.3F, Tolerance);
	EXPECT_NE(manifold.points()[0].featureId() & 0x4000U, 0U);

	ASSERT_TRUE(computeContactManifold(Capsule< float >{Vec3{0.0F, 0.1F, 0.0F}, Vec3{0.0F, -1.5F, 0.0F}, 0.2F}, floorTriangle(), manifold));
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.3F, Tolerance);
}

TEST(MathSpace3DContacts, capsuleAcrossATriangleEdge)
{
	/* A sloping capsule beyond edge AB (z = 1) at z = 1.3, crossing the plane's height at x = 0.25: its closest point
	 * to the edge is inside the segment (0.3 away), not one of its ends (an end parallel to the edge would tie). */
	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(Capsule< float >{Vec3{-0.5F, 0.3F, 1.3F}, Vec3{0.5F, -0.1F, 1.3F}, 0.4F}, floorTriangle(), manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	EXPECT_NEAR(manifold.points()[0].depth(), 0.1F, Tolerance);
	EXPECT_NEAR(manifold.points()[0].position()[X], 0.25F, Tolerance);
	EXPECT_EQ(manifold.points()[0].featureId(), 0x2010U);

	EXPECT_FALSE(computeContactManifold(Capsule< float >{Vec3{-0.5F, 0.1F, 1.6F}, Vec3{0.5F, 0.1F, 1.6F}, 0.4F}, floorTriangle(), manifold));
}

TEST(MathSpace3DContacts, degenerateCapsuleOnATriangleActsAsASphere)
{
	ContactManifold< float > fromCapsule;
	ContactManifold< float > fromSphere;

	ASSERT_TRUE(computeContactManifold(Capsule< float >{Vec3{0.2F, 0.3F, 0.1F}, Vec3{0.2F, 0.3F, 0.1F}, 0.5F}, floorTriangle(), fromCapsule));
	ASSERT_TRUE(computeContactManifold(Sphere< float >{0.5F, Vec3{0.2F, 0.3F, 0.1F}}, floorTriangle(), fromSphere));
	EXPECT_NEAR(fromCapsule.points()[0].depth(), fromSphere.points()[0].depth(), Tolerance);
	expectNormal(fromCapsule, fromSphere.normal());
}

TEST(MathSpace3DContacts, randomSpheresMatchABruteForceTriangleDistance)
{
	Draw draw{0x7A1AU};
	size_t shallow = 0;

	for ( int trial = 0; trial < 500; ++trial )
	{
		const Triangle< float > triangle{
			Vec3{draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F)},
			Vec3{draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F)},
			Vec3{draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F)}
		};
		const Sphere< float > sphere{draw.uniform(0.1F, 1.0F), Vec3{draw.uniform(-1.5F, 1.5F), draw.uniform(-1.5F, 1.5F), draw.uniform(-1.5F, 1.5F)}};

		/* Brute force: a 300 × 300 barycentric grid over the triangle. */
		float distance = 1.0e9F;
		constexpr int Steps = 300;

		for ( int u = 0; u <= Steps; ++u )
		{
			for ( int v = 0; v <= Steps - u; ++v )
			{
				const float fu = static_cast< float >(u) / static_cast< float >(Steps);
				const float fv = static_cast< float >(v) / static_cast< float >(Steps);
				const Vec3 point = triangle.pointA() + ((triangle.pointB() - triangle.pointA()) * fu) + ((triangle.pointC() - triangle.pointA()) * fv);

				distance = std::min(distance, (point - sphere.position()).length());
			}
		}

		ContactManifold< float > manifold;
		const bool contact = computeContactManifold(sphere, triangle, manifold);

		/* The grid is coarser than the exact answer by at most ~1 cm on these triangles. */
		if ( distance > 0.02F && distance < sphere.radius() - 0.02F )
		{
			++shallow;

			ASSERT_TRUE(contact) << "trial " << trial;
			expectWellFormed(manifold);
			EXPECT_NEAR(manifold.maximumDepth(), sphere.radius() - distance, 0.012F) << "trial " << trial;
			EXPECT_LE(manifold.maximumDepth(), sphere.radius() - distance + 1.0e-4F) << "trial " << trial;
		}
		else if ( distance > sphere.radius() + 1.0e-4F )
		{
			EXPECT_FALSE(contact) << "trial " << trial;
		}
	}

	EXPECT_GT(shallow, 50U);
}

TEST(MathSpace3DContacts, randomCapsulesMatchTheExactTriangleDistance)
{
	Draw draw{0x7A1BU};
	size_t shallow = 0;

	for ( int trial = 0; trial < 2000; ++trial )
	{
		const Triangle< float > triangle{
			Vec3{draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F)},
			Vec3{draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F)},
			Vec3{draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F)}
		};
		const Vec3 start{draw.uniform(-1.5F, 1.5F), draw.uniform(-1.5F, 1.5F), draw.uniform(-1.5F, 1.5F)};
		const Vec3 end = start + (draw.unitVector() * draw.uniform(0.0F, 1.5F));
		const Capsule< float > capsule{start, end, draw.uniform(0.1F, 0.8F)};

		Vec3 normal;

		if ( !TriangleDetail::unitNormal(triangle, normal) )
		{
			continue;
		}

		/* Dense sampling of the segment against the exact point ↔ triangle distance (validated above). */
		float distance = 1.0e9F;

		for ( int step = 0; step <= 2000; ++step )
		{
			const Vec3 point = start + ((end - start) * (static_cast< float >(step) / 2000.0F));
			TriangleDetail::Region region{TriangleDetail::Region::Face};

			distance = std::min(distance, (TriangleDetail::closestPointOnTriangle(point, triangle, region) - point).length());
		}

		ContactManifold< float > manifold;
		const bool contact = computeContactManifold(capsule, triangle, manifold);

		if ( distance > 1.0e-2F && distance < capsule.radius() - 1.0e-3F )
		{
			++shallow;

			ASSERT_TRUE(contact) << "trial " << trial;
			expectWellFormed(manifold);
			EXPECT_NEAR(manifold.maximumDepth(), capsule.radius() - distance, 1.0e-3F) << "trial " << trial;
		}
		else if ( distance > capsule.radius() + 1.0e-3F )
		{
			EXPECT_FALSE(contact) << "trial " << trial;
		}
		else if ( contact )
		{
			expectWellFormed(manifold);
		}
	}

	EXPECT_GT(shallow, 200U);
}

/* ===== Round shapes: sphere ↔ sphere, sphere ↔ capsule, capsule ↔ capsule ===== */

TEST(MathSpace3DContacts, twoSpheres)
{
	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(Sphere< float >{0.5F, Vec3{0.0F, 0.0F, 0.0F}}, Sphere< float >{0.5F, Vec3{0.8F, 0.0F, 0.0F}}, manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	expectNormal(manifold, {1.0F, 0.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.2F, Tolerance);
	EXPECT_NEAR(manifold.points()[0].position()[X], 0.4F, Tolerance);

	EXPECT_FALSE(computeContactManifold(Sphere< float >{0.5F, Vec3{0.0F, 0.0F, 0.0F}}, Sphere< float >{0.5F, Vec3{1.1F, 0.0F, 0.0F}}, manifold));

	/* Coincident centres: the documented +Y fallback, the full reach as depth. */
	ASSERT_TRUE(computeContactManifold(Sphere< float >{0.5F, Vec3{1.0F, 2.0F, 3.0F}}, Sphere< float >{0.5F, Vec3{1.0F, 2.0F, 3.0F}}, manifold));
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 1.0F, Tolerance);
}

TEST(MathSpace3DContacts, sphereAgainstACapsuleCylinderAndCap)
{
	const Capsule< float > capsule{Vec3{-1.0F, 0.0F, 0.0F}, Vec3{1.0F, 0.0F, 0.0F}, 0.3F};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(Sphere< float >{0.4F, Vec3{0.5F, 0.6F, 0.0F}}, capsule, manifold));
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.1F, Tolerance);
	EXPECT_EQ(manifold.points()[0].featureId(), 3U);

	ASSERT_TRUE(computeContactManifold(Sphere< float >{0.3F, Vec3{1.5F, 0.0F, 0.0F}}, capsule, manifold));
	expectNormal(manifold, {-1.0F, 0.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.1F, Tolerance);
	EXPECT_EQ(manifold.points()[0].featureId(), 2U);

	ASSERT_TRUE(computeContactManifold(capsule, Sphere< float >{0.3F, Vec3{1.5F, 0.0F, 0.0F}}, manifold));
	expectNormal(manifold, {1.0F, 0.0F, 0.0F});
}

TEST(MathSpace3DContacts, crossedCapsulesGiveOnePoint)
{
	const Capsule< float > alongX{Vec3{-1.0F, 0.0F, 0.0F}, Vec3{1.0F, 0.0F, 0.0F}, 0.3F};
	const Capsule< float > alongZ{Vec3{0.0F, 0.5F, -1.0F}, Vec3{0.0F, 0.5F, 1.0F}, 0.3F};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(alongX, alongZ, manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});
	EXPECT_NEAR(manifold.points()[0].depth(), 0.1F, Tolerance);
	EXPECT_NEAR(manifold.points()[0].position()[Y], 0.25F, Tolerance);
	EXPECT_EQ(manifold.points()[0].featureId(), 0x33U);
}

TEST(MathSpace3DContacts, parallelCapsulesGiveTwoPoints)
{
	/* B lies 0.5 above A, overlapping it over x in [0, 1]. */
	const Capsule< float > capsuleA{Vec3{-1.0F, 0.0F, 0.0F}, Vec3{1.0F, 0.0F, 0.0F}, 0.3F};
	const Capsule< float > capsuleB{Vec3{2.0F, 0.5F, 0.0F}, Vec3{0.0F, 0.5F, 0.0F}, 0.3F};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(capsuleA, capsuleB, manifold));
	ASSERT_EQ(manifold.points().size(), 2U);
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});

	float minX = 10.0F;
	float maxX = -10.0F;

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.1F, Tolerance);
		minX = std::min(minX, point.position()[X]);
		maxX = std::max(maxX, point.position()[X]);
	}

	EXPECT_NEAR(minX, 0.0F, Tolerance);
	EXPECT_NEAR(maxX, 1.0F, Tolerance);
}

TEST(MathSpace3DContacts, capsulesWithCrossingAxesUseTheirCommonPerpendicular)
{
	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(Capsule< float >{Vec3{-1.0F, 0.0F, 0.0F}, Vec3{1.0F, 0.0F, 0.0F}, 0.3F}, Capsule< float >{Vec3{0.0F, 0.0F, -1.0F}, Vec3{0.0F, 0.0F, 1.0F}, 0.3F}, manifold));
	ASSERT_EQ(manifold.points().size(), 1U);
	EXPECT_NEAR(manifold.normal().length(), 1.0F, Tolerance);
	EXPECT_NEAR(std::abs(manifold.normal()[Y]), 1.0F, Tolerance);
	EXPECT_NEAR(manifold.points()[0].depth(), 0.6F, Tolerance);
}

TEST(MathSpace3DContacts, randomCapsulePairsMatchABruteForceDistance)
{
	Draw draw{0xCA2CU};
	size_t shallow = 0;

	for ( int trial = 0; trial < 500; ++trial )
	{
		const Vec3 startA{draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F)};
		const Vec3 endA = startA + (draw.unitVector() * draw.uniform(0.0F, 1.5F));
		const Vec3 startB{draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F), draw.uniform(-1.0F, 1.0F)};
		const Vec3 endB = startB + (draw.unitVector() * draw.uniform(0.0F, 1.5F));
		const Capsule< float > capsuleA{startA, endA, draw.uniform(0.1F, 0.6F)};
		const Capsule< float > capsuleB{startB, endB, draw.uniform(0.1F, 0.6F)};

		/* Brute force: both segments sampled, 300 × 300. */
		float distance = 1.0e9F;

		for ( int i = 0; i <= 300; ++i )
		{
			const Vec3 pointA = startA + ((endA - startA) * (static_cast< float >(i) / 300.0F));

			for ( int j = 0; j <= 300; ++j )
			{
				distance = std::min(distance, (pointA - (startB + ((endB - startB) * (static_cast< float >(j) / 300.0F)))).length());
			}
		}

		const float reach = capsuleA.radius() + capsuleB.radius();

		ContactManifold< float > manifold;
		const bool contact = computeContactManifold(capsuleA, capsuleB, manifold);

		if ( distance > 0.02F && distance < reach - 0.02F )
		{
			++shallow;

			ASSERT_TRUE(contact) << "trial " << trial;
			expectWellFormed(manifold);
			EXPECT_NEAR(manifold.maximumDepth(), reach - distance, 0.006F) << "trial " << trial;
			EXPECT_LE(manifold.maximumDepth(), reach - distance + 1.0e-4F) << "trial " << trial;
		}
		else if ( distance > reach + 1.0e-4F )
		{
			EXPECT_FALSE(contact) << "trial " << trial;
		}
	}

	EXPECT_GT(shallow, 100U);
}

/* ===== Box ↔ triangle (two-sided) ===== */

TEST(MathSpace3DContacts, boxRestingFlatOnATriangle)
{
	/* A 0.5 m box over the floor triangle, its bottom 1 cm into it, its footprint inside the triangle. */
	const auto box = makeBox({0.0F, 0.24F, 0.2F}, {0.25F, 0.25F, 0.25F});

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(box, floorTriangle(), manifold));
	ASSERT_EQ(manifold.points().size(), 4U);
	/* From the box to the triangle: down. */
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.01F, Tolerance);
		EXPECT_NEAR(std::abs(point.position()[X]), 0.25F, Tolerance);
	}

	ContactManifold< float > swapped;

	ASSERT_TRUE(computeContactManifold(floorTriangle(), box, swapped));
	expectNormal(swapped, {0.0F, 1.0F, 0.0F});
}

TEST(MathSpace3DContacts, boxOnTheLowPartOfALargeSlopedTriangle)
{
	/* A 20 m triangle on a 20° slope, the box resting near its LOW corner: the triangle's centroid stands far above
	 * the box — the side must come from the projections, not from the centroid. */
	const float angle = std::numbers::pi_v< float > / 9.0F;
	const Vec3 slopeX{std::cos(angle), std::sin(angle), 0.0F};
	const Vec3 up{-std::sin(angle), std::cos(angle), 0.0F};
	const Vec3 lowCorner{0.0F, 0.0F, 0.0F};
	const Triangle< float > slope{lowCorner + (Vec3{0.0F, 0.0F, 10.0F}), lowCorner + (slopeX * 20.0F), lowCorner + (Vec3{0.0F, 0.0F, -10.0F})};
	/* The box aligned with the slope, 1 cm into it, 2 m up the slope from the low edge. */
	const Vec3 downSlope{std::cos(angle), std::sin(angle), 0.0F};
	const Vec3 center = (downSlope * 2.0F) + (up * (0.25F - 0.01F));
	const OrientedBox< float > box{center, {downSlope, up, Vec3{0.0F, 0.0F, 1.0F}}, Vec3{0.25F, 0.25F, 0.25F}};

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(box, slope, manifold));
	ASSERT_EQ(manifold.points().size(), 4U);
	expectNormal(manifold, -up);

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.01F, Tolerance);
	}
}

TEST(MathSpace3DContacts, boxStraddlingATriangleEdgeKeepsThePointsOverIt)
{
	/* The box straddles edge AB (z = 1): only the corners over the triangle are kept, inside its prism. */
	const auto box = makeBox({0.0F, 0.24F, 1.0F}, {0.25F, 0.25F, 0.25F});

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(box, floorTriangle(), manifold));
	ASSERT_FALSE(manifold.empty());
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});

	for ( const auto & point : manifold.points() )
	{
		EXPECT_LE(point.position()[Z], 1.0F + Tolerance);
		EXPECT_NEAR(point.depth(), 0.01F, Tolerance);
	}
}

TEST(MathSpace3DContacts, largeBoxOnASmallTriangleTouchesItsCorners)
{
	/* A 4 m box sitting on the 2 m floor triangle: the contact is the triangle itself, its 3 corners. */
	const auto box = makeBox({0.0F, 1.99F, 0.0F}, {2.0F, 2.0F, 2.0F});

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(box, floorTriangle(), manifold));
	ASSERT_EQ(manifold.points().size(), 3U);
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.01F, Tolerance);
	}
}

TEST(MathSpace3DContacts, boxOnAnEdgeOnATriangle)
{
	const float halfDiagonal = 0.25F * std::numbers::sqrt2_v< float >;
	const auto box = makeBox({0.0F, halfDiagonal - 0.01F, 0.0F}, {0.25F, 0.25F, 0.25F}, {0.0F, 0.0F, 1.0F}, std::numbers::pi_v< float > * 0.25F);

	ContactManifold< float > manifold;

	ASSERT_TRUE(computeContactManifold(box, floorTriangle(), manifold));
	ASSERT_EQ(manifold.points().size(), 2U);
	expectNormal(manifold, {0.0F, -1.0F, 0.0F});

	for ( const auto & point : manifold.points() )
	{
		EXPECT_NEAR(point.depth(), 0.01F, Tolerance);
	}
}

TEST(MathSpace3DContacts, boxUnderATriangleAndSpecialCases)
{
	ContactManifold< float > manifold;

	/* Two-sided: from under the triangle, pushed down. */
	ASSERT_TRUE(computeContactManifold(makeBox({0.0F, -0.24F, 0.2F}, {0.25F, 0.25F, 0.25F}), floorTriangle(), manifold));
	expectNormal(manifold, {0.0F, 1.0F, 0.0F});

	EXPECT_FALSE(computeContactManifold(makeBox({0.0F, 0.26F, 0.2F}, {0.25F, 0.25F, 0.25F}), floorTriangle(), manifold));

	const Triangle< float > collinear{Vec3{0.0F, 0.0F, 0.0F}, Vec3{1.0F, 0.0F, 0.0F}, Vec3{2.0F, 0.0F, 0.0F}};

	EXPECT_FALSE(computeContactManifold(makeBox({1.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}), collinear, manifold));
}

TEST(MathSpace3DContacts, randomBoxTrianglePairsSeparateAlongTheNormal)
{
	Draw draw{0xB07AU};
	size_t contacts = 0;

	for ( int trial = 0; trial < 2000; ++trial )
	{
		const auto box = draw.box();
		const Triangle< float > triangle{
			Vec3{draw.uniform(-1.5F, 1.5F), draw.uniform(-1.5F, 1.5F), draw.uniform(-1.5F, 1.5F)},
			Vec3{draw.uniform(-1.5F, 1.5F), draw.uniform(-1.5F, 1.5F), draw.uniform(-1.5F, 1.5F)},
			Vec3{draw.uniform(-1.5F, 1.5F), draw.uniform(-1.5F, 1.5F), draw.uniform(-1.5F, 1.5F)}
		};

		ContactManifold< float > manifold;

		if ( !computeContactManifold(box, triangle, manifold) )
		{
			continue;
		}

		++contacts;

		expectWellFormed(manifold);

		/* Moving the box (A) back along the normal by the deepest depth separates it. */
		const OrientedBox< float > moved{box.center() - (manifold.normal() * (manifold.maximumDepth() + 1.0e-3F)), box.axes(), box.halfExtents()};
		ContactManifold< float > after;

		EXPECT_FALSE(computeContactManifold(moved, triangle, after)) << "trial " << trial;
	}

	EXPECT_GT(contacts, 300U);
}
