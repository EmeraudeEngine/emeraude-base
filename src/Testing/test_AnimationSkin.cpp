/*
 * src/Testing/test_AnimationSkin.cpp
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

/* Local inclusions. */
#include "Animation/Skin.hpp"
#include "Math/Matrix.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::Animation;

/* Skin::rootTransform() carries the glTF correction of a skinned mesh whose skeleton hangs under transformed
 * non-joint ancestors (BrainStem, 2026-10-01): the inverse of the mesh node's world transform times the world transform
 * of the common ancestor of the mesh and the skeleton. These tests pin its storage; the engine's SkeletalAnimator
 * applies it above every root joint. */

TEST(AnimationSkin, rootTransformDefaultsToIdentity)
{
	const Skin< float > skin{};

	EXPECT_TRUE(skin.rootTransform().isIdentity());

	const Skin< float > populated{{0, 1}, {Math::Matrix< 4, float >{}, Math::Matrix< 4, float >{}}};

	EXPECT_TRUE(populated.rootTransform().isIdentity());
}

TEST(AnimationSkin, rootTransformIsStoredAndCopied)
{
	Skin< float > skin{{0}, {Math::Matrix< 4, float >{}}};

	const auto rotation = Math::Matrix< 4, float >::rotation(Math::Radian(90.0F), 1.0F, 0.0F, 0.0F);

	skin.setRootTransform(rotation);

	EXPECT_EQ(skin.rootTransform(), rotation);

	/* NOTE: The loader hands each skinned mesh its own copy, with its own root transform. */
	auto copy = skin;
	copy.setRootTransform(Math::Matrix< 4, float >{});

	EXPECT_EQ(skin.rootTransform(), rotation);
	EXPECT_TRUE(copy.rootTransform().isIdentity());

	/* The joint data is untouched by the root transform. */
	EXPECT_EQ(skin.jointCount(), 1U);
	EXPECT_EQ(skin.skeletonJointIndex(0), 0);
}
