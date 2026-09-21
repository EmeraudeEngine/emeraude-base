/*
 * src/VertexFactory/TreeGenerator.hpp
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

/* Local inclusions for usages. */
#include "TreeColonizationGrower.hpp"
#include "TreeMesh.hpp"
#include "TreeParameters.hpp"
#include "TreeSkinningOptions.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief Generates a complete tree: grows a skeleton, skins it into a chain of levels of
	 * detail, and builds the card that replaces it in the distance.
	 * @note This is the façade over the two growers and the skinner. Use it unless you need a
	 * skeleton without a mesh, or a mesh from a skeleton you built yourself — both growers and
	 * `TreeSkinner` are usable on their own.
	 * @note Deliberately NOT a template, unlike the rest of this module. It is the single compiled
	 * unit of the `emeraude_base_vertex` object library, and a tree is generated at load time, not
	 * per frame, so `float` costs nothing here and the compile time is spent once.
	 */
	class TreeGenerator final
	{
		public:

			/**
			 * @brief Which model grows the skeleton.
			 */
			enum class GrowerType : uint8_t
			{
				/** @brief Weber & Penn: species parameters drive the shape. */
				Parametric,
				/** @brief Runions et al.: the crown volume drives the shape. */
				SpaceColonization
			};

			/** @brief The deepest level chain that can be asked for. */
			static constexpr uint32_t MaxLevelOfDetailCount{8};

			/**
			 * @brief Constructs a generator growing the default parametric species.
			 */
			TreeGenerator () noexcept = default;

			/**
			 * @brief Sets which model grows the skeleton.
			 * @param type The model.
			 * @return void
			 */
			void
			setGrowerType (GrowerType type) noexcept
			{
				m_growerType = type;
			}

			/**
			 * @brief Returns which model grows the skeleton.
			 * @return GrowerType
			 */
			[[nodiscard]]
			GrowerType
			growerType () const noexcept
			{
				return m_growerType;
			}

			/**
			 * @brief Gives mutable access to the species parameters of the parametric model.
			 * @return TreeParameters< float > &
			 */
			[[nodiscard]]
			TreeParameters< float > &
			parameters () noexcept
			{
				return m_parameters;
			}

			/**
			 * @brief Gives access to the species parameters of the parametric model.
			 * @return const TreeParameters< float > &
			 */
			[[nodiscard]]
			const TreeParameters< float > &
			parameters () const noexcept
			{
				return m_parameters;
			}

			/**
			 * @brief Gives mutable access to the space colonization model.
			 * @return TreeColonizationGrower< float > &
			 */
			[[nodiscard]]
			TreeColonizationGrower< float > &
			colonizationGrower () noexcept
			{
				return m_colonizationGrower;
			}

			/**
			 * @brief Gives access to the space colonization model.
			 * @return const TreeColonizationGrower< float > &
			 */
			[[nodiscard]]
			const TreeColonizationGrower< float > &
			colonizationGrower () const noexcept
			{
				return m_colonizationGrower;
			}

			/**
			 * @brief Gives mutable access to the skinning options of the FINEST level; the coarser
			 * ones are derived from it.
			 * @return TreeSkinningOptions< float > &
			 */
			[[nodiscard]]
			TreeSkinningOptions< float > &
			skinningOptions () noexcept
			{
				return m_skinningOptions;
			}

			/**
			 * @brief Gives access to the skinning options of the finest level.
			 * @return const TreeSkinningOptions< float > &
			 */
			[[nodiscard]]
			const TreeSkinningOptions< float > &
			skinningOptions () const noexcept
			{
				return m_skinningOptions;
			}

			/**
			 * @brief Sets how many levels of detail are skinned, 1 meaning the finest alone.
			 * @param count The count.
			 * @return void
			 */
			void
			setLevelOfDetailCount (uint32_t count) noexcept
			{
				m_levelOfDetailCount = std::clamp(count, 1U, MaxLevelOfDetailCount);
			}

			/**
			 * @brief Returns how many levels of detail are skinned.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			levelOfDetailCount () const noexcept
			{
				return m_levelOfDetailCount;
			}

			/**
			 * @brief Sets whether the crossed-quads card is built.
			 * @param state The state.
			 * @return void
			 */
			void
			enableImposter (bool state) noexcept
			{
				m_imposterEnabled = state;
			}

			/**
			 * @brief Returns whether the crossed-quads card is built.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			imposterEnabled () const noexcept
			{
				return m_imposterEnabled;
			}

			/**
			 * @brief Sets how many quads cross each other in the card.
			 * @param count The count.
			 * @return void
			 */
			void
			setImposterQuadCount (uint32_t count) noexcept
			{
				m_imposterQuadCount = std::max(1U, count);
			}

			/**
			 * @brief Returns how many quads cross each other in the card.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			imposterQuadCount () const noexcept
			{
				return m_imposterQuadCount;
			}

			/**
			 * @brief Grows and skins a complete tree.
			 * @note The same seed and the same settings always give the same tree.
			 * @param seed The generation seed. Default 0.
			 * @return TreeMesh< float >
			 */
			[[nodiscard]]
			TreeMesh< float > generate (uint32_t seed = 0) const noexcept;

		private:

			TreeParameters< float > m_parameters;
			TreeColonizationGrower< float > m_colonizationGrower;
			TreeSkinningOptions< float > m_skinningOptions;
			uint32_t m_levelOfDetailCount{3};
			uint32_t m_imposterQuadCount{3};
			GrowerType m_growerType{GrowerType::Parametric};
			bool m_imposterEnabled{false};
	};
}
