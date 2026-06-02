/*
 * src/Testing/test_AnimationCubicSpline.cpp
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
#include "Animation/AnimationChannel.hpp"
#include "Math/Base.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base;

/* Ave robustus! (Axis B): the ChannelInterpolation::CubicSpline mode was declared but undeliverable —
 * keyframes had no in/out tangent storage and there was no GLTF cubic evaluator. These tests prove
 * both: the evaluator (Math::cubicSplineInterpolation) and the keyframe tangent storage. */

TEST(AnimationCubicSpline, scalarHitsEndpoints)
{
	/* h00(0)=1 / h01(1)=1 regardless of the tangents -> the endpoints are exact. */
	EXPECT_FLOAT_EQ(Math::cubicSplineInterpolation(10.0F, 5.0F, 20.0F, 3.0F, 1.0F, 0.0F), 10.0F);
	EXPECT_FLOAT_EQ(Math::cubicSplineInterpolation(10.0F, 5.0F, 20.0F, 3.0F, 1.0F, 1.0F), 20.0F);
}

TEST(AnimationCubicSpline, scalarMidpointWithFlatTangents)
{
	/* Zero tangents -> the midpoint is the average of the two values. */
	EXPECT_FLOAT_EQ(Math::cubicSplineInterpolation(10.0F, 0.0F, 20.0F, 0.0F, 1.0F, 0.5F), 15.0F);
}

TEST(AnimationCubicSpline, vectorEndpointsAndMidpoint)
{
	const Math::Vector< 3, float > start{0.0F, 0.0F, 0.0F};
	const Math::Vector< 3, float > end{10.0F, 20.0F, 30.0F};
	const Math::Vector< 3, float > zeroTangent{0.0F, 0.0F, 0.0F};

	const auto atStart = Math::cubicSplineInterpolation(start, zeroTangent, end, zeroTangent, 1.0F, 0.0F);
	EXPECT_FLOAT_EQ(atStart.x(), 0.0F);
	EXPECT_FLOAT_EQ(atStart.z(), 0.0F);

	const auto atEnd = Math::cubicSplineInterpolation(start, zeroTangent, end, zeroTangent, 1.0F, 1.0F);
	EXPECT_FLOAT_EQ(atEnd.x(), 10.0F);
	EXPECT_FLOAT_EQ(atEnd.z(), 30.0F);

	const auto mid = Math::cubicSplineInterpolation(start, zeroTangent, end, zeroTangent, 1.0F, 0.5F);
	EXPECT_FLOAT_EQ(mid.x(), 5.0F);
	EXPECT_FLOAT_EQ(mid.y(), 10.0F);
	EXPECT_FLOAT_EQ(mid.z(), 15.0F);
}

TEST(AnimationCubicSpline, keyframeTangentsAreStoredAndUsable)
{
	/* The keyframe structs now carry in/out tangents; a CubicSpline channel can hold them, and the
	 * evaluator consumes them to interpolate across the segment. */
	Animation::VectorKeyFrame< float > k0{};
	k0.time = 0.0F;
	k0.value = Math::Vector< 3, float >{0.0F, 0.0F, 0.0F};
	k0.outTangent = Math::Vector< 3, float >{1.0F, 0.0F, 0.0F};

	Animation::VectorKeyFrame< float > k1{};
	k1.time = 2.0F;
	k1.value = Math::Vector< 3, float >{4.0F, 0.0F, 0.0F};
	k1.inTangent = Math::Vector< 3, float >{1.0F, 0.0F, 0.0F};

	Animation::AnimationChannel< float > channel{};
	channel.target = Animation::ChannelTarget::Translation;
	channel.interpolation = Animation::ChannelInterpolation::CubicSpline;
	channel.vectorKeyFrames = {k0, k1};

	ASSERT_EQ(channel.keyFrameCount(), 2U);
	EXPECT_EQ(channel.interpolation, Animation::ChannelInterpolation::CubicSpline);

	const auto delta = k1.time - k0.time;

	const auto atStart = Math::cubicSplineInterpolation(k0.value, k0.outTangent, k1.value, k1.inTangent, delta, 0.0F);
	EXPECT_FLOAT_EQ(atStart.x(), 0.0F);

	const auto atEnd = Math::cubicSplineInterpolation(k0.value, k0.outTangent, k1.value, k1.inTangent, delta, 1.0F);
	EXPECT_FLOAT_EQ(atEnd.x(), 4.0F);
}
