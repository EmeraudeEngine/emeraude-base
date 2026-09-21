/*
 * src/VertexFactory/TreeSkeleton.hpp
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
#include <span>
#include <type_traits>
#include <vector>

/* Local inclusions for usages. */
#include "Math/Space3D/AACuboid.hpp"
#include "TreeLeafAttachment.hpp"
#include "TreeSegment.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief The children of every segment of a skeleton, in compressed row form.
	 * @note Built on demand rather than kept in the skeleton: the skinning phase walks it once,
	 * and a vector of vectors would allocate once per segment.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 */
	template< typename vertex_data_t = float >
	requires (std::is_floating_point_v< vertex_data_t >)
	class TreeChildTable final
	{
		public:

			/**
			 * @brief Builds the table from a segment list.
			 * @param segments A reference to the segment list.
			 */
			explicit
			TreeChildTable (const std::vector< TreeSegment< vertex_data_t > > & segments) noexcept
			{
				const auto segmentCount = segments.size();

				m_offsets.assign(segmentCount + 1, 0);

				/* First pass: count the children of every segment. */
				for ( const auto & segment : segments )
				{
					if ( !segment.isRoot() )
					{
						++m_offsets[segment.parentIndex() + 1];
					}
				}

				/* Prefix sum, so m_offsets[i] is where the children of 'i' start. */
				for ( size_t index = 0; index < segmentCount; ++index )
				{
					m_offsets[index + 1] += m_offsets[index];
				}

				m_indexes.resize(m_offsets.back());

				/* Second pass: fill, using a moving cursor per parent. */
				std::vector< uint32_t > cursors{m_offsets.cbegin(), m_offsets.cend() - 1};

				for ( size_t index = 0; index < segmentCount; ++index )
				{
					const auto & segment = segments[index];

					if ( segment.isRoot() )
					{
						continue;
					}

					m_indexes[cursors[segment.parentIndex()]++] = static_cast< uint32_t >(index);
				}
			}

			/**
			 * @brief Returns the children of a segment.
			 * @param segmentIndex The index of the segment.
			 * @return std::span< const uint32_t >
			 */
			[[nodiscard]]
			std::span< const uint32_t >
			children (uint32_t segmentIndex) const noexcept
			{
				if ( segmentIndex + 1 >= m_offsets.size() )
				{
					return {};
				}

				const auto first = m_offsets[segmentIndex];

				return {m_indexes.data() + first, m_offsets[segmentIndex + 1] - first};
			}

		private:

			std::vector< uint32_t > m_offsets;
			std::vector< uint32_t > m_indexes;
	};

	/**
	 * @brief The botanical result of growing a tree: what the branches are, where they go, how
	 * thick they are, and where the leaves hang. It holds no mesh.
	 * @note ⚠️ Every grower MUST add a segment only after its parent, so a parent index is always
	 * smaller than its child's. The reverse passes here (the pipe model) and the skinning phase
	 * both rely on it, and nothing checks it for you.
	 * @note Keeping this separate from the mesh is what lets one tree be re-skinned at several LOD
	 * levels, baked into an imposter, turned into physics capsules, or given a wind hierarchy,
	 * without growing it again.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 */
	template< typename vertex_data_t = float >
	requires (std::is_floating_point_v< vertex_data_t >)
	class TreeSkeleton final
	{
		public:

			/**
			 * @brief Constructs an empty skeleton.
			 */
			TreeSkeleton () noexcept = default;

			/**
			 * @brief Reserves room for the expected content.
			 * @param segmentCount The expected number of segments.
			 * @param leafCount The expected number of leaves.
			 * @return void
			 */
			void
			reserve (size_t segmentCount, size_t leafCount) noexcept
			{
				m_segments.reserve(segmentCount);
				m_leaves.reserve(leafCount);
			}

			/**
			 * @brief Appends a segment and returns its index.
			 * @warning The parent must have been added first.
			 * @param segment A reference to the segment.
			 * @return uint32_t
			 */
			uint32_t
			addSegment (const TreeSegment< vertex_data_t > & segment) noexcept
			{
				const auto index = static_cast< uint32_t >(m_segments.size());

				m_segments.emplace_back(segment);

				m_maxOrder = std::max(m_maxOrder, segment.order());
				m_branchCount = std::max(m_branchCount, segment.branchIndex() + 1);

				this->mergeSegmentBounds(segment);

				return index;
			}

			/**
			 * @brief Appends a leaf attachment.
			 * @param leaf A reference to the leaf attachment.
			 * @return void
			 */
			void
			addLeaf (const TreeLeafAttachment< vertex_data_t > & leaf) noexcept
			{
				m_leaves.emplace_back(leaf);

				m_boundingBox.merge(leaf.frame().position());
			}

			/**
			 * @brief Gives access to the segment list.
			 * @return const std::vector< TreeSegment< vertex_data_t > > &
			 */
			[[nodiscard]]
			const std::vector< TreeSegment< vertex_data_t > > &
			segments () const noexcept
			{
				return m_segments;
			}

			/**
			 * @brief Gives access to the leaf attachment list.
			 * @return const std::vector< TreeLeafAttachment< vertex_data_t > > &
			 */
			[[nodiscard]]
			const std::vector< TreeLeafAttachment< vertex_data_t > > &
			leaves () const noexcept
			{
				return m_leaves;
			}

			/**
			 * @brief Returns the number of segments.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			segmentCount () const noexcept
			{
				return m_segments.size();
			}

			/**
			 * @brief Returns the number of leaf attachments.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			leafCount () const noexcept
			{
				return m_leaves.size();
			}

			/**
			 * @brief Returns the number of distinct branches.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			branchCount () const noexcept
			{
				return m_branchCount;
			}

			/**
			 * @brief Returns the deepest branching order reached, 0 for a bare trunk.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			maxOrder () const noexcept
			{
				return m_maxOrder;
			}

			/**
			 * @brief Returns whether the skeleton holds nothing.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			empty () const noexcept
			{
				return m_segments.empty();
			}

			/**
			 * @brief Returns the box enclosing the branches and the leaf attachments.
			 * @return const Math::Space3D::AACuboid< vertex_data_t > &
			 */
			[[nodiscard]]
			const Math::Space3D::AACuboid< vertex_data_t > &
			boundingBox () const noexcept
			{
				return m_boundingBox;
			}

			/**
			 * @brief Returns the height of the tree, along the world Y axis.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			height () const noexcept
			{
				if ( !m_boundingBox.isValid() )
				{
					return 0;
				}

				return m_boundingBox.maximum()[Math::Y] - m_boundingBox.minimum()[Math::Y];
			}

			/**
			 * @brief Returns the summed length of every segment.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			totalLength () const noexcept
			{
				vertex_data_t total = 0;

				for ( const auto & segment : m_segments )
				{
					total += segment.length();
				}

				return total;
			}

			/**
			 * @brief Builds the child table of this skeleton.
			 * @return TreeChildTable< vertex_data_t >
			 */
			[[nodiscard]]
			TreeChildTable< vertex_data_t >
			buildChildTable () const noexcept
			{
				return TreeChildTable< vertex_data_t >{m_segments};
			}

			/**
			 * @brief Assigns every radius from the discrete pipe model (Leonardo da Vinci's rule,
			 * generalized by Murray's law): a segment carries as much section as the tips it feeds.
			 * @note `radius = tipRadius * tipCount^(1/exponent)`, and a segment ends on the radius of
			 * its THICKEST child so the surface stays continuous; the thinner children bud off that
			 * surface. Use it for a grower that produces topology without radii, such as space
			 * colonization. The parametric grower sets its own radii from the Weber & Penn formulas
			 * and must not call this.
			 * @warning ⚠️ On an unbranched chain of segments the tip count never changes, so this
			 * model yields NO taper there. That is the model, not a defect: a space-colonization
			 * skeleton branches often enough for the taper to come out of the topology. A grower
			 * that makes long bare chains needs its own taper on top.
			 * @param tipRadius The radius of a branch tip.
			 * @param exponent Murray's exponent; 2 is da Vinci's rule, 2.49 is the measured average.
			 * @return void
			 */
			void
			computeRadiiFromPipeModel (vertex_data_t tipRadius, vertex_data_t exponent = static_cast< vertex_data_t >(2.49)) noexcept
			{
				if ( m_segments.empty() || exponent <= 0 )
				{
					return;
				}

				const auto childTable = this->buildChildTable();

				/* Reverse pass: a tip feeds itself, a fork feeds the sum of its children. The
				 * segments are ordered parent before child, so one backward sweep is enough. */
				std::vector< vertex_data_t > tipCounts(m_segments.size(), static_cast< vertex_data_t >(1));

				for ( size_t offset = m_segments.size(); offset > 0; --offset )
				{
					const auto index = static_cast< uint32_t >(offset - 1);
					const auto children = childTable.children(index);

					if ( children.empty() )
					{
						continue;
					}

					vertex_data_t total = 0;

					for ( const auto childIndex : children )
					{
						total += tipCounts[childIndex];
					}

					tipCounts[index] = total;
				}

				const auto inverseExponent = static_cast< vertex_data_t >(1) / exponent;

				for ( size_t index = 0; index < m_segments.size(); ++index )
				{
					auto & segment = m_segments[index];

					const auto radius = tipRadius * std::pow(tipCounts[index], inverseExponent);

					segment.setStartRadius(radius);

					const auto children = childTable.children(static_cast< uint32_t >(index));

					if ( children.empty() )
					{
						segment.setEndRadius(tipRadius);
						segment.setBranchTip(true);

						continue;
					}

					/* End on the thickest child, so the trunk narrows into the branch that
					 * continues it and the others bud off that surface. */
					vertex_data_t thickest = 0;

					for ( const auto childIndex : children )
					{
						thickest = std::max(thickest, tipCounts[childIndex]);
					}

					segment.setEndRadius(tipRadius * std::pow(thickest, inverseExponent));
				}

				this->rebuildBounds();
			}

			/**
			 * @brief Clears the skeleton.
			 * @return void
			 */
			void
			clear () noexcept
			{
				m_segments.clear();
				m_leaves.clear();
				m_boundingBox.reset();
				m_branchCount = 0;
				m_maxOrder = 0;
			}

		private:

			/**
			 * @brief Extends the bounding box with one segment, radius included.
			 * @param segment A reference to the segment.
			 * @return void
			 */
			void
			mergeSegmentBounds (const TreeSegment< vertex_data_t > & segment) noexcept
			{
				const auto widest = std::max(segment.startRadius(), segment.endRadius());
				const Math::Vector< 3, vertex_data_t > margin{widest, widest, widest};

				const auto start = segment.startPoint();
				const auto end = segment.endPoint();

				m_boundingBox.merge(start + margin);
				m_boundingBox.merge(start - margin);
				m_boundingBox.merge(end + margin);
				m_boundingBox.merge(end - margin);
			}

			/**
			 * @brief Recomputes the bounding box from scratch.
			 * @return void
			 */
			void
			rebuildBounds () noexcept
			{
				m_boundingBox.reset();

				for ( const auto & segment : m_segments )
				{
					this->mergeSegmentBounds(segment);
				}

				for ( const auto & leaf : m_leaves )
				{
					m_boundingBox.merge(leaf.frame().position());
				}
			}

			std::vector< TreeSegment< vertex_data_t > > m_segments;
			std::vector< TreeLeafAttachment< vertex_data_t > > m_leaves;
			Math::Space3D::AACuboid< vertex_data_t > m_boundingBox;
			uint32_t m_branchCount{0};
			uint32_t m_maxOrder{0};
	};
}
