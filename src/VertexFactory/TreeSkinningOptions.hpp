/*
 * src/VertexFactory/TreeSkinningOptions.hpp
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

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief How a leaf attachment is turned into geometry.
	 */
	enum class TreeLeafCardMode : uint8_t
	{
		/** @brief One quad per leaf. Cheapest, and flat when seen edge-on. */
		SingleQuad,
		/** @brief Two quads at a right angle. Twice the triangles, never edge-on. */
		CrossedQuads,
		/** @brief No leaf geometry at all. */
		None
	};

	/**
	 * @brief How a tree skeleton is turned into a mesh, for ONE level of detail.
	 * @note A level of detail is produced by re-skinning the SAME skeleton with a coarser set of
	 * these, never by decimating the finest mesh: a quadric decimator cannot merge two leaf cards
	 * into a bigger one, and foliage is where the triangles are.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 */
	template< typename vertex_data_t = float >
	requires (std::is_floating_point_v< vertex_data_t >)
	class TreeSkinningOptions final
	{
		public:

			/**
			 * @brief Constructs default skinning options.
			 */
			TreeSkinningOptions () noexcept = default;

			/** @brief Sets the ring resolution of the thickest branch. @param value The count. @return void */
			void setRadialSegmentsMax (uint32_t value) noexcept { m_radialSegmentsMax = std::max(3U, value); }

			/** @brief Returns the ring resolution of the thickest branch. @return uint32_t */
			[[nodiscard]] uint32_t radialSegmentsMax () const noexcept { return m_radialSegmentsMax; }

			/** @brief Sets the ring resolution of the thinnest branch. @param value The count. @return void */
			void setRadialSegmentsMin (uint32_t value) noexcept { m_radialSegmentsMin = std::max(3U, value); }

			/** @brief Returns the ring resolution of the thinnest branch. @return uint32_t */
			[[nodiscard]] uint32_t radialSegmentsMin () const noexcept { return m_radialSegmentsMin; }

			/** @brief Sets the edge length a ring aims for; it is what makes the resolution follow the radius. @param value The length. @return void */
			void setTargetEdgeLength (vertex_data_t value) noexcept { m_targetEdgeLength = std::max(value, static_cast< vertex_data_t >(1e-4)); }

			/** @brief Returns the edge length a ring aims for. @return vertex_data_t */
			[[nodiscard]] vertex_data_t targetEdgeLength () const noexcept { return m_targetEdgeLength; }

			/**
			 * @brief Sets how many skeleton segments one ring of the tube spans.
			 * @note 1 puts a ring on every segment. Above that the tube skips stations, which is the
			 * AXIAL half of the level of detail — the radial half being the ring resolution. A
			 * skeleton made of many short segments, as space colonization produces, is barely
			 * reducible without it: its triangle count is set by its topology, not by its radii.
			 * The first and last stations of a branch are always kept, so it never shortens.
			 * @param value The stride.
			 * @return void
			 */
			void setAxialStride (uint32_t value) noexcept { m_axialStride = std::max(1U, value); }

			/** @brief Returns how many skeleton segments one ring of the tube spans. @return uint32_t */
			[[nodiscard]] uint32_t axialStride () const noexcept { return m_axialStride; }

			/** @brief Sets the radius below which a branch is dropped entirely. @param value The radius. @return void */
			void setMinimumRadius (vertex_data_t value) noexcept { m_minimumRadius = std::max(value, static_cast< vertex_data_t >(0)); }

			/** @brief Returns the radius below which a branch is dropped entirely. @return vertex_data_t */
			[[nodiscard]] vertex_data_t minimumRadius () const noexcept { return m_minimumRadius; }

			/** @brief Sets how many metres of bark one V unit of texture covers. @param value The length. @return void */
			void setBarkTextureLength (vertex_data_t value) noexcept { m_barkTextureLength = std::max(value, static_cast< vertex_data_t >(1e-3)); }

			/** @brief Returns how many metres of bark one V unit of texture covers. @return vertex_data_t */
			[[nodiscard]] vertex_data_t barkTextureLength () const noexcept { return m_barkTextureLength; }

			/** @brief Sets how many times the bark texture wraps around a branch. @param value The count. @return void */
			void setBarkTextureWraps (vertex_data_t value) noexcept { m_barkTextureWraps = std::max(value, static_cast< vertex_data_t >(1e-3)); }

			/** @brief Returns how many times the bark texture wraps around a branch. @return vertex_data_t */
			[[nodiscard]] vertex_data_t barkTextureWraps () const noexcept { return m_barkTextureWraps; }

			/** @brief Sets how much a branch swells where it leaves its parent. @param value The multiplier, 1 for none. @return void */
			void setCollarScale (vertex_data_t value) noexcept { m_collarScale = std::max(value, static_cast< vertex_data_t >(1)); }

			/** @brief Returns how much a branch swells where it leaves its parent. @return vertex_data_t */
			[[nodiscard]] vertex_data_t collarScale () const noexcept { return m_collarScale; }

			/** @brief Sets how leaves become geometry. @param value The mode. @return void */
			void setLeafCardMode (TreeLeafCardMode value) noexcept { m_leafCardMode = value; }

			/** @brief Returns how leaves become geometry. @return TreeLeafCardMode */
			[[nodiscard]] TreeLeafCardMode leafCardMode () const noexcept { return m_leafCardMode; }

			/** @brief Sets the fraction of leaves kept, the survivors being enlarged to hold the canopy. @param value The fraction in ]0, 1]. @return void */
			void setLeafFraction (vertex_data_t value) noexcept { m_leafFraction = std::clamp(value, static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(1)); }

			/** @brief Returns the fraction of leaves kept. @return vertex_data_t */
			[[nodiscard]] vertex_data_t leafFraction () const noexcept { return m_leafFraction; }

			/** @brief Sets how much wider than long a leaf card is. @param value The ratio. @return void */
			void setLeafAspectRatio (vertex_data_t value) noexcept { m_leafAspectRatio = std::max(value, static_cast< vertex_data_t >(1e-3)); }

			/** @brief Returns how much wider than long a leaf card is. @return vertex_data_t */
			[[nodiscard]] vertex_data_t leafAspectRatio () const noexcept { return m_leafAspectRatio; }

			/** @brief Sets how dark the densest part of the canopy is baked, 0 disabling the baked occlusion. @param value The strength in [0, 1]. @return void */
			void setAmbientOcclusionStrength (vertex_data_t value) noexcept { m_ambientOcclusionStrength = std::clamp(value, static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(1)); }

			/** @brief Returns how dark the densest part of the canopy is baked. @return vertex_data_t */
			[[nodiscard]] vertex_data_t ambientOcclusionStrength () const noexcept { return m_ambientOcclusionStrength; }

			/** @brief Sets the side of one cell of the occlusion density grid. @param value The length. @return void */
			void setOcclusionCellSize (vertex_data_t value) noexcept { m_occlusionCellSize = std::max(value, static_cast< vertex_data_t >(1e-2)); }

			/** @brief Returns the side of one cell of the occlusion density grid. @return vertex_data_t */
			[[nodiscard]] vertex_data_t occlusionCellSize () const noexcept { return m_occlusionCellSize; }

			/** @brief Sets the exponent shaping the trunk bending weight; above 1 keeps the foot stiffer. @param value The exponent. @return void */
			void setTrunkBendExponent (vertex_data_t value) noexcept { m_trunkBendExponent = std::max(value, static_cast< vertex_data_t >(1e-3)); }

			/** @brief Returns the exponent shaping the trunk bending weight. @return vertex_data_t */
			[[nodiscard]] vertex_data_t trunkBendExponent () const noexcept { return m_trunkBendExponent; }

			/** @brief Sets whether the R/G/B/A vertex channels are filled. @param state The state. @return void */
			void enableWindChannels (bool state) noexcept { m_windChannelsEnabled = state; }

			/** @brief Returns whether the R/G/B/A vertex channels are filled. @return bool */
			[[nodiscard]] bool windChannelsEnabled () const noexcept { return m_windChannelsEnabled; }

			/** @brief Sets whether the duplicated vertices are merged after the build. @param state The state. @return void */
			void enableVertexMerge (bool state) noexcept { m_vertexMergeEnabled = state; }

			/** @brief Returns whether the duplicated vertices are merged after the build. @return bool */
			[[nodiscard]] bool vertexMergeEnabled () const noexcept { return m_vertexMergeEnabled; }

			/**
			 * @brief Returns these options coarsened for a lower level of detail.
			 * @note Halves the ring resolution and the leaf count per step, and raises the radius
			 * below which a branch disappears. The kept leaves are enlarged by the skinner so the
			 * canopy keeps roughly its coverage.
			 * @param level The level of detail, 0 returning these options unchanged.
			 * @param trunkRadius The radius of the thickest branch, which sets the pruning scale.
			 * @return TreeSkinningOptions
			 */
			[[nodiscard]]
			TreeSkinningOptions
			coarsened (uint32_t level, vertex_data_t trunkRadius) const noexcept
			{
				if ( level == 0 )
				{
					return *this;
				}

				auto coarse = *this;

				const auto factor = static_cast< vertex_data_t >(1) / static_cast< vertex_data_t >(1U << level);

				coarse.setRadialSegmentsMax(std::max(m_radialSegmentsMin, static_cast< uint32_t >(static_cast< vertex_data_t >(m_radialSegmentsMax) * factor)));
				coarse.setTargetEdgeLength(m_targetEdgeLength / factor);
				coarse.setAxialStride(m_axialStride * (1U << level));
				/* Half the leaves per step (owner decision 2026-09-23, "½ par niveau"): the survivors are enlarged by
				 * sqrt(2) per step and pulled toward the crown centre by the skinner, so the canopy keeps its coverage
				 * AND its volume. ⚠️ It was a QUARTER per step (the leaves are most of the triangles): level 3 then
				 * enlarged each leaf 8 times, a card as large as the crown, and no placement could keep it inside —
				 * the broadleaf canopy measured -12 % radius / +15 % height at level 3 even recentred (+62 % / +28 %
				 * before the recentring), where half per step reads -8 % / +2 %, at 12.5 % of the finest level's leaf
				 * triangles instead of 1.6 %. */
				coarse.setLeafFraction(m_leafFraction * factor);

				/* A twig of less than a few per cent of the trunk costs a tube and covers a pixel.
				 * ⚠️ Gently: at 2 % of the trunk per level, the first step already ate every branch
				 * that carried foliage. */
				coarse.setMinimumRadius(std::max(m_minimumRadius, trunkRadius * static_cast< vertex_data_t >(0.01) * static_cast< vertex_data_t >(1U << (level - 1))));

				return coarse;
			}

		private:

			vertex_data_t m_targetEdgeLength{static_cast< vertex_data_t >(0.08)};
			vertex_data_t m_minimumRadius{0};
			vertex_data_t m_barkTextureLength{2};
			vertex_data_t m_barkTextureWraps{1};
			vertex_data_t m_collarScale{static_cast< vertex_data_t >(1.25)};
			vertex_data_t m_leafFraction{1};
			vertex_data_t m_leafAspectRatio{static_cast< vertex_data_t >(0.7)};
			vertex_data_t m_ambientOcclusionStrength{static_cast< vertex_data_t >(0.55)};
			vertex_data_t m_occlusionCellSize{1};
			vertex_data_t m_trunkBendExponent{2};
			uint32_t m_axialStride{1};
			uint32_t m_radialSegmentsMax{12};
			uint32_t m_radialSegmentsMin{3};
			TreeLeafCardMode m_leafCardMode{TreeLeafCardMode::CrossedQuads};
			bool m_windChannelsEnabled{true};
			bool m_vertexMergeEnabled{true};
	};
}
