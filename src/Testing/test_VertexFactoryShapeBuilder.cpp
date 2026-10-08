/*
 * src/Testing/test_VertexFactoryShapeBuilder.cpp
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
#include <cstdint>

/* Local inclusions. */
#include "Math/Vector.hpp"
#include "VertexFactory/Shape.hpp"
#include "VertexFactory/ShapeBuilder.hpp"
#include "VertexFactory/ShapeGenerator.hpp"
#include "VertexFactory/ShapeProcessor.hpp"

using namespace EmEn::Base::VertexFactory;

/* Ave robustus! (Axis B — correction marker): confirms the ConstructionMode::TriangleFan vertex
 * shift (the resolved `FIXME: Check this` in ShapeBuilder). A fan of one origin + 4 rim vertices
 * (5 total) must emit N-2 = 3 triangles, all sharing the fan origin. */
TEST(VertexFactoryShapeBuilder, triangleFanProducesNMinus2Triangles)
{
	Shape< float, uint32_t > shape;
	ShapeBuilder< float, uint32_t > builder{shape};

	builder.options().enableGlobalNormal(EmEn::Base::Math::Vector< 3, float >::positiveZ());
	builder.beginConstruction(ConstructionMode::TriangleFan);

	builder.setPosition(0.0F, 0.0F, 0.0F);   builder.newVertex();   /* fan origin */
	builder.setPosition(1.0F, 0.0F, 0.0F);   builder.newVertex();
	builder.setPosition(1.0F, 1.0F, 0.0F);   builder.newVertex();
	builder.setPosition(0.0F, 1.0F, 0.0F);   builder.newVertex();
	builder.setPosition(-1.0F, 1.0F, 0.0F);  builder.newVertex();

	builder.endConstruction();

	EXPECT_EQ(shape.triangles().size(), 3U);
}

/* Ave robustus! (Axis B — correction marker): Shape::addEdge() returned m_edges.size() AFTER the
 * emplace_back, i.e. the index PLUS ONE. Every edge index stored in a triangle, and one half of
 * every shared-edge cross-link, pointed at the next edge; the very last one pointed one past the
 * end, which is what Silhouette.hpp:98-100 dereferences. Measured before the 2026-09-21 fix on a
 * 16x8 sphere: 0 of 768 edge indices correct, 767 mismatched, 1 out of range. */
TEST(VertexFactoryShapeBuilder, triangleEdgeIndexesPointAtTheirOwnEdge)
{
	const auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 16U, 8U);

	const auto & edges = shape.edges();

	ASSERT_FALSE(edges.empty()) << "no edge was built, the check would be vacuous";

	for ( const auto & triangle : shape.triangles() )
	{
		for ( uint32_t corner = 0; corner < 3; ++corner )
		{
			const auto edgeIndex = triangle.edgeIndex(corner);

			ASSERT_LT(edgeIndex, edges.size()) << "edge index " << edgeIndex << " is out of the " << edges.size() << " edges built";

			/* The edge stored for that corner must join that corner to the next one. */
			EXPECT_TRUE(edges[edgeIndex].same(triangle.vertexIndex(corner), triangle.vertexIndex((corner + 1) % 3)))
				<< "the edge at index " << edgeIndex << " does not join the two vertices of corner " << corner;
		}
	}
}

/* Ave robustus! (Axis B — correction marker): the two half-edges of a shared edge must point at
 * each other. The same off-by-one made the older half point one past its mate.
 * NOTE: an unpaired edge is NOT a defect here. A sphere is not watertight in the EDGE sense:
 * the two sides of its UV seam carry u = 0 and u = 1, and its poles fan out, so those vertices
 * are genuinely distinct whatever the merge tolerance. Measured after addVertex() became
 * hash-based on 2026-09-22: still 48 unpaired edges on a 16x8 sphere. Only the edges that DID
 * pair are checked. */
TEST(VertexFactoryShapeBuilder, sharedEdgeCrossLinksAreReciprocal)
{
	const auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 16U, 8U);

	const auto & edges = shape.edges();

	ASSERT_FALSE(edges.empty()) << "no edge was built, the check would be vacuous";

	size_t pairedCount = 0;

	for ( size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex )
	{
		const auto & edge = edges[edgeIndex];

		if ( !edge.isShared() )
		{
			continue;
		}

		++pairedCount;

		const auto mateIndex = edge.sharedIndex();

		ASSERT_LT(mateIndex, edges.size()) << "edge " << edgeIndex << " points at " << mateIndex << ", past the " << edges.size() << " edges built";

		EXPECT_EQ(edges[mateIndex].sharedIndex(), edgeIndex) << "the cross-link between " << edgeIndex << " and " << mateIndex << " is not reciprocal";

		EXPECT_TRUE(edges[mateIndex].same(edge.vertexIndexA(), edge.vertexIndexB())) << "edge " << edgeIndex << " is paired with an edge joining other vertices";
	}

	/* The vast majority of a sphere's edges are interior ones; if almost nothing paired, the
	 * reciprocity check above ran on nothing and proves nothing. */
	EXPECT_GT(pairedCount, edges.size() / 2) << "only " << pairedCount << " of " << edges.size() << " edges paired, the check is nearly vacuous";
}

/* Ave robustus! (Axis B — correction marker): ShapeProcessor::deduplicateVertices() renumbers the
 * vertices and remaps the triangles, and used to leave the edge list untouched. A ShapeEdge holds
 * VERTEX indices and a triangle holds EDGE indices, so both were stale afterwards. Measured before
 * the 2026-09-22 fix on a 16x8 sphere deduplicated from 768 to 153 vertices: 765 of the 768 edge
 * indices wrong, and 615 edges still naming vertices that no longer existed. It was latent only
 * because nothing in the cascade consumes the edge list yet. */
TEST(VertexFactoryShapeBuilder, deduplicatingVerticesKeepsTheEdgeListValid)
{
	/* Data economy OFF so the build really produces duplicates for the pass to remove. */
	ShapeBuilderOptions< float > options;
	options.enableDataEconomy(false);

	auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 16U, 8U, options);

	ASSERT_FALSE(shape.edges().empty());

	ShapeProcessor< float, uint32_t > processor{shape};

	const auto removed = processor.deduplicateVertices();

	ASSERT_GT(removed, 0U) << "nothing was merged, the check would be vacuous";

	const auto vertexCount = shape.vertices().size();

	/* No edge may name a vertex that the merge removed. */
	for ( const auto & edge : shape.edges() )
	{
		ASSERT_LT(edge.vertexIndexA(), vertexCount) << "an edge still names a vertex the merge removed";
		ASSERT_LT(edge.vertexIndexB(), vertexCount) << "an edge still names a vertex the merge removed";
	}

	/* And every triangle edge index must still point at its own edge. */
	for ( const auto & triangle : shape.triangles() )
	{
		for ( uint32_t corner = 0; corner < 3; ++corner )
		{
			const auto edgeIndex = triangle.edgeIndex(corner);

			ASSERT_LT(edgeIndex, shape.edges().size()) << "edge index out of the rebuilt list";

			EXPECT_TRUE(shape.edges()[edgeIndex].same(triangle.vertexIndex(corner), triangle.vertexIndex((corner + 1) % 3)))
				<< "the edge at index " << edgeIndex << " does not join the two vertices of corner " << corner;
		}
	}
}

/* The secondary texture coordinates (2026-10-03): two vertices differing ONLY in their second set are two vertices
 * (a baked occlusion unwrap has its own seams). Before, the dedupe keyed on position, normal and the primary set,
 * and merged them. */
TEST(VertexFactoryShapeBuilder, deduplicatingVerticesKeepsDistinctSecondaryTextureCoordinates)
{
	ShapeBuilderOptions< float > options;
	options.enableDataEconomy(false);

	auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 16U, 8U, options);
	const auto vertexCount = shape.vertices().size();

	ASSERT_GT(vertexCount, 0U);

	float unique = 0.0F;

	for ( auto & vertex : shape.vertices() )
	{
		vertex.setSecondaryTextureCoordinates(EmEn::Base::Math::Vector< 2, float >{unique, 1.0F - unique});
		unique += 1.0F / static_cast< float >(vertexCount);
	}

	ShapeProcessor< float, uint32_t > processor{shape};

	EXPECT_EQ(processor.deduplicateVertices(), 0U) << "vertices with distinct secondary texture coordinates were merged";
	EXPECT_EQ(shape.vertices().size(), vertexCount);
}

/* The secondary set goes right after the primary one in a vertex buffer (the engine's vertex format order). */
TEST(VertexFactoryShapeBuilder, indexedVertexBufferWritesTheSecondarySetAfterThePrimaryOne)
{
	auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 8U, 4U);

	for ( auto & vertex : shape.vertices() )
	{
		vertex.setSecondaryTextureCoordinates(EmEn::Base::Math::Vector< 2, float >{vertex.position()[EmEn::Base::Math::X] + 10.0F, vertex.position()[EmEn::Base::Math::Y] + 20.0F});
	}

	std::vector< float > vertexBuffer;
	std::vector< uint32_t > indexBuffer;

	const auto elementCount = shape.createIndexedVertexBuffer(vertexBuffer, indexBuffer, NormalType::None, TextureCoordinatesType::UV, VertexColorType::None, SkeletalAnimationType::None, TextureCoordinatesType::UV);

	ASSERT_EQ(elementCount, 7U) << "position (3) + primary UV (2) + secondary UV (2)";
	ASSERT_FALSE(vertexBuffer.empty());
	ASSERT_EQ(vertexBuffer.size() % elementCount, 0U);

	for ( size_t offset = 0; offset < vertexBuffer.size(); offset += elementCount )
	{
		/* Each written vertex carries the secondary set its own position was given. */
		EXPECT_FLOAT_EQ(vertexBuffer[offset + 5], vertexBuffer[offset + 0] + 10.0F);
		EXPECT_FLOAT_EQ(vertexBuffer[offset + 6], vertexBuffer[offset + 1] + 20.0F);
	}
}

namespace
{
	/* A strip of quads along X: 2 triangles per quad, built through the merging calls (addVertex(),
	 * addVertexColor(), addTriangle()), each quad sharing its left edge with the previous one and its
	 * colour with all of them. */
	void
	appendQuads (Shape< float, uint32_t > & shape, uint32_t first, uint32_t count) noexcept
	{
		const EmEn::Base::Math::Vector< 3, float > up{0.0F, 1.0F, 0.0F};

		for ( auto quad = first; quad < first + count; ++quad )
		{
			const auto x = static_cast< float >(quad);
			const auto a = shape.addVertex({x, 0.0F, 0.0F}, up);
			const auto b = shape.addVertex({x + 1.0F, 0.0F, 0.0F}, up);
			const auto c = shape.addVertex({x + 1.0F, 0.0F, 1.0F}, up);
			const auto d = shape.addVertex({x, 0.0F, 1.0F}, up);

			static_cast< void >(shape.addVertexColor({0.75F, 0.5F, 0.25F, 1.0F}));

			ShapeTriangle< float > lower{a, b, c};
			ShapeTriangle< float > upper{a, c, d};

			shape.addTriangle(lower);
			shape.addTriangle(upper);
		}
	}
}

TEST(VertexFactoryShapeBuilder, releasingTheConstructionIndexesFreesTheirMemory)
{
	Shape< float, uint32_t > shape;

	appendQuads(shape, 0, 64);

	const auto before = shape.memoryOccupied();

	shape.releaseConstructionIndexes();

	ASSERT_TRUE(shape.constructionIndexesReleased());
	EXPECT_LT(shape.memoryOccupied(), before);
	EXPECT_EQ(shape.vertices().size(), 65U * 2U);
	EXPECT_EQ(shape.triangles().size(), 128U);
}

/* An edit after the release must behave as if the indexes had never been released: the same merges, the
 * same edge pairing (a new quad pairs with the last edge built before the release). */
TEST(VertexFactoryShapeBuilder, anEditAfterTheReleaseRebuildsTheConstructionIndexes)
{
	Shape< float, uint32_t > kept;
	Shape< float, uint32_t > released;

	appendQuads(kept, 0, 8);
	appendQuads(released, 0, 8);

	released.releaseConstructionIndexes();

	appendQuads(kept, 8, 4);
	appendQuads(released, 8, 4);

	EXPECT_FALSE(released.constructionIndexesReleased());
	ASSERT_EQ(released.vertices().size(), kept.vertices().size());
	ASSERT_EQ(released.vertexColors().size(), 1U);
	ASSERT_EQ(kept.vertexColors().size(), 1U);
	ASSERT_EQ(released.edges().size(), kept.edges().size());
	ASSERT_EQ(released.triangles().size(), kept.triangles().size());

	for ( size_t index = 0; index < kept.edges().size(); ++index )
	{
		EXPECT_EQ(released.edges()[index].sharedIndex(), kept.edges()[index].sharedIndex()) << "edge " << index;
	}

	for ( size_t index = 0; index < kept.triangles().size(); ++index )
	{
		for ( uint32_t corner = 0; corner < 3; ++corner )
		{
			EXPECT_EQ(released.triangles()[index].vertexIndex(corner), kept.triangles()[index].vertexIndex(corner)) << "triangle " << index;
		}
	}
}

/* 2026-10-08 (base item find-boundary-loops-throwing-at): a triangle referring to a vertex past the vertex array (an
 * inconsistent shape, from a loader or built by hand) made findBoundaryLoops() call a throwing .at() — an abort under
 * -fno-exceptions — and createIndexedVertexBuffer() abort the same way (or read the vertex colors out of bounds). Both
 * refuse it now; indicesInRange() tells it. */
TEST(VertexFactoryShape, anInconsistentShapeIsRefusedNotAborted)
{
	auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 8, 4);

	ASSERT_TRUE(shape.indicesInRange());

	const auto vertexCount = static_cast< uint32_t >(shape.vertices().size());

	shape.triangles().emplace_back(0U, 1U, vertexCount + 5U);

	EXPECT_FALSE(shape.indicesInRange());

	{
		const ShapeProcessor< float, uint32_t > processor{shape};

		EXPECT_TRUE(processor.findBoundaryLoops().empty());
		EXPECT_FALSE(processor.hasBoundaryLoops());
	}

	std::vector< float > vertexBuffer{1.0F};
	std::vector< uint32_t > indexBuffer{1U};

	EXPECT_EQ(shape.createIndexedVertexBuffer(vertexBuffer, indexBuffer, NormalType::Normal), 0U);
	EXPECT_TRUE(vertexBuffer.empty());
	EXPECT_TRUE(indexBuffer.empty());
}

/* A vertex color index past the color array is refused when the colors are requested (it was read out of bounds). */
TEST(VertexFactoryShape, aVertexColorIndexOutOfRangeIsRefused)
{
	auto shape = ShapeGenerator::generateCuboid< float, uint32_t >(1.0F, 1.0F, 1.0F);

	ASSERT_FALSE(shape.vertexColors().empty());
	ASSERT_TRUE(shape.indicesInRange(true));

	shape.triangles().front().setVertexColorIndex(0, static_cast< uint32_t >(shape.vertexColors().size()) + 3U);

	EXPECT_TRUE(shape.indicesInRange(false));
	EXPECT_FALSE(shape.indicesInRange(true));

	std::vector< float > vertexBuffer;
	std::vector< uint32_t > indexBuffer;

	EXPECT_EQ(shape.createIndexedVertexBuffer(vertexBuffer, indexBuffer, NormalType::Normal, TextureCoordinatesType::None, VertexColorType::RGBA), 0U);
	EXPECT_GT(shape.createIndexedVertexBuffer(vertexBuffer, indexBuffer, NormalType::Normal), 0U);
}
