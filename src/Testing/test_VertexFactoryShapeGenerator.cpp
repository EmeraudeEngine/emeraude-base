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