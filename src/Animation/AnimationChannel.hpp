/*
 * src/Animation/AnimationChannel.hpp
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
 *
 * --- THIS IS AUTOMATICALLY GENERATED, DO NOT CHANGE ---
 */

#pragma once

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <vector>

/* Local inclusions. */
#include "Math/Base.hpp"
#include "Math/Quaternion.hpp"
#include "Math/Vector.hpp"

namespace EmEn::Base::Animation
{
	using namespace Math;

	/**
	 * @brief Interpolation method for animation keyframes.
	 * @note Matches the GLTF 2.0 animation sampler interpolation types.
	 */
	enum class ChannelInterpolation : uint8_t
	{
		Step,		/**< No interpolation — holds the value until the next keyframe. */
		Linear,	  /**< Linear interpolation (LERP for T/S, SLERP for R). */
		CubicSpline  /**< Cubic spline interpolation (GLTF cubic with in/out tangents). */
	};

	/**
	 * @brief The transform component targeted by an animation channel.
	 */
	enum class ChannelTarget : uint8_t
	{
		Translation,
		Rotation,
		Scale
	};

	/**
	 * @brief A single keyframe for a translation or scale channel.
	 * @note inTangent/outTangent are only used by ChannelInterpolation::CubicSpline (the GLTF cubic
	 * sampler, see Math::cubicSplineInterpolation()). They are ignored for Step/Linear.
	 * @tparam precision_t Floating point type.
	 */
	template< typename precision_t = float >
	requires (std::is_floating_point_v< precision_t >)
	struct VectorKeyFrame final
	{
		precision_t time{0};
		Vector< 3, precision_t > value{};
		Vector< 3, precision_t > inTangent{};
		Vector< 3, precision_t > outTangent{};
	};

	/**
	 * @brief A single keyframe for a rotation channel.
	 * @note inTangent/outTangent are only used by ChannelInterpolation::CubicSpline (the GLTF cubic
	 * sampler). They are ignored for Step/Linear. The cubic result must be normalized.
	 * @tparam precision_t Floating point type.
	 */
	template< typename precision_t = float >
	requires (std::is_floating_point_v< precision_t >)
	struct QuaternionKeyFrame final
	{
		precision_t time{0};
		Quaternion< precision_t > value{};
		Quaternion< precision_t > inTangent{};
		Quaternion< precision_t > outTangent{};
	};

	/**
	 * @brief An animation channel targeting a specific joint's transform component.
	 * @note Each channel contains keyframes for ONE component (translation, rotation, or scale)
	 * of ONE joint. Timestamps are in seconds. Keyframes must be sorted by time (ascending).
	 *
	 * A joint may have 0 to 3 channels in a clip (one per T/R/S component).
	 * If a component has no channel, the bind-pose value is used.
	 *
	 * @tparam precision_t Floating point type. Default float.
	 */
	template< typename precision_t = float >
	requires (std::is_floating_point_v< precision_t >)
	struct AnimationChannel final
	{
		/**
		 * @brief Index of the animated target inside the structure the clip belongs to.
		 * @note ⚠️ The clip is target-AGNOSTIC on purpose: a SKELETAL clip indexes the Skeleton's
		 * joint array, a NODE clip indexes the imported hierarchy's node array. The two are never
		 * interchangeable — feeding one to the other's evaluator silently animates the wrong
		 * things — so the producer and the consumer must agree on which structure this indexes.
		 */
		int32_t targetIndex{-1};

		/** @brief Which transform component this channel animates. */
		ChannelTarget target{ChannelTarget::Translation};

		/** @brief Interpolation method for this channel. */
		ChannelInterpolation interpolation{ChannelInterpolation::Linear};

		/** @brief Keyframes for translation or scale channels. Empty if target is Rotation. */
		std::vector< VectorKeyFrame< precision_t > > vectorKeyFrames{};

		/** @brief Keyframes for rotation channels. Empty if target is Translation or Scale. */
		std::vector< QuaternionKeyFrame< precision_t > > quaternionKeyFrames{};

		/**
		 * @brief Returns the number of keyframes in this channel.
		 * @return size_t
		 */
		[[nodiscard]]
		size_t
		keyFrameCount () const noexcept
		{
			if ( target == ChannelTarget::Rotation )
			{
				return quaternionKeyFrames.size();
			}

			return vectorKeyFrames.size();
		}

		/**
		 * @brief Returns true if this channel has no keyframes.
		 * @return bool
		 */
		[[nodiscard]]
		bool
		empty () const noexcept
		{
			return keyFrameCount() == 0;
		}

		/**
		 * @brief Returns the timestamp of the last keyframe, or 0 if empty.
		 * @return precision_t
		 */
		[[nodiscard]]
		precision_t
		duration () const noexcept
		{
			if ( target == ChannelTarget::Rotation )
			{
				return quaternionKeyFrames.empty() ? precision_t{0} : quaternionKeyFrames.back().time;
			}

			return vectorKeyFrames.empty() ? precision_t{0} : vectorKeyFrames.back().time;
		}

		/**
		 * @brief Samples the translation or scale track at the given time.
		 * @note ⚠️ Lives HERE rather than in an evaluator: sampling a keyframe track is a property
		 * of the track, and the engine has more than one animator reading these channels — a
		 * skeletal one and a node one. A second copy of this interpolation is a second place for
		 * the CubicSpline stride and the tangent scaling to drift apart.
		 * @param time The time to sample at, in seconds. Clamped to the track's own range.
		 * @return Math::Vector< 3, precision_t >
		 */
		[[nodiscard]]
		Math::Vector< 3, precision_t >
		sampleVector (precision_t time) const noexcept
		{
			const auto & keyFrames = vectorKeyFrames;

			if ( keyFrames.empty() )
			{
				return {};
			}

			if ( keyFrames.size() == 1 || time <= keyFrames.front().time )
			{
				return keyFrames.front().value;
			}

			if ( time >= keyFrames.back().time )
			{
				return keyFrames.back().value;
			}

			const auto index = findKeyFrameIndex(keyFrames, time);
			const auto & previous = keyFrames[index];
			const auto & next = keyFrames[index + 1];

			if ( interpolation == ChannelInterpolation::Step )
			{
				return previous.value;
			}

			const auto span = next.time - previous.time;
			const auto factor = span > precision_t{0} ? (time - previous.time) / span : precision_t{0};

			/* glTF cubic: the tangents are scaled by the segment duration inside the evaluator. */
			if ( interpolation == ChannelInterpolation::CubicSpline )
			{
				return Math::cubicSplineInterpolation(previous.value, previous.outTangent, next.value, next.inTangent, span, factor);
			}

			return Math::Vector< 3, precision_t >{
				previous.value[0] + (next.value[0] - previous.value[0]) * factor,
				previous.value[1] + (next.value[1] - previous.value[1]) * factor,
				previous.value[2] + (next.value[2] - previous.value[2]) * factor
			};
		}

		/**
		 * @brief Samples the rotation track at the given time.
		 * @param time The time to sample at, in seconds. Clamped to the track's own range.
		 * @return Math::Quaternion< precision_t >
		 */
		[[nodiscard]]
		Math::Quaternion< precision_t >
		sampleQuaternion (precision_t time) const noexcept
		{
			const auto & keyFrames = quaternionKeyFrames;

			if ( keyFrames.empty() )
			{
				return {};
			}

			if ( keyFrames.size() == 1 || time <= keyFrames.front().time )
			{
				return keyFrames.front().value;
			}

			if ( time >= keyFrames.back().time )
			{
				return keyFrames.back().value;
			}

			const auto index = findKeyFrameIndex(keyFrames, time);
			const auto & previous = keyFrames[index];
			const auto & next = keyFrames[index + 1];

			if ( interpolation == ChannelInterpolation::Step )
			{
				return previous.value;
			}

			const auto span = next.time - previous.time;
			const auto factor = span > precision_t{0} ? (time - previous.time) / span : precision_t{0};

			/* ⚠️ glTF cubic on a rotation is evaluated component-wise, so the result is NOT unit
			 * length and must be normalized before it reaches a transform matrix. */
			if ( interpolation == ChannelInterpolation::CubicSpline )
			{
				auto result = Math::cubicSplineInterpolation(previous.value, previous.outTangent, next.value, next.inTangent, span, factor);
				result.normalize();

				return result;
			}

			return Math::Quaternion< precision_t >::slerp(previous.value, next.value, factor, precision_t{0.05});
		}

		private:

		/**
		 * @brief Finds the index of the last keyframe at or before the given time.
		 * @param keyFrames A reference to the keyframe list.
		 * @param time The time to look for.
		 * @return size_t The index, or 0 when the time precedes every keyframe.
		 */
		template< typename keyframe_t >
		[[nodiscard]]
		static
		size_t
		findKeyFrameIndex (const std::vector< keyframe_t > & keyFrames, precision_t time) noexcept
		{
			if ( keyFrames.size() <= 1 )
			{
				return 0;
			}

			size_t low = 0;
			size_t high = keyFrames.size() - 1;

			while ( low < high - 1 )
			{
				const auto mid = (low + high) / 2;

				if ( keyFrames[mid].time <= time )
				{
					low = mid;
				}
				else
				{
					high = mid;
				}
			}

			return low;
		}
	};

	using AnimationChannelF = AnimationChannel< float >;
	using AnimationChannelD = AnimationChannel< double >;
}
