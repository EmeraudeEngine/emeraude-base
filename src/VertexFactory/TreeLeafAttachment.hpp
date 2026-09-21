/*
 * src/VertexFactory/TreeLeafAttachment.hpp
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
#include <type_traits>

/* Local inclusions for usages. */
#include "Math/CartesianFrame.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief Where a leaf hangs on a tree skeleton, and how it is oriented.
	 * @note The frame sits at the petiole, its LOCAL +Y points along the leaf, away from the
	 * branch, and its local +Z is the leaf blade normal. Read the growth axis through
	 * CartesianFrame::localYAxis(), never through upwardVector().
	 * @note This carries no geometry: turning an attachment into a card is the skinning phase's
	 * job, so the same skeleton can produce a full card at LOD 0 and a merged cluster further out.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 */
	template< typename vertex_data_t = float >
	requires (std::is_floating_point_v< vertex_data_t >)
	class TreeLeafAttachment final
	{
		public:

			/**
			 * @brief Constructs a default leaf attachment.
			 */
			TreeLeafAttachment () noexcept = default;

			/**
			 * @brief Constructs a leaf attachment.
			 * @param frame The frame at the petiole, local +Y along the leaf.
			 * @param scale The size of the leaf, as a multiplier of the species leaf size.
			 * @param segmentIndex The index of the segment the leaf hangs on.
			 */
			TreeLeafAttachment (const Math::CartesianFrame< vertex_data_t > & frame, vertex_data_t scale, uint32_t segmentIndex) noexcept
				: m_frame(frame),
				m_scale(scale),
				m_segmentIndex(segmentIndex)
			{

			}

			/**
			 * @brief Returns the frame at the petiole.
			 * @return const Math::CartesianFrame< vertex_data_t > &
			 */
			[[nodiscard]]
			const Math::CartesianFrame< vertex_data_t > &
			frame () const noexcept
			{
				return m_frame;
			}

			/**
			 * @brief Returns the size of the leaf, as a multiplier of the species leaf size.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			scale () const noexcept
			{
				return m_scale;
			}

			/**
			 * @brief Returns the index of the segment the leaf hangs on.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			segmentIndex () const noexcept
			{
				return m_segmentIndex;
			}

		private:

			Math::CartesianFrame< vertex_data_t > m_frame;
			vertex_data_t m_scale{1};
			uint32_t m_segmentIndex{0};
	};
}
