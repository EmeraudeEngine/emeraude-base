/*
 * src/Testing/test_VertexFactoryShapeSimplifier.cpp
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

#include <gtest/gtest.h>

/* STL inclusions. */
#include <cmath>
#include <vector>

/* Local inclusions. */
#include "VertexFactory/ShapeDecimator.hpp"
#include "VertexFactory/ShapeGenerator.hpp"
#include "VertexFactory/ShapeSimplifier.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::VertexFactory;

TEST(VertexFactoryShapeSimplifier, RefusesBadInput)
{
	const Shape< float > empty;

	EXPECT_FALSE(simplifyShape(empty).has_value());

	const auto plane = ShapeGenerator::generatePlane< float, uint32_t >(10.0F, 10.0F, 8U, 8U);

	EXPECT_FALSE(simplifyShape(plane, {.targetRatio = 0.0F}).has_value());
	EXPECT_FALSE(simplifyShape(plane, {.targetRatio = 1.5F}).has_value());
	EXPECT_FALSE(simplifyShape(plane, {.targetRatio = std::nanf("")}).has_value());
	EXPECT_FALSE(simplifyShape(plane, {.targetRatio = 0.5F, .targetError = -1.0F}).has_value());
}

TEST(VertexFactoryShapeSimplifier, FlatPlaneCollapsesToFewTriangles)
{
	/* A flat grid carries no information beyond its outline: the quadric pass may remove almost everything. */
	const auto plane = ShapeGenerator::generatePlane< float, uint32_t >(10.0F, 10.0F, 32U, 32U);
	const auto simplified = simplifyShape(plane, {.targetRatio = 0.1F, .targetError = 0.01F});

	ASSERT_TRUE(simplified.has_value());
	EXPECT_LE(simplified->triangles().size(), plane.triangles().size() / 10 + 2);
	EXPECT_GE(simplified->triangles().size(), 2U);

	/* The outline is kept: the same bounding box. */
	EXPECT_NEAR(simplified->boundingBox().width(), plane.boundingBox().width(), 1e-4F);
	EXPECT_NEAR(simplified->boundingBox().depth(), plane.boundingBox().depth(), 1e-4F);
}

TEST(VertexFactoryShapeSimplifier, SphereKeepsItsShape)
{
	const auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 64U, 32U);
	const auto simplified = simplifyShape(sphere, {.targetRatio = 0.25F, .targetError = 0.05F});

	ASSERT_TRUE(simplified.has_value());
	EXPECT_LT(simplified->triangles().size(), sphere.triangles().size() / 2);

	/* Every kept vertex is a source vertex: still on the unit sphere. */
	for ( const auto & vertex : simplified->vertices() )
	{
		EXPECT_NEAR(vertex.position().length(), 1.0F, 1e-4F);
	}

	/* No index out of range. */
	for ( const auto & triangle : simplified->triangles() )
	{
		for ( uint32_t corner = 0; corner < 3; ++corner )
		{
			EXPECT_LT(triangle.vertexIndex(corner), simplified->vertices().size());
		}
	}
}

TEST(VertexFactoryShapeSimplifier, GroupsAreKeptOneByOne)
{
	/* Two groups: a multi-material mesh must keep both sub-geometries on every level. */
	auto shape = ShapeGenerator::generatePlane< float, uint32_t >(10.0F, 10.0F, 16U, 16U);
	const auto triangleCount = static_cast< uint32_t >(shape.triangles().size());
	const auto half = triangleCount / 2;

	shape.groups().clear();
	shape.groups().emplace_back(0U, half);
	shape.groups().emplace_back(half, triangleCount - half);

	const auto simplified = simplifyShape(shape, {.targetRatio = 0.2F, .targetError = 0.01F});

	ASSERT_TRUE(simplified.has_value());
	ASSERT_EQ(simplified->groups().size(), 2U);
	EXPECT_EQ(simplified->groups()[0].first, 0U);
	EXPECT_GT(simplified->groups()[0].second, 0U);
	EXPECT_EQ(simplified->groups()[1].first, simplified->groups()[0].second);
	EXPECT_EQ(simplified->groups()[0].second + simplified->groups()[1].second, simplified->triangles().size());
}

TEST(VertexFactoryShapeSimplifier, EmptyGroupStaysEmpty)
{
	/* JungleRuins' trees carry an empty group: it must survive as an empty group, never refuse the level. */
	auto shape = ShapeGenerator::generatePlane< float, uint32_t >(10.0F, 10.0F, 16U, 16U);
	const auto triangleCount = static_cast< uint32_t >(shape.triangles().size());

	shape.groups().clear();
	shape.groups().emplace_back(0U, 0U);
	shape.groups().emplace_back(0U, triangleCount);

	const auto simplified = simplifyShape(shape, {.targetRatio = 0.2F, .targetError = 0.01F});

	ASSERT_TRUE(simplified.has_value());
	ASSERT_EQ(simplified->groups().size(), 2U);
	EXPECT_EQ(simplified->groups()[0].second, 0U);
	EXPECT_EQ(simplified->groups()[1].first, 0U);
	EXPECT_EQ(simplified->groups()[1].second, simplified->triangles().size());
	EXPECT_LT(simplified->triangles().size(), triangleCount);
}

/*
 * The decimator rebuilds its output vertices with saveVertex() (handedness +1): until 2026-10-06 it copied only the
 * 3D tangent, so every LOD level of a mirrored UV island (handedness -1) lit its normal map backwards.
 * Since 2026-10-07 the computed frame derives the handedness from the UV winding, and the decimator recomputes the
 * frame of its output: the island must be GENUINELY mirrored (its U reversed), not a sphere tagged -1 by hand.
 */
TEST(VertexFactoryShapeDecimator, decimatedVerticesKeepTheTangentHandedness)
{
	auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 32, 16, ShapeBuilderOptions< float >{true, true, false, false, false});

	for ( auto & vertex : sphere.vertices() )
	{
		auto textureCoordinates = vertex.textureCoordinates();
		textureCoordinates[EmEn::Base::Math::X] = 1.0F - textureCoordinates[EmEn::Base::Math::X];

		vertex.setTextureCoordinates(textureCoordinates);
	}

	ASSERT_TRUE(sphere.computeTriangleTangent());
	ASSERT_TRUE(sphere.computeVertexTangent());

	for ( const auto & vertex : sphere.vertices() )
	{
		ASSERT_EQ(vertex.tangentHandedness(), -1.0F) << "the source island must be mirrored";
	}

	const ShapeDecimator< float, uint32_t > decimator{sphere, 0.5F};
	const auto decimated = decimator.decimate();

	ASSERT_GT(decimated.vertices().size(), 0U);
	ASSERT_LT(decimated.triangles().size(), sphere.triangles().size());

	/* The decimator recomputes the frame of its output from the UVs, and folds a few of them over (27 of 448 triangles
	 * here, measured 2026-10-07): a vertex next to a fold-over may side with it. A vertex whose triangles are ALL on the
	 * island's mirrored side must be -1. */
	std::vector< bool > touchesAnUnmirroredTriangle(decimated.vertices().size(), false);

	for ( const auto & triangle : decimated.triangles() )
	{
		if ( triangle.surfaceTangentHandedness() > 0.0F )
		{
			for ( uint32_t corner = 0; corner < 3; ++corner )
			{
				touchesAnUnmirroredTriangle[triangle.vertexIndex(corner)] = true;
			}
		}
	}

	size_t inside = 0;
	size_t lost = 0;

	for ( size_t index = 0; index < decimated.vertices().size(); ++index )
	{
		if ( touchesAnUnmirroredTriangle[index] )
		{
			continue;
		}

		++inside;

		if ( decimated.vertices()[index].tangentHandedness() != -1.0F )
		{
			++lost;
		}
	}

	EXPECT_GT(inside * 10, decimated.vertices().size() * 8) << "most of the island must stay mirrored";
	EXPECT_EQ(lost, 0U) << lost << " of " << inside << " vertices inside the mirrored island lost their handedness in the decimation.";
}
