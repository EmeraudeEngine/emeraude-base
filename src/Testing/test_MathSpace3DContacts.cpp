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
#include "Math/Space3D/Contacts/BoxBox.hpp"
#include "Math/Space3D/Contacts/ContactManifold.hpp"
#include "Math/Space3D/OrientedBox.hpp"
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
