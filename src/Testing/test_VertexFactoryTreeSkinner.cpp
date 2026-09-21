/*
 * src/Testing/test_VertexFactoryTreeSkinner.cpp
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
#include <numbers>
#include <vector>

/* Local inclusions. */
#include "Math/Vector.hpp"
#include "VertexFactory/ShapeGenerator.hpp"
#include "VertexFactory/TreeColonizationGrower.hpp"
#include "VertexFactory/TreeGenerator.hpp"
#include "VertexFactory/TreeParametricGrower.hpp"
#include "VertexFactory/TreeSkinner.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::VertexFactory;

namespace
{
	/** @brief Returns the fraction of triangles whose winding disagrees with their own normals. */
	double
	windingDisagreement (const Shape< float > & shape)
	{
		size_t wrong = 0;
		size_t total = 0;

		for ( const auto & triangle : shape.triangles() )
		{
			const auto & first = shape.vertices()[triangle.vertexIndex(0)];
			const auto & second = shape.vertices()[triangle.vertexIndex(1)];
			const auto & third = shape.vertices()[triangle.vertexIndex(2)];

			const auto geometric = Math::Vector< 3, float >::crossProduct(second.position() - first.position(), third.position() - first.position());

			if ( geometric.lengthSquared() < 1e-12F )
			{
				continue;
			}

			++total;

			if ( Math::Vector< 3, float >::dotProduct(geometric.normalized(), first.normal() + second.normal() + third.normal()) < 0.0F )
			{
				++wrong;
			}
		}

		return total == 0 ? 1.0 : static_cast< double >(wrong) / static_cast< double >(total);
	}

	/** @brief Returns the length of the longest edge of the mesh. */
	float
	longestEdge (const Shape< float > & shape)
	{
		float longest = 0.0F;

		for ( const auto & triangle : shape.triangles() )
		{
			const auto & first = shape.vertices()[triangle.vertexIndex(0)].position();
			const auto & second = shape.vertices()[triangle.vertexIndex(1)].position();
			const auto & third = shape.vertices()[triangle.vertexIndex(2)].position();

			longest = std::max({longest, (second - first).length(), (third - second).length(), (first - third).length()});
		}

		return longest;
	}

	/** @brief Returns the summed area of every triangle. */
	double
	surfaceArea (const Shape< float > & shape)
	{
		double area = 0.0;

		for ( const auto & triangle : shape.triangles() )
		{
			const auto & first = shape.vertices()[triangle.vertexIndex(0)];
			const auto & second = shape.vertices()[triangle.vertexIndex(1)];
			const auto & third = shape.vertices()[triangle.vertexIndex(2)];

			area += 0.5 * static_cast< double >(Math::Vector< 3, float >::crossProduct(second.position() - first.position(), third.position() - first.position()).length());
		}

		return area;
	}
}

/* The engine expects counter-clockwise front faces. Rather than assume it, the house convention
 * is read off the shipped primitives and the tree is required to agree with them. */
TEST(VertexFactoryTreeSkinner, theWindingAgreesWithTheShippedPrimitives)
{
	ASSERT_NEAR(windingDisagreement(ShapeGenerator::generateCylinder< float >(1.0F, 0.5F, 2.0F, 16U, 4U)), 0.0, 1e-9)
		<< "the reference primitive itself disagrees, the check means nothing";

	const TreeParametricGrower< float > grower{TreeParameters< float >::quakingAspen()};

	const TreeSkinner< float > skinner;

	EXPECT_NEAR(windingDisagreement(skinner.skin(grower.grow(1))), 0.0, 1e-9);
}

/* ⚠️ The regression that matters most on the level chain: the first coarser level used to lose
 * 100 % of the foliage, because a leaf whose twig had been pruned was dropped with it. At the
 * distance where a two-pixel twig goes, the canopy IS the tree. */
TEST(VertexFactoryTreeSkinner, theCanopySurvivesEveryLevelOfDetail)
{
	TreeGenerator generator;
	generator.parameters() = TreeParameters< float >::quakingAspen();
	generator.setLevelOfDetailCount(4);

	const auto mesh = generator.generate(1);

	ASSERT_EQ(mesh.levelCount(), 4U);

	for ( uint32_t level = 0; level < mesh.levelCount(); ++level )
	{
		const auto & shape = mesh.shape(level);

		ASSERT_EQ(shape.groupCount(), 2U) << "level " << level << " does not carry the bark and leaf groups";

		EXPECT_GT(shape.groups()[TreeMesh< float >::BarkGroup].second, 0U) << "level " << level << " has no bark";
		EXPECT_GT(shape.groups()[TreeMesh< float >::LeafGroup].second, 0U) << "level " << level << " lost its whole canopy";
	}
}

TEST(VertexFactoryTreeSkinner, everyLevelOfDetailIsLighterThanThePreviousOne)
{
	TreeGenerator generator;
	generator.parameters() = TreeParameters< float >::conifer();
	generator.setLevelOfDetailCount(3);

	const auto mesh = generator.generate(2);

	ASSERT_EQ(mesh.levelCount(), 3U);

	for ( uint32_t level = 1; level < mesh.levelCount(); ++level )
	{
		EXPECT_LT(mesh.triangleCount(level), mesh.triangleCount(level - 1))
			<< "level " << level << " is not lighter than level " << (level - 1);
	}
}

/* ⚠️ The skinner must build its OWN rotation minimizing frame along a branch. The segment frames
 * cannot be used: makeTreeFrame() picks its spin from the world axis least aligned with the
 * direction, so it flips when a branch crosses that threshold, and a tube following those frames
 * corkscrews. A corkscrew is not a hole and not an inversion — it passes every topological check
 * — so it is caught on the LONGEST EDGE instead: when two consecutive rings fall out of
 * phase, the edges joining them stop running along the tube and start cutting across it.
 * ⚠️ Measured on this very geometry: the rotation minimizing frame gives 1.04 to 1.10 segment
 * lengths, a per-segment frame gives 1.42 to 1.46. The AREA does NOT separate them (1.002
 * against 1.019) — the first version of this test measured the area, passed on both, and so
 * tested nothing at all. */
TEST(VertexFactoryTreeSkinner, theTubeDoesNotCorkscrewOnIndependentlyFramedSegments)
{
	constexpr float Radius = 0.25F;
	constexpr float SegmentLength = 0.5F;
	constexpr uint32_t SegmentCount = 24;

	TreeSkeleton< float > skeleton;

	Math::Vector< 3, float > position{0.0F, 0.0F, 0.0F};

	uint32_t parent = TreeSegment< float >::NoParent;
	float arc = 0.0F;

	/* A branch sweeping from straight up to horizontal, so it crosses the |y| > 0.9 threshold
	 * where makeTreeFrame() changes the world axis it derives its spin from. */
	for ( uint32_t index = 0; index < SegmentCount; ++index )
	{
		const auto angle = std::numbers::pi_v< float > * 0.5F * static_cast< float >(index) / static_cast< float >(SegmentCount);

		const Math::Vector< 3, float > direction{std::sin(angle), std::cos(angle), 0.0F};

		TreeSegment< float > segment{makeTreeFrame(position, direction), SegmentLength, Radius, Radius, arc, parent, 0U, 0U};
		segment.setBranchTip(index + 1 == SegmentCount);

		parent = skeleton.addSegment(segment);

		position += direction * SegmentLength;
		arc += SegmentLength;
	}

	TreeSkinningOptions< float > options;
	options.setLeafCardMode(TreeLeafCardMode::None);
	options.setCollarScale(1.0F);

	const TreeSkinner< float > skinner{options};

	const auto shape = skinner.skin(skeleton);

	ASSERT_FALSE(shape.empty());

	/* The tube of a bent cylinder, plus the apex cone that closes it. */
	const auto expected = 2.0 * std::numbers::pi * static_cast< double >(Radius) * static_cast< double >(SegmentLength * SegmentCount);

	/* The area still says whether the tube is CLOSED, which is worth keeping. */
	const auto measured = surfaceArea(shape);

	EXPECT_GT(measured, expected * 0.85) << "the tube lost surface, it is not closed";
	EXPECT_LT(measured, expected * 1.15) << "the tube has surface it should not have";

	/* And the longest edge says whether it is IN PHASE. */
	EXPECT_LT(longestEdge(shape), SegmentLength * 1.25F)
		<< "an edge cuts across the tube instead of running along it: the rings are out of phase";
}

/* The channels are the whole point of filling them at skinning time. They must be usable as they
 * are: inside [0, 1], and the trunk weight must actually grow with the height. */
TEST(VertexFactoryTreeSkinner, theWindChannelsAreUsableAsWritten)
{
	const TreeParametricGrower< float > grower{TreeParameters< float >::broadleaf()};

	const auto skeleton = grower.grow(1);

	const TreeSkinner< float > skinner;

	const auto shape = skinner.skin(skeleton);

	ASSERT_FALSE(shape.vertexColors().empty());

	const auto & box = shape.boundingBox();

	const auto middle = (box.maximum()[Math::Y] + box.minimum()[Math::Y]) * 0.5F;

	double topSum = 0.0;
	double bottomSum = 0.0;
	size_t topCount = 0;
	size_t bottomCount = 0;

	for ( const auto & triangle : shape.triangles() )
	{
		for ( uint32_t corner = 0; corner < 3; ++corner )
		{
			const auto & color = shape.vertexColors()[triangle.vertexColorIndex(corner)];

			for ( size_t channel = 0; channel < 4; ++channel )
			{
				ASSERT_GE(color[channel], 0.0F) << "channel " << channel << " went below 0";
				ASSERT_LE(color[channel], 1.0F) << "channel " << channel << " went above 1";
			}

			if ( shape.vertices()[triangle.vertexIndex(corner)].position()[Math::Y] > middle )
			{
				topSum += static_cast< double >(color[Math::X]);
				++topCount;
			}
			else
			{
				bottomSum += static_cast< double >(color[Math::X]);
				++bottomCount;
			}
		}
	}

	ASSERT_GT(topCount, 0U);
	ASSERT_GT(bottomCount, 0U);

	EXPECT_GT(topSum / static_cast< double >(topCount), bottomSum / static_cast< double >(bottomCount))
		<< "the trunk bending weight does not grow with the height, the tree would sway from its foot";
}

TEST(VertexFactoryTreeSkinner, theImposterIsACrossOfQuadsSpanningTheTree)
{
	TreeGenerator generator;
	generator.parameters() = TreeParameters< float >::quakingAspen();
	generator.setLevelOfDetailCount(1);
	generator.enableImposter(true);
	generator.setImposterQuadCount(3);

	const auto mesh = generator.generate(1);

	ASSERT_TRUE(mesh.hasImposter());

	EXPECT_EQ(mesh.imposter().triangles().size(), 6U) << "three crossed quads are six triangles";

	const auto & card = mesh.imposter().boundingBox();
	const auto & tree = mesh.skeleton().boundingBox();

	ASSERT_TRUE(card.isValid());
	ASSERT_TRUE(tree.isValid());

	EXPECT_NEAR(card.maximum()[Math::Y] - card.minimum()[Math::Y], tree.maximum()[Math::Y] - tree.minimum()[Math::Y], 1e-3F)
		<< "the card is not as tall as the tree it replaces";
}

TEST(VertexFactoryTreeSkinner, anEmptySkeletonSkinsToNothing)
{
	const TreeSkeleton< float > empty;

	const TreeSkinner< float > skinner;

	EXPECT_TRUE(skinner.skin(empty).empty());
	EXPECT_TRUE(skinner.skinImposter(empty).empty());

	TreeGenerator generator;
	generator.parameters().setScale(0.0F);
	generator.parameters().setScaleVariation(0.0F);

	EXPECT_TRUE(generator.generate(1).empty());
}
