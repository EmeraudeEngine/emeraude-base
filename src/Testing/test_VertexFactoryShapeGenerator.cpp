/*
 * src/Testing/test_VertexFactoryShapeGenerator.cpp
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
#include <cmath>
#include <cstdint>

/* Local inclusions. */
#include "Math/Vector.hpp"
#include "VertexFactory/Shape.hpp"
#include "VertexFactory/ShapeGenerator.hpp"

using namespace EmEn::Base::VertexFactory;

namespace
{
	constexpr auto Tolerance = 1.0E-5F;

	/* Order: normals, texture coordinates, vertex colours, influences, weights.
	 *
	 * Declared explicitly to state intent, NOT because it is required: every ShapeBuilderOptions flag
	 * defaults to false, yet these assertions still hold on a default-constructed set. Two reasons,
	 * both verified by running this suite with `ShapeBuilderOptions< float >{}` (11/11 still passed):
	 *   - the normals arrive because enableGlobalNormal() sets m_normalsEnabled itself, and every
	 *     generator here calls it;
	 *   - the UVs arrive because ShapeBuilder::emitTriangle() copies the texture coordinate array
	 *     into the shape UNCONDITIONALLY, without consulting isTextureCoordinatesEnabled().
	 * Keep the explicit set anyway: it stops depending on that self-enabling side effect. */
	ShapeBuilderOptions< float >
	uvOptions () noexcept
	{
		return ShapeBuilderOptions< float >{true, true, true, false, false};
	}

	/**
	 * @brief Asserts the Y-up UV pairing on every VERTICAL face of a shape.
	 *
	 * The convention (see VertexFactory/AGENTS.md, "Texture coordinate convention"): V = 0 is the
	 * top row of the image, so on a face whose normal lies in the XZ plane it must pair with the
	 * +Y edge. A vertex above the shape's mid-height therefore carries a SMALLER V than one below.
	 *
	 * This is the mechanical detector for the Y-up "V pairing" defect class: the geometry, the
	 * normals and the winding can all be correct while the texture renders upside down.
	 */
	void
	expectVerticalFacesPairVZeroWithMaxY (const Shape< float, uint32_t > & shape, const char * label)
	{
		auto verticalVertexCount = 0U;

		for ( const auto & vertex : shape.vertices() )
		{
			const auto & normal = vertex.normal();

			/* Keep only the vertical faces: a horizontal face follows generatePlane instead. */
			if ( std::abs(normal[EmEn::Base::Math::Y]) > 0.5F )
			{
				continue;
			}

			++verticalVertexCount;

			const auto positionY = vertex.position()[EmEn::Base::Math::Y];
			const auto texV = vertex.textureCoordinates()[EmEn::Base::Math::Y];

			if ( positionY > Tolerance )
			{
				EXPECT_NEAR(texV, 0.0F, Tolerance)
					<< label << ": vertex above mid-height (Y=" << positionY
					<< ") must carry V=0, the image TOP. V paired with -Y is the Y-down authoring.";
			}
			else if ( positionY < -Tolerance )
			{
				EXPECT_NEAR(texV, 1.0F, Tolerance)
					<< label << ": vertex below mid-height (Y=" << positionY
					<< ") must carry V=1, the image BOTTOM.";
			}
		}

		EXPECT_GT(verticalVertexCount, 0U) << label << ": no vertical face found, the check was vacuous.";
	}
}

/*
 * generateTriangle must produce an EQUILATERAL triangle of side 'size', centred on its bounding box.
 *
 * Regression: the apex was derived from the height (size * sqrt(3) / 2) while the base sat at
 * -size/2, which made the two legs 5.9% longer than the base. The shape was isoceles, not
 * equilateral, and it was centred on neither its bounding box nor its centroid.
 */
TEST(VertexFactoryShapeGenerator, triangleIsEquilateral)
{
	constexpr auto Side = 2.0F;

	const auto shape = ShapeGenerator::generateTriangle< float, uint32_t >(Side, uvOptions());

	ASSERT_EQ(shape.vertices().size(), 3U);

	const auto & a = shape.vertices()[0].position();
	const auto & b = shape.vertices()[1].position();
	const auto & c = shape.vertices()[2].position();

	EXPECT_NEAR((b - a).length(), Side, Tolerance);
	EXPECT_NEAR((c - b).length(), Side, Tolerance);
	EXPECT_NEAR((a - c).length(), Side, Tolerance);
}

/* The same shape must be symmetric about the origin on Y: the apex sits at +halfHeight and the base
 * at -halfHeight. Symmetric bounds are what let the volumetric vertex colour formula span [0,1]. */
TEST(VertexFactoryShapeGenerator, triangleIsBoundingBoxCentredOnY)
{
	constexpr auto Side = 2.0F;
	const auto halfHeight = Side * std::sqrt(3.0F) * 0.25F;

	const auto shape = ShapeGenerator::generateTriangle< float, uint32_t >(Side, uvOptions());

	ASSERT_EQ(shape.vertices().size(), 3U);

	/* The apex is the only vertex above the mid-height; the two base vertices share the bottom. */
	EXPECT_NEAR(shape.vertices()[0].position()[EmEn::Base::Math::Y], halfHeight, Tolerance);
	EXPECT_NEAR(shape.vertices()[1].position()[EmEn::Base::Math::Y], -halfHeight, Tolerance);
	EXPECT_NEAR(shape.vertices()[2].position()[EmEn::Base::Math::Y], -halfHeight, Tolerance);
}

/*
 * The Y-up V pairing, on every hand-authored generator that has vertical faces.
 *
 * These four all failed before Aug 2026: the Y-up switch reversed the emission order (the winding)
 * but left every setTextureCoordinates call paired with the position it had when -Y was up, so the
 * texture rendered upside down on geometry that was otherwise perfectly correct. The suite compiled
 * clean and passed 1975/1975 while the defect was live, because nothing asserted the PAIRING.
 */
TEST(VertexFactoryShapeGenerator, trianglePairsVZeroWithMaxY)
{
	const auto shape = ShapeGenerator::generateTriangle< float, uint32_t >(2.0F, uvOptions());

	expectVerticalFacesPairVZeroWithMaxY(shape, "generateTriangle");
}

TEST(VertexFactoryShapeGenerator, quadPairsVZeroWithMaxY)
{
	const auto shape = ShapeGenerator::generateQuad< float, uint32_t >(2.0F, 2.0F, uvOptions());

	expectVerticalFacesPairVZeroWithMaxY(shape, "generateQuad");
}

TEST(VertexFactoryShapeGenerator, cuboidPairsVZeroWithMaxY)
{
	const auto shape = ShapeGenerator::generateCuboid< float, uint32_t >(2.0F, 2.0F, 2.0F, uvOptions());

	expectVerticalFacesPairVZeroWithMaxY(shape, "generateCuboid(w, h, d)");
}

TEST(VertexFactoryShapeGenerator, cuboidFromCornersPairsVZeroWithMaxY)
{
	const EmEn::Base::Math::Vector< 3, float > max{1.0F, 1.0F, 1.0F};
	const EmEn::Base::Math::Vector< 3, float > min{-1.0F, -1.0F, -1.0F};

	const auto shape = ShapeGenerator::generateCuboid< float, uint32_t >(max, min, uvOptions());

	expectVerticalFacesPairVZeroWithMaxY(shape, "generateCuboid(max, min)");
}

/*
 * generateScreenQuad is a DELIBERATE exception to the pairing rule above.
 *
 * It is a fullscreen NDC quad for the post-processor and the overlay manager, whose source images
 * are already in screen space, so its V pairs with +Y — the OPPOSITE of every world-space generator.
 * These tests exist so that a future "harmonisation" sweep over the V axis FAILS LOUDLY here
 * instead of silently flipping the whole post-process chain and the entire overlay.
 */
TEST(VertexFactoryShapeGenerator, screenQuadSpansNormalizedDeviceCoordinates)
{
	const auto shape = ShapeGenerator::generateScreenQuad< float, uint32_t >();

	ASSERT_EQ(shape.vertices().size(), 4U);

	for ( const auto & vertex : shape.vertices() )
	{
		const auto & position = vertex.position();

		EXPECT_NEAR(std::abs(position[EmEn::Base::Math::X]), 1.0F, Tolerance);
		EXPECT_NEAR(std::abs(position[EmEn::Base::Math::Y]), 1.0F, Tolerance);
		EXPECT_NEAR(position[EmEn::Base::Math::Z], 0.0F, Tolerance);
	}
}

TEST(VertexFactoryShapeGenerator, screenQuadPairsVWithPositiveYOnPurpose)
{
	const auto shape = ShapeGenerator::generateScreenQuad< float, uint32_t >();

	ASSERT_EQ(shape.vertices().size(), 4U);

	for ( const auto & vertex : shape.vertices() )
	{
		const auto positionY = vertex.position()[EmEn::Base::Math::Y];
		const auto texV = vertex.textureCoordinates()[EmEn::Base::Math::Y];

		/* Screen space, NOT world space: +Y carries V=1 here. Do not "fix" this to match the
		 * world-space generators — the post-processor and the overlay depend on the inversion. */
		EXPECT_NEAR(texV, positionY > 0.0F ? 1.0F : 0.0F, Tolerance)
			<< "generateScreenQuad is screen space: V must pair with +Y, inverted on purpose.";
	}
}

/* generateScreenQuad takes NO ShapeBuilderOptions, so it builds on a default-constructed set where
 * every flag is false. enableGlobalNormal() is then a NO-OP, because newVertex() only copies the
 * global normal when isNormalsEnabled() is true — while emitTriangle() writes the normal array to
 * the shape unconditionally. The declared +Z normal must actually reach the vertices. */
TEST(VertexFactoryShapeGenerator, screenQuadCarriesItsDeclaredNormal)
{
	const auto shape = ShapeGenerator::generateScreenQuad< float, uint32_t >();

	ASSERT_EQ(shape.vertices().size(), 4U);

	for ( const auto & vertex : shape.vertices() )
	{
		EXPECT_NEAR(vertex.normal()[EmEn::Base::Math::Z], 1.0F, Tolerance)
			<< "the declared global +Z normal never reached the shape: ShapeBuilderOptions defaults "
			   "every flag to false, so newVertex() skipped the global-normal copy.";
	}
}

/*
 * The HORIZONTAL faces follow generatePlane instead: on a +Y-facing surface U grows with +X and V
 * grows with +Z. generatePlane is the reference because the owner confirmed the ground renders its
 * texture correctly on screen; the cuboid's top face must agree with it.
 */
TEST(VertexFactoryShapeGenerator, planeTopFaceGrowsVWithPositiveZ)
{
	const auto shape = ShapeGenerator::generatePlane< float, uint32_t >(2.0F, 2.0F, 1U, 1U, uvOptions());

	ASSERT_FALSE(shape.vertices().empty());

	for ( const auto & vertex : shape.vertices() )
	{
		EXPECT_GT(vertex.normal()[EmEn::Base::Math::Y], 0.5F) << "generatePlane must face +Y (up).";

		const auto positionZ = vertex.position()[EmEn::Base::Math::Z];
		const auto texV = vertex.textureCoordinates()[EmEn::Base::Math::Y];

		if ( positionZ > Tolerance )
		{
			EXPECT_NEAR(texV, 1.0F, Tolerance) << "generatePlane: V must grow with +Z.";
		}
		else if ( positionZ < -Tolerance )
		{
			EXPECT_NEAR(texV, 0.0F, Tolerance) << "generatePlane: V must grow with +Z.";
		}
	}
}

TEST(VertexFactoryShapeGenerator, cuboidTopFaceAgreesWithPlane)
{
	const auto shape = ShapeGenerator::generateCuboid< float, uint32_t >(2.0F, 2.0F, 2.0F, uvOptions());

	auto topVertexCount = 0U;

	for ( const auto & vertex : shape.vertices() )
	{
		if ( vertex.normal()[EmEn::Base::Math::Y] < 0.5F )
		{
			continue;
		}

		++topVertexCount;

		const auto positionZ = vertex.position()[EmEn::Base::Math::Z];
		const auto texV = vertex.textureCoordinates()[EmEn::Base::Math::Y];

		if ( positionZ > Tolerance )
		{
			EXPECT_NEAR(texV, 1.0F, Tolerance) << "cuboid +Y face: V must grow with +Z, like generatePlane.";
		}
		else if ( positionZ < -Tolerance )
		{
			EXPECT_NEAR(texV, 0.0F, Tolerance) << "cuboid +Y face: V must grow with +Z, like generatePlane.";
		}
	}

	EXPECT_EQ(topVertexCount, 4U) << "the cube must expose exactly four +Y-facing vertices.";
}