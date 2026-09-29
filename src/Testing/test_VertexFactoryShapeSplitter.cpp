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
#include <cstdint>
#include <string>

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
