/*
 * src/Testing/test_VertexFactoryShapeSplitter.cpp
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
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <string>
#include <tuple>
#include <vector>

/* Local inclusions. */
#include "Math/Plane.hpp"
#include "Math/Vector.hpp"
#include "VertexFactory/Shape.hpp"
#include "VertexFactory/ShapeGenerator.hpp"
#include "VertexFactory/ShapeProcessor.hpp"
#include "VertexFactory/ShapeSplitter.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::VertexFactory;

namespace
{
	using V3 = Math::Vector< 3, float >;

	/**
	 * @brief A closed unit sphere, welded like a loaded model (the UV seam duplicates its vertices).
	 * @return Shape< float, uint32_t >
	 */
	Shape< float, uint32_t >
	weldedSphere () noexcept
	{
		auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 32, 16, ShapeBuilderOptions< float >{true, true, true, false, false});

		ShapeProcessor< float, uint32_t > welder{shape};
		static_cast< void >(welder.deduplicateVertices());

		return shape;
	}

	struct CapCounts final
	{
		size_t frontFacing{0};
		size_t backFacing{0};
		size_t inward{0};
	};

	/**
	 * @brief Judges the cap triangles of a part, recognised by their three vertex normals all equal to the cap
	 * normal, by COMPUTING their winding (docs/coordinate-system.md § Winding Conventions: never by eye).
	 * @param part The part.
	 * @param capNormal The normal the cap must carry and wind CCW around.
	 * @param inside A point inside the part: the cap must face away from it.
	 * @return CapCounts
	 */
	CapCounts
	judgeCap (const Shape< float, uint32_t > & part, const V3 & capNormal, const V3 & inside) noexcept
	{
		CapCounts counts;

		for ( const auto & triangle : part.triangles() )
		{
			const auto & a = part.vertices()[triangle.vertexIndex(0)];
			const auto & b = part.vertices()[triangle.vertexIndex(1)];
			const auto & c = part.vertices()[triangle.vertexIndex(2)];

			if ( V3::dotProduct(a.normal(), capNormal) < 0.999F || V3::dotProduct(b.normal(), capNormal) < 0.999F || V3::dotProduct(c.normal(), capNormal) < 0.999F )
			{
				continue;
			}

			const auto geometric = V3::crossProduct(b.position() - a.position(), c.position() - a.position());

			if ( geometric.length() < 1.0E-7F )
			{
				continue;
			}

			if ( V3::dotProduct(geometric, capNormal) > 0.0F )
			{
				++counts.frontFacing;
			}
			else
			{
				++counts.backFacing;
			}

			if ( V3::dotProduct(a.position() - inside, capNormal) <= 0.0F )
			{
				++counts.inward;
			}
		}

		return counts;
	}
}

/*
 * The cut of a sealed split is a FRONT face seen from outside the part: it winds CCW around its normal and
 * that normal leaves the part. Until Sep 2026 every cap was emitted C/B/A — a Y-down mirror compensation left
 * behind by the Y-up migration — so back-face culling removed the cap and showed the inside of the model
 * (the `geometry-generator` demo of projet-alpha).
 */
TEST(VertexFactoryShapeSplitter, sealedCapsWindCCWAndFaceOutOfTheirPart)
{
	const auto sphere = weldedSphere();
	const auto normal = V3{1.0F, 0.0F, 1.0F}.normalized();
	const Math::Plane< float > plane{normal, V3{}};

	const ShapeSplitter< float, uint32_t > splitter{sphere, plane, 1.0E-5F, true};
	const auto result = splitter.split();

	ASSERT_TRUE(result.wasSplit);
	ASSERT_FALSE(result.frontParts.empty());
	ASSERT_FALSE(result.backParts.empty());

	const auto check = [] (const Shape< float, uint32_t > & part, const V3 & capNormal, const V3 & inside, const std::string & label) {
		const auto counts = judgeCap(part, capNormal, inside);

		EXPECT_GT(counts.frontFacing, 0U) << label << ": no cap triangle found.";
		EXPECT_EQ(counts.backFacing, 0U) << label << ": " << counts.backFacing << " cap triangles wind clockwise around their normal.";
		EXPECT_EQ(counts.inward, 0U) << label << ": " << counts.inward << " cap triangles face into the part.";
	};

	/* The front part lies along +normal: its cap faces -normal, and the reverse for the back part. */
	for ( const auto & part : result.frontParts )
	{
		check(part, -normal, normal * 0.5F, "front part");
	}

	for ( const auto & part : result.backParts )
	{
		check(part, normal, normal * -0.5F, "back part");
	}
}

/*
 * A split copies the source vertices and interpolates new ones on the cut edges: both must keep the bitangent
 * HANDEDNESS. Until 2026-10-06 they were created with saveVertex() (handedness +1) and given only the 3D tangent,
 * so a mirrored UV island (handedness -1) came out of a split with its bitangent flipped.
 */
TEST(VertexFactoryShapeSplitter, partsKeepTheTangentHandedness)
{
	auto sphere = weldedSphere();

	for ( auto & vertex : sphere.vertices() )
	{
		vertex.setTangentHandedness(-1.0F);
	}

	const Math::Plane< float > plane{V3{1.0F, 0.0F, 1.0F}.normalized(), V3{}};
	const ShapeSplitter< float, uint32_t > splitter{sphere, plane, 1.0E-5F, false};
	const auto result = splitter.split();

	ASSERT_TRUE(result.wasSplit);

	size_t checked = 0;
	size_t lost = 0;

	const auto count = [&checked, &lost] (const Shape< float, uint32_t > & part) {
		for ( const auto & vertex : part.vertices() )
		{
			++checked;

			if ( vertex.tangentHandedness() != -1.0F )
			{
				++lost;
			}
		}
	};

	for ( const auto & part : result.frontParts )
	{
		count(part);
	}

	for ( const auto & part : result.backParts )
	{
		count(part);
	}

	EXPECT_GT(checked, 0U);
	EXPECT_EQ(lost, 0U) << lost << " of " << checked << " vertices lost their handedness in the split.";
}

/* The coverage pass (2026-10-17): sealed parts are CLOSED and their volumes add up, whatever the plane — off the
 * vertices, through a ring of vertices (the on-plane edge cases), oblique, missing the shape; a torus cut through its
 * hole (an annulus: an outer and an inner loop) keeps its hole; two disjoint bodies; large coordinates. */
namespace
{
	/* The signed volume of a closed shape (divergence theorem). */
	double
	volumeOf (const Shape< float, uint32_t > & shape) noexcept
	{
		double volume = 0.0;

		for ( const auto & triangle : shape.triangles() )
		{
			const auto & a = shape.vertices()[triangle.vertexIndex(0)].position();
			const auto & b = shape.vertices()[triangle.vertexIndex(1)].position();
			const auto & c = shape.vertices()[triangle.vertexIndex(2)].position();

			volume += static_cast< double >(V3::dotProduct(a, V3::crossProduct(b, c))) / 6.0;
		}

		return volume;
	}

	/* The number of edges NOT shared by exactly two triangles, vertices welded by position (0 = closed). */
	size_t
	openEdgesOf (const Shape< float, uint32_t > & shape, float cell) noexcept
	{
		std::map< std::tuple< long long, long long, long long >, size_t > ids;
		std::map< std::pair< size_t, size_t >, size_t > edges;

		const auto idOf = [&ids, cell] (const V3 & position) {
			const auto key = std::make_tuple(std::llround(position[0] / cell), std::llround(position[1] / cell), std::llround(position[2] / cell));

			return ids.try_emplace(key, ids.size()).first->second;
		};

		for ( const auto & triangle : shape.triangles() )
		{
			const auto a = idOf(shape.vertices()[triangle.vertexIndex(0)].position());
			const auto b = idOf(shape.vertices()[triangle.vertexIndex(1)].position());
			const auto c = idOf(shape.vertices()[triangle.vertexIndex(2)].position());

			if ( a == b || b == c || a == c )
			{
				continue;
			}

			++edges[std::minmax(a, b)];
			++edges[std::minmax(b, c)];
			++edges[std::minmax(c, a)];
		}

		return static_cast< size_t >(std::ranges::count_if(edges, [] (const auto & entry) { return entry.second != 2; }));
	}

	/* Splits with sealing; expects one closed part on each side, their volumes adding up to the source's. */
	void
	expectClosedHalves (const Shape< float, uint32_t > & source, const Math::Plane< float > & plane, float cell, const std::string & label)
	{
		const ShapeSplitter< float, uint32_t > splitter{source, plane, 1.0E-5F, true};
		const auto result = splitter.split();

		ASSERT_TRUE(result.wasSplit) << label;
		ASSERT_EQ(result.frontParts.size(), 1U) << label;
		ASSERT_EQ(result.backParts.size(), 1U) << label;

		EXPECT_EQ(openEdgesOf(result.frontParts[0], cell), 0U) << label << ": the front part is not closed";
		EXPECT_EQ(openEdgesOf(result.backParts[0], cell), 0U) << label << ": the back part is not closed";

		const auto front = volumeOf(result.frontParts[0]);
		const auto back = volumeOf(result.backParts[0]);
		const auto whole = volumeOf(source);

		EXPECT_GT(front, 0.0) << label;
		EXPECT_GT(back, 0.0) << label;
		EXPECT_NEAR(front + back, whole, std::abs(whole) * 1.0E-4) << label;
	}

	/* Appends a copy of `source`, scaled then moved by `offset`, to `target`; `inward` reverses its faces. */
	void
	appendShape (Shape< float, uint32_t > & target, const Shape< float, uint32_t > & source, const V3 & offset, float scale = 1.0F, bool inward = false)
	{
		std::vector< uint32_t > indices;

		for ( const auto & vertex : source.vertices() )
		{
			indices.push_back(target.saveVertex((vertex.position() * scale) + offset, inward ? -vertex.normal() : vertex.normal(), vertex.textureCoordinates()));
		}

		for ( const auto & triangle : source.triangles() )
		{
			const auto a = indices[triangle.vertexIndex(0)];
			const auto b = indices[triangle.vertexIndex(inward ? 2 : 1)];
			const auto c = indices[triangle.vertexIndex(inward ? 1 : 2)];
			ShapeTriangle< float > copy{a, b, c};
			target.addTriangle(copy);
		}
	}
}

TEST(VertexFactoryShapeSplitter, sealedPartsAreClosedAndTheirVolumesAddUp)
{
	const auto sphere = weldedSphere();

	expectClosedHalves(sphere, Math::Plane< float >{V3{0.0F, 1.0F, 0.0F}, V3{0.0F, 0.1F, 0.0F}}, 1.0E-4F, "off the vertices");
	expectClosedHalves(sphere, Math::Plane< float >{V3{0.0F, 1.0F, 0.0F}, V3{}}, 1.0E-4F, "through the equator's ring of vertices");
	expectClosedHalves(sphere, Math::Plane< float >{V3{1.0F, 2.0F, 3.0F}.normalized(), V3{0.1F, -0.2F, 0.05F}}, 1.0E-4F, "oblique");
}

TEST(VertexFactoryShapeSplitter, aPlaneMissingTheShapeLeavesItWhole)
{
	const auto sphere = weldedSphere();
	const ShapeSplitter< float, uint32_t > splitter{sphere, Math::Plane< float >{V3{0.0F, 1.0F, 0.0F}, V3{0.0F, 5.0F, 0.0F}}, 1.0E-5F, true};
	const auto result = splitter.split();

	EXPECT_FALSE(result.wasSplit);
	ASSERT_EQ(result.frontParts.size() + result.backParts.size(), 1U);

	const auto & whole = result.frontParts.empty() ? result.backParts[0] : result.frontParts[0];
	EXPECT_EQ(whole.triangles().size(), sphere.triangles().size());
}

TEST(VertexFactoryShapeSplitter, aTorusCutThroughItsHoleKeepsTheHole)
{
	/* The cut of a torus through its middle plane is an ANNULUS: an outer and an inner loop. A cap must not cover the
	 * hole: no cap triangle closer to the axis than the inner radius (major - minor = 0.7). */
	auto torus = ShapeGenerator::generateTorus< float, uint32_t >(1.0F, 0.3F, 48, 24, ShapeBuilderOptions< float >{true, true, true, false, false});
	ShapeProcessor< float, uint32_t > welder{torus};
	static_cast< void >(welder.deduplicateVertices());

	/* The torus axis: the dimension of smallest extent. */
	V3 minimum{1e9F, 1e9F, 1e9F};
	V3 maximum{-1e9F, -1e9F, -1e9F};

	for ( const auto & vertex : torus.vertices() )
	{
		for ( size_t axis = 0; axis < 3; ++axis )
		{
			minimum[axis] = std::min(minimum[axis], vertex.position()[axis]);
			maximum[axis] = std::max(maximum[axis], vertex.position()[axis]);
		}
	}

	size_t axis = 0;

	for ( size_t candidate = 1; candidate < 3; ++candidate )
	{
		if ( maximum[candidate] - minimum[candidate] < maximum[axis] - minimum[axis] )
		{
			axis = candidate;
		}
	}

	V3 normal{};
	normal[axis] = 1.0F;

	/* Slightly off the middle ring of vertices. */
	const Math::Plane< float > plane{normal, normal * 0.01F};

	expectClosedHalves(torus, plane, 1.0E-4F, "torus");

	const ShapeSplitter< float, uint32_t > splitter{torus, plane, 1.0E-5F, true};
	const auto result = splitter.split();
	ASSERT_EQ(result.frontParts.size(), 1U);

	size_t inHole = 0;

	for ( const auto & triangle : result.frontParts[0].triangles() )
	{
		const auto & vertices = result.frontParts[0].vertices();
		const auto centroid = (vertices[triangle.vertexIndex(0)].position() + vertices[triangle.vertexIndex(1)].position() + vertices[triangle.vertexIndex(2)].position()) / 3.0F;
		auto radial = centroid;
		radial[axis] = 0.0F;

		if ( radial.length() < 0.69F )
		{
			++inHole;
		}
	}

	EXPECT_EQ(inHole, 0U) << inHole << " triangles cover the torus hole";
}

TEST(VertexFactoryShapeSplitter, twoDisjointBodiesAreCutAndSealedApart)
{
	const auto sphere = weldedSphere();
	Shape< float, uint32_t > pair;
	appendShape(pair, sphere, V3{-3.0F, 0.0F, 0.0F});
	appendShape(pair, sphere, V3{3.0F, 0.0F, 0.0F});

	EXPECT_EQ((ShapeSplitter< float, uint32_t >::extractConnectedComponents(pair).size()), 2U);

	expectClosedHalves(pair, Math::Plane< float >{V3{0.0F, 1.0F, 0.0F}, V3{0.0F, 0.1F, 0.0F}}, 1.0E-4F, "two spheres");
}

TEST(VertexFactoryShapeSplitter, largeCoordinatesAreSealedWithoutOverflow)
{
	/* The cut points are welded by quantizing their position to 1e-4 into an int32: a coordinate beyond ~214 748
	 * (2 km in centimetres) overflowed the conversion (undefined behaviour). A sphere of radius 1 placed 1e6 away. */
	const auto sphere = weldedSphere();
	Shape< float, uint32_t > far;
	appendShape(far, sphere, V3{1.0E6F, 0.0F, 0.0F});

	const ShapeSplitter< float, uint32_t > splitter{far, Math::Plane< float >{V3{0.0F, 1.0F, 0.0F}, V3{0.0F, 0.1F, 0.0F}}, 1.0E-5F, true};
	const auto result = splitter.split();

	ASSERT_TRUE(result.wasSplit);
	ASSERT_EQ(result.frontParts.size(), 1U);
	ASSERT_EQ(result.backParts.size(), 1U);
	EXPECT_GT(result.frontParts[0].triangles().size(), 0U);
}

TEST(VertexFactoryShapeSplitter, nestedLoopsKeepTheirGapsAndIslands)
{
	/* A solid sphere (radius 1) inside a hollow shell (inner face radius 1.5, outer 2): the cut through the centre
	 * holds three loops — a contour (2), a hole (1.5) and an island (1). The caps fill [1.5, 2] and [0, 1], never the
	 * gap (1, 1.5). */
	const auto sphere = weldedSphere();
	Shape< float, uint32_t > nested;
	appendShape(nested, sphere, V3{}, 2.0F);
	appendShape(nested, sphere, V3{}, 1.5F, true);
	appendShape(nested, sphere, V3{}, 1.0F);

	const Math::Plane< float > plane{V3{0.0F, 1.0F, 0.0F}, V3{0.0F, 0.01F, 0.0F}};
	expectClosedHalves(nested, plane, 1.0E-4F, "nested spheres");

	const ShapeSplitter< float, uint32_t > splitter{nested, plane, 1.0E-5F, true};
	const auto result = splitter.split();
	ASSERT_EQ(result.frontParts.size(), 1U);

	size_t inGap = 0;
	const auto & part = result.frontParts[0];

	for ( const auto & triangle : part.triangles() )
	{
		const auto centroid = (part.vertices()[triangle.vertexIndex(0)].position() + part.vertices()[triangle.vertexIndex(1)].position() + part.vertices()[triangle.vertexIndex(2)].position()) / 3.0F;

		if ( std::abs(centroid[1] - 0.01F) < 1.0E-3F )
		{
			const auto radius = std::hypot(centroid[0], centroid[2]);

			if ( radius > 1.02F && radius < 1.48F )
			{
				++inGap;
			}
		}
	}

	EXPECT_EQ(inGap, 0U) << inGap << " cap triangles fill the gap between the solid sphere and the shell";
}

