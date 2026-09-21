/*
 * src/VertexFactory/TreeSkinner.hpp
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
#include <numbers>
#include <type_traits>
#include <unordered_map>
#include <vector>

/* Local inclusions for usages. */
#include "Math/Vector.hpp"
#include "Shape.hpp"
#include "ShapeBuilder.hpp"
#include "ShapeProcessor.hpp"
#include "TreeMesh.hpp"
#include "TreeSkinningOptions.hpp"
#include "TreeSkeleton.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief Turns a tree skeleton into a mesh: branches as generalized cylinders, leaves as cards.
	 * @note The branches of one skeleton branch share their rings, so a branch is ONE continuous
	 * tube with continuous texture coordinates, not a pile of capped cylinders. That pile was the
	 * dead end of the old TreeGenerator stub.
	 * @note The result carries two groups, `TreeMesh::BarkGroup` then `TreeMesh::LeafGroup`, which
	 * the engine turns into two sub-geometries and therefore two materials.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 * @tparam index_data_t The precision type of index data. Default uint32_t.
	 */
	template< typename vertex_data_t = float, typename index_data_t = uint32_t >
	requires (std::is_floating_point_v< vertex_data_t > && std::is_unsigned_v< index_data_t >)
	class TreeSkinner final
	{
		public:

			/**
			 * @brief Constructs a skinner.
			 * @param options A reference to the skinning options. Default the finest level.
			 */
			explicit
			TreeSkinner (const TreeSkinningOptions< vertex_data_t > & options = {}) noexcept
				: m_options(options)
			{

			}

			/**
			 * @brief Gives mutable access to the skinning options.
			 * @return TreeSkinningOptions< vertex_data_t > &
			 */
			[[nodiscard]]
			TreeSkinningOptions< vertex_data_t > &
			options () noexcept
			{
				return m_options;
			}

			/**
			 * @brief Gives access to the skinning options.
			 * @return const TreeSkinningOptions< vertex_data_t > &
			 */
			[[nodiscard]]
			const TreeSkinningOptions< vertex_data_t > &
			options () const noexcept
			{
				return m_options;
			}

			/**
			 * @brief Skins a skeleton into a two-group mesh.
			 * @param skeleton A reference to the skeleton.
			 * @return Shape< vertex_data_t, index_data_t >
			 */
			[[nodiscard]]
			Shape< vertex_data_t, index_data_t >
			skin (const TreeSkeleton< vertex_data_t > & skeleton) const noexcept
			{
				Shape< vertex_data_t, index_data_t > shape;

				if ( skeleton.empty() )
				{
					return shape;
				}

				/* ⚠️ Data economy OFF, and it stays off even though addVertex() stopped being quadratic on
				 * 2026-09-22. MEASURED both ways on this generator: economy OFF plus the batch pass builds
				 * the aspen chain in 122 ms and the conifer in 124 ms; economy ON takes 208 ms and 263 ms.
				 * A canopy is the opposite of a sphere: almost every leaf-card vertex is UNIQUE, so the
				 * in-build hash pays an insertion per corner and merges nothing, while the batch pass walks
				 * a contiguous array once. On a sphere, where 83 %% of the corners merge, it is the other
				 * way round (21.6 ms against 25.5 ms). The rule is the merge RATIO, not the vertex count. */
				ShapeBuilderOptions< vertex_data_t > builderOptions{true, true, true, false, false};
				builderOptions.enableDataEconomy(false);

				ShapeBuilder< vertex_data_t, index_data_t > builder{shape, builderOptions};

				builder.beginConstruction(ConstructionMode::Triangles);

				const auto branches = this->buildBranches(skeleton);
				const auto occlusion = OcclusionField{skeleton, m_options.occlusionCellSize()};

				const auto baseHeight = skeleton.boundingBox().isValid() ? skeleton.boundingBox().minimum()[Math::Y] : static_cast< vertex_data_t >(0);
				const auto treeHeight = std::max(skeleton.height(), static_cast< vertex_data_t >(1e-3));

				Context context{skeleton, occlusion, baseHeight, treeHeight};

				/* Group 0: the bark. */
				for ( const auto & branch : branches )
				{
					this->emitBranch(builder, context, branch);
				}

				/* Group 1: the leaves. It is declared even when empty, so every level of detail
				 * shows the engine the same two sub-geometries.
				 * ⚠️ A leaf is NOT dropped when the twig it hangs on was pruned away. Written down it
				 * looks wrong — the leaf floats where its twig used to be — and it is what every
				 * vegetation level of detail does on purpose: at the distance where a two-pixel twig is
				 * dropped, the canopy IS the silhouette. Dropping those leaves instead cost the first
				 * level of detail 100 % of its foliage, measured. */
				builder.newGroup();

				this->emitLeaves(builder, context);

				builder.endConstruction();

				if ( m_options.vertexMergeEnabled() && !shape.empty() )
				{
					ShapeProcessor< vertex_data_t, index_data_t > processor{shape};

					processor.deduplicateVertices();
				}

				return shape;
			}

			/**
			 * @brief Builds the crossed-quads card that stands in for the tree in the distance.
			 * @note Geometry only. What the card shows is an atlas baked from the real mesh, which
			 * needs a renderer and therefore belongs to the engine.
			 * @param skeleton A reference to the skeleton.
			 * @param quadCount How many quads cross each other. Default 3.
			 * @return Shape< vertex_data_t, index_data_t >
			 */
			[[nodiscard]]
			Shape< vertex_data_t, index_data_t >
			skinImposter (const TreeSkeleton< vertex_data_t > & skeleton, uint32_t quadCount = 3) const noexcept
			{
				Shape< vertex_data_t, index_data_t > shape;

				const auto & box = skeleton.boundingBox();

				if ( skeleton.empty() || !box.isValid() || quadCount == 0 )
				{
					return shape;
				}

				ShapeBuilderOptions< vertex_data_t > builderOptions{true, true, true, false, false};
				builderOptions.enableDataEconomy(false);

				ShapeBuilder< vertex_data_t, index_data_t > builder{shape, builderOptions};

				builder.beginConstruction(ConstructionMode::Triangles);

				const auto minimum = box.minimum();
				const auto maximum = box.maximum();

				const auto halfWidth = std::max(maximum[Math::X] - minimum[Math::X], maximum[Math::Z] - minimum[Math::Z]) / static_cast< vertex_data_t >(2);
				const auto centreX = (maximum[Math::X] + minimum[Math::X]) / static_cast< vertex_data_t >(2);
				const auto centreZ = (maximum[Math::Z] + minimum[Math::Z]) / static_cast< vertex_data_t >(2);
				const auto treeHeight = std::max(maximum[Math::Y] - minimum[Math::Y], static_cast< vertex_data_t >(1e-3));

				for ( uint32_t quad = 0; quad < quadCount; ++quad )
				{
					const auto angle = std::numbers::pi_v< vertex_data_t > * static_cast< vertex_data_t >(quad) / static_cast< vertex_data_t >(quadCount);

					const Math::Vector< 3, vertex_data_t > across{std::cos(angle), 0, std::sin(angle)};
					const Math::Vector< 3, vertex_data_t > normal{-std::sin(angle), 0, std::cos(angle)};

					const Math::Vector< 3, vertex_data_t > centreBottom{centreX, minimum[Math::Y], centreZ};

					std::array< SkinVertex, 4 > corners{};

					for ( uint32_t corner = 0; corner < 4; ++corner )
					{
						const auto right = corner == 1 || corner == 2;
						const auto top = corner >= 2;

						corners[corner].position = centreBottom
							+ across * (right ? halfWidth : -halfWidth)
							+ Math::Vector< 3, vertex_data_t >{0, top ? treeHeight : static_cast< vertex_data_t >(0), 0};

						corners[corner].normal = normal;
						corners[corner].textureU = right ? static_cast< vertex_data_t >(1) : static_cast< vertex_data_t >(0);
						corners[corner].textureV = top ? static_cast< vertex_data_t >(1) : static_cast< vertex_data_t >(0);

						/* The card still sways: only the trunk channel makes sense on it. */
						corners[corner].color = {
							m_options.windChannelsEnabled() ? std::pow(top ? static_cast< vertex_data_t >(1) : static_cast< vertex_data_t >(0), m_options.trunkBendExponent()) : static_cast< vertex_data_t >(0),
							0,
							0,
							1
						};
					}

					this->emitTriangle(builder, corners[0], corners[1], corners[2]);
					this->emitTriangle(builder, corners[0], corners[2], corners[3]);
				}

				builder.endConstruction();

				return shape;
			}

		private:

			/**
			 * @brief One vertex on its way into the builder.
			 */
			struct SkinVertex final
			{
				Math::Vector< 3, vertex_data_t > position;
				Math::Vector< 3, vertex_data_t > normal;
				Math::Vector< 4, vertex_data_t > color{0, 0, 0, 1};
				vertex_data_t textureU{0};
				vertex_data_t textureV{0};
			};

			/**
			 * @brief A chain of segments forming one continuous tube.
			 */
			struct Branch final
			{
				std::vector< uint32_t > segments;
			};

			/**
			 * @brief How crowded the canopy is around a point, for the baked occlusion channel.
			 * @note ⚠️ This is a DENSITY estimate, not ray-traced ambient occlusion: it counts the
			 * leaves and branch nodes in the cells around a point. It captures the one thing that
			 * matters visually — the inside of a canopy is darker than its rim — for the cost of a
			 * hash lookup. Real occlusion needs rays, and belongs to a bake, not to a generator.
			 */
			class OcclusionField final
			{
				public:

					/**
					 * @brief Builds the field from a skeleton.
					 * @param skeleton A reference to the skeleton.
					 * @param cellSize The side of one cell.
					 */
					OcclusionField (const TreeSkeleton< vertex_data_t > & skeleton, vertex_data_t cellSize) noexcept
						: m_cellSize(cellSize)
					{
						for ( const auto & leaf : skeleton.leaves() )
						{
							++m_counts[this->key(leaf.frame().position())];
						}

						for ( const auto & segment : skeleton.segments() )
						{
							++m_counts[this->key(segment.startPoint())];
						}

						/* Normalising on the densest neighbourhood keeps the channel in [0, 1]
						 * whatever the species. A max is order-independent, so iterating the map
						 * here does NOT make the result depend on the hash table layout. */
						for ( const auto & entry : m_counts )
						{
							m_peak = std::max(m_peak, entry.second);
						}
					}

					/**
					 * @brief Returns how crowded a point is, in [0, 1].
					 * @param position A reference to the point.
					 * @return vertex_data_t
					 */
					[[nodiscard]]
					vertex_data_t
					density (const Math::Vector< 3, vertex_data_t > & position) const noexcept
					{
						if ( m_peak == 0 )
						{
							return 0;
						}

						const auto countIt = m_counts.find(this->key(position));

						if ( countIt == m_counts.cend() )
						{
							return 0;
						}

						return static_cast< vertex_data_t >(countIt->second) / static_cast< vertex_data_t >(m_peak);
					}

				private:

					/**
					 * @brief Returns the cell key of a point.
					 * @param position A reference to the point.
					 * @return uint64_t
					 */
					[[nodiscard]]
					uint64_t
					key (const Math::Vector< 3, vertex_data_t > & position) const noexcept
					{
						const auto cellX = static_cast< uint32_t >(static_cast< int32_t >(std::floor(position[Math::X] / m_cellSize)));
						const auto cellY = static_cast< uint32_t >(static_cast< int32_t >(std::floor(position[Math::Y] / m_cellSize)));
						const auto cellZ = static_cast< uint32_t >(static_cast< int32_t >(std::floor(position[Math::Z] / m_cellSize)));

						return (static_cast< uint64_t >(cellX) * 73856093ULL) ^ (static_cast< uint64_t >(cellY) * 19349663ULL) ^ (static_cast< uint64_t >(cellZ) * 83492791ULL);
					}

					std::unordered_map< uint64_t, uint32_t > m_counts;
					vertex_data_t m_cellSize;
					uint32_t m_peak{0};
			};

			/**
			 * @brief What every emission step needs to know about the tree as a whole.
			 */
			struct Context final
			{
				const TreeSkeleton< vertex_data_t > & skeleton;
				const OcclusionField & occlusion;
				vertex_data_t baseHeight;
				vertex_data_t treeHeight;
			};

			/**
			 * @brief Groups the segments of a skeleton into continuous branches.
			 * @param skeleton A reference to the skeleton.
			 * @return std::vector< Branch >
			 */
			[[nodiscard]]
			std::vector< Branch >
			buildBranches (const TreeSkeleton< vertex_data_t > & skeleton) const noexcept
			{
				std::vector< Branch > branches(skeleton.branchCount());

				for ( size_t index = 0; index < skeleton.segmentCount(); ++index )
				{
					const auto branchIndex = skeleton.segments()[index].branchIndex();

					if ( branchIndex < branches.size() )
					{
						branches[branchIndex].segments.emplace_back(static_cast< uint32_t >(index));
					}
				}

				/* A grower is free to interleave its branches, so the chain is ordered here rather
				 * than assumed. Sorting on the arc length is what makes a branch a tube. */
				for ( auto & branch : branches )
				{
					std::ranges::sort(branch.segments, [&skeleton] (uint32_t left, uint32_t right) {
						return skeleton.segments()[left].arcLength() < skeleton.segments()[right].arcLength();
					});
				}

				return branches;
			}

			/**
			 * @brief Returns how many vertices a ring of a given radius gets.
			 * @param radius The radius of the branch.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			radialSegments (vertex_data_t radius) const noexcept
			{
				const auto circumference = static_cast< vertex_data_t >(2) * std::numbers::pi_v< vertex_data_t > * radius;

				const auto wanted = static_cast< uint32_t >(std::lround(circumference / m_options.targetEdgeLength()));

				return std::clamp(wanted, m_options.radialSegmentsMin(), m_options.radialSegmentsMax());
			}

			/**
			 * @brief Emits one branch as a continuous tube closed by an apex.
			 * @param builder A reference to the shape builder.
			 * @param context A reference to the tree context.
			 * @param branch A reference to the branch.
			 * @return void
			 */
			void
			emitBranch (ShapeBuilder< vertex_data_t, index_data_t > & builder, const Context & context, const Branch & branch) const noexcept
			{
				if ( branch.segments.empty() )
				{
					return;
				}

				const auto & segments = context.skeleton.segments();
				const auto & first = segments[branch.segments.front()];

				/* A branch thinner than the threshold disappears whole: cutting one in the middle
				 * would leave an open tube. */
				if ( first.startRadius() < m_options.minimumRadius() )
				{
					return;
				}

				const auto radial = this->radialSegments(first.startRadius());

				/* The ring stations: the start of every segment, plus the end of the last one. */
				std::vector< Math::Vector< 3, vertex_data_t > > positions;
				std::vector< Math::Vector< 3, vertex_data_t > > tangents;
				std::vector< vertex_data_t > radii;
				std::vector< vertex_data_t > arcs;

				positions.reserve(branch.segments.size() + 1);
				tangents.reserve(branch.segments.size() + 1);
				radii.reserve(branch.segments.size() + 1);
				arcs.reserve(branch.segments.size() + 1);

				const auto stride = static_cast< size_t >(m_options.axialStride());

				for ( size_t station = 0; station < branch.segments.size(); station += stride )
				{
					const auto & segment = segments[branch.segments[station]];

					positions.emplace_back(segment.startPoint());
					tangents.emplace_back(segment.axis());
					radii.emplace_back(segment.startRadius());
					arcs.emplace_back(segment.arcLength());
				}

				{
					const auto & last = segments[branch.segments.back()];

					positions.emplace_back(last.endPoint());
					tangents.emplace_back(last.axis());
					radii.emplace_back(last.endRadius());
					arcs.emplace_back(last.arcLength() + last.length());
				}

				/* The branch swells where it leaves its parent. This is the cheap junction: the
				 * first rings sit on the parent AXIS and are simply swallowed by the parent tube,
				 * with a collar faking the swelling. The expensive version stitches the two
				 * surfaces into a real bifurcation; it is not done, because the cheap one is
				 * invisible under bark at any distance a tree is seen from, and the stitch would
				 * have to be redone for every level of detail. */
				if ( !first.isRoot() && m_options.collarScale() > 1 )
				{
					radii.front() *= m_options.collarScale();
				}

				const auto references = this->buildRotationMinimizingFrames(positions, tangents);

				const auto branchLength = arcs.back() > 0 ? arcs.back() : static_cast< vertex_data_t >(1);
				const auto isTrunk = first.order() == 0;

				/* The rings, then the quads between them. */
				std::vector< std::vector< SkinVertex > > rings;
				rings.reserve(positions.size());

				for ( size_t station = 0; station < positions.size(); ++station )
				{
					rings.emplace_back(this->buildRing(context, positions[station], tangents[station], references[station], radii[station], radial, arcs[station], branchLength, isTrunk));
				}

				for ( size_t station = 0; station + 1 < rings.size(); ++station )
				{
					const auto & lower = rings[station];
					const auto & upper = rings[station + 1];

					for ( uint32_t side = 0; side < radial; ++side )
					{
						const auto next = side + 1;

						this->emitTriangle(builder, lower[side], lower[next], upper[next]);
						this->emitTriangle(builder, lower[side], upper[next], upper[side]);
					}
				}

				/* Close on a single apex, never a cap disk: a disk on a branch tip is a visible
				 * flat lid, and it doubles the vertices of the thinnest geometry of the tree. */
				this->emitApex(builder, context, rings.back(), positions.back(), tangents.back(), radii.back(), arcs.back(), branchLength, isTrunk);
			}

			/**
			 * @brief Returns a reference vector per station that never spins around the branch.
			 * @note Double reflection, Wang, Jüttler, Zheng & Liu, *Computation of rotation
			 * minimizing frames*, ACM TOG 27(1), 2008. The segment frames CANNOT be used for this:
			 * the space-colonization grower builds each one independently from a world axis, so
			 * its spin flips from one segment to the next and the tube would corkscrew.
			 * @param positions A reference to the station positions.
			 * @param tangents A reference to the station tangents.
			 * @return std::vector< Math::Vector< 3, vertex_data_t > >
			 */
			[[nodiscard]]
			std::vector< Math::Vector< 3, vertex_data_t > >
			buildRotationMinimizingFrames (const std::vector< Math::Vector< 3, vertex_data_t > > & positions, const std::vector< Math::Vector< 3, vertex_data_t > > & tangents) const noexcept
			{
				std::vector< Math::Vector< 3, vertex_data_t > > references;

				references.reserve(positions.size());

				if ( positions.empty() )
				{
					return references;
				}

				const auto helper = std::abs(tangents.front()[Math::Y]) > static_cast< vertex_data_t >(0.9) ?
					Math::Vector< 3, vertex_data_t >::positiveX() :
					Math::Vector< 3, vertex_data_t >::positiveY();

				references.emplace_back(Math::Vector< 3, vertex_data_t >::crossProduct(tangents.front(), helper).normalized());

				for ( size_t station = 0; station + 1 < positions.size(); ++station )
				{
					const auto step = positions[station + 1] - positions[station];
					const auto stepLength = step.lengthSquared();

					auto reflected = references[station];
					auto reflectedTangent = tangents[station];

					if ( stepLength > static_cast< vertex_data_t >(1e-12) )
					{
						const auto factor = static_cast< vertex_data_t >(2) / stepLength;

						reflected = reflected - step * (factor * Math::Vector< 3, vertex_data_t >::dotProduct(step, reflected));
						reflectedTangent = reflectedTangent - step * (factor * Math::Vector< 3, vertex_data_t >::dotProduct(step, reflectedTangent));
					}

					const auto secondStep = tangents[station + 1] - reflectedTangent;
					const auto secondLength = secondStep.lengthSquared();

					if ( secondLength > static_cast< vertex_data_t >(1e-12) )
					{
						reflected = reflected - secondStep * (static_cast< vertex_data_t >(2) / secondLength * Math::Vector< 3, vertex_data_t >::dotProduct(secondStep, reflected));
					}

					references.emplace_back(reflected.normalized());
				}

				return references;
			}

			/**
			 * @brief Builds one ring of vertices around a station.
			 * @param context A reference to the tree context.
			 * @param position A reference to the centre of the ring.
			 * @param tangent A reference to the branch direction there.
			 * @param reference A reference to the rotation minimizing reference vector.
			 * @param radius The radius of the ring.
			 * @param radial How many vertices the ring holds, the seam being duplicated.
			 * @param arc The distance from the start of the branch.
			 * @param branchLength The total length of the branch.
			 * @param isTrunk Whether the branch is the trunk.
			 * @return std::vector< SkinVertex >
			 */
			[[nodiscard]]
			std::vector< SkinVertex >
			buildRing (const Context & context, const Math::Vector< 3, vertex_data_t > & position, const Math::Vector< 3, vertex_data_t > & tangent, const Math::Vector< 3, vertex_data_t > & reference, vertex_data_t radius, uint32_t radial, vertex_data_t arc, vertex_data_t branchLength, bool isTrunk) const noexcept
			{
				std::vector< SkinVertex > ring;

				/* radial + 1 vertices: the seam is emitted twice, with u = 0 and u = 1, because a
				 * single vertex cannot carry two texture coordinates. */
				ring.reserve(radial + 1);

				const auto binormal = Math::Vector< 3, vertex_data_t >::crossProduct(tangent, reference);

				const auto color = this->barkColor(context, position, arc, branchLength, isTrunk);

				const auto textureV = arc / m_options.barkTextureLength();

				for ( uint32_t side = 0; side <= radial; ++side )
				{
					const auto angle = static_cast< vertex_data_t >(2) * std::numbers::pi_v< vertex_data_t > * static_cast< vertex_data_t >(side) / static_cast< vertex_data_t >(radial);

					const auto outward = reference * std::cos(angle) + binormal * std::sin(angle);

					SkinVertex vertex;
					vertex.position = position + outward * radius;
					vertex.normal = outward;
					vertex.textureU = m_options.barkTextureWraps() * static_cast< vertex_data_t >(side) / static_cast< vertex_data_t >(radial);
					vertex.textureV = textureV;
					vertex.color = color;

					ring.emplace_back(vertex);
				}

				return ring;
			}

			/**
			 * @brief Closes a branch on a single apex vertex.
			 * @param builder A reference to the shape builder.
			 * @param context A reference to the tree context.
			 * @param ring A reference to the last ring.
			 * @param position A reference to the centre of the last ring.
			 * @param tangent A reference to the branch direction there.
			 * @param radius The radius of the last ring.
			 * @param arc The distance from the start of the branch.
			 * @param branchLength The total length of the branch.
			 * @param isTrunk Whether the branch is the trunk.
			 * @return void
			 */
			void
			emitApex (ShapeBuilder< vertex_data_t, index_data_t > & builder, const Context & context, const std::vector< SkinVertex > & ring, const Math::Vector< 3, vertex_data_t > & position, const Math::Vector< 3, vertex_data_t > & tangent, vertex_data_t radius, vertex_data_t arc, vertex_data_t branchLength, bool isTrunk) const noexcept
			{
				if ( ring.size() < 2 )
				{
					return;
				}

				SkinVertex apex;
				apex.position = position + tangent * radius;
				apex.normal = tangent;
				apex.textureV = (arc + radius) / m_options.barkTextureLength();
				apex.color = this->barkColor(context, apex.position, arc + radius, branchLength, isTrunk);

				for ( size_t side = 0; side + 1 < ring.size(); ++side )
				{
					auto tip = apex;
					tip.textureU = (ring[side].textureU + ring[side + 1].textureU) / static_cast< vertex_data_t >(2);

					this->emitTriangle(builder, ring[side], ring[side + 1], tip);
				}
			}

			/**
			 * @brief Returns the vertex channels of a point of bark.
			 * @param context A reference to the tree context.
			 * @param position A reference to the point.
			 * @param arc The distance from the start of the branch.
			 * @param branchLength The total length of the branch.
			 * @param isTrunk Whether the branch is the trunk.
			 * @return Math::Vector< 4, vertex_data_t >
			 */
			[[nodiscard]]
			Math::Vector< 4, vertex_data_t >
			barkColor (const Context & context, const Math::Vector< 3, vertex_data_t > & position, vertex_data_t arc, vertex_data_t branchLength, bool isTrunk) const noexcept
			{
				if ( !m_options.windChannelsEnabled() )
				{
					return {0, 0, 0, 1};
				}

				const auto heightRatio = std::clamp((position[Math::Y] - context.baseHeight) / context.treeHeight, static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(1));

				/* R: how much the whole tree carries this point. The exponent keeps the foot of the
				 * trunk still while the crown swings. */
				const auto trunkBend = std::pow(heightRatio, m_options.trunkBendExponent());

				/* G: how much the BRANCH carries it. A trunk has no branch bending of its own, or
				 * it would sway twice. */
				const auto branchBend = isTrunk ? static_cast< vertex_data_t >(0) : std::clamp(arc / branchLength, static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(1));

				return {trunkBend, branchBend, 0, this->ambientOcclusion(context, position)};
			}

			/**
			 * @brief Returns the baked occlusion of a point.
			 * @param context A reference to the tree context.
			 * @param position A reference to the point.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			ambientOcclusion (const Context & context, const Math::Vector< 3, vertex_data_t > & position) const noexcept
			{
				if ( m_options.ambientOcclusionStrength() <= 0 )
				{
					return 1;
				}

				return static_cast< vertex_data_t >(1) - m_options.ambientOcclusionStrength() * context.occlusion.density(position);
			}

			/**
			 * @brief Emits every leaf card, in the group that is already open.
			 * @param builder A reference to the shape builder.
			 * @param context A reference to the tree context.
			 * @return void
			 */
			void
			emitLeaves (ShapeBuilder< vertex_data_t, index_data_t > & builder, const Context & context) const noexcept
			{
				if ( m_options.leafCardMode() == TreeLeafCardMode::None || m_options.leafFraction() <= 0 )
				{
					return;
				}

				const auto fraction = m_options.leafFraction();

				/* Enlarging the survivors by 1/sqrt(fraction) keeps the canopy roughly as opaque
				 * as it was: halve the cards, each covers twice the area. */
				const auto scaleCompensation = static_cast< vertex_data_t >(1) / std::sqrt(std::max(fraction, static_cast< vertex_data_t >(1e-3)));

				vertex_data_t accumulator = 0;
				uint32_t leafIndex = 0;

				for ( const auto & leaf : context.skeleton.leaves() )
				{
					++leafIndex;

					/* Deterministic thinning: a running accumulator keeps the survivors spread
					 * over the whole canopy, where "every n-th leaf" would carve visible rows. */
					accumulator += fraction;

					if ( accumulator < static_cast< vertex_data_t >(1) )
					{
						continue;
					}

					accumulator -= static_cast< vertex_data_t >(1);

					this->emitLeafCard(builder, context, leaf, leaf.scale() * scaleCompensation, leafIndex);
				}
			}

			/**
			 * @brief Emits one leaf card.
			 * @param builder A reference to the shape builder.
			 * @param context A reference to the tree context.
			 * @param leaf A reference to the leaf attachment.
			 * @param length The length of the card.
			 * @param leafIndex The rank of the leaf, used to give it its own flutter phase.
			 * @return void
			 */
			void
			emitLeafCard (ShapeBuilder< vertex_data_t, index_data_t > & builder, const Context & context, const TreeLeafAttachment< vertex_data_t > & leaf, vertex_data_t length, uint32_t leafIndex) const noexcept
			{
				const auto & frame = leaf.frame();

				const auto along = frame.localYAxis();
				const auto width = length * m_options.leafAspectRatio();

				/* A phase that is stable for a given leaf and spread over [0, 1]: without it every
				 * leaf of the tree flutters in unison, which reads as a pulsing blob. */
				const auto phase = m_options.windChannelsEnabled() ?
					static_cast< vertex_data_t >((leafIndex * 2654435761U) % 1024U) / static_cast< vertex_data_t >(1024) :
					static_cast< vertex_data_t >(0);

				const auto occlusion = this->ambientOcclusion(context, frame.position());

				this->emitCard(builder, frame.position(), along, frame.rightVector(), frame.backwardVector(), width, length, phase, occlusion, context);

				if ( m_options.leafCardMode() == TreeLeafCardMode::CrossedQuads )
				{
					this->emitCard(builder, frame.position(), along, frame.backwardVector(), frame.rightVector().inversed(), width, length, phase, occlusion, context);
				}
			}

			/**
			 * @brief Emits one quad standing on a petiole.
			 * @param builder A reference to the shape builder.
			 * @param origin A reference to the petiole.
			 * @param along A reference to the direction the blade extends toward.
			 * @param across A reference to the direction the blade widens toward.
			 * @param normal A reference to the blade normal.
			 * @param width The width of the blade.
			 * @param length The length of the blade.
			 * @param phase The flutter phase of the leaf.
			 * @param occlusion The baked occlusion of the leaf.
			 * @param context A reference to the tree context.
			 * @return void
			 */
			void
			emitCard (ShapeBuilder< vertex_data_t, index_data_t > & builder, const Math::Vector< 3, vertex_data_t > & origin, const Math::Vector< 3, vertex_data_t > & along, const Math::Vector< 3, vertex_data_t > & across, const Math::Vector< 3, vertex_data_t > & normal, vertex_data_t width, vertex_data_t length, vertex_data_t phase, vertex_data_t occlusion, const Context & context) const noexcept
			{
				const auto half = width / static_cast< vertex_data_t >(2);

				std::array< SkinVertex, 4 > corners{};

				for ( uint32_t corner = 0; corner < 4; ++corner )
				{
					const auto right = corner == 1 || corner == 2;
					const auto top = corner >= 2;

					corners[corner].position = origin + across * (right ? half : -half) + along * (top ? length : static_cast< vertex_data_t >(0));
					corners[corner].normal = normal;
					corners[corner].textureU = right ? static_cast< vertex_data_t >(1) : static_cast< vertex_data_t >(0);
					corners[corner].textureV = top ? static_cast< vertex_data_t >(1) : static_cast< vertex_data_t >(0);

					if ( m_options.windChannelsEnabled() )
					{
						const auto heightRatio = std::clamp((corners[corner].position[Math::Y] - context.baseHeight) / context.treeHeight, static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(1));

						/* A leaf sits at the end of everything, so it takes a full share of the branch
						 * bending — and the tip of the blade takes more of it than the petiole, which is
						 * attached. The flutter phase is constant over the card: it identifies the leaf,
						 * it does not vary across it. */
						corners[corner].color = {
							std::pow(heightRatio, m_options.trunkBendExponent()),
							top ? static_cast< vertex_data_t >(1) : static_cast< vertex_data_t >(0.5),
							phase,
							occlusion
						};
					}
					else
					{
						corners[corner].color = {0, 0, 0, occlusion};
					}
				}

				this->emitTriangle(builder, corners[0], corners[1], corners[2]);
				this->emitTriangle(builder, corners[0], corners[2], corners[3]);
			}

			/**
			 * @brief Pushes one triangle into the builder.
			 * @param builder A reference to the shape builder.
			 * @param first A reference to the first vertex.
			 * @param second A reference to the second vertex.
			 * @param third A reference to the third vertex.
			 * @return void
			 */
			void
			emitTriangle (ShapeBuilder< vertex_data_t, index_data_t > & builder, const SkinVertex & first, const SkinVertex & second, const SkinVertex & third) const noexcept
			{
				for ( const auto * vertex : {&first, &second, &third} )
				{
					builder.setPosition(vertex->position);
					builder.setNormal(vertex->normal);
					builder.setTextureCoordinates(vertex->textureU, vertex->textureV);
					builder.setVertexColor(vertex->color);

					builder.newVertex();
				}
			}

			TreeSkinningOptions< vertex_data_t > m_options;
	};
}
