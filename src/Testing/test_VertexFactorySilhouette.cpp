/*
 * src/Testing/test_VertexFactorySilhouette.cpp
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
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>

/* Local inclusions. */
#include "Math/Vector.hpp"
#include "VertexFactory/Shape.hpp"
#include "VertexFactory/ShapeGenerator.hpp"
#include "VertexFactory/Silhouette.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::VertexFactory;

namespace
{
	using Vector3 = Math::Vector< 3, float >;

	/**
	 * @brief Returns true when every endpoint position of the silhouette is used by exactly two edges, i.e.
	 * the edges form closed loops across the welded seams.
	 */
	bool
	formsClosedLoops (const Shape< float, uint32_t > & shape, const Silhouette< float, uint32_t > & silhouette)
	{
		std::map< std::array< float, 3 >, int > useCount;

		for ( const auto & edge : silhouette.edges() )
		{
			for ( const auto vertexIndex : {edge.vertexIndexA(), edge.vertexIndexB()} )
			{
				const auto & position = shape.vertices()[vertexIndex].position();

				++useCount[{position.x(), position.y(), position.z()}];
			}
		}

		for ( const auto & [position, count] : useCount )
		{
			if ( count != 2 )
			{
				return false;
			}
		}

		return !useCount.empty();
	}
}

/* Ave robustus! (Axis B — correction marker): build() tested `if ( geometry.isValid() )` and returned
 * false, so it refused every valid shape and processed empty ones. */
TEST(VertexFactorySilhouette, emptyShapeIsRefusedAndValidShapeAccepted)
{
	Silhouette< float, uint32_t > silhouette;

	EXPECT_FALSE(silhouette.prepare(Shape< float, uint32_t >{}));
	EXPECT_FALSE(silhouette.build(Vector3{0.0F, 0.0F, 10.0F}));
	EXPECT_TRUE(silhouette.edges().empty());

	const auto cube = ShapeGenerator::generateCuboid< float, uint32_t >(1.0F);

	EXPECT_TRUE(silhouette.prepare(cube));
	EXPECT_TRUE(silhouette.build(Vector3{0.0F, 0.0F, 10.0F}));
}

/* Seen face-on, the silhouette of a cube is the outline of its front face: the old code returned the
 * three edges of every front triangle (the wireframe, diagonal included). The edges must come from the
 * front face vertices (normal +Z, the visible side of the seam) and run counter-clockwise seen from +Z. */
TEST(VertexFactorySilhouette, cubeSeenFaceOnGivesTheFrontFaceOutline)
{
	const auto cube = ShapeGenerator::generateCuboid< float, uint32_t >(1.0F);

	Silhouette< float, uint32_t > silhouette;

	ASSERT_TRUE(silhouette.prepare(cube));
	ASSERT_TRUE(silhouette.build(Vector3{0.0F, 0.0F, 10.0F}));

	ASSERT_EQ(silhouette.edges().size(), 4U);
	EXPECT_TRUE(formsClosedLoops(cube, silhouette));

	const Vector3 faceCenter{0.0F, 0.0F, 0.5F};

	for ( const auto & edge : silhouette.edges() )
	{
		const auto & vertexA = cube.vertices()[edge.vertexIndexA()];
		const auto & vertexB = cube.vertices()[edge.vertexIndexB()];

		EXPECT_FLOAT_EQ(vertexA.position().z(), 0.5F);
		EXPECT_FLOAT_EQ(vertexB.position().z(), 0.5F);
		EXPECT_FLOAT_EQ(vertexA.normal().z(), 1.0F) << "edge not taken from the visible side of the seam";
		EXPECT_FLOAT_EQ(vertexB.normal().z(), 1.0F) << "edge not taken from the visible side of the seam";

		const auto turn = Vector3::crossProduct(vertexA.position() - faceCenter, vertexB.position() - faceCenter);

		EXPECT_GT(turn.z(), 0.0F) << "edge not counter-clockwise seen from the viewer";
	}
}

/* The cuboid has one set of vertices per face, so Shape::edges() pairs no edge between two faces. Without
 * welding by position, every front face outline would be reported (9 edges); the true contour seen from
 * a corner is a hexagon that avoids the nearest and the farthest corners. */
TEST(VertexFactorySilhouette, cubeSeenFromACornerGivesAHexagonAcrossTheSeams)
{
	const auto cube = ShapeGenerator::generateCuboid< float, uint32_t >(1.0F);

	Silhouette< float, uint32_t > silhouette;

	ASSERT_TRUE(silhouette.prepare(cube));
	ASSERT_TRUE(silhouette.build(Vector3{10.0F, 10.0F, 10.0F}));

	ASSERT_EQ(silhouette.edges().size(), 6U);
	EXPECT_TRUE(formsClosedLoops(cube, silhouette));

	for ( const auto & edge : silhouette.edges() )
	{
		for ( const auto vertexIndex : {edge.vertexIndexA(), edge.vertexIndexB()} )
		{
			const auto & position = cube.vertices()[vertexIndex].position();
			const auto signSum = position.x() + position.y() + position.z();

			EXPECT_LT(std::abs(signSum), 1.0F) << "the contour passes through the nearest or the farthest corner";
		}
	}
}

/* A perspective viewer close to a sphere sees less than a hemisphere: from (0, 3, 0) the contour lies at
 * the latitude where the facing flips, the ring y = cos(3π/8) of a 16x8 sphere. An orthographic viewer
 * sees exactly a hemisphere: the equator ring. The old code used the forward vector for every triangle,
 * i.e. the orthographic answer whatever the distance. */
TEST(VertexFactorySilhouette, sphereContourDependsOnThePerspective)
{
	const auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 16U, 8U);

	Silhouette< float, uint32_t > silhouette;

	ASSERT_TRUE(silhouette.prepare(sphere));

	ASSERT_TRUE(silhouette.build(Vector3{0.0F, 3.0F, 0.0F}));
	ASSERT_EQ(silhouette.edges().size(), 16U);
	EXPECT_TRUE(formsClosedLoops(sphere, silhouette)) << "the UV seam must be welded";

	const auto ringY = std::cos(3.0F * std::numbers::pi_v< float > / 8.0F);

	for ( const auto & edge : silhouette.edges() )
	{
		EXPECT_NEAR(sphere.vertices()[edge.vertexIndexA()].position().y(), ringY, 1.0e-5F);
		EXPECT_NEAR(sphere.vertices()[edge.vertexIndexB()].position().y(), ringY, 1.0e-5F);
	}

	ASSERT_TRUE(silhouette.buildOrthographic(Vector3{0.0F, -1.0F, 0.0F}));
	ASSERT_EQ(silhouette.edges().size(), 16U);
	EXPECT_TRUE(formsClosedLoops(sphere, silhouette));

	for ( const auto & edge : silhouette.edges() )
	{
		EXPECT_NEAR(sphere.vertices()[edge.vertexIndexA()].position().y(), 0.0F, 1.0e-5F);
		EXPECT_NEAR(sphere.vertices()[edge.vertexIndexB()].position().y(), 0.0F, 1.0e-5F);
	}
}

/* An open quad (normal +Z, counter-clockwise around +Z) has only boundary edges: all four are kept from
 * its front, none from behind. This also pins the facing sign against the generator's declared normal. */
TEST(VertexFactorySilhouette, openQuadBoundaryIsKeptOnlyFromItsFront)
{
	const auto quad = ShapeGenerator::generateQuad< float, uint32_t >(1.0F, 1.0F);

	Silhouette< float, uint32_t > silhouette;

	ASSERT_TRUE(silhouette.prepare(quad));

	ASSERT_TRUE(silhouette.build(Vector3{0.0F, 0.0F, 5.0F}));
	EXPECT_EQ(silhouette.edges().size(), 4U);
	EXPECT_TRUE(formsClosedLoops(quad, silhouette));

	ASSERT_TRUE(silhouette.build(Vector3{0.0F, 0.0F, -5.0F}));
	EXPECT_TRUE(silhouette.edges().empty());

	ASSERT_TRUE(silhouette.buildOrthographic(Vector3{0.0F, 0.0F, -1.0F}));
	EXPECT_EQ(silhouette.edges().size(), 4U);
}
