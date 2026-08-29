/*
 * src/Testing/test_AnimationChannelSampling.cpp
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
#include "Math/Quaternion.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::Animation;

/* AnimationChannel::sampleVector() / sampleQuaternion() were promoted out of the engine's skeletal
 * animator when a SECOND evaluator appeared (the node-animation component): a track knows how to
 * read itself, and one copy of that interpolation means the CubicSpline stride and the tangent
 * scaling can no longer drift between the two consumers. These tests pin the promoted behaviour. */

namespace
{
	AnimationChannel< float >
	makeVectorChannel (ChannelInterpolation interpolation, const std::vector< std::pair< float, Math::Vector< 3, float > > > & keys)
	{
		AnimationChannel< float > channel{};
		channel.target = ChannelTarget::Translation;
		channel.interpolation = interpolation;

		for ( const auto & [time, value] : keys )
		{
			VectorKeyFrame< float > keyFrame{};
			keyFrame.time = time;
			keyFrame.value = value;

			channel.vectorKeyFrames.push_back(keyFrame);
		}

		return channel;
	}
}

TEST(AnimationChannelSampling, emptyTrackYieldsZero)
{
	const AnimationChannel< float > channel{};

	const auto sampled = channel.sampleVector(1.0F);

	EXPECT_FLOAT_EQ(sampled[Math::X], 0.0F);
	EXPECT_FLOAT_EQ(sampled[Math::Y], 0.0F);
	EXPECT_FLOAT_EQ(sampled[Math::Z], 0.0F);
}

TEST(AnimationChannelSampling, singleKeyFrameHoldsItsValue)
{
	const auto channel = makeVectorChannel(ChannelInterpolation::Linear, {{2.0F, {1.0F, 2.0F, 3.0F}}});

	for ( const auto time : {-5.0F, 2.0F, 100.0F} )
	{
		const auto sampled = channel.sampleVector(time);

		EXPECT_FLOAT_EQ(sampled[Math::X], 1.0F);
		EXPECT_FLOAT_EQ(sampled[Math::Y], 2.0F);
		EXPECT_FLOAT_EQ(sampled[Math::Z], 3.0F);
	}
}

TEST(AnimationChannelSampling, timeOutsideTheTrackIsClamped)
{
	const auto channel = makeVectorChannel(ChannelInterpolation::Linear, {
		{1.0F, {0.0F, 0.0F, 0.0F}},
		{2.0F, {10.0F, 0.0F, 0.0F}}
	});

	EXPECT_FLOAT_EQ(channel.sampleVector(0.0F)[Math::X], 0.0F);
	EXPECT_FLOAT_EQ(channel.sampleVector(99.0F)[Math::X], 10.0F);
}

TEST(AnimationChannelSampling, linearInterpolatesTheMidpoint)
{
	const auto channel = makeVectorChannel(ChannelInterpolation::Linear, {
		{0.0F, {0.0F, 0.0F, 0.0F}},
		{2.0F, {10.0F, -20.0F, 4.0F}}
	});

	const auto sampled = channel.sampleVector(1.0F);

	EXPECT_FLOAT_EQ(sampled[Math::X], 5.0F);
	EXPECT_FLOAT_EQ(sampled[Math::Y], -10.0F);
	EXPECT_FLOAT_EQ(sampled[Math::Z], 2.0F);
}

TEST(AnimationChannelSampling, stepHoldsThePreviousKeyFrame)
{
	const auto channel = makeVectorChannel(ChannelInterpolation::Step, {
		{0.0F, {1.0F, 0.0F, 0.0F}},
		{2.0F, {9.0F, 0.0F, 0.0F}}
	});

	/* Anywhere INSIDE the segment the previous value stands — that is the whole point of Step. */
	EXPECT_FLOAT_EQ(channel.sampleVector(1.999F)[Math::X], 1.0F);
	EXPECT_FLOAT_EQ(channel.sampleVector(2.0F)[Math::X], 9.0F);
}

TEST(AnimationChannelSampling, theRightSegmentIsPickedOnALongTrack)
{
	/* The lookup is a binary search: a track of more than two keyframes is what tells a correct
	 * search from one that always lands on the first segment. */
	const auto channel = makeVectorChannel(ChannelInterpolation::Linear, {
		{0.0F, {0.0F, 0.0F, 0.0F}},
		{1.0F, {10.0F, 0.0F, 0.0F}},
		{2.0F, {20.0F, 0.0F, 0.0F}},
		{3.0F, {30.0F, 0.0F, 0.0F}},
		{4.0F, {40.0F, 0.0F, 0.0F}}
	});

	EXPECT_FLOAT_EQ(channel.sampleVector(0.5F)[Math::X], 5.0F);
	EXPECT_FLOAT_EQ(channel.sampleVector(1.5F)[Math::X], 15.0F);
	EXPECT_FLOAT_EQ(channel.sampleVector(2.5F)[Math::X], 25.0F);
	EXPECT_FLOAT_EQ(channel.sampleVector(3.5F)[Math::X], 35.0F);
}

TEST(AnimationChannelSampling, cubicSplineHitsItsEndpoints)
{
	auto channel = makeVectorChannel(ChannelInterpolation::CubicSpline, {
		{0.0F, {0.0F, 0.0F, 0.0F}},
		{1.0F, {10.0F, 0.0F, 0.0F}}
	});

	/* Non-zero tangents: an evaluator ignoring them would still pass with flat ones. */
	channel.vectorKeyFrames[0].outTangent = Math::Vector< 3, float >{50.0F, 0.0F, 0.0F};
	channel.vectorKeyFrames[1].inTangent = Math::Vector< 3, float >{-30.0F, 0.0F, 0.0F};

	EXPECT_NEAR(channel.sampleVector(0.0F)[Math::X], 0.0F, 1e-4F);
	EXPECT_NEAR(channel.sampleVector(1.0F)[Math::X], 10.0F, 1e-4F);

	/* The tangents must actually bend the curve away from the linear midpoint. */
	EXPECT_GT(channel.sampleVector(0.5F)[Math::X], 5.0F);
}

TEST(AnimationChannelSampling, quaternionSlerpStaysUnitLength)
{
	AnimationChannel< float > channel{};
	channel.target = ChannelTarget::Rotation;
	channel.interpolation = ChannelInterpolation::Linear;

	QuaternionKeyFrame< float > first{};
	first.time = 0.0F;
	first.value = Math::Quaternion< float >{0.0F, 0.0F, 0.0F, 1.0F};

	QuaternionKeyFrame< float > second{};
	second.time = 1.0F;
	second.value = Math::Quaternion< float >{0.0F, 0.7071068F, 0.0F, 0.7071068F};

	channel.quaternionKeyFrames.push_back(first);
	channel.quaternionKeyFrames.push_back(second);

	const auto sampled = channel.sampleQuaternion(0.5F);

	EXPECT_NEAR(sampled.length(), 1.0F, 1e-4F);
	/* Halfway between identity and a 90° yaw is a 45° yaw: its Y component sits between the two. */
	EXPECT_GT(sampled[Math::Y], 0.0F);
	EXPECT_LT(sampled[Math::Y], 0.7071068F);
}

TEST(AnimationChannelSampling, cubicSplineRotationIsRenormalized)
{
	/* ⚠️ A component-wise cubic on a quaternion does NOT come out unit length. Feeding that
	 * straight into a transform matrix scales the whole node — the normalization is load-bearing. */
	AnimationChannel< float > channel{};
	channel.target = ChannelTarget::Rotation;
	channel.interpolation = ChannelInterpolation::CubicSpline;

	QuaternionKeyFrame< float > first{};
	first.time = 0.0F;
	first.value = Math::Quaternion< float >{0.0F, 0.0F, 0.0F, 1.0F};
	first.outTangent = Math::Quaternion< float >{0.0F, 2.0F, 0.0F, 0.0F};

	QuaternionKeyFrame< float > second{};
	second.time = 1.0F;
	second.value = Math::Quaternion< float >{0.0F, 0.7071068F, 0.0F, 0.7071068F};
	second.inTangent = Math::Quaternion< float >{0.0F, 2.0F, 0.0F, 0.0F};

	channel.quaternionKeyFrames.push_back(first);
	channel.quaternionKeyFrames.push_back(second);

	EXPECT_NEAR(channel.sampleQuaternion(0.5F).length(), 1.0F, 1e-4F);
}
