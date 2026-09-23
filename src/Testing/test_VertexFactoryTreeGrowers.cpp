/*
 * src/Testing/test_VertexFactoryTreeGrowers.cpp
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
#include <algorithm>
#include <vector>

/* Local inclusions. */
#include "Math/Vector.hpp"
#include "VertexFactory/TreeColonizationGrower.hpp"
#include "VertexFactory/TreeParametricGrower.hpp"
#include "VertexFactory/TreeSkeleton.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::VertexFactory;

namespace
{
	/**
	 * @brief Returns whether two skeletons hold exactly the same geometry.
	 * @param first A reference to the first skeleton.
	 * @param second A reference to the second skeleton.
	 * @return bool
	 */
	bool
	sameSkeleton (const TreeSkeleton< float > & first, const TreeSkeleton< float > & second)
	{
		if ( first.segmentCount() != second.segmentCount() || first.leafCount() != second.leafCount() )
		{
			return false;
		}

		for ( size_t index = 0; index < first.segmentCount(); ++index )
		{
			const auto & left = first.segments()[index];
			const auto & right = second.segments()[index];

			if ( !(left.startPoint() == right.startPoint()) || left.startRadius() != right.startRadius() || left.parentIndex() != right.parentIndex() )
			{
				return false;
			}
		}

		return true;
	}
}

/* makeTreeFrame() is what every grower uses to turn a direction into a segment frame. If its
 * local +Y is not the growth axis, every branch points somewhere else than where it grew. */
TEST(VertexFactoryTreeSkeleton, makeTreeFrameAlignsLocalYWithTheGrowthAxis)
{
	const std::vector< Math::Vector< 3, float > > directions{
		{0.0F, 1.0F, 0.0F},
		{0.0F, -1.0F, 0.0F},
		{1.0F, 0.0F, 0.0F},
		{0.0F, 0.0F, 1.0F},
		{0.3F, 0.9F, -0.2F},
		{-0.7F, 0.1F, 0.6F}
	};

	for ( const auto & direction : directions )
	{
		const auto frame = makeTreeFrame(Math::Vector< 3, float >{1.0F, 2.0F, 3.0F}, direction);

		const auto expected = direction.normalized();

		EXPECT_NEAR((Math::Vector< 3, float >::dotProduct(frame.localYAxis(), expected)), 1.0F, 1e-4F)
			<< "the frame local +Y does not follow the growth direction";

		/* The basis must stay orthonormal, or the skinning rings come out sheared. */
		EXPECT_NEAR((Math::Vector< 3, float >::dotProduct(frame.localYAxis(), frame.backwardVector())), 0.0F, 1e-4F);
		EXPECT_NEAR((Math::Vector< 3, float >::dotProduct(frame.localYAxis(), frame.rightVector())), 0.0F, 1e-4F);
		EXPECT_NEAR(frame.localYAxis().length(), 1.0F, 1e-4F);
	}
}

/* ⚠️ The skeleton contract every reverse pass relies on: a parent is always added before its
 * child. Break it and the pipe model reads radii that have not been computed yet. */
TEST(VertexFactoryTreeSkeleton, growersAddAParentBeforeItsChild)
{
	const TreeParametricGrower< float > parametric{TreeParameters< float >::broadleaf()};

	const auto parametricSkeleton = parametric.grow(3);

	ASSERT_FALSE(parametricSkeleton.empty());

	for ( size_t index = 0; index < parametricSkeleton.segmentCount(); ++index )
	{
		const auto & segment = parametricSkeleton.segments()[index];

		if ( !segment.isRoot() )
		{
			EXPECT_LT(segment.parentIndex(), index) << "parametric segment " << index << " was added before its parent";
		}
	}

	const TreeColonizationGrower< float > colonization;

	const auto colonizationSkeleton = colonization.grow(3);

	ASSERT_FALSE(colonizationSkeleton.empty());

	for ( size_t index = 0; index < colonizationSkeleton.segmentCount(); ++index )
	{
		const auto & segment = colonizationSkeleton.segments()[index];

		if ( !segment.isRoot() )
		{
			EXPECT_LT(segment.parentIndex(), index) << "colonization segment " << index << " was added before its parent";
		}
	}
}

/* nBaseSplits forks the ORIGINAL trunk once, at its base (Weber & Penn). Every fork clone is a level-0 stem too, and
 * until 2026-09-23 each one forked again at its own base: 364 "trunks" on the broadleaf (2 base splits, 6 trunk
 * segments: 1 + 3 + 9 + 27 + 81 + 243), the 243 last ones bare and poking 2 m out of the crown, popping in as the
 * camera came close (the finest level of detail is the only one to keep such thin stems). */
TEST(VertexFactoryTreeParametricGrower, theTrunkForksOnlyAtItsBaseAndNoStemIsBare)
{
	const auto parameters = TreeParameters< float >::broadleaf();
	const TreeParametricGrower< float > grower{parameters};

	const auto skeleton = grower.grow(1);

	ASSERT_FALSE(skeleton.empty());

	std::vector< uint32_t > carried(skeleton.branchCount(), 0);
	std::vector< uint32_t > orderOf(skeleton.branchCount(), 0);

	for ( const auto & segment : skeleton.segments() )
	{
		orderOf[segment.branchIndex()] = segment.order();

		if ( !segment.isRoot() && segment.parentIndex() < skeleton.segmentCount() && skeleton.segments()[segment.parentIndex()].branchIndex() != segment.branchIndex() )
		{
			carried[skeleton.segments()[segment.parentIndex()].branchIndex()]++;
		}
	}

	for ( const auto & leaf : skeleton.leaves() )
	{
		carried[skeleton.segments()[leaf.segmentIndex()].branchIndex()]++;
	}

	const auto trunks = static_cast< uint32_t >(std::ranges::count(orderOf, 0U));

	/* The trunk below the fork, plus baseSplits + 1 clones. */
	EXPECT_LE(trunks, parameters.level(0).baseSplits() + 2U) << "the trunk forked again at the base of its own clones";

	for ( size_t branch = 0; branch < carried.size(); ++branch )
	{
		EXPECT_GT(carried[branch], 0U) << "branch " << branch << " (order " << orderOf[branch] << ") carries neither a child nor a leaf: a bare whip";
	}
}

/* A grower that is not reproducible cannot be used to bench anything: two captures of "the same"
 * tree would differ. The generator is local to the call precisely for this. */
TEST(VertexFactoryTreeParametricGrower, sameSeedGivesTheSameTree)
{
	const TreeParametricGrower< float > grower{TreeParameters< float >::quakingAspen()};

	EXPECT_TRUE(sameSkeleton(grower.grow(11), grower.grow(11)));
	EXPECT_FALSE(sameSkeleton(grower.grow(11), grower.grow(12))) << "two different seeds gave the very same tree";
}

TEST(VertexFactoryTreeColonizationGrower, sameSeedGivesTheSameTree)
{
	const TreeColonizationGrower< float > grower;

	EXPECT_TRUE(sameSkeleton(grower.grow(11), grower.grow(11)));
}

/* The three shipped parameter sets must produce a tree, not a stick and not an explosion. The
 * ceiling matters: a fork clone used to restart a full-length stem, which forked again at the
 * same relative place and never ended. */
TEST(VertexFactoryTreeParametricGrower, theShippedSpeciesGrowWithinTheirCeiling)
{
	struct Species final
	{
		const char * name;
		TreeParameters< float > parameters;
	};

	const std::vector< Species > species{
		{"quakingAspen", TreeParameters< float >::quakingAspen()},
		{"broadleaf", TreeParameters< float >::broadleaf()},
		{"conifer", TreeParameters< float >::conifer()}
	};

	for ( const auto & entry : species )
	{
		const TreeParametricGrower< float > grower{entry.parameters};

		const auto skeleton = grower.grow(5);

		EXPECT_GT(skeleton.segmentCount(), 100U) << entry.name << " grew almost nothing";
		EXPECT_LT(skeleton.segmentCount(), TreeParametricGrower< float >::MaxSegments) << entry.name << " hit the segment ceiling, the growth ran away";
		EXPECT_GT(skeleton.leafCount(), 0U) << entry.name << " carries no leaf";
		EXPECT_GT(skeleton.height(), 1.0F) << entry.name << " is flat";
		EXPECT_GT(skeleton.maxOrder(), 0U) << entry.name << " never branched";
	}
}

/* The height parameter must actually drive the height, or no parameter can be trusted. */
TEST(VertexFactoryTreeParametricGrower, theScaleParameterDrivesTheHeight)
{
	auto parameters = TreeParameters< float >::conifer();
	parameters.setScaleVariation(0.0F);

	parameters.setScale(10.0F);
	const auto small = TreeParametricGrower< float >{parameters}.grow(1);

	parameters.setScale(20.0F);
	const auto tall = TreeParametricGrower< float >{parameters}.grow(1);

	EXPECT_GT(tall.height(), small.height() * 1.5F) << "doubling the scale did not roughly double the height";
}

/* The pipe model is the only thing giving the colonization tree its radii: a trunk must come out
 * thicker than a twig, and a segment must never be thinner than the child it carries. */
TEST(VertexFactoryTreeSkeleton, thePipeModelThickensTowardTheRoot)
{
	const TreeColonizationGrower< float > grower;

	const auto skeleton = grower.grow(4);

	ASSERT_GT(skeleton.segmentCount(), 50U);

	const auto tipRadius = grower.tipRadius();

	float thickest = 0.0F;

	for ( const auto & segment : skeleton.segments() )
	{
		thickest = std::max(thickest, segment.startRadius());

		EXPECT_GE(segment.startRadius(), tipRadius - 1e-6F) << "a segment came out thinner than a branch tip";
	}

	EXPECT_GT(thickest, tipRadius * 3.0F) << "the trunk is barely thicker than a twig, the pipe model did not accumulate";

	/* Continuity: a segment ends on the radius its thickest child starts with. */
	const auto childTable = skeleton.buildChildTable();

	for ( size_t index = 0; index < skeleton.segmentCount(); ++index )
	{
		const auto children = childTable.children(static_cast< uint32_t >(index));

		for ( const auto childIndex : children )
		{
			EXPECT_LE(skeleton.segments()[childIndex].startRadius(), skeleton.segments()[index].startRadius() + 1e-5F)
				<< "child " << childIndex << " is thicker than its parent " << index;
		}
	}
}

/* The colonization grower is steered by the SHAPE of its crown, so a tall narrow crown must give
 * a tall narrow tree and a flat wide one a flat wide tree. */
TEST(VertexFactoryTreeColonizationGrower, theCrownShapeDrivesTheTreeShape)
{
	TreeColonizationGrower< float > columnar;
	columnar.setAttractorCount(1200);
	columnar.setCrownCenter({0.0F, 9.0F, 0.0F});
	columnar.setCrownRadii({1.5F, 6.0F, 1.5F});
	columnar.setTrunkHeight(2.0F);

	TreeColonizationGrower< float > spreading;
	spreading.setAttractorCount(1200);
	spreading.setCrownCenter({0.0F, 5.0F, 0.0F});
	spreading.setCrownRadii({6.0F, 1.5F, 6.0F});
	spreading.setTrunkHeight(2.0F);

	const auto columnarSkeleton = columnar.grow(2);
	const auto spreadingSkeleton = spreading.grow(2);

	ASSERT_FALSE(columnarSkeleton.empty());
	ASSERT_FALSE(spreadingSkeleton.empty());

	const auto width = [] (const TreeSkeleton< float > & skeleton) {
		const auto & box = skeleton.boundingBox();

		return box.maximum()[Math::X] - box.minimum()[Math::X];
	};

	EXPECT_GT(columnarSkeleton.height(), spreadingSkeleton.height()) << "the columnar crown did not grow the taller tree";
	EXPECT_GT(width(spreadingSkeleton), width(columnarSkeleton)) << "the spreading crown did not grow the wider tree";
}

/* Degenerate parameters must return an empty skeleton, never crash and never loop. */
TEST(VertexFactoryTreeParametricGrower, degenerateParametersGrowNothing)
{
	TreeParameters< float > parameters;
	parameters.setScale(0.0F);
	parameters.setScaleVariation(0.0F);

	EXPECT_TRUE(TreeParametricGrower< float >{parameters}.grow(1).empty());

	TreeColonizationGrower< float > grower;
	grower.setAttractorCount(0);

	EXPECT_TRUE(grower.grow(1).empty());
}
