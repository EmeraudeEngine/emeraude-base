/*
 * src/VertexFactory/TreeSegment.hpp
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
#include <cstdint>
#include <cmath>
#include <limits>
#include <type_traits>

/* Local inclusions for usages. */
#include "Math/CartesianFrame.hpp"
#include "Math/Vector.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief One internode of a tree skeleton: a straight, tapered piece of branch.
	 * @note The segment frame sits at the START of the piece, and its LOCAL +Y is the growth
	 * direction. Use CartesianFrame::localYAxis() to read it: that accessor carries no up/down
	 * meaning, so it cannot invert if the world convention ever moves again.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 */
	template< typename vertex_data_t = float >
	requires (std::is_floating_point_v< vertex_data_t >)
	class TreeSegment final
	{
		public:

			/** @brief The parent index of a segment that starts a tree. */
			static constexpr uint32_t NoParent{std::numeric_limits< uint32_t >::max()};

			/**
			 * @brief Constructs a default segment.
			 */
			TreeSegment () noexcept = default;

			/**
			 * @brief Constructs a segment.
			 * @param frame The frame at the start of the segment, local +Y along the growth direction.
			 * @param length The length of the segment.
			 * @param startRadius The radius at the start of the segment.
			 * @param endRadius The radius at the end of the segment.
			 * @param arcLength The distance from the start of the branch this segment belongs to.
			 * @param parentIndex The index of the segment this one grows from, or NoParent.
			 * @param branchIndex The index of the branch this segment belongs to.
			 * @param order The branching order, 0 for the trunk.
			 */
			TreeSegment (const Math::CartesianFrame< vertex_data_t > & frame, vertex_data_t length, vertex_data_t startRadius, vertex_data_t endRadius, vertex_data_t arcLength, uint32_t parentIndex, uint32_t branchIndex, uint32_t order) noexcept
				: m_frame(frame),
				m_length(length),
				m_startRadius(startRadius),
				m_endRadius(endRadius),
				m_arcLength(arcLength),
				m_parentIndex(parentIndex),
				m_branchIndex(branchIndex),
				m_order(order)
			{

			}

			/**
			 * @brief Returns the frame at the start of the segment.
			 * @return const Math::CartesianFrame< vertex_data_t > &
			 */
			[[nodiscard]]
			const Math::CartesianFrame< vertex_data_t > &
			frame () const noexcept
			{
				return m_frame;
			}

			/**
			 * @brief Returns the growth direction of the segment.
			 * @return const Math::Vector< 3, vertex_data_t > &
			 */
			[[nodiscard]]
			const Math::Vector< 3, vertex_data_t > &
			axis () const noexcept
			{
				return m_frame.localYAxis();
			}

			/**
			 * @brief Returns the point where the segment starts.
			 * @return const Math::Vector< 3, vertex_data_t > &
			 */
			[[nodiscard]]
			const Math::Vector< 3, vertex_data_t > &
			startPoint () const noexcept
			{
				return m_frame.position();
			}

			/**
			 * @brief Returns the point where the segment ends.
			 * @return Math::Vector< 3, vertex_data_t >
			 */
			[[nodiscard]]
			Math::Vector< 3, vertex_data_t >
			endPoint () const noexcept
			{
				return m_frame.position() + (m_frame.localYAxis() * m_length);
			}

			/**
			 * @brief Returns the length of the segment.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			length () const noexcept
			{
				return m_length;
			}

			/**
			 * @brief Returns the radius at the start of the segment.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			startRadius () const noexcept
			{
				return m_startRadius;
			}

			/**
			 * @brief Returns the radius at the end of the segment.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			endRadius () const noexcept
			{
				return m_endRadius;
			}

			/**
			 * @brief Sets the radius at the start of the segment.
			 * @param radius The radius.
			 * @return void
			 */
			void
			setStartRadius (vertex_data_t radius) noexcept
			{
				m_startRadius = radius;
			}

			/**
			 * @brief Sets the radius at the end of the segment.
			 * @param radius The radius.
			 * @return void
			 */
			void
			setEndRadius (vertex_data_t radius) noexcept
			{
				m_endRadius = radius;
			}

			/**
			 * @brief Returns the distance from the start of the branch this segment belongs to.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			arcLength () const noexcept
			{
				return m_arcLength;
			}

			/**
			 * @brief Returns the index of the segment this one grows from.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			parentIndex () const noexcept
			{
				return m_parentIndex;
			}

			/**
			 * @brief Returns the index of the branch this segment belongs to.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			branchIndex () const noexcept
			{
				return m_branchIndex;
			}

			/**
			 * @brief Returns the branching order, 0 for the trunk.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			order () const noexcept
			{
				return m_order;
			}

			/**
			 * @brief Returns whether the segment starts a tree.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isRoot () const noexcept
			{
				return m_parentIndex == NoParent;
			}

			/**
			 * @brief Returns whether the segment ends its branch.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isBranchTip () const noexcept
			{
				return m_isBranchTip;
			}

			/**
			 * @brief Declares the segment as the end of its branch.
			 * @param state The state.
			 * @return void
			 */
			void
			setBranchTip (bool state) noexcept
			{
				m_isBranchTip = state;
			}

		private:

			Math::CartesianFrame< vertex_data_t > m_frame;
			vertex_data_t m_length{0};
			vertex_data_t m_startRadius{0};
			vertex_data_t m_endRadius{0};
			vertex_data_t m_arcLength{0};
			uint32_t m_parentIndex{NoParent};
			uint32_t m_branchIndex{0};
			uint32_t m_order{0};
			bool m_isBranchTip{false};
	};

	/**
	 * @brief Builds a frame whose LOCAL +Y follows a growth direction.
	 * @note The spin around that direction is arbitrary but stable: it is picked from the world
	 * axis least aligned with the growth direction, so a nearly vertical branch does not flip its
	 * frame from one segment to the next.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 * @param position The origin of the frame.
	 * @param growthAxis The direction the local +Y must follow. It does not have to be normalized.
	 * @return Math::CartesianFrame< vertex_data_t >
	 */
	template< typename vertex_data_t = float >
	[[nodiscard]]
	Math::CartesianFrame< vertex_data_t >
	makeTreeFrame (const Math::Vector< 3, vertex_data_t > & position, const Math::Vector< 3, vertex_data_t > & growthAxis) noexcept
		requires (std::is_floating_point_v< vertex_data_t >)
	{
		Math::CartesianFrame< vertex_data_t > frame;
		frame.setPosition(position);

		const auto length = growthAxis.length();

		if ( length < static_cast< vertex_data_t >(1e-6) )
		{
			return frame;
		}

		const auto upward = growthAxis / length;

		const auto helper = std::abs(upward[Math::Y]) > static_cast< vertex_data_t >(0.9) ?
			Math::Vector< 3, vertex_data_t >::positiveX() :
			Math::Vector< 3, vertex_data_t >::positiveY();

		/* rightVector() is cross(upward, backward), so backward has to be built the other way
		 * round for the basis to come back consistent. */
		const auto right = Math::Vector< 3, vertex_data_t >::crossProduct(upward, helper).normalized();
		const auto backward = Math::Vector< 3, vertex_data_t >::crossProduct(right, upward).normalized();

		frame.setOrientationVectors(backward, upward);

		return frame;
	}
}
