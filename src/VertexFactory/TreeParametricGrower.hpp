/*
 * src/VertexFactory/TreeParametricGrower.hpp
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
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

/* Local inclusions for usages. */
#include "Math/Base.hpp"
#include "Math/CartesianFrame.hpp"
#include "Randomizer.hpp"
#include "TreeParameters.hpp"
#include "TreeSkeleton.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief Grows a tree skeleton from a species parameter set.
	 * @note This is the recursive parametric model of Weber & Penn, *Creation and Rendering of
	 * Realistic Trees*, SIGGRAPH '95 — the model Arbaro and Blender's Sapling implement, so a
	 * parameter table published for any of them transfers here unchanged.
	 * @note It produces the BOTANY only. Turning the skeleton into a mesh is the skinning phase.
	 * @warning ⚠️ Implemented from the paper except for two things, both deliberate: the periodic
	 * taper range ]2, 3] (a string of spheres, for cacti) is clamped away by treeTaperedRadius(),
	 * and pruning (`PruneRatio` and friends) is not modelled at all.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 */
	template< typename vertex_data_t = float >
	requires (std::is_floating_point_v< vertex_data_t >)
	class TreeParametricGrower final
	{
		public:

			/** @brief The number of segments beyond which the growth stops, whatever the parameters ask. */
			static constexpr size_t MaxSegments{400000};

			/**
			 * @brief Constructs a grower for a species.
			 * @param parameters A reference to the species parameters. Default a spherical tree.
			 */
			explicit
			TreeParametricGrower (const TreeParameters< vertex_data_t > & parameters = {}) noexcept
				: m_parameters(parameters)
			{

			}

			/**
			 * @brief Gives mutable access to the species parameters.
			 * @return TreeParameters< vertex_data_t > &
			 */
			[[nodiscard]]
			TreeParameters< vertex_data_t > &
			parameters () noexcept
			{
				return m_parameters;
			}

			/**
			 * @brief Gives access to the species parameters.
			 * @return const TreeParameters< vertex_data_t > &
			 */
			[[nodiscard]]
			const TreeParameters< vertex_data_t > &
			parameters () const noexcept
			{
				return m_parameters;
			}

			/**
			 * @brief Grows a tree.
			 * @note The same seed and the same parameters always give the same skeleton: the
			 * generator is local to this call, so two growers never disturb each other.
			 * @param seed The generation seed.
			 * @return TreeSkeleton< vertex_data_t >
			 */
			[[nodiscard]]
			TreeSkeleton< vertex_data_t >
			grow (uint32_t seed = 0) const noexcept
			{
				TreeSkeleton< vertex_data_t > skeleton;

				Randomizer< vertex_data_t > randomizer{seed};

				GrowthState state{skeleton, randomizer};

				const auto treeScale = m_parameters.scale() + this->spread(randomizer, m_parameters.scaleVariation());

				if ( treeScale <= 0 )
				{
					return skeleton;
				}

				const auto & trunkParameters = m_parameters.level(0);

				const auto trunkLength = std::max(
					static_cast< vertex_data_t >(0),
					(trunkParameters.length() + this->spread(randomizer, trunkParameters.lengthVariation())) * treeScale
				);

				if ( trunkLength <= 0 )
				{
					return skeleton;
				}

				StemRequest request;
				request.frame.setPosition(0, 0, 0);
				request.length = trunkLength;
				request.baseRadius = trunkLength * m_parameters.ratio();
				request.offsetInParent = 0;
				request.parentLength = trunkLength;
				request.parentSegmentIndex = TreeSegment< vertex_data_t >::NoParent;
				request.level = 0;

				this->growStem(state, request);

				return skeleton;
			}

		private:

			/**
			 * @brief What one call to growStem() needs to know about the stem it must build.
			 */
			struct StemRequest final
			{
				Math::CartesianFrame< vertex_data_t > frame;
				vertex_data_t length{0};
				vertex_data_t baseRadius{0};
				vertex_data_t offsetInParent{0};
				vertex_data_t parentLength{0};
				uint32_t parentSegmentIndex{TreeSegment< vertex_data_t >::NoParent};
				uint32_t level{0};
				/* NOTE: 0 means "as many as the level asks". A fork clone replaces only what is LEFT of
				 * the stem it interrupts, so it always gets fewer segments than that stem had. Without
				 * this, a clone restarts a full-length stem, forks again at the same relative place, and
				 * the recursion never ends — it overflowed the stack. */
				uint32_t segmentCountOverride{0};
				/* NOTE: A fork does not duplicate a stem, it divides it. The clones therefore SHARE the
				 * children the stem was going to carry, and they carry on the stem's own fork error
				 * instead of starting a fresh one. Getting either wrong multiplies the tree: with both
				 * reset, a forking broadleaf reached the 400 000 segment ceiling. */
				vertex_data_t childShare{1};
				vertex_data_t inheritedSplitError{0};
				/* NOTE: nBaseSplits forks the ORIGINAL trunk at its first segment, once (Weber & Penn). A fork clone is
				 * grown as a level-0 stem too and restarts at segment 0: without this flag every clone forked again at
				 * its own base — 1 + 3 + 9 + 27 + 81 + 243 = 364 "trunks" on the broadleaf, the 243 last ones bare,
				 * thin and poking 2 m out of the crown (owner, 2026-09-23: branches popping in on approach). */
				bool baseSplitAllowed{true};
			};

			/**
			 * @brief The mutable state shared by the whole growth, passed down the recursion.
			 */
			struct GrowthState final
			{
				TreeSkeleton< vertex_data_t > & skeleton;
				Randomizer< vertex_data_t > & randomizer;
				uint32_t nextBranchIndex{0};
			};

			/**
			 * @brief Returns a random value in [-spread, +spread].
			 * @param randomizer A reference to the generator.
			 * @param halfRange The half range. A negative range is taken as its absolute value.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			spread (Randomizer< vertex_data_t > & randomizer, vertex_data_t halfRange) const noexcept
			{
				if ( halfRange == 0 )
				{
					return 0;
				}

				const auto half = std::abs(halfRange);

				return randomizer.value(-half, half);
			}

			/**
			 * @brief Returns the radius of a stem at a relative position along it.
			 * @param request A reference to the stem request.
			 * @param unitPosition The position along the stem, in [0, 1].
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			stemRadius (const StemRequest & request, vertex_data_t unitPosition) const noexcept
			{
				const auto & levelParameters = m_parameters.level(request.level);

				auto radius = treeTaperedRadius(request.baseRadius, unitPosition, levelParameters.taper(), request.length);

				/* The trunk widens at its foot. Weber & Penn, § 4.4 "Flare": the term dies out by
				 * an eighth of the trunk. */
				if ( request.level == 0 && m_parameters.flare() > 0 )
				{
					const auto exponent = static_cast< vertex_data_t >(1) - static_cast< vertex_data_t >(8) * unitPosition;

					radius *= m_parameters.flare() * (std::pow(static_cast< vertex_data_t >(100), exponent) - static_cast< vertex_data_t >(1)) / static_cast< vertex_data_t >(100) + static_cast< vertex_data_t >(1);
				}

				return std::max(static_cast< vertex_data_t >(0), radius);
			}

			/**
			 * @brief Bends a frame toward the world up axis, without moving it.
			 * @note Weber & Penn's `AttractionUp`. The bend happens around the horizontal axis
			 * perpendicular to the stem, which is NOT the frame's local X in general, so the axis is
			 * computed in world space and brought back into the frame.
			 * @param frame A reference to the frame to bend.
			 * @param strength How much of the remaining declination is taken back.
			 * @return void
			 */
			void
			bendTowardWorldUp (Math::CartesianFrame< vertex_data_t > & frame, vertex_data_t strength) const noexcept
			{
				if ( strength == 0 )
				{
					return;
				}

				const auto & growthAxis = frame.localYAxis();
				const auto worldUp = Math::Vector< 3, vertex_data_t >::positiveY();

				const auto bendAxis = Math::Vector< 3, vertex_data_t >::crossProduct(growthAxis, worldUp);

				if ( bendAxis.length() < static_cast< vertex_data_t >(1e-5) )
				{
					return;
				}

				const auto declination = std::acos(std::clamp(Math::Vector< 3, vertex_data_t >::dotProduct(growthAxis, worldUp), static_cast< vertex_data_t >(-1), static_cast< vertex_data_t >(1)));

				const auto worldAxis = bendAxis.normalized();

				/* rotate(…, local = true) multiplies the axis by the frame rotation, so the world
				 * axis has to be brought into the frame first; local = false would also move the
				 * position around the world origin, which is not what a bend is. */
				const Math::Vector< 3, vertex_data_t > localAxis{
					Math::Vector< 3, vertex_data_t >::dotProduct(worldAxis, frame.rightVector()),
					Math::Vector< 3, vertex_data_t >::dotProduct(worldAxis, frame.localYAxis()),
					Math::Vector< 3, vertex_data_t >::dotProduct(worldAxis, frame.backwardVector())
				};

				frame.rotate(declination * strength, localAxis, true);
			}

			/**
			 * @brief Grows one stem and everything it carries.
			 * @param state A reference to the growth state.
			 * @param request A reference to the stem request.
			 * @return void
			 */
			void
			growStem (GrowthState & state, const StemRequest & request) const noexcept
			{
				if ( request.length <= 0 || state.skeleton.segmentCount() >= MaxSegments )
				{
					return;
				}

				const auto & levelParameters = m_parameters.level(request.level);

				const auto segmentCount = request.segmentCountOverride > 0 ? request.segmentCountOverride : levelParameters.segmentCount();
				const auto segmentLength = request.length / static_cast< vertex_data_t >(segmentCount);
				const auto branchIndex = state.nextBranchIndex++;

				auto frame = request.frame;
				auto parentSegmentIndex = request.parentSegmentIndex;
				vertex_data_t arcLength = 0;
				auto splitError = request.inheritedSplitError;

				std::vector< uint32_t > stemSegments;
				stemSegments.reserve(segmentCount);

				for ( uint32_t index = 0; index < segmentCount; ++index )
				{
					if ( state.skeleton.segmentCount() >= MaxSegments )
					{
						break;
					}

					const auto unitStart = static_cast< vertex_data_t >(index) / static_cast< vertex_data_t >(segmentCount);
					const auto unitEnd = static_cast< vertex_data_t >(index + 1) / static_cast< vertex_data_t >(segmentCount);

					TreeSegment< vertex_data_t > segment{
						frame,
						segmentLength,
						this->stemRadius(request, unitStart),
						this->stemRadius(request, unitEnd),
						arcLength,
						parentSegmentIndex,
						branchIndex,
						request.level
					};

					segment.setBranchTip(index + 1 == segmentCount);

					parentSegmentIndex = state.skeleton.addSegment(segment);

					stemSegments.emplace_back(parentSegmentIndex);

					arcLength += segmentLength;

					frame.translateY(segmentLength, true);

					/* How many times this boundary forks. The trunk's first boundary uses
					 * nBaseSplits; everywhere else the fractional nSegSplits is accumulated so that
					 * a value of 0.3 forks roughly every third segment. */
					uint32_t splitCount = 0;

					if ( index == 0 && request.level == 0 && request.baseSplitAllowed && levelParameters.baseSplits() > 0 )
					{
						splitCount = levelParameters.baseSplits();
					}
					else if ( levelParameters.segmentSplits() > 0 )
					{
						splitError += levelParameters.segmentSplits();

						splitCount = static_cast< uint32_t >(std::floor(splitError + static_cast< vertex_data_t >(0.5)));

						splitError -= static_cast< vertex_data_t >(splitCount);
					}

					const auto remainingLength = request.length - arcLength;

					if ( splitCount > 0 && index + 1 < segmentCount && remainingLength > request.length * static_cast< vertex_data_t >(0.02) )
					{
						this->growSplit(state, request, frame, parentSegmentIndex, splitCount, remainingLength, segmentCount - index - 1, this->stemRadius(request, unitEnd), splitError);

						/* Nothing continues straight on past a fork, but the part already built still
						 * carries its own children: stop the loop, do not leave the function. */
						break;
					}

					/* Curvature, spent over the remaining segments. An S-shaped stem spends nCurve
					 * over its first half and nCurveBack over its second. */
					const auto halfCount = static_cast< vertex_data_t >(segmentCount) / static_cast< vertex_data_t >(2);

					auto curveAngle = levelParameters.curveBack() == 0 ?
						levelParameters.curve() / static_cast< vertex_data_t >(segmentCount) :
						(static_cast< vertex_data_t >(index) < halfCount ? levelParameters.curve() / halfCount : levelParameters.curveBack() / halfCount);

					curveAngle += this->spread(state.randomizer, levelParameters.curveVariation()) / static_cast< vertex_data_t >(segmentCount);

					frame.pitch(Math::Radian(curveAngle), true);

					if ( request.level >= 2 )
					{
						this->bendTowardWorldUp(frame, m_parameters.attractionUp() / static_cast< vertex_data_t >(segmentCount));
					}
				}

				if ( stemSegments.empty() )
				{
					return;
				}

				if ( request.level + 1 < m_parameters.levels() )
				{
					this->growChildren(state, request, stemSegments, arcLength);
				}
				else
				{
					this->placeLeaves(state, stemSegments, arcLength);
				}
			}

			/**
			 * @brief Replaces the rest of a stem by several diverging clones.
			 * @param state A reference to the growth state.
			 * @param request A reference to the stem request being interrupted.
			 * @param frame A reference to the frame at the fork.
			 * @param forkSegmentIndex The index of the segment the fork sits on.
			 * @param splitCount How many EXTRA stems the fork creates.
			 * @param remainingLength What is left of the stem past the fork.
			 * @param remainingSegments How many segments are left of the stem past the fork.
			 * @param forkRadius The stem radius measured at the fork.
			 * @param splitError The fork error the stem had accumulated, carried on by the clones.
			 * @return void
			 */
			void
			growSplit (GrowthState & state, const StemRequest & request, const Math::CartesianFrame< vertex_data_t > & frame, uint32_t forkSegmentIndex, uint32_t splitCount, vertex_data_t remainingLength, uint32_t remainingSegments, vertex_data_t forkRadius, vertex_data_t splitError) const noexcept
			{
				if ( remainingLength <= 0 || remainingSegments == 0 )
				{
					return;
				}

				const auto & levelParameters = m_parameters.level(request.level);

				const auto cloneCount = splitCount + 1;

				/* The clones share the section of the stem they replace, so each is thinner. */
				const auto radiusShare = std::pow(static_cast< vertex_data_t >(1) / static_cast< vertex_data_t >(cloneCount), static_cast< vertex_data_t >(1) / std::max(m_parameters.ratioPower(), static_cast< vertex_data_t >(0.01)));

				const auto azimuthStep = static_cast< vertex_data_t >(360) / static_cast< vertex_data_t >(cloneCount);
				const auto azimuthOffset = state.randomizer.value(static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(360));

				for ( uint32_t clone = 0; clone < cloneCount; ++clone )
				{
					auto cloneFrame = frame;

					cloneFrame.yaw(Math::Radian(azimuthOffset + azimuthStep * static_cast< vertex_data_t >(clone)), true);

					const auto divergence = (levelParameters.splitAngle() + this->spread(state.randomizer, levelParameters.splitAngleVariation())) / static_cast< vertex_data_t >(2);

					cloneFrame.pitch(Math::Radian(divergence), true);

					StemRequest cloneRequest;
					cloneRequest.frame = cloneFrame;
					cloneRequest.length = remainingLength;
					cloneRequest.baseRadius = forkRadius * radiusShare;
					cloneRequest.offsetInParent = request.offsetInParent;
					cloneRequest.parentLength = request.parentLength;
					cloneRequest.parentSegmentIndex = forkSegmentIndex;
					cloneRequest.level = request.level;
					cloneRequest.segmentCountOverride = remainingSegments;
					cloneRequest.childShare = request.childShare / static_cast< vertex_data_t >(cloneCount);
					cloneRequest.inheritedSplitError = splitError;
					cloneRequest.baseSplitAllowed = false;

					this->growStem(state, cloneRequest);
				}
			}

			/**
			 * @brief Returns the frame sitting at a distance along a stem.
			 * @param state A reference to the growth state.
			 * @param stemSegments A reference to the segment indexes of the stem.
			 * @param offset The distance from the base of the stem.
			 * @return Math::CartesianFrame< vertex_data_t >
			 */
			[[nodiscard]]
			Math::CartesianFrame< vertex_data_t >
			frameAlongStem (const GrowthState & state, const std::vector< uint32_t > & stemSegments, vertex_data_t offset) const noexcept
			{
				const auto & segments = state.skeleton.segments();

				for ( const auto segmentIndex : stemSegments )
				{
					const auto & segment = segments[segmentIndex];

					if ( offset <= segment.arcLength() + segment.length() )
					{
						auto frame = segment.frame();

						frame.translateY(std::max(static_cast< vertex_data_t >(0), offset - segment.arcLength()), true);

						return frame;
					}
				}

				/* Past the end: sit on the tip of the last segment. */
				const auto & last = segments[stemSegments.back()];

				auto frame = last.frame();

				frame.translateY(last.length(), true);

				return frame;
			}

			/**
			 * @brief Returns the index of the segment covering a distance along a stem.
			 * @param state A reference to the growth state.
			 * @param stemSegments A reference to the segment indexes of the stem.
			 * @param offset The distance from the base of the stem.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			segmentAlongStem (const GrowthState & state, const std::vector< uint32_t > & stemSegments, vertex_data_t offset) const noexcept
			{
				const auto & segments = state.skeleton.segments();

				for ( const auto segmentIndex : stemSegments )
				{
					const auto & segment = segments[segmentIndex];

					if ( offset <= segment.arcLength() + segment.length() )
					{
						return segmentIndex;
					}
				}

				return stemSegments.back();
			}

			/**
			 * @brief Returns the angle a child leaves its parent by, in degrees.
			 * @param state A reference to the growth state.
			 * @param childParameters A reference to the child level parameters.
			 * @param positionRatio Where the child sits along the usable span of the parent, in [0, 1].
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			childDownAngle (GrowthState & state, const TreeLevelParameters< vertex_data_t > & childParameters, vertex_data_t positionRatio) const noexcept
			{
				/* Weber & Penn, § 4.5: a NEGATIVE nDownAngleV is not a spread, it makes the angle
				 * vary with the position along the parent — that is what lets the low branches of an
				 * aspen hang while the high ones reach up. */
				if ( childParameters.downAngleVariation() < 0 )
				{
					return childParameters.downAngle() + childParameters.downAngleVariation() *
						(static_cast< vertex_data_t >(1) - static_cast< vertex_data_t >(2) * treeShapeRatio(TreeCrownShape::Conical, positionRatio));
				}

				return childParameters.downAngle() + this->spread(state.randomizer, childParameters.downAngleVariation());
			}

			/**
			 * @brief Grows every child stem carried by a stem.
			 * @param state A reference to the growth state.
			 * @param request A reference to the parent stem request.
			 * @param stemSegments A reference to the segment indexes of the parent stem.
			 * @param builtLength How much of the stem was actually built, a fork can cut it short.
			 * @return void
			 */
			void
			growChildren (GrowthState & state, const StemRequest & request, const std::vector< uint32_t > & stemSegments, vertex_data_t builtLength) const noexcept
			{
				const auto childLevel = request.level + 1;
				const auto & childParameters = m_parameters.level(childLevel);

				const auto maxChildLength = childParameters.length() + this->spread(state.randomizer, childParameters.lengthVariation());

				if ( maxChildLength <= 0 || childParameters.branches() == 0 )
				{
					return;
				}

				/* The foot of the trunk carries nothing; deeper stems branch from their base. */
				const auto offsetStart = request.level == 0 ? m_parameters.baseSize() * request.length : static_cast< vertex_data_t >(0);
				const auto nominalSpan = request.length - offsetStart;
				const auto usableLength = builtLength - offsetStart;

				if ( usableLength <= 0 )
				{
					return;
				}

				/* Weber & Penn, § 4.5: a stem that attaches far along its own parent carries fewer
				 * children than one at its base. */
				auto childCount = static_cast< vertex_data_t >(childParameters.branches()) * request.childShare;

				if ( request.level > 0 && request.parentLength > 0 )
				{
					childCount *= static_cast< vertex_data_t >(1) - static_cast< vertex_data_t >(0.5) * (request.offsetInParent / request.parentLength);
				}

				const auto count = static_cast< uint32_t >(std::max(static_cast< vertex_data_t >(0), std::floor(childCount + static_cast< vertex_data_t >(0.5))));

				if ( count == 0 )
				{
					return;
				}

				vertex_data_t rotateAngle = 0;

				for ( uint32_t index = 0; index < count; ++index )
				{
					if ( state.skeleton.segmentCount() >= MaxSegments )
					{
						return;
					}

					const auto span = (static_cast< vertex_data_t >(index) + static_cast< vertex_data_t >(0.5)) / static_cast< vertex_data_t >(count);
					const auto offset = offsetStart + span * usableLength;

					/* The position along the usable span, counted from the TOP: 1 at the tip. */
					const auto positionRatio = nominalSpan > 0 ? (request.length - offset) / nominalSpan : static_cast< vertex_data_t >(0);

					const auto childLength = request.level == 0 ?
						request.length * maxChildLength * treeShapeRatio(m_parameters.shape(), positionRatio) :
						maxChildLength * (request.length - static_cast< vertex_data_t >(0.6) * offset);

					if ( childLength <= 0 )
					{
						continue;
					}

					/* Spin around the parent. A negative nRotate alternates sides instead of
					 * winding always the same way. */
					if ( childParameters.rotate() >= 0 )
					{
						rotateAngle += childParameters.rotate() + this->spread(state.randomizer, childParameters.rotateVariation());
					}
					else
					{
						const auto side = index % 2 == 0 ? static_cast< vertex_data_t >(1) : static_cast< vertex_data_t >(-1);

						rotateAngle = side * (static_cast< vertex_data_t >(180) + childParameters.rotate() + this->spread(state.randomizer, childParameters.rotateVariation()));
					}

					auto childFrame = this->frameAlongStem(state, stemSegments, offset);

					childFrame.yaw(Math::Radian(rotateAngle), true);
					childFrame.pitch(Math::Radian(this->childDownAngle(state, childParameters, positionRatio)), true);

					const auto parentRadius = this->stemRadius(request, offset / request.length);

					StemRequest childRequest;
					childRequest.frame = childFrame;
					childRequest.length = childLength;
					childRequest.baseRadius = std::min(
						parentRadius * static_cast< vertex_data_t >(0.9),
						parentRadius * std::pow(childLength / request.length, m_parameters.ratioPower())
					);
					childRequest.offsetInParent = offset;
					childRequest.parentLength = request.length;
					childRequest.parentSegmentIndex = this->segmentAlongStem(state, stemSegments, offset);
					childRequest.level = childLevel;

					this->growStem(state, childRequest);
				}
			}

			/**
			 * @brief Hangs the leaves of a terminal stem.
			 * @param state A reference to the growth state.
			 * @param stemSegments A reference to the segment indexes of the stem.
			 * @param builtLength How much of the stem was actually built, a fork can cut it short.
			 * @return void
			 */
			void
			placeLeaves (GrowthState & state, const std::vector< uint32_t > & stemSegments, vertex_data_t builtLength) const noexcept
			{
				const auto count = m_parameters.leaves();

				if ( count == 0 || m_parameters.leafScale() <= 0 )
				{
					return;
				}

				vertex_data_t rotateAngle = 0;

				for ( uint32_t index = 0; index < count; ++index )
				{
					const auto span = (static_cast< vertex_data_t >(index) + static_cast< vertex_data_t >(0.5)) / static_cast< vertex_data_t >(count);
					const auto offset = span * builtLength;

					auto leafFrame = this->frameAlongStem(state, stemSegments, offset);

					rotateAngle += static_cast< vertex_data_t >(137.5) + this->spread(state.randomizer, static_cast< vertex_data_t >(15));

					leafFrame.yaw(Math::Radian(rotateAngle), true);
					leafFrame.pitch(Math::Radian(static_cast< vertex_data_t >(60) + this->spread(state.randomizer, static_cast< vertex_data_t >(25))), true);

					const auto scale = m_parameters.leafScale() * (static_cast< vertex_data_t >(1) + this->spread(state.randomizer, static_cast< vertex_data_t >(0.2)));

					state.skeleton.addLeaf({leafFrame, scale, this->segmentAlongStem(state, stemSegments, offset)});
				}
			}

			TreeParameters< vertex_data_t > m_parameters;
	};
}
