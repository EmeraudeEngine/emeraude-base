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
