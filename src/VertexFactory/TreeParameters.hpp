/*
 * src/VertexFactory/TreeParameters.hpp
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

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief The envelope that governs how branch length varies along the trunk.
	 * @note Weber & Penn's "Shape" parameter (SIGGRAPH '95, § 4.3). The enumeration order IS the
	 * paper's numbering, so a parameter set quoted from the paper transfers unchanged.
	 */
	enum class TreeCrownShape : uint8_t
	{
		Conical = 0,
		Spherical = 1,
		Hemispherical = 2,
		Cylindrical = 3,
		TaperedCylindrical = 4,
		Flame = 5,
		InverseConical = 6,
		TendFlame = 7
	};

	/**
	 * @brief Returns the relative branch length at a position along the parent, for a crown shape.
	 * @note Weber & Penn, SIGGRAPH '95, § 4.3 "ShapeRatio".
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 * @param shape The crown shape.
	 * @param ratio The position along the usable part of the trunk, in [0, 1].
	 * @return vertex_data_t
	 */
	template< typename vertex_data_t = float >
	[[nodiscard]]
	vertex_data_t
	treeShapeRatio (TreeCrownShape shape, vertex_data_t ratio) noexcept
		requires (std::is_floating_point_v< vertex_data_t >)
	{
		constexpr auto One = static_cast< vertex_data_t >(1);
		constexpr auto Half = static_cast< vertex_data_t >(0.5);

		ratio = std::clamp(ratio, static_cast< vertex_data_t >(0), One);

		switch ( shape )
		{
			case TreeCrownShape::Conical :
				return static_cast< vertex_data_t >(0.2) + static_cast< vertex_data_t >(0.8) * ratio;

			case TreeCrownShape::Spherical :
				return static_cast< vertex_data_t >(0.2) + static_cast< vertex_data_t >(0.8) * std::sin(std::numbers::pi_v< vertex_data_t > * ratio);

			case TreeCrownShape::Hemispherical :
				return static_cast< vertex_data_t >(0.2) + static_cast< vertex_data_t >(0.8) * std::sin(Half * std::numbers::pi_v< vertex_data_t > * ratio);

			case TreeCrownShape::Cylindrical :
				return One;

			case TreeCrownShape::TaperedCylindrical :
				return Half + Half * ratio;

			case TreeCrownShape::Flame :
				return ratio <= static_cast< vertex_data_t >(0.7) ?
					ratio / static_cast< vertex_data_t >(0.7) :
					(One - ratio) / static_cast< vertex_data_t >(0.3);

			case TreeCrownShape::InverseConical :
				return One - static_cast< vertex_data_t >(0.8) * ratio;

			case TreeCrownShape::TendFlame :
				return ratio <= static_cast< vertex_data_t >(0.7) ?
					Half + Half * ratio / static_cast< vertex_data_t >(0.7) :
					Half + Half * (One - ratio) / static_cast< vertex_data_t >(0.3);

			default:
				return One;
		}
	}

	/**
	 * @brief Returns the radius of a stem at a position along it.
	 * @note Weber & Penn, SIGGRAPH '95, § 4.4 "Stem radius". `taper` in [0, 1] interpolates from a
	 * cylinder to a cone; in ]1, 2] it rounds the end into a hemisphere.
	 * @warning The periodic range ]2, 3] of the paper (a string of spheres, for cacti) is NOT
	 * implemented: a taper above 2 is clamped to 2.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 * @param baseRadius The radius at the start of the stem.
	 * @param unitPosition The position along the stem, in [0, 1].
	 * @param taper The taper parameter, in [0, 2].
	 * @param stemLength The length of the stem.
	 * @return vertex_data_t
	 */
	template< typename vertex_data_t = float >
	[[nodiscard]]
	vertex_data_t
	treeTaperedRadius (vertex_data_t baseRadius, vertex_data_t unitPosition, vertex_data_t taper, vertex_data_t stemLength) noexcept
		requires (std::is_floating_point_v< vertex_data_t >)
	{
		constexpr auto One = static_cast< vertex_data_t >(1);
		constexpr auto Zero = static_cast< vertex_data_t >(0);

		unitPosition = std::clamp(unitPosition, Zero, One);
		taper = std::clamp(taper, Zero, static_cast< vertex_data_t >(2));

		const auto unitTaper = taper < One ? taper : static_cast< vertex_data_t >(2) - taper;

		const auto conicalRadius = baseRadius * (One - unitTaper * unitPosition);

		if ( taper < One )
		{
			return conicalRadius;
		}

		/* Rounded end: the last 'conicalRadius' of the stem closes as a hemisphere. */
		const auto remaining = (One - unitPosition) * stemLength;

		if ( remaining >= conicalRadius )
		{
			return conicalRadius;
		}

		const auto offset = conicalRadius - remaining;

		return std::sqrt(std::max(Zero, conicalRadius * conicalRadius - offset * offset));
	}

	/**
	 * @brief The parameters describing ONE branching level of a tree.
	 * @note Weber & Penn's `n…` parameters, where n is the level index. Level 0 is the trunk.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 */
	template< typename vertex_data_t = float >
	requires (std::is_floating_point_v< vertex_data_t >)
	class TreeLevelParameters final
	{
		public:

			/**
			 * @brief Constructs default level parameters.
			 */
			TreeLevelParameters () noexcept = default;

			/** @brief Sets the number of stems of this level carried by ONE parent stem. Unused on level 0. @param value The count. @return void */
			void setBranches (uint32_t value) noexcept { m_branches = value; }

			/** @brief Returns the number of stems of this level carried by ONE parent stem. @return uint32_t */
			[[nodiscard]] uint32_t branches () const noexcept { return m_branches; }

			/** @brief Sets the length, relative to the parent stem. @param value The ratio. @return void */
			void setLength (vertex_data_t value) noexcept { m_length = value; }

			/** @brief Returns the length, relative to the parent stem. @return vertex_data_t */
			[[nodiscard]] vertex_data_t length () const noexcept { return m_length; }

			/** @brief Sets the random spread of the length. @param value The spread. @return void */
			void setLengthVariation (vertex_data_t value) noexcept { m_lengthVariation = value; }

			/** @brief Returns the random spread of the length. @return vertex_data_t */
			[[nodiscard]] vertex_data_t lengthVariation () const noexcept { return m_lengthVariation; }

			/** @brief Sets the taper, 0 cylinder, 1 cone, 2 rounded end. @param value The taper. @return void */
			void setTaper (vertex_data_t value) noexcept { m_taper = value; }

			/** @brief Returns the taper. @return vertex_data_t */
			[[nodiscard]] vertex_data_t taper () const noexcept { return m_taper; }

			/** @brief Sets how many segments a stem of this level is cut into (nCurveRes). @param value The count. @return void */
			void setSegmentCount (uint32_t value) noexcept { m_segmentCount = std::max(1U, value); }

			/** @brief Returns how many segments a stem of this level is cut into. @return uint32_t */
			[[nodiscard]] uint32_t segmentCount () const noexcept { return m_segmentCount; }

			/** @brief Sets the total curvature of a stem, in degrees. @param value The angle. @return void */
			void setCurve (vertex_data_t value) noexcept { m_curve = value; }

			/** @brief Returns the total curvature of a stem, in degrees. @return vertex_data_t */
			[[nodiscard]] vertex_data_t curve () const noexcept { return m_curve; }

			/** @brief Sets the curvature of the second half of a stem, in degrees, for an S shape. @param value The angle. @return void */
			void setCurveBack (vertex_data_t value) noexcept { m_curveBack = value; }

			/** @brief Returns the curvature of the second half of a stem, in degrees. @return vertex_data_t */
			[[nodiscard]] vertex_data_t curveBack () const noexcept { return m_curveBack; }

			/** @brief Sets the random spread of the curvature, in degrees. @param value The angle. @return void */
			void setCurveVariation (vertex_data_t value) noexcept { m_curveVariation = value; }

			/** @brief Returns the random spread of the curvature, in degrees. @return vertex_data_t */
			[[nodiscard]] vertex_data_t curveVariation () const noexcept { return m_curveVariation; }

			/** @brief Sets the angle away from the parent axis, in degrees. @param value The angle. @return void */
			void setDownAngle (vertex_data_t value) noexcept { m_downAngle = value; }

			/** @brief Returns the angle away from the parent axis, in degrees. @return vertex_data_t */
			[[nodiscard]] vertex_data_t downAngle () const noexcept { return m_downAngle; }

			/** @brief Sets the spread of the down angle; NEGATIVE makes it vary with the position along the parent. @param value The angle. @return void */
			void setDownAngleVariation (vertex_data_t value) noexcept { m_downAngleVariation = value; }

			/** @brief Returns the spread of the down angle. @return vertex_data_t */
			[[nodiscard]] vertex_data_t downAngleVariation () const noexcept { return m_downAngleVariation; }

			/** @brief Sets the spin around the parent axis between two successive children, in degrees; NEGATIVE alternates sides. @param value The angle. @return void */
			void setRotate (vertex_data_t value) noexcept { m_rotate = value; }

			/** @brief Returns the spin around the parent axis between two successive children. @return vertex_data_t */
			[[nodiscard]] vertex_data_t rotate () const noexcept { return m_rotate; }

			/** @brief Sets the random spread of the spin, in degrees. @param value The angle. @return void */
			void setRotateVariation (vertex_data_t value) noexcept { m_rotateVariation = value; }

			/** @brief Returns the random spread of the spin, in degrees. @return vertex_data_t */
			[[nodiscard]] vertex_data_t rotateVariation () const noexcept { return m_rotateVariation; }

			/** @brief Sets how many times a segment forks, fractional (nSegSplits). @param value The count. @return void */
			void setSegmentSplits (vertex_data_t value) noexcept { m_segmentSplits = std::max(static_cast< vertex_data_t >(0), value); }

			/** @brief Returns how many times a segment forks. @return vertex_data_t */
			[[nodiscard]] vertex_data_t segmentSplits () const noexcept { return m_segmentSplits; }

			/** @brief Sets how many times the FIRST segment of the trunk forks (nBaseSplits). @param value The count. @return void */
			void setBaseSplits (uint32_t value) noexcept { m_baseSplits = value; }

			/** @brief Returns how many times the first segment of the trunk forks. @return uint32_t */
			[[nodiscard]] uint32_t baseSplits () const noexcept { return m_baseSplits; }

			/** @brief Sets the angle a fork opens by, in degrees. @param value The angle. @return void */
			void setSplitAngle (vertex_data_t value) noexcept { m_splitAngle = value; }

			/** @brief Returns the angle a fork opens by, in degrees. @return vertex_data_t */
			[[nodiscard]] vertex_data_t splitAngle () const noexcept { return m_splitAngle; }

			/** @brief Sets the random spread of the fork angle, in degrees. @param value The angle. @return void */
			void setSplitAngleVariation (vertex_data_t value) noexcept { m_splitAngleVariation = value; }

			/** @brief Returns the random spread of the fork angle, in degrees. @return vertex_data_t */
			[[nodiscard]] vertex_data_t splitAngleVariation () const noexcept { return m_splitAngleVariation; }

		private:

			vertex_data_t m_length{0.5};
			vertex_data_t m_lengthVariation{0};
			vertex_data_t m_taper{1};
			vertex_data_t m_curve{0};
			vertex_data_t m_curveBack{0};
			vertex_data_t m_curveVariation{0};
			vertex_data_t m_downAngle{60};
			vertex_data_t m_downAngleVariation{0};
			vertex_data_t m_rotate{140};
			vertex_data_t m_rotateVariation{0};
			vertex_data_t m_segmentSplits{0};
			vertex_data_t m_splitAngle{0};
			vertex_data_t m_splitAngleVariation{0};
			uint32_t m_branches{0};
			uint32_t m_segmentCount{3};
			uint32_t m_baseSplits{0};
	};

	/**
	 * @brief The full description of a tree species, for the parametric grower.
	 * @note This is Weber & Penn's parameter set (*Creation and Rendering of Realistic Trees*,
	 * SIGGRAPH '95), the one the Arbaro and Blender Sapling implementations also speak, so a
	 * published parameter table transfers here with no translation.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 */
	template< typename vertex_data_t = float >
	requires (std::is_floating_point_v< vertex_data_t >)
	class TreeParameters final
	{
		public:

			/** @brief The deepest branching level the paper's parameter set describes. */
			static constexpr uint32_t MaxLevels{4};

			/**
			 * @brief Constructs a default parameter set.
			 */
			TreeParameters () noexcept = default;

			/**
			 * @brief Gives mutable access to one level.
			 * @param levelIndex The level index, 0 for the trunk, clamped to MaxLevels - 1.
			 * @return TreeLevelParameters< vertex_data_t > &
			 */
			[[nodiscard]]
			TreeLevelParameters< vertex_data_t > &
			level (uint32_t levelIndex) noexcept
			{
				return m_levels[std::min(levelIndex, MaxLevels - 1U)];
			}

			/**
			 * @brief Gives access to one level.
			 * @param levelIndex The level index, 0 for the trunk, clamped to MaxLevels - 1.
			 * @return const TreeLevelParameters< vertex_data_t > &
			 */
			[[nodiscard]]
			const TreeLevelParameters< vertex_data_t > &
			level (uint32_t levelIndex) const noexcept
			{
				return m_levels[std::min(levelIndex, MaxLevels - 1U)];
			}

			/** @brief Sets the crown envelope. @param value The shape. @return void */
			void setShape (TreeCrownShape value) noexcept { m_shape = value; }

			/** @brief Returns the crown envelope. @return TreeCrownShape */
			[[nodiscard]] TreeCrownShape shape () const noexcept { return m_shape; }

			/** @brief Sets how many branching levels are grown, 1 to MaxLevels. @param value The count. @return void */
			void setLevels (uint32_t value) noexcept { m_levelCount = std::clamp(value, 1U, MaxLevels); }

			/** @brief Returns how many branching levels are grown. @return uint32_t */
			[[nodiscard]] uint32_t levels () const noexcept { return m_levelCount; }

			/** @brief Sets the height of the tree. @param value The height. @return void */
			void setScale (vertex_data_t value) noexcept { m_scale = value; }

			/** @brief Returns the height of the tree. @return vertex_data_t */
			[[nodiscard]] vertex_data_t scale () const noexcept { return m_scale; }

			/** @brief Sets the random spread of the height. @param value The spread. @return void */
			void setScaleVariation (vertex_data_t value) noexcept { m_scaleVariation = value; }

			/** @brief Returns the random spread of the height. @return vertex_data_t */
			[[nodiscard]] vertex_data_t scaleVariation () const noexcept { return m_scaleVariation; }

			/** @brief Sets the bare fraction at the foot of the trunk, in [0, 1[. @param value The fraction. @return void */
			void setBaseSize (vertex_data_t value) noexcept { m_baseSize = std::clamp(value, static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(0.95)); }

			/** @brief Returns the bare fraction at the foot of the trunk. @return vertex_data_t */
			[[nodiscard]] vertex_data_t baseSize () const noexcept { return m_baseSize; }

			/** @brief Sets the trunk radius as a fraction of the trunk length. @param value The ratio. @return void */
			void setRatio (vertex_data_t value) noexcept { m_ratio = value; }

			/** @brief Returns the trunk radius as a fraction of the trunk length. @return vertex_data_t */
			[[nodiscard]] vertex_data_t ratio () const noexcept { return m_ratio; }

			/** @brief Sets how fast a child stem thins relative to its parent. @param value The exponent. @return void */
			void setRatioPower (vertex_data_t value) noexcept { m_ratioPower = value; }

			/** @brief Returns how fast a child stem thins relative to its parent. @return vertex_data_t */
			[[nodiscard]] vertex_data_t ratioPower () const noexcept { return m_ratioPower; }

			/** @brief Sets how much the trunk widens at its foot. @param value The flare. @return void */
			void setFlare (vertex_data_t value) noexcept { m_flare = value; }

			/** @brief Returns how much the trunk widens at its foot. @return vertex_data_t */
			[[nodiscard]] vertex_data_t flare () const noexcept { return m_flare; }

			/** @brief Sets how strongly branches bend back toward the sky. @param value The strength. @return void */
			void setAttractionUp (vertex_data_t value) noexcept { m_attractionUp = value; }

			/** @brief Returns how strongly branches bend back toward the sky. @return vertex_data_t */
			[[nodiscard]] vertex_data_t attractionUp () const noexcept { return m_attractionUp; }

			/** @brief Sets how many leaves a stem of the deepest level carries. @param value The count. @return void */
			void setLeaves (uint32_t value) noexcept { m_leaves = value; }

			/** @brief Returns how many leaves a stem of the deepest level carries. @return uint32_t */
			[[nodiscard]] uint32_t leaves () const noexcept { return m_leaves; }

			/** @brief Sets the size of one leaf. @param value The size. @return void */
			void setLeafScale (vertex_data_t value) noexcept { m_leafScale = value; }

			/** @brief Returns the size of one leaf. @return vertex_data_t */
			[[nodiscard]] vertex_data_t leafScale () const noexcept { return m_leafScale; }

			/** @brief Sets how many lobes the trunk cross-section has; read by the skinning phase. @param value The count. @return void */
			void setLobes (uint32_t value) noexcept { m_lobes = value; }

			/** @brief Returns how many lobes the trunk cross-section has. @return uint32_t */
			[[nodiscard]] uint32_t lobes () const noexcept { return m_lobes; }

			/** @brief Sets how deep the trunk lobes cut; read by the skinning phase. @param value The depth. @return void */
			void setLobeDepth (vertex_data_t value) noexcept { m_lobeDepth = value; }

			/** @brief Returns how deep the trunk lobes cut. @return vertex_data_t */
			[[nodiscard]] vertex_data_t lobeDepth () const noexcept { return m_lobeDepth; }

			/**
			 * @brief Returns the "Quaking Aspen" set of the paper.
			 * @note Weber & Penn, SIGGRAPH '95, parameter table. A tall, slender aspen: a nearly
			 * straight trunk, many short first-order branches, and a tend-flame crown.
			 * @return TreeParameters
			 */
			[[nodiscard]]
			static
			TreeParameters
			quakingAspen () noexcept
			{
				TreeParameters parameters;

				parameters.setShape(TreeCrownShape::TendFlame);
				parameters.setLevels(3);
				parameters.setScale(13);
				parameters.setScaleVariation(3);
				parameters.setBaseSize(static_cast< vertex_data_t >(0.4));
				parameters.setRatio(static_cast< vertex_data_t >(0.015));
				parameters.setRatioPower(static_cast< vertex_data_t >(1.2));
				parameters.setFlare(static_cast< vertex_data_t >(0.6));
				parameters.setLobes(5);
				parameters.setLobeDepth(static_cast< vertex_data_t >(0.07));
				parameters.setAttractionUp(static_cast< vertex_data_t >(0.5));
				parameters.setLeaves(25);
				parameters.setLeafScale(static_cast< vertex_data_t >(0.17));

				auto & trunk = parameters.level(0);
				trunk.setLength(1);
				trunk.setTaper(1);
				trunk.setSegmentCount(3);
				trunk.setCurve(0);
				trunk.setCurveVariation(20);

				auto & first = parameters.level(1);
				first.setBranches(50);
				first.setLength(static_cast< vertex_data_t >(0.3));
				first.setTaper(1);
				first.setSegmentCount(5);
				first.setCurve(-40);
				first.setCurveVariation(50);
				first.setDownAngle(60);
				first.setDownAngleVariation(-50);
				first.setRotate(140);

				auto & second = parameters.level(2);
				second.setBranches(30);
				second.setLength(static_cast< vertex_data_t >(0.6));
				second.setTaper(1);
				second.setSegmentCount(3);
				second.setCurve(-40);
				second.setCurveVariation(75);
				second.setDownAngle(45);
				second.setDownAngleVariation(10);
				second.setRotate(140);

				auto & third = parameters.level(3);
				third.setBranches(10);
				third.setLength(0);
				third.setTaper(1);
				third.setSegmentCount(1);
				third.setDownAngle(45);
				third.setDownAngleVariation(10);
				third.setRotate(77);

				return parameters;
			}

			/**
			 * @brief Returns a broad, forking deciduous tree.
			 * @warning ⚠️ Designed here, NOT a parameter set from the paper. A spherical crown, a
			 * trunk that forks twice at its base, and strongly curved boughs.
			 * @return TreeParameters
			 */
			[[nodiscard]]
			static
			TreeParameters
			broadleaf () noexcept
			{
				TreeParameters parameters;

				parameters.setShape(TreeCrownShape::Spherical);
				parameters.setLevels(3);
				parameters.setScale(9);
				parameters.setScaleVariation(2);
				parameters.setBaseSize(static_cast< vertex_data_t >(0.25));
				parameters.setRatio(static_cast< vertex_data_t >(0.03));
				parameters.setRatioPower(static_cast< vertex_data_t >(1.1));
				parameters.setFlare(static_cast< vertex_data_t >(1.2));
				parameters.setLobes(7);
				parameters.setLobeDepth(static_cast< vertex_data_t >(0.08));
				parameters.setAttractionUp(static_cast< vertex_data_t >(0.4));
				parameters.setLeaves(30);
				parameters.setLeafScale(static_cast< vertex_data_t >(0.22));

				auto & trunk = parameters.level(0);
				trunk.setLength(1);
				trunk.setTaper(static_cast< vertex_data_t >(1.1));
				trunk.setSegmentCount(6);
				trunk.setCurve(10);
				trunk.setCurveVariation(40);
				trunk.setBaseSplits(2);
				trunk.setSplitAngle(28);
				trunk.setSplitAngleVariation(8);

				auto & first = parameters.level(1);
				first.setBranches(38);
				first.setLength(static_cast< vertex_data_t >(0.45));
				first.setLengthVariation(static_cast< vertex_data_t >(0.1));
				first.setTaper(1);
				first.setSegmentCount(6);
				first.setCurve(-25);
				first.setCurveVariation(60);
				first.setDownAngle(52);
				first.setDownAngleVariation(-40);
				first.setRotate(137);
				first.setSegmentSplits(static_cast< vertex_data_t >(0.3));
				first.setSplitAngle(22);
				first.setSplitAngleVariation(10);

				auto & second = parameters.level(2);
				second.setBranches(24);
				second.setLength(static_cast< vertex_data_t >(0.55));
				second.setTaper(1);
				second.setSegmentCount(4);
				second.setCurve(-30);
				second.setCurveVariation(70);
				second.setDownAngle(48);
				second.setDownAngleVariation(20);
				second.setRotate(137);

				return parameters;
			}

			/**
			 * @brief Returns a narrow conifer.
			 * @warning ⚠️ Designed here, NOT a parameter set from the paper. A conical crown, a
			 * straight unforked trunk, and whorls of nearly horizontal branches.
			 * @return TreeParameters
			 */
			[[nodiscard]]
			static
			TreeParameters
			conifer () noexcept
			{
				TreeParameters parameters;

				parameters.setShape(TreeCrownShape::Conical);
				parameters.setLevels(3);
				parameters.setScale(18);
				parameters.setScaleVariation(4);
				parameters.setBaseSize(static_cast< vertex_data_t >(0.15));
				parameters.setRatio(static_cast< vertex_data_t >(0.018));
				parameters.setRatioPower(static_cast< vertex_data_t >(1.4));
				parameters.setFlare(static_cast< vertex_data_t >(0.8));
				parameters.setLobes(0);
				parameters.setLobeDepth(0);
				parameters.setAttractionUp(static_cast< vertex_data_t >(0.1));
				parameters.setLeaves(40);
				parameters.setLeafScale(static_cast< vertex_data_t >(0.1));

				auto & trunk = parameters.level(0);
				trunk.setLength(1);
				trunk.setTaper(static_cast< vertex_data_t >(1.05));
				trunk.setSegmentCount(8);
				trunk.setCurve(0);
				trunk.setCurveVariation(8);

				auto & first = parameters.level(1);
				first.setBranches(60);
				first.setLength(static_cast< vertex_data_t >(0.28));
				first.setLengthVariation(static_cast< vertex_data_t >(0.05));
				first.setTaper(1);
				first.setSegmentCount(4);
				first.setCurve(-8);
				first.setCurveVariation(20);
				first.setDownAngle(80);
				first.setDownAngleVariation(-20);
				first.setRotate(120);

				auto & second = parameters.level(2);
				second.setBranches(20);
				second.setLength(static_cast< vertex_data_t >(0.4));
				second.setTaper(1);
				second.setSegmentCount(2);
				second.setCurve(-10);
				second.setCurveVariation(30);
				second.setDownAngle(70);
				second.setDownAngleVariation(15);
				second.setRotate(130);

				return parameters;
			}

		private:

			std::array< TreeLevelParameters< vertex_data_t >, MaxLevels > m_levels;
			vertex_data_t m_scale{10};
			vertex_data_t m_scaleVariation{0};
			vertex_data_t m_baseSize{static_cast< vertex_data_t >(0.3)};
			vertex_data_t m_ratio{static_cast< vertex_data_t >(0.02)};
			vertex_data_t m_ratioPower{1};
			vertex_data_t m_flare{0};
			vertex_data_t m_attractionUp{0};
			vertex_data_t m_leafScale{static_cast< vertex_data_t >(0.2)};
			vertex_data_t m_lobeDepth{0};
			uint32_t m_levelCount{3};
			uint32_t m_leaves{20};
			uint32_t m_lobes{0};
			TreeCrownShape m_shape{TreeCrownShape::Spherical};
	};
}
