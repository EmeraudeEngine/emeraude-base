/*
 * src/VertexFactory/TreeMesh.hpp
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
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

/* Local inclusions for usages. */
#include "Shape.hpp"
#include "TreeSkeleton.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief Everything one generated tree is made of: the botany it was grown from, the meshes it
	 * was skinned into, and the card that replaces it in the distance.
	 * @note ⚠️ The generator returns THIS, not a bare `Shape` (owner decision, 2026-09-21). A
	 * skeleton thrown away at the door has to be regrown by anyone who later wants a collision
	 * capsule, a wind hierarchy, another level of detail or an imposter bake.
	 * @note Every level of detail carries the SAME two groups, `BarkGroup` then `LeafGroup`, so the
	 * engine sees the same two sub-geometries whatever the level it draws.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 * @tparam index_data_t The precision type of index data. Default uint32_t.
	 */
	template< typename vertex_data_t = float, typename index_data_t = uint32_t >
	requires (std::is_floating_point_v< vertex_data_t > && std::is_unsigned_v< index_data_t >)
	class TreeMesh final
	{
		public:

			/** @brief The group holding the branches, to be given the bark material. */
			static constexpr uint32_t BarkGroup{0};

			/** @brief The group holding the leaf cards, to be given the alpha-masked material. */
			static constexpr uint32_t LeafGroup{1};

			/**
			 * @brief Constructs an empty tree mesh.
			 */
			TreeMesh () noexcept = default;

			/**
			 * @brief Sets the skeleton the meshes were skinned from.
			 * @param skeleton The skeleton.
			 * @return void
			 */
			void
			setSkeleton (TreeSkeleton< vertex_data_t > && skeleton) noexcept
			{
				m_skeleton = std::move(skeleton);
			}

			/**
			 * @brief Gives access to the skeleton the meshes were skinned from.
			 * @return const TreeSkeleton< vertex_data_t > &
			 */
			[[nodiscard]]
			const TreeSkeleton< vertex_data_t > &
			skeleton () const noexcept
			{
				return m_skeleton;
			}

			/**
			 * @brief Appends a level of detail, from the finest to the coarsest.
			 * @param shape The mesh of that level.
			 * @return void
			 */
			void
			addLevelOfDetail (Shape< vertex_data_t, index_data_t > && shape) noexcept
			{
				m_levelsOfDetail.emplace_back(std::move(shape));
			}

			/**
			 * @brief Gives access to every level of detail, finest first.
			 * @return const std::vector< Shape< vertex_data_t, index_data_t > > &
			 */
			[[nodiscard]]
			const std::vector< Shape< vertex_data_t, index_data_t > > &
			levelsOfDetail () const noexcept
			{
				return m_levelsOfDetail;
			}

			/**
			 * @brief Returns how many levels of detail were skinned.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			levelCount () const noexcept
			{
				return static_cast< uint32_t >(m_levelsOfDetail.size());
			}

			/**
			 * @brief Returns one level of detail, the coarsest one if the level asked does not exist.
			 * @param level The level, 0 being the finest.
			 * @return const Shape< vertex_data_t, index_data_t > &
			 */
			[[nodiscard]]
			const Shape< vertex_data_t, index_data_t > &
			shape (uint32_t level = 0) const noexcept
			{
				if ( m_levelsOfDetail.empty() )
				{
					return m_emptyShape;
				}

				return m_levelsOfDetail[std::min(static_cast< size_t >(level), m_levelsOfDetail.size() - 1)];
			}

			/**
			 * @brief Sets the crossed-quads card that replaces the tree in the distance.
			 * @note It holds ONE group, not two: the card samples a single baked atlas, which is
			 * the engine's job to produce. That is why it is kept apart from the level chain
			 * instead of being its last rung.
			 * @param shape The card geometry.
			 * @return void
			 */
			void
			setImposter (Shape< vertex_data_t, index_data_t > && shape) noexcept
			{
				m_imposter = std::move(shape);
			}

			/**
			 * @brief Gives access to the crossed-quads card.
			 * @return const Shape< vertex_data_t, index_data_t > &
			 */
			[[nodiscard]]
			const Shape< vertex_data_t, index_data_t > &
			imposter () const noexcept
			{
				return m_imposter;
			}

			/**
			 * @brief Returns whether a card was produced.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			hasImposter () const noexcept
			{
				return !m_imposter.empty();
			}

			/**
			 * @brief Returns whether nothing was skinned.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			empty () const noexcept
			{
				return m_levelsOfDetail.empty();
			}

			/**
			 * @brief Returns the number of triangles of one level of detail.
			 * @param level The level, 0 being the finest.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			triangleCount (uint32_t level = 0) const noexcept
			{
				return this->shape(level).triangles().size();
			}

			/**
			 * @brief Clears everything.
			 * @return void
			 */
			void
			clear () noexcept
			{
				m_skeleton.clear();
				m_levelsOfDetail.clear();
				m_imposter.clear();
			}

		private:

			TreeSkeleton< vertex_data_t > m_skeleton;
			std::vector< Shape< vertex_data_t, index_data_t > > m_levelsOfDetail;
			Shape< vertex_data_t, index_data_t > m_imposter;
			/* NOTE: What shape() hands back when nothing was skinned, so the accessor can return a
			 * reference without ever handing out a dangling one. */
			Shape< vertex_data_t, index_data_t > m_emptyShape;
	};
}
