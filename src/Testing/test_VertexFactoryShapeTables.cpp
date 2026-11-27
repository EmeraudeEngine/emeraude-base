/*
 * src/Testing/test_VertexFactoryShapeTables.cpp
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
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <set>
#include <string>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Math/Plane.hpp"
#include "Math/Vector.hpp"
#include "VertexFactory/Shape.hpp"
#include "VertexFactory/ShapeBuilder.hpp"
#include "VertexFactory/ShapeBuilderOptions.hpp"
#include "VertexFactory/ShapeGenerator.hpp"
#include "VertexFactory/ShapeProcessor.hpp"
#include "VertexFactory/ShapeSplitter.hpp"
#include "VertexFactory/ShapeTriangle.hpp"
#include "VertexFactory/ShapeVertex.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::VertexFactory;

/* The attribute tables of a Shape (2026-11-26): the skinning and the triangles' color and edge indexes left ShapeVertex
 * (92 -> 60 bytes) and ShapeTriangle (64 -> 40 bytes) for tables of the Shape, allocated by the first value that is not
 * the default. Every pass that renumbers vertices or triangles must carry them. */
namespace
{
	using V3 = Math::Vector< 3, float >;
	using Influences = Math::Vector< 4, int32_t >;
	using Weights = Math::Vector< 4, float >;

	/**
	 * @brief The skinning a test gives a vertex from its position: every copy of a position gets the same one, so a
	 * pass that merges or renumbers vertices can be checked vertex by vertex.
	 * @param position The vertex position.
	 * @return Influences
	 */
	Influences
	influencesAt (const V3 & position) noexcept
	{
		const auto octant = (position[Math::X] > 0.0F ? 1 : 0) + (position[Math::Y] > 0.0F ? 2 : 0) + (position[Math::Z] > 0.0F ? 4 : 0);

		return Influences{octant, 8 + octant, -1, -1};
	}

	/**
	 * @brief Returns the (vertex index, color index) pairs of a triangle's corners.
	 * @param shape The shape.
	 * @param triangleIndex The triangle.
	 * @return std::set< std::pair< uint32_t, uint32_t > >
	 */
	std::set< std::pair< uint32_t, uint32_t > >
	cornerPairs (const Shape< float, uint32_t > & shape, uint32_t triangleIndex) noexcept
	{
		std::set< std::pair< uint32_t, uint32_t > > pairs;

		for ( uint32_t corner = 0; corner < 3; ++corner )
		{
			pairs.emplace(shape.triangles()[triangleIndex].vertexIndex(corner), shape.triangleVertexColorIndex(triangleIndex, corner));
		}

		return pairs;
	}
}

TEST(VertexFactoryShapeTables, aShapeWithoutSkinningAnswersTheDefaults)
{
	const auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 8U, 4U);

	ASSERT_FALSE(shape.vertices().empty());
	EXPECT_FALSE(shape.isSkinningAvailable());
	EXPECT_EQ(shape.vertexInfluences(0), (Influences{-1, -1, -1, -1}));
	EXPECT_EQ(shape.vertexWeights(0), (Weights{0.0F, 0.0F, 0.0F, 0.0F}));

	/* Past the table, the same defaults: never an out-of-bounds read. */
	const auto past = static_cast< uint32_t >(shape.vertices().size() + 10);
	EXPECT_EQ(shape.vertexInfluences(past), (Influences{-1, -1, -1, -1}));
	EXPECT_EQ(shape.triangleVertexColorIndex(static_cast< uint32_t >(shape.triangles().size() + 10), 0), 0U);
}

/* A table holding only defaults is not allocated: a shape without skinning stays without it, whatever a loader or the
 * builder sets. */
TEST(VertexFactoryShapeTables, settingTheDefaultsAllocatesNoTable)
{
	auto shape = ShapeGenerator::generateCuboid< float, uint32_t >(1.0F, 1.0F, 1.0F);

	const auto before = shape.memoryOccupied();

	shape.setVertexSkinning(0, Influences{-1, -1, -1, -1}, Weights{0.0F, 0.0F, 0.0F, 0.0F});

	EXPECT_FALSE(shape.isSkinningAvailable());
	EXPECT_EQ(shape.memoryOccupied(), before);

	/* The first value that is not the default allocates the table for every vertex, the others at the defaults. */
	shape.setVertexSkinning(2, Influences{4, -1, -1, -1}, Weights{1.0F, 0.0F, 0.0F, 0.0F});

	ASSERT_TRUE(shape.isSkinningAvailable());
	EXPECT_EQ(shape.vertexInfluences(2), (Influences{4, -1, -1, -1}));
	EXPECT_EQ(shape.vertexInfluences(1), (Influences{-1, -1, -1, -1}));
	EXPECT_EQ(shape.vertexWeights(1), (Weights{0.0F, 0.0F, 0.0F, 0.0F}));
	EXPECT_GE(shape.memoryOccupied(), before + (shape.vertices().size() * (sizeof(Influences) + sizeof(Weights))));
}

/* The builder writes the skinning declared for each corner. */
TEST(VertexFactoryShapeTables, theBuilderWritesTheSkinningOfEachCorner)
{
	Shape< float, uint32_t > shape;
	ShapeBuilder< float, uint32_t > builder{shape};

	builder.options().enableGlobalNormal(V3::positiveZ());
	builder.beginConstruction(ConstructionMode::Triangles);

	builder.setPosition(0.0F, 0.0F, 0.0F);   builder.setInfluences(0, -1, -1, -1);   builder.setWeights(1.0F, 0.0F, 0.0F, 0.0F);   builder.newVertex();
	builder.setPosition(1.0F, 0.0F, 0.0F);   builder.setInfluences(1, 2, -1, -1);    builder.setWeights(0.5F, 0.5F, 0.0F, 0.0F);   builder.newVertex();
	builder.setPosition(0.0F, 1.0F, 0.0F);   builder.setInfluences(3, -1, -1, -1);   builder.setWeights(1.0F, 0.0F, 0.0F, 0.0F);   builder.newVertex();

	builder.endConstruction();

	ASSERT_EQ(shape.triangles().size(), 1U);
	ASSERT_TRUE(shape.isSkinningAvailable());

	const auto & triangle = shape.triangles()[0];

	EXPECT_EQ(shape.vertexInfluences(triangle.vertexIndex(0)), (Influences{0, -1, -1, -1}));
	EXPECT_EQ(shape.vertexInfluences(triangle.vertexIndex(1)), (Influences{1, 2, -1, -1}));
	EXPECT_EQ(shape.vertexWeights(triangle.vertexIndex(1)), (Weights{0.5F, 0.5F, 0.0F, 0.0F}));
	EXPECT_EQ(shape.vertexInfluences(triangle.vertexIndex(2)), (Influences{3, -1, -1, -1}));
}

/* ShapeProcessor::deduplicateVertices() replaces the vertex list: the skinning table must follow the renumbering
 * (Shape::remapVertexAttributes()), or a merged vertex would take the skinning of whatever vertex sat at its new index. */
TEST(VertexFactoryShapeTables, deduplicationKeepsEachVertexSkinning)
{
	ShapeBuilderOptions< float > options;
	options.enableDataEconomy(false);

	auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 16U, 8U, options);

	for ( uint32_t vertexIndex = 0; vertexIndex < shape.vertices().size(); ++vertexIndex )
	{
		shape.setVertexSkinning(vertexIndex, influencesAt(shape.vertices()[vertexIndex].position()), Weights{0.5F, 0.5F, 0.0F, 0.0F});
	}

	ShapeProcessor< float, uint32_t > processor{shape};

	ASSERT_GT(processor.deduplicateVertices(), 0U) << "nothing was merged, the check would be vacuous";

	size_t wrong = 0;

	for ( uint32_t vertexIndex = 0; vertexIndex < shape.vertices().size(); ++vertexIndex )
	{
		if ( shape.vertexInfluences(vertexIndex) != influencesAt(shape.vertices()[vertexIndex].position()) )
		{
			++wrong;
		}
	}

	EXPECT_EQ(wrong, 0U) << wrong << " of " << shape.vertices().size() << " vertices carry another vertex's skinning";
}

/* flipSurface() and reverseWinding() swap two corners of every triangle: each corner keeps its color. */
TEST(VertexFactoryShapeTables, aFlipKeepsTheColorOfEachCorner)
{
	auto shape = ShapeGenerator::generateCuboid< float, uint32_t >(1.0F, 1.0F, 1.0F);

	shape.vertexColors().resize(3, Math::Vector< 4, float >{1.0F, 1.0F, 1.0F, 1.0F});

	for ( uint32_t triangleIndex = 0; triangleIndex < shape.triangles().size(); ++triangleIndex )
	{
		shape.setTriangleVertexColorIndexes(triangleIndex, {0, 1, 2});
	}

	std::vector< std::set< std::pair< uint32_t, uint32_t > > > before;
	before.reserve(shape.triangles().size());

	for ( uint32_t triangleIndex = 0; triangleIndex < shape.triangles().size(); ++triangleIndex )
	{
		before.push_back(cornerPairs(shape, triangleIndex));
	}

	shape.flipSurface();

	for ( uint32_t triangleIndex = 0; triangleIndex < shape.triangles().size(); ++triangleIndex )
	{
		EXPECT_EQ(cornerPairs(shape, triangleIndex), before[triangleIndex]) << "flipSurface(), triangle " << triangleIndex;
	}

	shape.reverseWinding();

	for ( uint32_t triangleIndex = 0; triangleIndex < shape.triangles().size(); ++triangleIndex )
	{
		EXPECT_EQ(cornerPairs(shape, triangleIndex), before[triangleIndex]) << "reverseWinding(), triangle " << triangleIndex;
	}
}

namespace
{
	/**
	 * @brief A welded sphere, every vertex given the same skinning when asked, cut by an oblique plane.
	 * @param skinned Whether the sphere has skinning.
	 * @return ShapeSplitter< float, uint32_t >::SplitResult
	 */
	auto
	splitSphere (bool skinned) noexcept
	{
		auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 16U, 8U, ShapeBuilderOptions< float >{true, true, true, false, false});

		ShapeProcessor< float, uint32_t > welder{sphere};
		static_cast< void >(welder.deduplicateVertices());

		for ( uint32_t vertexIndex = 0; skinned && vertexIndex < sphere.vertices().size(); ++vertexIndex )
		{
			sphere.setVertexSkinning(vertexIndex, Influences{2, 5, -1, -1}, Weights{0.75F, 0.25F, 0.0F, 0.0F});
		}

		const Math::Plane< float > plane{V3{1.0F, 0.0F, 1.0F}.normalized(), V3{0.1F, 0.0F, 0.0F}};
		const ShapeSplitter< float, uint32_t > splitter{sphere, plane, 1.0E-5F, false};

		return splitter.split();
	}

	/**
	 * @brief Counts the vertices of a part that do not carry the skinning splitSphere() gave.
	 * @param part The part.
	 * @return size_t
	 */
	size_t
	verticesWithoutTheSkinning (const Shape< float, uint32_t > & part) noexcept
	{
		size_t lost = 0;

		for ( uint32_t vertexIndex = 0; vertexIndex < part.vertices().size(); ++vertexIndex )
		{
			if ( part.vertexInfluences(vertexIndex) != Influences{2, 5, -1, -1} || part.vertexWeights(vertexIndex) != Weights{0.75F, 0.25F, 0.0F, 0.0F} )
			{
				++lost;
			}
		}

		return lost;
	}
}

/* A split builds new shapes from the source's corners and from intersection vertices interpolated along the cut: both
 * carry the skinning (a uniform one, so the interpolation must give it back exactly). */
TEST(VertexFactoryShapeTables, aSplitKeepsTheSkinning)
{
	const auto result = splitSphere(true);

	ASSERT_TRUE(result.wasSplit);

	size_t checked = 0;
	size_t lost = 0;

	for ( const auto * parts : {&result.frontParts, &result.backParts} )
	{
		for ( const auto & part : *parts )
		{
			EXPECT_TRUE(part.isSkinningAvailable());

			checked += part.vertices().size();
			lost += verticesWithoutTheSkinning(part);
		}
	}

	EXPECT_GT(checked, 0U);
	EXPECT_EQ(lost, 0U) << lost << " of " << checked << " vertices lost their skinning in the split";
}

/* A source without skinning gives parts without it. */
TEST(VertexFactoryShapeTables, aSplitOfAShapeWithoutSkinningAddsNone)
{
	const auto result = splitSphere(false);

	ASSERT_TRUE(result.wasSplit);

	for ( const auto * parts : {&result.frontParts, &result.backParts} )
	{
		for ( const auto & part : *parts )
		{
			EXPECT_FALSE(part.isSkinningAvailable());
		}
	}
}
