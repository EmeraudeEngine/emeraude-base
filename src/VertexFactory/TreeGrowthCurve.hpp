/*
 * src/VertexFactory/TreeGrowthCurve.hpp
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

/* Local inclusions. */
#include "TreeColonizationGrower.hpp"
#include "TreeParameters.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief How a species grows with AGE, and what age does to its shape (allometry).
	 * @note The HEIGHT follows the Chapman-Richards growth function, the standard height-age model of forestry:
	 * H(a) = Hmax · (1 - exp(-k·a))^p (F. J. Richards, "A flexible growth function for empirical use", Journal of
	 * Experimental Botany 10, 1959; D. G. Chapman, 1961). Only the RATIO to the reference age is used, so Hmax
	 * cancels out: a preset describes its tree at the reference age and stays exactly that tree there.
	 * What age does beyond the height, h being that height ratio:
	 * - the trunk thickens FASTER than it lengthens — elastic similarity, D ∝ H^1.5 (T. A. McMahon, "Size and
	 *   shape in biology", Science 179, 1973; McMahon & Kronauer, "Tree structures: deducing the principle of
	 *   mechanical design", Journal of Theoretical Biology 59, 1976) — so radius/length grows as h^0.5;
	 * - the crown LIFTS: the lower branches die in their own shade and fall (natural pruning), the live crown
	 *   lengthening as H^CrownLengthExponent while the tree grows as H;
	 * - the foot flares (buttress roots) and the heavy old limbs droop (less upward attraction), both as h^0.5;
	 * - a leaf keeps its size in reality. The generator's stems only LENGTHEN with h, so the leaf count grows as h
	 *   and the leaf card as h^0.5: the crown keeps its foliage cover (h · (h^0.5)² / h²) for h times the
	 *   triangles, not h².
	 * ⚠️ These are the model's own exponents, not measurements of one species: they give the SHAPE of an old tree
	 * (taller, much thicker at the foot, a raised crown), and the preset's curve gives its pace.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 */
	template< typename vertex_data_t = float >
	requires (std::is_floating_point_v< vertex_data_t >)
	class TreeGrowthCurve final
	{
		public:

			/** @brief The live crown lengthens as H^0.6 while the tree grows as H: the bare foot of the trunk rises with age. */
			static constexpr vertex_data_t CrownLengthExponent{0.6};
			/** @brief The highest bare fraction age can reach: an old tree keeps a fifth of its height in leaves. */
			static constexpr vertex_data_t MaxBaseSize{0.8};
			/** @brief The widest foot age can give, as a multiple of the preset's flare. */
			static constexpr vertex_data_t MaxFlareFactor{2.5};

			/**
			 * @brief Constructs a growth curve: a middle-aged, moderately fast temperate tree.
			 */
			constexpr TreeGrowthCurve () noexcept = default;

			/**
			 * @brief Constructs a growth curve.
			 * @param referenceAge The age, in years, the species parameters describe.
			 * @param rate The growth rate k of the Chapman-Richards function, per year.
			 * @param shape The shape exponent p of the Chapman-Richards function.
			 */
			constexpr
			TreeGrowthCurve (vertex_data_t referenceAge, vertex_data_t rate, vertex_data_t shape) noexcept
				: m_referenceAge{std::max(referenceAge, static_cast< vertex_data_t >(1))},
				m_rate{std::max(rate, static_cast< vertex_data_t >(1e-4))},
				m_shape{std::max(shape, static_cast< vertex_data_t >(0.1))}
			{

			}

			/** @brief Returns the age, in years, the species parameters describe. @return vertex_data_t */
			[[nodiscard]] vertex_data_t referenceAge () const noexcept { return m_referenceAge; }

			/** @brief Returns the growth rate k, per year. @return vertex_data_t */
			[[nodiscard]] vertex_data_t rate () const noexcept { return m_rate; }

			/** @brief Returns the shape exponent p. @return vertex_data_t */
			[[nodiscard]] vertex_data_t shape () const noexcept { return m_shape; }

			/**
			 * @brief Returns the height of the tree at an age, relative to its height at the reference age.
			 * @note 1 at the reference age, monotonic, saturating at (1 - exp(-k·ref))^-p: an old tree stops growing
			 * up, never runs away.
			 * @param years The age in years. Zero or less means the reference age.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			heightFactor (vertex_data_t years) const noexcept
			{
				if ( years <= 0 )
				{
					return 1;
				}

				return std::pow((1 - std::exp(-m_rate * years)) / (1 - std::exp(-m_rate * m_referenceAge)), m_shape);
			}

			/**
			 * @brief Ages a Weber & Penn parameter set, in place.
			 * @note Nothing changes at the reference age (or an age of zero), so an un-aged preset is bit-exact.
			 * @param parameters A writable reference to the parameters, which describe the reference age.
			 * @param years The age in years. Zero or less means the reference age.
			 * @return void
			 */
			void
			apply (TreeParameters< vertex_data_t > & parameters, vertex_data_t years) const noexcept
			{
				if ( years <= 0 || years == m_referenceAge )
				{
					return;
				}

				const auto height = this->heightFactor(years);
				const auto root = std::sqrt(height);

				parameters.setScale(parameters.scale() * height);
				parameters.setScaleVariation(parameters.scaleVariation() * height);
				parameters.setRatio(parameters.ratio() * root);
				parameters.setBaseSize(std::min(1 - (1 - parameters.baseSize()) * std::pow(height, CrownLengthExponent - 1), std::max(parameters.baseSize(), MaxBaseSize)));
				parameters.setFlare(parameters.flare() * std::min(root, MaxFlareFactor));
				parameters.setAttractionUp(parameters.attractionUp() / root);
				parameters.setLeaves(static_cast< uint32_t >(std::lround(static_cast< vertex_data_t >(parameters.leaves()) * height)));
				parameters.setLeafScale(parameters.leafScale() * root);
			}

			/**
			 * @brief Ages a space colonization grower, in place.
			 * @note The crown and the clear trunk grow with the height; the branch spacing (influence, kill distance,
			 * segment length) as its square root, so an old crown holds more, finer branches than an enlarged young
			 * one; the attractors follow the crown SURFACE (h²) that the tips fill. The pipe model thickens the trunk
			 * from the tip count by itself.
			 * @param grower A writable reference to the grower, which describes the reference age.
			 * @param years The age in years. Zero or less means the reference age.
			 * @return void
			 */
			void
			apply (TreeColonizationGrower< vertex_data_t > & grower, vertex_data_t years) const noexcept
			{
				if ( years <= 0 || years == m_referenceAge )
				{
					return;
				}

				const auto height = this->heightFactor(years);
				const auto root = std::sqrt(height);

				grower.setCrownCenter(grower.crownCenter() * height);
				grower.setCrownRadii(grower.crownRadii() * height);
				grower.setTrunkHeight(grower.trunkHeight() * height);
				grower.setInfluenceRadius(grower.influenceRadius() * root);
				grower.setKillDistance(grower.killDistance() * root);
				grower.setSegmentLength(grower.segmentLength() * root);
				grower.setAttractorCount(static_cast< uint32_t >(std::lround(static_cast< vertex_data_t >(grower.attractorCount()) * height * height)));
				grower.setMaxIterations(static_cast< uint32_t >(std::lround(static_cast< vertex_data_t >(grower.maxIterations()) * height)));
				grower.setLeafScale(grower.leafScale() * root);
			}

		private:

			vertex_data_t m_referenceAge{40};
			vertex_data_t m_rate{static_cast< vertex_data_t >(0.025)};
			vertex_data_t m_shape{static_cast< vertex_data_t >(1.3)};
	};
}
