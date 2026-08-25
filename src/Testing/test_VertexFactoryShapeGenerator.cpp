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
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

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

	struct WindingCounts
	{
		unsigned agreeing{0};
		unsigned disagreeing{0};
	};

	/**
	 * @brief Counts the triangles whose geometric winding agrees with their own vertex normals.
	 *
	 * A front face winds COUNTER-CLOCKWISE around its outward normal
	 * (`VK_FRONT_FACE_COUNTER_CLOCKWISE`, the pipeline default), so `cross(B-A, C-A)` must fall on
	 * the same side as the normals the generator authored. This is the winding check
	 * `docs/coordinate-system.md` mandates: COMPUTE it, never judge it by eye.
	 *
	 * ⚠️ **Not circular, and that is load-bearing.** `ShapeGenerator` never calls
	 * `computeVertexNormal()` or `computeTriangleNormal()` — every normal is set explicitly from the
	 * parametric surface, so the normals are independent evidence about the emission order. If a
	 * generator ever starts deriving its normals FROM the winding, this check silently becomes
	 * vacuous and `theWindingCheckRejectsAMirroredShape` below is what will catch it.
	 *
	 * ⚠️⚠️ Two families of triangle carry NO evidence and are skipped, otherwise the result is noise:
	 *  - **degenerate** ones (zero area) — the collapsed quads at a sphere's poles;
	 *  - ones **straddling a normal discontinuity** — a cap fan sharing a rim vertex with the side.
	 *    Averaging a radial normal with a cap normal gives a direction that is NOT the face's
	 *    outward normal. Skipping this filter reported 13 false positives on `generateArrow` alone,
	 *    which read exactly like a real mirror defect.
	 */
	WindingCounts
	countWindingAgreement (const Shape< float, uint32_t > & shape) noexcept
	{
		using V3 = EmEn::Base::Math::Vector< 3, float >;

		WindingCounts counts;

		for ( const auto & triangle : shape.triangles() )
		{
			const auto & a = shape.vertices()[triangle.vertexIndex(0)];
			const auto & b = shape.vertices()[triangle.vertexIndex(1)];
			const auto & c = shape.vertices()[triangle.vertexIndex(2)];

			const auto geometric = V3::crossProduct(b.position() - a.position(), c.position() - a.position());

			if ( geometric.length() < 1.0E-7F )
			{
				continue;
			}

			const auto normalA = a.normal();
			const auto normalB = b.normal();
			const auto normalC = c.normal();

			if ( V3::dotProduct(normalA, normalB) < 0.5F || V3::dotProduct(normalA, normalC) < 0.5F || V3::dotProduct(normalB, normalC) < 0.5F )
			{
				continue;
			}

			if ( V3::dotProduct(geometric, normalA + normalB + normalC) > 0.0F )
			{
				++counts.agreeing;
			}
			else
			{
				++counts.disagreeing;
			}
		}

		return counts;
	}

	struct OutwardCounts
	{
		unsigned windingOutward{0};
		unsigned windingInward{0};
		unsigned normalsOutward{0};
		unsigned normalsInward{0};
	};

	/**
	 * @brief Measures a CONVEX shape against a reference independent of its authored normals.
	 *
	 * For a convex solid the outward direction at a face is `faceCentre - centroid`, which owes
	 * nothing to what the generator wrote into its normals. That independence is the whole point
	 * here: the gem generators are the one family whose authored normals cannot be trusted as the
	 * reference, so `countWindingAgreement()` — which compares against exactly those normals —
	 * reports a defect for every facet whose normal points the wrong way and tells you nothing
	 * about the winding. Measured Aug 2026: judged that way, 11 of the 12 cuts looked
	 * "mirror-wound" and NONE of them was.
	 *
	 * ⚠️ Valid for CONVEX shapes only. Every gem cut here is convex; do not reuse this on a shape
	 * with a concavity, where a face centre can legitimately sit behind the centroid.
	 */
	OutwardCounts
	countOutwardAgreement (const Shape< float, uint32_t > & shape) noexcept
	{
		using V3 = EmEn::Base::Math::Vector< 3, float >;

		OutwardCounts counts;

		V3 centroid{0.0F, 0.0F, 0.0F};

		for ( const auto & vertex : shape.vertices() )
		{
			centroid += vertex.position();
		}

		centroid /= static_cast< float >(shape.vertices().size());

		for ( const auto & triangle : shape.triangles() )
		{
			const auto & a = shape.vertices()[triangle.vertexIndex(0)];
			const auto & b = shape.vertices()[triangle.vertexIndex(1)];
			const auto & c = shape.vertices()[triangle.vertexIndex(2)];

			/* ⚠️⚠️ Judge normalizability the way the library does, not with an epsilon of our own.
			 * `normalized()` gives up when `lengthSquared()` trips `Utility::isZero()`, so a sliver
			 * can clear a `length() > 1e-7` test and still have NO normal at all — the generators
			 * fall back to the authored one there, and its direction is arbitrary. A triangle the
			 * library cannot normalize carries no evidence, exactly like a degenerate one. */
			const auto geometric = V3::crossProduct(b.position() - a.position(), c.position() - a.position()).normalized();

			if ( geometric.lengthSquared() < 0.5F )
			{
				continue;
			}

			const auto outward = ((a.position() + b.position() + c.position()) / 3.0F) - centroid;

			if ( outward.length() < 1.0E-5F )
			{
				continue;
			}

			(V3::dotProduct(geometric, outward) > 0.0F ? counts.windingOutward : counts.windingInward)++;
			(V3::dotProduct(a.normal(), outward) > 0.0F ? counts.normalsOutward : counts.normalsInward)++;
		}

		return counts;
	}

	void
	expectConvexShapeFacesOutward (const Shape< float, uint32_t > & shape, const char * label)
	{
		const auto counts = countOutwardAgreement(shape);

		EXPECT_GT(counts.windingOutward, 0U) << label << ": no usable face, the check was vacuous.";

		EXPECT_EQ(counts.windingInward, 0U)
			<< label << ": " << counts.windingInward << " face(s) wind the wrong way round the solid.";

		EXPECT_EQ(counts.normalsInward, 0U)
			<< label << ": " << counts.normalsInward << " facet normal(s) point INTO the solid, so those "
			<< "facets are lit as if facing away. A flat facet's normal must be its own geometric normal.";
	}

	void
	expectFrontFacesWindCCWAroundTheirNormal (const Shape< float, uint32_t > & shape, const char * label)
	{
		const auto counts = countWindingAgreement(shape);

		EXPECT_EQ(counts.disagreeing, 0U)
			<< label << ": " << counts.disagreeing << " triangle(s) wind CLOCKWISE around their own "
			<< "outward normal, i.e. mirror-wound. Front faces must be CCW.";

		EXPECT_GT(counts.agreeing, 0U)
			<< label << ": no triangle carried usable evidence, the check was vacuous.";
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

/*
 * ⚠️⚠️ WINDING GATE for the loop-driven generators.
 *
 * These were listed as "still mirror-wound" by the Y-up migration plan for weeks. MEASURED
 * 2026-08-25: all of them are CORRECT, so this test locks that in rather than fixing anything.
 * The three already-audited shapes are kept as CONTROLS: a probe that fails them is a broken probe,
 * not a discovery.
 */
TEST(VertexFactoryShapeGenerator, loopDrivenGeneratorsWindCCWAroundTheirNormals)
{
	/* Controls — audited and shipped before this gate existed. */
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generateCuboid< float, uint32_t >(1.0F, 1.0F, 1.0F, uvOptions()), "cuboid [control]");
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generatePlane< float, uint32_t >(1.0F, 1.0F, 1, 1, uvOptions()), "plane [control]");
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generateTriangle< float, uint32_t >(1.0F, uvOptions()), "triangle [control]");

	/* The loop-driven generators. */
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 16, 8, uvOptions()), "sphere");
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generateCylinder< float, uint32_t >(1.0F, 1.0F, 2.0F, 16, 4, CapUVMapping::None, uvOptions()), "cylinder");
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generateCone< float, uint32_t >(1.0F, 2.0F, 16, 4, CapUVMapping::None, uvOptions()), "cone");
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generateDisk< float, uint32_t >(1.0F, 0.5F, 16, 1, CapUVMapping::Planar, uvOptions()), "disk");
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generateTorus< float, uint32_t >(1.0F, 0.3F, 16, 16, uvOptions()), "torus");
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generateCapsule< float, uint32_t >(0.5F, 2.0F, 16, 8, uvOptions()), "capsule");
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generateHemisphere< float, uint32_t >(1.0F, 16, 8, uvOptions()), "hemisphere");
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generateTube< float, uint32_t >(1.0F, 0.8F, 2.0F, 16, 4, CapUVMapping::Planar, uvOptions()), "tube");
	expectFrontFacesWindCCWAroundTheirNormal(ShapeGenerator::generateArrow< float, uint32_t >(0.05F, 0.15F, 0.7F, 0.3F, 16, uvOptions()), "arrow");
}

/*
 * ⚠️⚠️ The gate above is only worth its runtime if it can FAIL. This proves it permanently.
 *
 * `Shape::reverseWinding()` swaps the triangle indices and deliberately leaves the normals alone,
 * which is exactly the mirror-wound state the Y-up migration had to eradicate. Every triangle that
 * carried evidence must flip its verdict — not merely "some should fail".
 *
 * Without this, a future refactor that made the normals derive from the winding would turn the gate
 * into a tautology that passes forever while measuring nothing.
 */
TEST(VertexFactoryShapeGenerator, theWindingCheckRejectsAMirroredShape)
{
	auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 16, 8, uvOptions());

	const auto before = countWindingAgreement(shape);

	ASSERT_EQ(before.disagreeing, 0U);
	ASSERT_GT(before.agreeing, 0U);

	shape.reverseWinding();

	const auto after = countWindingAgreement(shape);

	EXPECT_EQ(after.agreeing, 0U) << "a mirror-wound shape must not agree with its own normals anywhere";
	EXPECT_EQ(after.disagreeing, before.agreeing) << "every triangle that carried evidence must flip its verdict";
}

/*
 * ⚠️⚠️ GEM CUT GATE — winding AND normal direction, judged against the solid itself.
 *
 * The gem generators are the one family whose authored normals are not a usable reference: they
 * pass a separately-computed normal to their emitTriangle(), which is free to disagree with the
 * winding of the triangle it is attached to. Measured Aug 2026: 11 of the 12 cuts carried facet
 * normals pointing INTO the solid (princess: every single one), while their winding was correct
 * everywhere. Judging them against their own normals reports the exact opposite conclusion.
 *
 * So this gate uses `faceCentre - centroid`, which is independent of both.
 */
TEST(VertexFactoryShapeGenerator, gemCutsWindAndFaceOutward)
{
	expectConvexShapeFacesOutward(ShapeGenerator::generateDiamondCutGem< float, uint32_t >(), "diamond cut");
	expectConvexShapeFacesOutward(ShapeGenerator::generateEmeraldCutGem< float, uint32_t >(), "emerald cut");
	expectConvexShapeFacesOutward(ShapeGenerator::generateAsscherCutGem< float, uint32_t >(), "asscher cut");
	expectConvexShapeFacesOutward(ShapeGenerator::generateBaguetteCutGem< float, uint32_t >(), "baguette cut");
	expectConvexShapeFacesOutward(ShapeGenerator::generatePrincessCutGem< float, uint32_t >(), "princess cut");
	expectConvexShapeFacesOutward(ShapeGenerator::generateTrillionCutGem< float, uint32_t >(), "trillion cut");
	expectConvexShapeFacesOutward(ShapeGenerator::generateOvalCutGem< float, uint32_t >(), "oval cut");
	expectConvexShapeFacesOutward(ShapeGenerator::generateCushionCutGem< float, uint32_t >(), "cushion cut");
	expectConvexShapeFacesOutward(ShapeGenerator::generateMarquiseCutGem< float, uint32_t >(), "marquise cut");
	expectConvexShapeFacesOutward(ShapeGenerator::generatePearCutGem< float, uint32_t >(), "pear cut");
	expectConvexShapeFacesOutward(ShapeGenerator::generateHeartCutGem< float, uint32_t >(), "heart cut");
	expectConvexShapeFacesOutward(ShapeGenerator::generateRoseCutGem< float, uint32_t >(), "rose cut");
}

/*
 * ⚠️⚠️ generateSphere had its texture coordinates TRANSPOSED until Aug 2026: the accumulator named
 * U advanced per STACK (latitude) and the one named V per SLICE (longitude), so each landed in the
 * other's slot and every texture came out rotated a quarter turn on the sphere. The latitude also
 * ran 1 at the +Y pole instead of 0.
 *
 * ⚠️ It is NOT a Y-up residue -- it predates the flip and depends on no axis sign. It survived the
 * whole migration audit because the obvious probe cannot see it: comparing V against Y on a sphere
 * reads a flat 0.5 above and below the equator when V is actually longitude, which looks like a
 * symmetric shape rather than a defect.
 *
 * The ring test below is what actually discriminates: within ONE latitude ring, longitude must vary
 * and latitude must not. Transposed, the ring shows the exact opposite.
 */
TEST(VertexFactoryShapeGenerator, sphereMapsUToLongitudeAndVToLatitude)
{
	constexpr auto Slices = 16;
	constexpr auto Stacks = 8;
	constexpr auto Tolerance = 1.0E-4F;

	const auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, Slices, Stacks, uvOptions());

	float minY = 1.0E9F;
	float maxY = -1.0E9F;
	float vAtMinY = -1.0F;
	float vAtMaxY = -1.0F;

	std::map< int, std::vector< std::pair< float, float > > > rings;

	for ( const auto & vertex : shape.vertices() )
	{
		const auto y = vertex.position()[EmEn::Base::Math::Y];
		const auto u = vertex.textureCoordinates()[EmEn::Base::Math::X];
		const auto v = vertex.textureCoordinates()[EmEn::Base::Math::Y];

		if ( y < minY ) { minY = y; vAtMinY = v; }
		if ( y > maxY ) { maxY = y; vAtMaxY = v; }

		rings[static_cast< int >(std::lround(y * 1000.0F))].emplace_back(u, v);
	}

	/* V = 0 is the image TOP and must pair with the +Y pole, as for every other generator. */
	EXPECT_NEAR(vAtMaxY, 0.0F, Tolerance) << "the +Y pole must carry V = 0, the image top";
	EXPECT_NEAR(vAtMinY, 1.0F, Tolerance) << "the -Y pole must carry V = 1, the image bottom";

	/* The most populated ring away from the poles, where a ring is a genuine circle of vertices
	 * rather than a collapsed point. */
	const std::vector< std::pair< float, float > > * widest = nullptr;

	for ( const auto & [key, ring] : rings )
	{
		if ( std::abs(static_cast< float >(key) / 1000.0F) < 0.9F && (widest == nullptr || ring.size() > widest->size()) )
		{
			widest = &ring;
		}
	}

	ASSERT_NE(widest, nullptr) << "no latitude ring found, the check would be vacuous";

	float uMin = 1.0E9F;
	float uMax = -1.0E9F;
	float vMin = 1.0E9F;
	float vMax = -1.0E9F;

	for ( const auto & [u, v] : *widest )
	{
		uMin = std::min(uMin, u);
		uMax = std::max(uMax, u);
		vMin = std::min(vMin, v);
		vMax = std::max(vMax, v);
	}

	/* THE discriminator: along a latitude ring the longitude sweeps and the latitude holds. */
	EXPECT_GT(uMax - uMin, 0.5F)
		<< "U must sweep the longitude along a latitude ring (measured span " << (uMax - uMin)
		<< "). A near-zero span means U carries the LATITUDE: the coordinates are transposed.";

	EXPECT_NEAR(vMax - vMin, 0.0F, Tolerance)
		<< "V must stay constant along a latitude ring (measured span " << (vMax - vMin)
		<< "). A wide span means V carries the LONGITUDE: the coordinates are transposed.";
}

/*
 * ⚠️⚠️ A sweeping U is not enough: it must sweep the RIGHT WAY ROUND, or the texture is mirrored.
 *
 * Untransposing the coordinates left this second defect standing, and it survived a polar screenshot
 * because a mirrored globe still converges cleanly at the pole and still shows a plausible Arctic --
 * only the handedness gives it away. The owner caught it on screen; this pins it.
 *
 * The rule: with north at +Y, EAST is the POSITIVE rotation about +Y (the right-hand rule, which is
 * why the Earth turns counter-clockwise seen from above the north pole). So walking a latitude ring
 * in the direction of increasing U must turn positively about +Y, i.e. cross(p1, p2) . +Y > 0.
 */
TEST(VertexFactoryShapeGenerator, sphereUGrowsEastwardNotWestward)
{
	using V3 = EmEn::Base::Math::Vector< 3, float >;

	constexpr auto Slices = 16;
	constexpr auto Stacks = 8;

	const auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, Slices, Stacks, uvOptions());

	/* Collect one latitude ring away from the poles, keyed like the test above. */
	std::map< int, std::vector< std::pair< float, V3 > > > rings;

	for ( const auto & vertex : shape.vertices() )
	{
		const auto & position = vertex.position();

		if ( std::abs(position[EmEn::Base::Math::Y]) > 0.9F )
		{
			continue;
		}

		rings[static_cast< int >(std::lround(position[EmEn::Base::Math::Y] * 1000.0F))]
			.emplace_back(vertex.textureCoordinates()[EmEn::Base::Math::X], position);
	}

	const std::vector< std::pair< float, V3 > > * widest = nullptr;

	for ( const auto & [key, ring] : rings )
	{
		if ( widest == nullptr || ring.size() > widest->size() )
		{
			widest = &ring;
		}
	}

	ASSERT_NE(widest, nullptr) << "no latitude ring found, the check would be vacuous";

	auto sorted = *widest;

	std::sort(sorted.begin(), sorted.end(), [] (const auto & lhs, const auto & rhs) {
		return lhs.first < rhs.first;
	});

	/* Consecutive samples only, and only those genuinely apart in U: a seam vertex duplicated at the
	 * same longitude carries no direction and would just add noise. */
	auto eastward = 0;
	auto westward = 0;

	for ( size_t index = 0; index + 1 < sorted.size(); ++index )
	{
		if ( sorted[index + 1].first - sorted[index].first < 1.0E-4F )
		{
			continue;
		}

		const auto & a = sorted[index].second;
		const auto & b = sorted[index + 1].second;

		/* Y component of the cross product: the sign of the rotation about +Y. */
		const auto turn = V3::crossProduct(a, b)[EmEn::Base::Math::Y];

		if ( turn > 0.0F ) { ++eastward; } else if ( turn < 0.0F ) { ++westward; }
	}

	EXPECT_GT(eastward, 0) << "no usable pair on the ring, the check was vacuous";

	EXPECT_EQ(westward, 0)
		<< westward << " step(s) of increasing U turn NEGATIVELY about +Y, i.e. westward. "
		<< "The texture is MIRRORED on the sphere: U must grow eastward, the positive rotation.";
}

/*
 * The seam has to live SOMEWHERE, so where it lives is a convention -- and an unwritten convention
 * is one nobody can preserve. This states it.
 *
 * U = 0 and U = 1 both fall on +Z, and U = 0.5 on -Z. On an equirectangular map the U edges are the
 * ANTIMERIDIAN (180 degrees) and U = 0.5 is the prime meridian, so this puts Greenwich on -Z -- the
 * engine's FORWARD -- and the seam on +Z. That is a good place for it: cartographers already put
 * the antimeridian mid-Pacific precisely so it cuts no land, so the seam falls where there is
 * nothing to misalign.
 *
 * ⚠️ The seam vertices are DUPLICATED at the same position, one carrying U = 0 and one U = 1. That
 * is what stops the texture being smeared backwards across the last quad. A generator that emitted
 * a single shared vertex there would interpolate U from 1 back to 0 across one slice and squeeze
 * the entire map into it.
 */
TEST(VertexFactoryShapeGenerator, sphereSeamSitsOnPositiveZ)
{
	constexpr auto Tolerance = 1.0E-3F;

	const auto shape = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 16, 8, uvOptions());

	auto seamAtZeroU = 0;
	auto seamAtOneU = 0;
	auto middleFound = false;

	for ( const auto & vertex : shape.vertices() )
	{
		const auto & position = vertex.position();

		/* Equatorial band only: near the poles every U converges and says nothing. */
		if ( std::abs(position[EmEn::Base::Math::Y]) > 0.4F )
		{
			continue;
		}

		const auto u = vertex.textureCoordinates()[EmEn::Base::Math::X];

		if ( u < Tolerance || u > 1.0F - Tolerance )
		{
			EXPECT_NEAR(position[EmEn::Base::Math::X], 0.0F, 0.01F) << "the seam must sit on the Z axis";
			EXPECT_GT(position[EmEn::Base::Math::Z], 0.0F) << "the seam must sit on +Z, not -Z";

			if ( u < Tolerance ) { ++seamAtZeroU; } else { ++seamAtOneU; }
		}

		if ( std::abs(u - 0.5F) < Tolerance )
		{
			EXPECT_LT(position[EmEn::Base::Math::Z], 0.0F)
				<< "U = 0.5 is the prime meridian and must face -Z, the engine's forward";
			middleFound = true;
		}
	}

	EXPECT_TRUE(middleFound) << "no U = 0.5 vertex found, the check was vacuous";

	/* Both sides of the seam must exist, or the texture wraps backwards across the last quad. */
	EXPECT_GT(seamAtZeroU, 0) << "no U = 0 vertex on the seam";
	EXPECT_GT(seamAtOneU, 0) << "no U = 1 vertex on the seam: the seam is not duplicated";
}

/*
 * ⚠️⚠️ GOLDEN GEOMETRY for the twelve gem cuts, captured 2026-08-25 from the code as it stood BEFORE
 * their facet math was re-authored from the retired Y-down frame to Y-up.
 *
 * That re-authoring had to be a NO-OP on the output -- the geometry was already correct, the mirror
 * step merely produced it a different way -- and these values are what said so. They are still the
 * reference: any future change to a cut's proportions is a deliberate act that re-captures a row,
 * never a quiet drift.
 *
 * Every invariant is ORDER-INDEPENDENT and taken over TRIANGLE CORNERS, because re-authoring
 * legitimately changed both the emission order (reversing a mirror reverses the winding, so each
 * face gets listed the other way round) and the vertex sharing that follows from it.
 *
 * ⚠️ A gate on winding and normals alone would NOT catch a botched re-authoring: a facet ring
 * displaced along Y still comes out convex and consistently wound. That is why this pins the shape
 * itself rather than only its orientation.
 *
 * If one of these fails after a deliberate change to a cut's proportions, re-capture the row -- but
 * never re-capture to make a refactor pass.
 */
TEST(VertexFactoryShapeGenerator, gemCutsKeepTheirGeometry)
{
	struct GemFingerprint
	{
		const char * label;
		size_t triangles;
		float sumAbsX;
		float sumAbsY;
		float sumAbsZ;
		float squaredRadius;
		float minY;
		float maxY;
		float sumU;
		float sumV;
		float sumAbsNormal;
		float sumUSquared;
		float sumVSquared;
	};

	const auto measure = [] (const Shape< float, uint32_t > & shape, const GemFingerprint & expected) {
		double sumX = 0.0;
		double sumY = 0.0;
		double sumZ = 0.0;
		double squared = 0.0;
		auto minY = 1.0E9F;
		auto maxY = -1.0E9F;
		double sumU = 0.0;
		double sumV = 0.0;
		double sumNormal = 0.0;
		double sumU2 = 0.0;
		double sumV2 = 0.0;

		/* ⚠️⚠️ Iterate TRIANGLE CORNERS, never the vertex array. That array is the result of
		 * DEDUPLICATION, which depends on the emission order — and re-authoring changes that order
		 * legitimately. The trillion cut lost 6 shared vertices with its geometry perfectly intact,
		 * and every sum taken over the vertex array moved with it, reporting a shape change that had
		 * not happened. Corners describe what is actually drawn. */
		for ( const auto & triangle : shape.triangles() )
		{
			for ( uint32_t corner = 0; corner < 3; ++corner )
			{
			const auto & vertex = shape.vertices()[triangle.vertexIndex(corner)];
			const auto & position = vertex.position();

			sumX += std::abs(position[EmEn::Base::Math::X]);
			sumY += std::abs(position[EmEn::Base::Math::Y]);
			sumZ += std::abs(position[EmEn::Base::Math::Z]);
			squared += static_cast< double >(position[0]) * position[0]
			         + static_cast< double >(position[1]) * position[1]
			         + static_cast< double >(position[2]) * position[2];

			minY = std::min(minY, position[EmEn::Base::Math::Y]);
			maxY = std::max(maxY, position[EmEn::Base::Math::Y]);

			const auto u = vertex.textureCoordinates()[EmEn::Base::Math::X];
			const auto v = vertex.textureCoordinates()[EmEn::Base::Math::Y];

			sumU += u;
			sumV += v;
			sumU2 += static_cast< double >(u) * u;
			sumV2 += static_cast< double >(v) * v;
			sumNormal += std::abs(vertex.normal()[0]) + std::abs(vertex.normal()[1]) + std::abs(vertex.normal()[2]);
			}
		}

		/* ⚠️ The vertex count is NOT pinned: it counts SHARED vertices, so it moves with the emission
		 * order without the shape changing at all. The triangle count is the real one. */
		EXPECT_EQ(shape.triangles().size(), expected.triangles) << expected.label << ": triangle count";
		EXPECT_NEAR(static_cast< float >(sumX), expected.sumAbsX, 0.01F) << expected.label << ": sum |x|";
		EXPECT_NEAR(static_cast< float >(sumY), expected.sumAbsY, 0.01F) << expected.label << ": sum |y|";
		EXPECT_NEAR(static_cast< float >(sumZ), expected.sumAbsZ, 0.01F) << expected.label << ": sum |z|";
		EXPECT_NEAR(static_cast< float >(squared), expected.squaredRadius, 0.05F) << expected.label << ": squared radius";
		EXPECT_NEAR(minY, expected.minY, 1.0E-3F) << expected.label << ": lowest point";
		EXPECT_NEAR(maxY, expected.maxY, 1.0E-3F) << expected.label << ": highest point";

		/* ⚠️ UVs are pinned too: flipYAxis() never touched them, so a re-authoring that negates the
		 * geometry's Y must ALSO leave them alone — including the up vector of the per-face tangent
		 * frame, which has to be negated in step or the whole texture layout shifts. */
		EXPECT_NEAR(static_cast< float >(sumU), expected.sumU, 0.02F) << expected.label << ": sum U";
		EXPECT_NEAR(static_cast< float >(sumV), expected.sumV, 0.02F) << expected.label << ": sum V";
		EXPECT_NEAR(static_cast< float >(sumNormal), expected.sumAbsNormal, 0.05F) << expected.label << ": normal magnitude";

		/* ⚠️⚠️ The SQUARED sums are the load-bearing half, and the plain sums alone are a trap.
		 * Removing the mirror flips the bitangent of the per-face tangent frame
		 * (cross(M.n, M.t) = -M.cross(n, t)), which sends v to 1 - v. But sum(1 - v) = n - sum(v),
		 * which EQUALS sum(v) whenever sum(v) = n/2 -- true of every symmetric facet. Three cuts
		 * were converted with their V silently flipped and the plain sums saw nothing; only the
		 * princess, whose chevrons are asymmetric, gave it away. Squares do not cancel that way. */
		EXPECT_NEAR(static_cast< float >(sumU2), expected.sumUSquared, 0.02F) << expected.label << ": sum U squared";
		EXPECT_NEAR(static_cast< float >(sumV2), expected.sumVSquared, 0.02F) << expected.label << ": sum V squared";
	};

	measure(ShapeGenerator::generateDiamondCutGem< float, uint32_t >(), {"diamond", 30, 38.8425F, 20.1882F, 36.6425F, 62.9202F, -0.8693F, 0.3151F, 42.6508F, 42.0401F, 133.1029F, 35.6921F, 31.5052F});
	measure(ShapeGenerator::generateEmeraldCutGem< float, uint32_t >(), {"emerald", 108, 142.1375F, 90.0000F, 90.7375F, 148.0905F, -0.7500F, 0.2500F, 111.9034F, 136.7632F, 480.4268F, 94.2063F, 112.6996F});
	measure(ShapeGenerator::generateAsscherCutGem< float, uint32_t >(), {"asscher", 108, 97.7062F, 90.0000F, 97.9312F, 113.1222F, -0.7500F, 0.2500F, 118.4708F, 124.0118F, 483.3290F, 105.5247F, 101.9094F});
	measure(ShapeGenerator::generateBaguetteCutGem< float, uint32_t >(), {"baguette", 36, 52.8750F, 18.0000F, 17.6250F, 40.0125F, -0.4500F, 0.1500F, 24.4843F, 50.5692F, 143.0502F, 18.0792F, 43.1324F});
	measure(ShapeGenerator::generatePrincessCutGem< float, uint32_t >(), {"princess", 108, 74.3800F, 90.0000F, 75.6800F, 100.1408F, -0.7500F, 0.2500F, 95.8312F, 146.2238F, 440.9015F, 68.8232F, 127.3519F});
	measure(ShapeGenerator::generateTrillionCutGem< float, uint32_t >(), {"trillion", 80, 76.6292F, 52.8000F, 83.5006F, 97.9768F, -0.6000F, 0.2000F, 72.0935F, 104.5209F, 358.7052F, 57.1323F, 85.2410F});
	measure(ShapeGenerator::generateOvalCutGem< float, uint32_t >(), {"oval", 62, 83.5075F, 34.8703F, 50.3768F, 91.4993F, -0.7172F, 0.2600F, 106.0761F, 73.6062F, 268.0640F, 102.1493F, 49.6606F});
	measure(ShapeGenerator::generateCushionCutGem< float, uint32_t >(), {"cushion", 62, 88.0344F, 39.0970F, 69.6632F, 118.3826F, -0.8041F, 0.2915F, 106.0761F, 67.1626F, 264.3181F, 102.1493F, 42.1261F});
	measure(ShapeGenerator::generateMarquiseCutGem< float, uint32_t >(), {"marquise", 62, 96.5835F, 37.0853F, 33.1720F, 102.5398F, -0.7653F, 0.2760F, 97.7936F, 84.9843F, 271.1887F, 87.5318F, 61.4567F});
	measure(ShapeGenerator::generatePearCutGem< float, uint32_t >(), {"pear", 62, 80.4862F, 35.8075F, 41.9581F, 83.7875F, -0.7203F, 0.2698F, 106.0709F, 75.6656F, 270.7177F, 102.1389F, 50.9361F});
	measure(ShapeGenerator::generateHeartCutGem< float, uint32_t >(), {"heart", 94, 103.5804F, 24.7672F, 76.3816F, 111.8631F, -0.4502F, 0.1012F, 137.8396F, 129.9527F, 376.2547F, 119.3239F, 96.1479F});
	measure(ShapeGenerator::generateRoseCutGem< float, uint32_t >(), {"rose", 70, 85.8850F, 28.0000F, 78.6171F, 113.5733F, -0.6000F, -0.0000F, 101.0012F, 95.2091F, 287.7543F, 89.7843F, 72.1867F});
}

/*
 * ⚠️⚠️ The gem cuts paint a VOLUMETRIC vertex colour -- position mapped to RGB -- so the green
 * channel encodes Y and must agree with the geometry it is attached to.
 *
 * It did not, back when those generators authored their facets Y-down and mirrored the finished
 * shape with convertYDownAuthoring() -> Shape::flipYAxis(). That mirrors positions, normals and
 * tangents -- but the vertex colours live in a SEPARATE vector (Shape::m_vertexColors) which it
 * never touches, so the colours kept describing the pre-mirror frame, green inverted against the
 * final geometry.
 *
 * ⚠️ The helper is gone and the generators are authored Y-up, so the defect cannot recur that way.
 * The test stays because the RULE outlives it: anything derived from a position before a transform
 * is stale after it, and flipYAxis() will not tell you.
 *
 * ⚠️ Visible, not theoretical: `parametric-geometries` has a vertex-colour row that iterates every
 * shape, gems included, so they were shaded upside down next to shapes that were not.
 *
 * A non-mirrored generator is included as a CONTROL: a probe that fails it is a broken probe.
 */
TEST(VertexFactoryShapeGenerator, gemVertexColoursAgreeWithGeometry)
{
	const auto expectGreenGrowsWithY = [] (const Shape< float, uint32_t > & shape, const char * label) {
		auto minY = 1.0E9F;
		auto maxY = -1.0E9F;

		for ( const auto & vertex : shape.vertices() )
		{
			minY = std::min(minY, vertex.position()[EmEn::Base::Math::Y]);
			maxY = std::max(maxY, vertex.position()[EmEn::Base::Math::Y]);
		}

		const auto middle = (minY + maxY) * 0.5F;

		double greenAbove = 0.0;
		double greenBelow = 0.0;
		auto countAbove = 0;
		auto countBelow = 0;

		for ( const auto & triangle : shape.triangles() )
		{
			for ( uint32_t corner = 0; corner < 3; ++corner )
			{
				const auto y = shape.vertices()[triangle.vertexIndex(corner)].position()[EmEn::Base::Math::Y];
				const auto green = shape.vertexColors()[triangle.vertexColorIndex(corner)][EmEn::Base::Math::Y];

				if ( y > middle ) { greenAbove += green; ++countAbove; } else { greenBelow += green; ++countBelow; }
			}
		}

		ASSERT_GT(countAbove, 0) << label << ": nothing above mid-height, the check would be vacuous";
		ASSERT_GT(countBelow, 0) << label << ": nothing below mid-height, the check would be vacuous";

		EXPECT_GT(greenAbove / countAbove, greenBelow / countBelow)
			<< label << ": the volumetric green is BRIGHTER at the bottom than at the top, so the "
			<< "colour describes a mirrored frame. flipYAxis() does not touch m_vertexColors.";
	};

	/* Control: never mirrored, so it must pass whatever happens to the gems. */
	expectGreenGrowsWithY(ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 16, 8, uvOptions()), "sphere [control]");

	expectGreenGrowsWithY(ShapeGenerator::generateDiamondCutGem< float, uint32_t >(), "diamond cut");
	expectGreenGrowsWithY(ShapeGenerator::generateEmeraldCutGem< float, uint32_t >(), "emerald cut");
	expectGreenGrowsWithY(ShapeGenerator::generateAsscherCutGem< float, uint32_t >(), "asscher cut");
	expectGreenGrowsWithY(ShapeGenerator::generateBaguetteCutGem< float, uint32_t >(), "baguette cut");
	expectGreenGrowsWithY(ShapeGenerator::generatePrincessCutGem< float, uint32_t >(), "princess cut");
	expectGreenGrowsWithY(ShapeGenerator::generateTrillionCutGem< float, uint32_t >(), "trillion cut");
	expectGreenGrowsWithY(ShapeGenerator::generateOvalCutGem< float, uint32_t >(), "oval cut");
	expectGreenGrowsWithY(ShapeGenerator::generateCushionCutGem< float, uint32_t >(), "cushion cut");
	expectGreenGrowsWithY(ShapeGenerator::generateMarquiseCutGem< float, uint32_t >(), "marquise cut");
	expectGreenGrowsWithY(ShapeGenerator::generatePearCutGem< float, uint32_t >(), "pear cut");
	expectGreenGrowsWithY(ShapeGenerator::generateHeartCutGem< float, uint32_t >(), "heart cut");
	expectGreenGrowsWithY(ShapeGenerator::generateRoseCutGem< float, uint32_t >(), "rose cut");
}