/*
 * src/VertexFactory/TreeColonizationGrower.hpp
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
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <vector>

/* Local inclusions for usages. */
#include "Math/Vector.hpp"
#include "Randomizer.hpp"
#include "TreeSkeleton.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief Grows a tree skeleton by letting branches compete for the free space of a crown.
	 * @note This is the space colonization algorithm of Runions, Lane & Prusinkiewicz, *Modeling
	 * Trees with a Space Colonization Algorithm*, Eurographics Workshop on Natural Phenomena 2007.
	 * A cloud of attraction points fills the crown; every branch tip steers toward the points that
	 * are closer to it than to any other tip, and a point is consumed once a tip reaches it. The
	 * branching pattern is not prescribed anywhere: it EMERGES from that competition, which is why
	 * it breaks the regularity a recursive model has trouble hiding.
	 * @note Where the parametric grower is driven by species parameters, this one is driven by the
	 * SHAPE of the crown: change the attractor cloud and you change the tree. Feed
	 * growFromAttractors() to use any volume, sampled however you like.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 */
	template< typename vertex_data_t = float >
	requires (std::is_floating_point_v< vertex_data_t >)
	class TreeColonizationGrower final
	{
		public:

			/** @brief The number of nodes beyond which the growth stops, whatever the parameters ask. */
			static constexpr size_t MaxNodes{200000};

			/**
			 * @brief Constructs a grower with a crown of default size.
			 */
			TreeColonizationGrower () noexcept = default;

			/** @brief Sets how many attraction points fill the crown. @param value The count. @return void */
			void setAttractorCount (uint32_t value) noexcept { m_attractorCount = value; }

			/** @brief Returns how many attraction points fill the crown. @return uint32_t */
			[[nodiscard]] uint32_t attractorCount () const noexcept { return m_attractorCount; }

			/** @brief Sets the centre of the ellipsoidal crown. @param value The centre. @return void */
			void setCrownCenter (const Math::Vector< 3, vertex_data_t > & value) noexcept { m_crownCenter = value; }

			/** @brief Returns the centre of the ellipsoidal crown. @return const Math::Vector< 3, vertex_data_t > & */
			[[nodiscard]] const Math::Vector< 3, vertex_data_t > & crownCenter () const noexcept { return m_crownCenter; }

			/** @brief Sets the three radii of the ellipsoidal crown. @param value The radii. @return void */
			void setCrownRadii (const Math::Vector< 3, vertex_data_t > & value) noexcept { m_crownRadii = value; }

			/** @brief Returns the three radii of the ellipsoidal crown. @return const Math::Vector< 3, vertex_data_t > & */
			[[nodiscard]] const Math::Vector< 3, vertex_data_t > & crownRadii () const noexcept { return m_crownRadii; }

			/** @brief Sets how far a tip can feel an attraction point. @param value The distance. @return void */
			void setInfluenceRadius (vertex_data_t value) noexcept { m_influenceRadius = std::max(value, static_cast< vertex_data_t >(1e-4)); }

			/** @brief Returns how far a tip can feel an attraction point. @return vertex_data_t */
			[[nodiscard]] vertex_data_t influenceRadius () const noexcept { return m_influenceRadius; }

			/** @brief Sets how close a tip must get for the point to be consumed. @param value The distance. @return void */
			void setKillDistance (vertex_data_t value) noexcept { m_killDistance = std::max(value, static_cast< vertex_data_t >(1e-4)); }

			/** @brief Returns how close a tip must get for the point to be consumed. @return vertex_data_t */
			[[nodiscard]] vertex_data_t killDistance () const noexcept { return m_killDistance; }

			/** @brief Sets how far a branch advances per growth step. @param value The length. @return void */
			void setSegmentLength (vertex_data_t value) noexcept { m_segmentLength = std::max(value, static_cast< vertex_data_t >(1e-4)); }

			/** @brief Returns how far a branch advances per growth step. @return vertex_data_t */
			[[nodiscard]] vertex_data_t segmentLength () const noexcept { return m_segmentLength; }

			/** @brief Sets the bare height grown before the crown is reached. @param value The height. @return void */
			void setTrunkHeight (vertex_data_t value) noexcept { m_trunkHeight = std::max(value, static_cast< vertex_data_t >(0)); }

			/** @brief Returns the bare height grown before the crown is reached. @return vertex_data_t */
			[[nodiscard]] vertex_data_t trunkHeight () const noexcept { return m_trunkHeight; }

			/** @brief Sets the direction branches drift toward, gravity or light. @param value The direction. @return void */
			void setTropism (const Math::Vector< 3, vertex_data_t > & value) noexcept { m_tropism = value; }

			/** @brief Returns the direction branches drift toward. @return const Math::Vector< 3, vertex_data_t > & */
			[[nodiscard]] const Math::Vector< 3, vertex_data_t > & tropism () const noexcept { return m_tropism; }

			/** @brief Sets how strongly the tropism pulls against the attraction points. @param value The weight. @return void */
			void setTropismWeight (vertex_data_t value) noexcept { m_tropismWeight = value; }

			/** @brief Returns how strongly the tropism pulls. @return vertex_data_t */
			[[nodiscard]] vertex_data_t tropismWeight () const noexcept { return m_tropismWeight; }

			/** @brief Sets the growth step ceiling. @param value The count. @return void */
			void setMaxIterations (uint32_t value) noexcept { m_maxIterations = value; }

			/** @brief Returns the growth step ceiling. @return uint32_t */
			[[nodiscard]] uint32_t maxIterations () const noexcept { return m_maxIterations; }

			/** @brief Sets the radius of a branch tip, from which the pipe model derives the rest. @param value The radius. @return void */
			void setTipRadius (vertex_data_t value) noexcept { m_tipRadius = std::max(value, static_cast< vertex_data_t >(1e-5)); }

			/** @brief Returns the radius of a branch tip. @return vertex_data_t */
			[[nodiscard]] vertex_data_t tipRadius () const noexcept { return m_tipRadius; }

			/** @brief Sets Murray's exponent for the pipe model; 2 is da Vinci's rule. @param value The exponent. @return void */
			void setPipeExponent (vertex_data_t value) noexcept { m_pipeExponent = std::max(value, static_cast< vertex_data_t >(1)); }

			/** @brief Returns Murray's exponent for the pipe model. @return vertex_data_t */
			[[nodiscard]] vertex_data_t pipeExponent () const noexcept { return m_pipeExponent; }

			/** @brief Sets how many leaves hang at each branch tip. @param value The count. @return void */
			void setLeavesPerTip (uint32_t value) noexcept { m_leavesPerTip = value; }

			/** @brief Returns how many leaves hang at each branch tip. @return uint32_t */
			[[nodiscard]] uint32_t leavesPerTip () const noexcept { return m_leavesPerTip; }

			/** @brief Sets the size of one leaf. @param value The size. @return void */
			void setLeafScale (vertex_data_t value) noexcept { m_leafScale = std::max(value, static_cast< vertex_data_t >(0)); }

			/** @brief Returns the size of one leaf. @return vertex_data_t */
			[[nodiscard]] vertex_data_t leafScale () const noexcept { return m_leafScale; }

			/**
			 * @brief Grows a tree inside the ellipsoidal crown described by the parameters.
			 * @param seed The generation seed.
			 * @return TreeSkeleton< vertex_data_t >
			 */
			[[nodiscard]]
			TreeSkeleton< vertex_data_t >
			grow (uint32_t seed = 0) const noexcept
			{
				Randomizer< vertex_data_t > randomizer{seed};

				return this->growFromAttractors(this->sampleCrown(randomizer), randomizer);
			}

			/**
			 * @brief Grows a tree toward an arbitrary cloud of attraction points.
			 * @note This is the general entry: the crown can be any volume at all, a scanned canopy
			 * or the inside of a mesh, as long as you can sample points in it.
			 * @param attractors A reference to the attraction points.
			 * @param randomizer A reference to the generator, for the leaf orientations.
			 * @return TreeSkeleton< vertex_data_t >
			 */
			[[nodiscard]]
			TreeSkeleton< vertex_data_t >
			growFromAttractors (const std::vector< Math::Vector< 3, vertex_data_t > > & attractors, Randomizer< vertex_data_t > & randomizer) const noexcept
			{
				TreeSkeleton< vertex_data_t > skeleton;

				if ( attractors.empty() )
				{
					return skeleton;
				}

				std::vector< Node > nodes;
				NodeGrid grid{m_influenceRadius};

				this->growTrunk(nodes, grid, attractors);

				if ( nodes.size() < 2 )
				{
					return skeleton;
				}

				this->colonize(nodes, grid, attractors);

				this->buildSkeleton(skeleton, nodes, randomizer);

				return skeleton;
			}

		private:

			/**
			 * @brief One growth node: a point, and the node it grew from.
			 */
			struct Node final
			{
				Math::Vector< 3, vertex_data_t > position;
				uint32_t parentIndex{std::numeric_limits< uint32_t >::max()};
			};

			/**
			 * @brief A uniform grid over the growth nodes, so that finding the tip closest to an
			 * attraction point does not mean walking every node.
			 * @note ⚠️ Nothing ever iterates the map itself: the growth reads it by key only, and
			 * every cell keeps its nodes in insertion order. That is what keeps a given seed
			 * reproducible — iterating an unordered_map would make the result depend on the hash
			 * table layout.
			 */
			class NodeGrid final
			{
				public:

					/**
					 * @brief Constructs a grid.
					 * @param cellSize The side of one cell, the influence radius.
					 */
					explicit
					NodeGrid (vertex_data_t cellSize) noexcept
						: m_cellSize(cellSize)
					{

					}

					/**
					 * @brief Files a node.
					 * @param position A reference to the node position.
					 * @param nodeIndex The index of the node.
					 * @return void
					 */
					void
					insert (const Math::Vector< 3, vertex_data_t > & position, uint32_t nodeIndex) noexcept
					{
						m_cells[this->key(this->cell(position))].emplace_back(nodeIndex);
					}

					/**
					 * @brief Calls a function on every node filed in the 27 cells around a point.
					 * @tparam function_t The type of the visitor.
					 * @param position A reference to the point.
					 * @param visitor The function to call with each candidate node index.
					 * @return void
					 */
					template< typename function_t >
					void
					visitNeighbourhood (const Math::Vector< 3, vertex_data_t > & position, function_t visitor) const noexcept
					{
						const auto centre = this->cell(position);

						for ( int32_t offsetX = -1; offsetX <= 1; ++offsetX )
						{
							for ( int32_t offsetY = -1; offsetY <= 1; ++offsetY )
							{
								for ( int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ )
								{
									const auto cellIt = m_cells.find(this->key({centre[0] + offsetX, centre[1] + offsetY, centre[2] + offsetZ}));

									if ( cellIt == m_cells.cend() )
									{
										continue;
									}

									for ( const auto nodeIndex : cellIt->second )
									{
										visitor(nodeIndex);
									}
								}
							}
						}
					}

				private:

					/**
					 * @brief Returns the cell coordinates of a point.
					 * @param position A reference to the point.
					 * @return std::array< int32_t, 3 >
					 */
					[[nodiscard]]
					std::array< int32_t, 3 >
					cell (const Math::Vector< 3, vertex_data_t > & position) const noexcept
					{
						return {
							static_cast< int32_t >(std::floor(position[Math::X] / m_cellSize)),
							static_cast< int32_t >(std::floor(position[Math::Y] / m_cellSize)),
							static_cast< int32_t >(std::floor(position[Math::Z] / m_cellSize))
						};
					}

					/**
					 * @brief Returns the map key of a cell.
					 * @param cell A reference to the cell coordinates.
					 * @return uint64_t
					 */
					[[nodiscard]]
					static
					uint64_t
					key (const std::array< int32_t, 3 > & cell) noexcept
					{
						const auto partX = static_cast< uint64_t >(static_cast< uint32_t >(cell[0])) * 73856093ULL;
						const auto partY = static_cast< uint64_t >(static_cast< uint32_t >(cell[1])) * 19349663ULL;
						const auto partZ = static_cast< uint64_t >(static_cast< uint32_t >(cell[2])) * 83492791ULL;

						return partX ^ partY ^ partZ;
					}

					std::unordered_map< uint64_t, std::vector< uint32_t > > m_cells;
					vertex_data_t m_cellSize;
			};

			/**
			 * @brief Samples the ellipsoidal crown with attraction points.
			 * @param randomizer A reference to the generator.
			 * @return std::vector< Math::Vector< 3, vertex_data_t > >
			 */
			[[nodiscard]]
			std::vector< Math::Vector< 3, vertex_data_t > >
			sampleCrown (Randomizer< vertex_data_t > & randomizer) const noexcept
			{
				std::vector< Math::Vector< 3, vertex_data_t > > attractors;

				if ( m_attractorCount == 0 )
				{
					return attractors;
				}

				attractors.reserve(m_attractorCount);

				constexpr auto One = static_cast< vertex_data_t >(1);

				/* Rejection sampling in the unit sphere, then scaled: it gives a uniform density,
				 * which a per-axis random radius would not. */
				const auto attempts = static_cast< size_t >(m_attractorCount) * 64;

				for ( size_t attempt = 0; attempt < attempts && attractors.size() < m_attractorCount; ++attempt )
				{
					const Math::Vector< 3, vertex_data_t > candidate{
						randomizer.value(-One, One),
						randomizer.value(-One, One),
						randomizer.value(-One, One)
					};

					if ( candidate.length() > One )
					{
						continue;
					}

					attractors.emplace_back(Math::Vector< 3, vertex_data_t >{
						m_crownCenter[Math::X] + candidate[Math::X] * m_crownRadii[Math::X],
						m_crownCenter[Math::Y] + candidate[Math::Y] * m_crownRadii[Math::Y],
						m_crownCenter[Math::Z] + candidate[Math::Z] * m_crownRadii[Math::Z]
					});
				}

				return attractors;
			}

			/**
			 * @brief Grows the bare trunk, straight up, until the crown is within reach.
			 * @param nodes A reference to the node list.
			 * @param grid A reference to the node grid.
			 * @param attractors A reference to the attraction points.
			 * @return void
			 */
			void
			growTrunk (std::vector< Node > & nodes, NodeGrid & grid, const std::vector< Math::Vector< 3, vertex_data_t > > & attractors) const noexcept
			{
				Node root;
				root.position = {0, 0, 0};

				nodes.emplace_back(root);
				grid.insert(root.position, 0);

				/* Enough steps to cross the trunk and then reach into the crown, but never enough
				 * to run away if the crown was placed out of reach. */
				const auto maxSteps = static_cast< uint32_t >(std::ceil(m_trunkHeight / m_segmentLength)) +
					static_cast< uint32_t >(std::ceil((m_crownCenter[Math::Y] + m_crownRadii[Math::Y]) / m_segmentLength)) + 2;

				for ( uint32_t step = 0; step < maxSteps; ++step )
				{
					const auto & tip = nodes.back();

					if ( tip.position[Math::Y] >= m_trunkHeight && this->isWithinInfluence(tip.position, attractors) )
					{
						return;
					}

					Node grown;
					grown.position = tip.position + Math::Vector< 3, vertex_data_t >{0, m_segmentLength, 0};
					grown.parentIndex = static_cast< uint32_t >(nodes.size() - 1);

					grid.insert(grown.position, static_cast< uint32_t >(nodes.size()));
					nodes.emplace_back(grown);
				}
			}

			/**
			 * @brief Returns whether any attraction point is within reach of a point.
			 * @param position A reference to the point.
			 * @param attractors A reference to the attraction points.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isWithinInfluence (const Math::Vector< 3, vertex_data_t > & position, const std::vector< Math::Vector< 3, vertex_data_t > > & attractors) const noexcept
			{
				const auto squaredReach = m_influenceRadius * m_influenceRadius;

				return std::ranges::any_of(attractors, [&] (const auto & attractor) {
					return (attractor - position).lengthSquared() <= squaredReach;
				});
			}

			/**
			 * @brief Runs the colonization loop until the crown is consumed.
			 * @param nodes A reference to the node list.
			 * @param grid A reference to the node grid.
			 * @param attractors A reference to the attraction points.
			 * @return void
			 */
			void
			colonize (std::vector< Node > & nodes, NodeGrid & grid, const std::vector< Math::Vector< 3, vertex_data_t > > & attractors) const noexcept
			{
				const auto squaredReach = m_influenceRadius * m_influenceRadius;
				const auto squaredKill = m_killDistance * m_killDistance;

				std::vector< bool > consumed(attractors.size(), false);
				std::vector< Math::Vector< 3, vertex_data_t > > pull;
				std::vector< uint32_t > pullCount;

				for ( uint32_t iteration = 0; iteration < m_maxIterations; ++iteration )
				{
					if ( nodes.size() >= MaxNodes )
					{
						return;
					}

					pull.assign(nodes.size(), Math::Vector< 3, vertex_data_t >{0, 0, 0});
					pullCount.assign(nodes.size(), 0);

					bool anyPull = false;

					for ( size_t attractorIndex = 0; attractorIndex < attractors.size(); ++attractorIndex )
					{
						if ( consumed[attractorIndex] )
						{
							continue;
						}

						const auto & attractor = attractors[attractorIndex];

						auto bestDistance = squaredReach;
						auto bestNode = std::numeric_limits< uint32_t >::max();

						grid.visitNeighbourhood(attractor, [&] (uint32_t nodeIndex) {
							const auto distance = (attractor - nodes[nodeIndex].position).lengthSquared();

							/* A strict comparison keeps the FIRST node at an equal distance, and
							 * the node order is deterministic, so ties break the same way twice. */
							if ( distance < bestDistance )
							{
								bestDistance = distance;
								bestNode = nodeIndex;
							}
						});

						if ( bestNode == std::numeric_limits< uint32_t >::max() )
						{
							continue;
						}

						const auto direction = attractor - nodes[bestNode].position;
						const auto length = direction.length();

						if ( length < static_cast< vertex_data_t >(1e-6) )
						{
							continue;
						}

						pull[bestNode] += direction / length;
						++pullCount[bestNode];

						anyPull = true;
					}

					if ( !anyPull )
					{
						return;
					}

					const auto previousCount = nodes.size();

					for ( size_t nodeIndex = 0; nodeIndex < previousCount; ++nodeIndex )
					{
						if ( pullCount[nodeIndex] == 0 || nodes.size() >= MaxNodes )
						{
							continue;
						}

						auto direction = pull[nodeIndex];

						if ( m_tropismWeight != 0 )
						{
							direction += m_tropism * m_tropismWeight * static_cast< vertex_data_t >(pullCount[nodeIndex]);
						}

						const auto length = direction.length();

						if ( length < static_cast< vertex_data_t >(1e-6) )
						{
							continue;
						}

						Node grown;
						grown.position = nodes[nodeIndex].position + (direction / length) * m_segmentLength;
						grown.parentIndex = static_cast< uint32_t >(nodeIndex);

						grid.insert(grown.position, static_cast< uint32_t >(nodes.size()));
						nodes.emplace_back(grown);
					}

					if ( nodes.size() == previousCount )
					{
						return;
					}

					/* Consume the points the new tips have reached. */
					for ( size_t attractorIndex = 0; attractorIndex < attractors.size(); ++attractorIndex )
					{
						if ( consumed[attractorIndex] )
						{
							continue;
						}

						for ( size_t nodeIndex = previousCount; nodeIndex < nodes.size(); ++nodeIndex )
						{
							if ( (attractors[attractorIndex] - nodes[nodeIndex].position).lengthSquared() <= squaredKill )
							{
								consumed[attractorIndex] = true;

								break;
							}
						}
					}
				}
			}

			/**
			 * @brief Turns the grown nodes into a skeleton, and gives it its radii and its leaves.
			 * @param skeleton A reference to the skeleton to fill.
			 * @param nodes A reference to the node list.
			 * @param randomizer A reference to the generator.
			 * @return void
			 */
			void
			buildSkeleton (TreeSkeleton< vertex_data_t > & skeleton, const std::vector< Node > & nodes, Randomizer< vertex_data_t > & randomizer) const noexcept
			{
				const auto nodeCount = nodes.size();

				/* Children of every node, so that the straightest one can be told from the others. */
				std::vector< std::vector< uint32_t > > children(nodeCount);

				for ( size_t index = 1; index < nodeCount; ++index )
				{
					children[nodes[index].parentIndex].emplace_back(static_cast< uint32_t >(index));
				}

				constexpr auto NoSegment = TreeSegment< vertex_data_t >::NoParent;

				std::vector< uint32_t > segmentOfNode(nodeCount, NoSegment);
				std::vector< uint32_t > orderOfNode(nodeCount, 0);
				std::vector< uint32_t > branchOfNode(nodeCount, 0);
				std::vector< vertex_data_t > arcOfNode(nodeCount, 0);

				uint32_t nextBranchIndex = 1;

				skeleton.reserve(nodeCount, static_cast< size_t >(m_leavesPerTip) * nodeCount / 8);

				/* A node index is always greater than its parent's, so one forward sweep both
				 * builds the segments in the order the skeleton requires and propagates the
				 * branch bookkeeping. */
				for ( size_t index = 1; index < nodeCount; ++index )
				{
					const auto & node = nodes[index];
					const auto parentIndex = node.parentIndex;

					const auto direction = node.position - nodes[parentIndex].position;
					const auto length = direction.length();

					if ( length < static_cast< vertex_data_t >(1e-6) )
					{
						continue;
					}

					/* The straightest child carries the branch on; the others start a new one. */
					const auto continues = this->isStraightestChild(nodes, children, parentIndex, static_cast< uint32_t >(index));

					orderOfNode[index] = continues ? orderOfNode[parentIndex] : orderOfNode[parentIndex] + 1;
					branchOfNode[index] = continues ? branchOfNode[parentIndex] : nextBranchIndex++;
					arcOfNode[index] = continues ? arcOfNode[parentIndex] + length : 0;

					TreeSegment< vertex_data_t > segment{
						makeTreeFrame(nodes[parentIndex].position, direction),
						length,
						m_tipRadius,
						m_tipRadius,
						arcOfNode[index] - length,
						segmentOfNode[parentIndex],
						branchOfNode[index],
						orderOfNode[index]
					};

					segment.setBranchTip(children[index].empty());

					segmentOfNode[index] = skeleton.addSegment(segment);
				}

				skeleton.computeRadiiFromPipeModel(m_tipRadius, m_pipeExponent);

				this->placeLeaves(skeleton, nodes, children, segmentOfNode, randomizer);
			}

			/**
			 * @brief Returns whether a child is the one that continues its parent's branch.
			 * @param nodes A reference to the node list.
			 * @param children A reference to the child lists.
			 * @param parentIndex The index of the parent node.
			 * @param childIndex The index of the child node.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isStraightestChild (const std::vector< Node > & nodes, const std::vector< std::vector< uint32_t > > & children, uint32_t parentIndex, uint32_t childIndex) const noexcept
			{
				const auto & siblings = children[parentIndex];

				if ( siblings.size() < 2 )
				{
					return true;
				}

				/* The parent's own direction, or straight up at the root. */
				const auto parentDirection = nodes[parentIndex].parentIndex == std::numeric_limits< uint32_t >::max() ?
					Math::Vector< 3, vertex_data_t >::positiveY() :
					(nodes[parentIndex].position - nodes[nodes[parentIndex].parentIndex].position).normalized();

				auto bestAlignment = std::numeric_limits< vertex_data_t >::lowest();
				auto bestSibling = siblings.front();

				for ( const auto siblingIndex : siblings )
				{
					const auto siblingDirection = (nodes[siblingIndex].position - nodes[parentIndex].position).normalized();

					const auto alignment = Math::Vector< 3, vertex_data_t >::dotProduct(parentDirection, siblingDirection);

					if ( alignment > bestAlignment )
					{
						bestAlignment = alignment;
						bestSibling = siblingIndex;
					}
				}

				return bestSibling == childIndex;
			}

			/**
			 * @brief Hangs the leaves at the branch tips.
			 * @param skeleton A reference to the skeleton.
			 * @param nodes A reference to the node list.
			 * @param children A reference to the child lists.
			 * @param segmentOfNode A reference to the segment index of every node.
			 * @param randomizer A reference to the generator.
			 * @return void
			 */
			void
			placeLeaves (TreeSkeleton< vertex_data_t > & skeleton, const std::vector< Node > & nodes, const std::vector< std::vector< uint32_t > > & children, const std::vector< uint32_t > & segmentOfNode, Randomizer< vertex_data_t > & randomizer) const noexcept
			{
				if ( m_leavesPerTip == 0 || m_leafScale <= 0 )
				{
					return;
				}

				constexpr auto NoSegment = TreeSegment< vertex_data_t >::NoParent;
				constexpr auto One = static_cast< vertex_data_t >(1);

				for ( size_t index = 1; index < nodes.size(); ++index )
				{
					if ( !children[index].empty() || segmentOfNode[index] == NoSegment )
					{
						continue;
					}

					for ( uint32_t leaf = 0; leaf < m_leavesPerTip; ++leaf )
					{
						const Math::Vector< 3, vertex_data_t > direction{
							randomizer.value(-One, One),
							randomizer.value(-One, One),
							randomizer.value(-One, One)
						};

						if ( direction.length() < static_cast< vertex_data_t >(1e-4) )
						{
							continue;
						}

						const auto scale = m_leafScale * (One + randomizer.value(static_cast< vertex_data_t >(-0.2), static_cast< vertex_data_t >(0.2)));

						skeleton.addLeaf({makeTreeFrame(nodes[index].position, direction), scale, segmentOfNode[index]});
					}
				}
			}

			Math::Vector< 3, vertex_data_t > m_crownCenter{0, 6, 0};
			Math::Vector< 3, vertex_data_t > m_crownRadii{3, 3, 3};
			Math::Vector< 3, vertex_data_t > m_tropism{0, 1, 0};
			vertex_data_t m_influenceRadius{2};
			vertex_data_t m_killDistance{static_cast< vertex_data_t >(0.5)};
			vertex_data_t m_segmentLength{static_cast< vertex_data_t >(0.25)};
			vertex_data_t m_trunkHeight{3};
			vertex_data_t m_tropismWeight{static_cast< vertex_data_t >(0.25)};
			vertex_data_t m_tipRadius{static_cast< vertex_data_t >(0.02)};
			vertex_data_t m_pipeExponent{static_cast< vertex_data_t >(2.49)};
			vertex_data_t m_leafScale{static_cast< vertex_data_t >(0.15)};
			uint32_t m_attractorCount{800};
			uint32_t m_maxIterations{300};
			uint32_t m_leavesPerTip{3};
	};
}
