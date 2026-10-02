/*
 * src/Testing/test_MathSpace3DTriangleMesh.cpp
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
#include <limits>
#include <numbers>
#include <set>
#include <vector>

/* Local inclusions. */
#include "Math/Space3D/Contacts/ContactManifold.hpp"
#include "Math/Space3D/TriangleMesh.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base::Math;
using namespace EmEn::Base::Math::Space3D;

namespace
{
	using Vec3 = Vector< 3, float >;
	using Mesh = TriangleMesh< float >;

	constexpr float Tolerance{1.0e-5F};
	constexpr float FiveDegrees{0.99619470F};

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

	/** The index of the built triangle equal to (a, b, c) up to a rotation of its corners, or the count. */
	size_t
	findTriangle (const Mesh & mesh, const Vec3 & a, const Vec3 & b, const Vec3 & c)
	{
		for ( size_t index = 0; index < mesh.triangleCount(); ++index )
		{
			const auto & triangle = mesh.triangle(index);
			const auto & first = triangle.pointA();
			const auto & second = triangle.pointB();
			const auto & third = triangle.pointC();

			/* The three rotations of its corners (the winding kept). */
			if ( (first == a && second == b && third == c) || (second == a && third == b && first == c) || (third == a && first == b && second == c) )
			{
				return index;
			}
		}

		return mesh.triangleCount();
	}

	/** The flag of the edge (from, to) of a built triangle, whatever the rotation of its corners. */
	bool
	isEdgeActive (const Mesh & mesh, size_t index, const Vec3 & from, const Vec3 & to)
	{
		const auto & triangle = mesh.triangle(index);
		const auto matches = [&from, &to] (const Vec3 & a, const Vec3 & b) {
			return (a == from && b == to) || (a == to && b == from);
		};

		if ( matches(triangle.pointA(), triangle.pointB()) )
		{
			return (mesh.activeEdges(index) & Mesh::EdgeABActive) != 0;
		}

		if ( matches(triangle.pointB(), triangle.pointC()) )
		{
			return (mesh.activeEdges(index) & Mesh::EdgeBCActive) != 0;
		}

		if ( matches(triangle.pointC(), triangle.pointA()) )
		{
			return (mesh.activeEdges(index) & Mesh::EdgeCAActive) != 0;
		}

		ADD_FAILURE() << "edge not found";

		return false;
	}

	/** A flat 2 × 2 quad on Y = 0 facing +Y, split along the diagonal (−1, 0, −1) → (1, 0, 1). */
	const std::vector< Vec3 > QuadVertices{{-1.0F, 0.0F, -1.0F}, {1.0F, 0.0F, -1.0F}, {1.0F, 0.0F, 1.0F}, {-1.0F, 0.0F, 1.0F}};
	/* Counter-clockwise seen from +Y: (B - A) × (C - A) points up. */
	const std::vector< uint32_t > QuadIndices{0, 2, 1, 0, 3, 2};
}

/* ===== Refusals ===== */

TEST(MathSpace3DTriangleMesh, RefusesInvalidInput)
{
	Mesh mesh;
	const std::vector< Vec3 > vertices{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}};

	EXPECT_FALSE(mesh.build(vertices, std::vector< uint32_t >{}, FiveDegrees));
	EXPECT_FALSE(mesh.build(vertices, std::vector< uint32_t >{0, 1}, FiveDegrees));
	EXPECT_FALSE(mesh.build(vertices, std::vector< uint32_t >{0, 1, 3}, FiveDegrees));
	EXPECT_FALSE(mesh.build(vertices, std::vector< uint32_t >{0, 2, 1}, std::numeric_limits< float >::quiet_NaN()));
	EXPECT_FALSE(mesh.build(vertices, std::vector< uint32_t >{0, 2, 1}, 1.5F));

	const std::vector< Vec3 > notFinite{{0.0F, 0.0F, 0.0F}, {std::numeric_limits< float >::infinity(), 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}};

	EXPECT_FALSE(mesh.build(notFinite, std::vector< uint32_t >{0, 2, 1}, FiveDegrees));

	/* Only degenerate triangles (collinear, repeated corner): nothing left. */
	const std::vector< Vec3 > collinear{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {2.0F, 0.0F, 0.0F}};

	EXPECT_FALSE(mesh.build(collinear, std::vector< uint32_t >{0, 1, 2, 0, 0, 1}, FiveDegrees));
	EXPECT_TRUE(mesh.empty());
	EXPECT_TRUE(mesh.nodes().empty());
}

TEST(MathSpace3DTriangleMesh, SkipsDegenerateTriangles)
{
	Mesh mesh;
	const std::vector< Vec3 > vertices{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {2.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}};

	ASSERT_TRUE(mesh.build(vertices, std::vector< uint32_t >{0, 1, 2, 0, 3, 1}, FiveDegrees));
	EXPECT_EQ(mesh.triangleCount(), 1U);

	/* The winding normal: (B - A) × (C - A) of (0,0,0) (0,0,1) (1,0,0) points up. */
	EXPECT_NEAR(mesh.normal(0)[Y], 1.0F, Tolerance);
}

/* ===== Hierarchy ===== */

TEST(MathSpace3DTriangleMesh, VisitMatchesBruteForce)
{
	Draw draw{20261002U};
	std::vector< Vec3 > vertices;
	std::vector< uint32_t > indices;

	for ( uint32_t index = 0; index < 600; ++index )
	{
		const auto center = draw.vector(-50.0F, 50.0F);

		vertices.push_back(center + draw.vector(-1.5F, 1.5F));
		vertices.push_back(center + draw.vector(-1.5F, 1.5F));
		vertices.push_back(center + draw.vector(-1.5F, 1.5F));
		indices.push_back(index * 3);
		indices.push_back((index * 3) + 1);
		indices.push_back((index * 3) + 2);
	}

	Mesh mesh;

	ASSERT_TRUE(mesh.build(vertices, indices, FiveDegrees));
	ASSERT_EQ(mesh.triangleCount(), 600U);

	for ( uint32_t query = 0; query < 300; ++query )
	{
		const auto center = draw.vector(-55.0F, 55.0F);
		const auto half = draw.vector(0.0F, 8.0F);
		const Vec3 lowest = center - half;
		const Vec3 highest = center + half;

		std::multiset< uint32_t > visited;

		mesh.visit(lowest, highest, [&visited] (uint32_t triangleIndex) {
			visited.insert(triangleIndex);
		});

		std::multiset< uint32_t > expected;

		for ( uint32_t index = 0; index < mesh.triangleCount(); ++index )
		{
			const auto & triangle = mesh.triangle(index);
			bool overlap = true;

			for ( size_t axis = 0; axis < 3; ++axis )
			{
				const auto low = std::min({triangle.pointA()[axis], triangle.pointB()[axis], triangle.pointC()[axis]});
				const auto high = std::max({triangle.pointA()[axis], triangle.pointB()[axis], triangle.pointC()[axis]});

				overlap = overlap && low <= highest[axis] && high >= lowest[axis];
			}

			if ( overlap )
			{
				expected.insert(index);
			}
		}

		ASSERT_EQ(visited, expected) << "query " << query;
	}
}

TEST(MathSpace3DTriangleMesh, HierarchyInvariants)
{
	Draw draw{7U};
	std::vector< Vec3 > vertices;
	std::vector< uint32_t > indices;

	for ( uint32_t index = 0; index < 1000; ++index )
	{
		const auto center = draw.vector(-20.0F, 20.0F);

		vertices.push_back(center + draw.vector(-0.5F, 0.5F));
		vertices.push_back(center + draw.vector(-0.5F, 0.5F));
		vertices.push_back(center + draw.vector(-0.5F, 0.5F));
		indices.push_back(index * 3);
		indices.push_back((index * 3) + 1);
		indices.push_back((index * 3) + 2);
	}

	Mesh mesh;

	ASSERT_TRUE(mesh.build(vertices, indices, FiveDegrees));

	const auto & nodes = mesh.nodes();
	std::vector< uint32_t > covered(mesh.triangleCount(), 0);

	for ( size_t index = 0; index < nodes.size(); ++index )
	{
		const auto & node = nodes[index];

		if ( node.count > 0 )
		{
			EXPECT_LE(node.count, Mesh::MaxLeafTriangles);

			for ( uint32_t triangle = node.first; triangle < node.first + node.count; ++triangle )
			{
				++covered[triangle];

				for ( const auto & corner : {mesh.triangle(triangle).pointA(), mesh.triangle(triangle).pointB(), mesh.triangle(triangle).pointC()} )
				{
					for ( size_t axis = 0; axis < 3; ++axis )
					{
						EXPECT_GE(corner[axis], node.minimum[axis]);
						EXPECT_LE(corner[axis], node.maximum[axis]);
					}
				}
			}

			continue;
		}

		/* An inner node: its left child follows it, its right child is after it; both inside it. */
		ASSERT_LT(index + 1, nodes.size());
		ASSERT_GT(node.right, index + 1);
		ASSERT_LT(node.right, nodes.size());

		for ( const auto child : {index + 1, static_cast< size_t >(node.right)} )
		{
			for ( size_t axis = 0; axis < 3; ++axis )
			{
				EXPECT_GE(nodes[child].minimum[axis], node.minimum[axis]);
				EXPECT_LE(nodes[child].maximum[axis], node.maximum[axis]);
			}
		}
	}

	/* Every triangle in exactly one leaf. */
	EXPECT_TRUE(std::ranges::all_of(covered, [] (uint32_t count) { return count == 1; }));
}

TEST(MathSpace3DTriangleMesh, IdenticalTrianglesStayBounded)
{
	/* 200 copies of one triangle: no split separates them; the depth limit and the median keep the build finite. */
	std::vector< Vec3 > vertices{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F}};
	std::vector< uint32_t > indices;

	for ( uint32_t index = 0; index < 200; ++index )
	{
		indices.insert(indices.end(), {0U, 1U, 2U});
	}

	Mesh mesh;

	ASSERT_TRUE(mesh.build(vertices, indices, FiveDegrees));
	EXPECT_EQ(mesh.triangleCount(), 200U);

	uint32_t visited = 0;

	mesh.visit(Vec3{-1.0F, -1.0F, -1.0F}, Vec3{2.0F, 1.0F, 2.0F}, [&visited] (uint32_t) { ++visited; });

	EXPECT_EQ(visited, 200U);
}

/* ===== Active edges ===== */

TEST(MathSpace3DTriangleMesh, FlatDiagonalIsInactiveBordersActive)
{
	Mesh mesh;

	ASSERT_TRUE(mesh.build(QuadVertices, QuadIndices, FiveDegrees));
	ASSERT_EQ(mesh.triangleCount(), 2U);

	const auto first = findTriangle(mesh, QuadVertices[0], QuadVertices[2], QuadVertices[1]);
	const auto second = findTriangle(mesh, QuadVertices[0], QuadVertices[3], QuadVertices[2]);

	ASSERT_LT(first, 2U);
	ASSERT_LT(second, 2U);

	EXPECT_FALSE(isEdgeActive(mesh, first, QuadVertices[0], QuadVertices[2]));
	EXPECT_FALSE(isEdgeActive(mesh, second, QuadVertices[0], QuadVertices[2]));
	EXPECT_TRUE(isEdgeActive(mesh, first, QuadVertices[2], QuadVertices[1]));
	EXPECT_TRUE(isEdgeActive(mesh, first, QuadVertices[1], QuadVertices[0]));
	EXPECT_TRUE(isEdgeActive(mesh, second, QuadVertices[0], QuadVertices[3]));
	EXPECT_TRUE(isEdgeActive(mesh, second, QuadVertices[3], QuadVertices[2]));
}

TEST(MathSpace3DTriangleMesh, SplitVerticesAreWelded)
{
	/* The same quad, every triangle with its own vertices (as an exporter splits them by normal or UV). */
	const std::vector< Vec3 > vertices{QuadVertices[0], QuadVertices[2], QuadVertices[1], QuadVertices[0], QuadVertices[3], QuadVertices[2]};
	Mesh mesh;

	ASSERT_TRUE(mesh.build(vertices, std::vector< uint32_t >{0, 1, 2, 3, 4, 5}, FiveDegrees));

	const auto first = findTriangle(mesh, QuadVertices[0], QuadVertices[2], QuadVertices[1]);

	ASSERT_LT(first, 2U);
	EXPECT_FALSE(isEdgeActive(mesh, first, QuadVertices[0], QuadVertices[2]));
}

TEST(MathSpace3DTriangleMesh, ConvexEdgeActiveConcaveInactive)
{
	/* A floor (Y = 0, facing up) meeting a wall. */
	const Vec3 a{0.0F, 0.0F, -1.0F};
	const Vec3 b{0.0F, 0.0F, 1.0F};

	{
		/* CONVEX: the floor x ∈ [-1, 0] and a wall going DOWN from its edge, facing +X (the top of a step). */
		const std::vector< Vec3 > vertices{a, b, {-1.0F, 0.0F, 0.0F}, {0.0F, -1.0F, 0.0F}};
		Mesh mesh;

		/* Floor (a, c, b): (c - a) × (b - a)... order chosen so that it faces +Y; the wall (a, b, d) faces +X. */
		ASSERT_TRUE(mesh.build(vertices, std::vector< uint32_t >{0, 2, 1, 0, 1, 3}, FiveDegrees));
		ASSERT_EQ(mesh.triangleCount(), 2U);
		ASSERT_GT(mesh.normal(findTriangle(mesh, a, vertices[2], b))[Y], 0.9F);
		ASSERT_GT(mesh.normal(findTriangle(mesh, a, b, vertices[3]))[X], 0.9F);

		EXPECT_TRUE(isEdgeActive(mesh, findTriangle(mesh, a, vertices[2], b), a, b));
		EXPECT_TRUE(isEdgeActive(mesh, findTriangle(mesh, a, b, vertices[3]), a, b));
	}

	{
		/* CONCAVE: the floor x ∈ [0, 1] and a wall going UP from its edge, facing +X (the foot of a wall). */
		const std::vector< Vec3 > vertices{a, b, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
		Mesh mesh;

		ASSERT_TRUE(mesh.build(vertices, std::vector< uint32_t >{0, 1, 2, 0, 3, 1}, FiveDegrees));
		ASSERT_EQ(mesh.triangleCount(), 2U);
		ASSERT_GT(mesh.normal(findTriangle(mesh, a, b, vertices[2]))[Y], 0.9F);
		ASSERT_GT(mesh.normal(findTriangle(mesh, a, vertices[3], b))[X], 0.9F);

		EXPECT_FALSE(isEdgeActive(mesh, findTriangle(mesh, a, b, vertices[2]), a, b));
		EXPECT_FALSE(isEdgeActive(mesh, findTriangle(mesh, a, vertices[3], b), a, b));
	}
}

TEST(MathSpace3DTriangleMesh, KnifeEdgeIsActive)
{
	/* Two triangles back to back on the same plane (a two-faced panel): opposite normals. */
	const std::vector< Vec3 > vertices{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
	Mesh mesh;

	ASSERT_TRUE(mesh.build(vertices, std::vector< uint32_t >{0, 1, 2, 0, 2, 1}, FiveDegrees));
	ASSERT_EQ(mesh.triangleCount(), 2U);

	EXPECT_EQ(mesh.activeEdges(0), Mesh::EdgeABActive | Mesh::EdgeBCActive | Mesh::EdgeCAActive);
	EXPECT_EQ(mesh.activeEdges(1), Mesh::EdgeABActive | Mesh::EdgeBCActive | Mesh::EdgeCAActive);
}

/* ===== Internal edge correction ===== */

TEST(MathSpace3DTriangleMesh, InternalEdgeNormalBecomesTheFaceNormal)
{
	Mesh mesh;

	ASSERT_TRUE(mesh.build(QuadVertices, QuadIndices, FiveDegrees));

	const auto first = findTriangle(mesh, QuadVertices[0], QuadVertices[2], QuadVertices[1]);

	ASSERT_LT(first, 2U);

	/* A sphere above the diagonal, its normal leaning (from the body DOWN to the triangle, tilted towards −X). */
	ContactManifold< float > manifold;
	const Vec3 leaning = Vec3{-0.3F, -1.0F, 0.3F}.normalized();

	manifold.setNormal(leaning);
	static_cast< void >(manifold.addPoint({Vec3{0.0F, 0.0F, 0.0F}, 0.02F, 7U}));

	ASSERT_TRUE(mesh.correctInternalEdgeNormal(first, manifold));
	EXPECT_NEAR(manifold.normal()[X], 0.0F, Tolerance);
	EXPECT_NEAR(manifold.normal()[Y], -1.0F, Tolerance);
	EXPECT_NEAR(manifold.normal()[Z], 0.0F, Tolerance);
	ASSERT_EQ(manifold.points().size(), 1U);
	EXPECT_NEAR(manifold.points()[0].depth(), 0.02F * Vec3::dotProduct(leaning, Vec3{0.0F, -1.0F, 0.0F}), Tolerance);
	EXPECT_EQ(manifold.points()[0].featureId(), 7U);
}

TEST(MathSpace3DTriangleMesh, ActiveEdgeNormalIsKept)
{
	Mesh mesh;

	ASSERT_TRUE(mesh.build(QuadVertices, QuadIndices, FiveDegrees));

	const auto first = findTriangle(mesh, QuadVertices[0], QuadVertices[2], QuadVertices[1]);

	/* On the border edge (1, 0, -1) → (1, 0, 1): active, the leaning normal stays. */
	ContactManifold< float > manifold;
	const Vec3 leaning = Vec3{-0.6F, -0.8F, 0.0F}.normalized();

	manifold.setNormal(leaning);
	static_cast< void >(manifold.addPoint({Vec3{1.0F, 0.0F, 0.2F}, 0.02F, 4U}));

	EXPECT_FALSE(mesh.correctInternalEdgeNormal(first, manifold));
	EXPECT_NEAR(manifold.normal()[X], leaning[X], Tolerance);
	EXPECT_NEAR(manifold.normal()[Y], leaning[Y], Tolerance);
}

TEST(MathSpace3DTriangleMesh, BackSideCorrectionFacesTheBody)
{
	Mesh mesh;

	ASSERT_TRUE(mesh.build(QuadVertices, QuadIndices, FiveDegrees));

	const auto first = findTriangle(mesh, QuadVertices[0], QuadVertices[2], QuadVertices[1]);

	/* A body UNDER the quad (its normal from the body up to the triangle, leaning): corrected to +Y. */
	ContactManifold< float > manifold;

	manifold.setNormal(Vec3{0.3F, 1.0F, 0.0F}.normalized());
	static_cast< void >(manifold.addPoint({Vec3{0.0F, 0.0F, 0.0F}, 0.01F, 1U}));

	ASSERT_TRUE(mesh.correctInternalEdgeNormal(first, manifold));
	EXPECT_NEAR(manifold.normal()[Y], 1.0F, Tolerance);
}

TEST(MathSpace3DTriangleMesh, SweepHitNormalOnInternalEdge)
{
	Mesh mesh;

	ASSERT_TRUE(mesh.build(QuadVertices, QuadIndices, FiveDegrees));

	const auto first = findTriangle(mesh, QuadVertices[0], QuadVertices[2], QuadVertices[1]);
	const auto & triangle = mesh.triangle(first);

	/* A sweep's hit on the diagonal with a leaning surface normal (towards the caster, above): the face normal. */
	Vec3 normal = Vec3{0.4F, 1.0F, -0.4F}.normalized();

	ASSERT_TRUE(Mesh::correctInternalEdgeNormal(triangle, mesh.normal(first), mesh.activeEdges(first), Vec3{0.0F, 0.0F, 0.0F}, normal));
	EXPECT_NEAR(normal[Y], 1.0F, Tolerance);

	/* On a border edge: kept. */
	Vec3 border = Vec3{0.6F, 0.8F, 0.0F}.normalized();

	EXPECT_FALSE(Mesh::correctInternalEdgeNormal(triangle, mesh.normal(first), mesh.activeEdges(first), Vec3{1.0F, 0.0F, 0.2F}, border));
	EXPECT_NEAR(border[X], 0.6F, Tolerance);
}
