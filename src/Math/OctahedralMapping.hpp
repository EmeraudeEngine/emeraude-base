/*
 * src/Math/OctahedralMapping.hpp
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
#include <type_traits>

/* Local inclusions for usages. */
#include "Vector.hpp"

namespace EmEn::Base::Math
{
	/**
	 * @brief Maps a direction onto the unit square, and back.
	 * @note The octahedral parametrisation folds the unit sphere onto an octahedron and unfolds it
	 * into a square. Against a latitude/longitude map it keeps a roughly uniform density, has no
	 * polar singularity, and costs only absolute values and signs — no trigonometry at all.
	 * @note Reference: Cigolle, Donow, Evangelakos, Mara, McGuire & Meyer, *A Survey of Efficient
	 * Representations for Independent Unit Vectors*, Journal of Computer Graphics Techniques 3(2),
	 * 2014, § 3.3 "Octahedral normal vectors".
	 * @note This is the shared core of an imposter atlas: the bake uses it to know which direction
	 * each cell must be rendered from, and the shader uses it to find the cells a view direction
	 * falls between. They MUST agree, which is why it lives here rather than in either of them.
	 * @note ⚠️⚠️ On the OUTER BORDER of the square the map is 2-to-1: two different border points
	 * denote the very same direction, so two border cells of an atlas hold the SAME view. On an
	 * 8x8 grid, cells (3, 7) and (4, 7) both decode to (0, -0.143, 0.857). This is a property of
	 * the parametrisation, not a defect: the blend still lands on the right direction and its
	 * weights still sum to 1. A baker may skip re-rendering a duplicate; it must NOT try to make
	 * the border cells distinct. Never assert that a cell recognises its own INDEX — assert on
	 * the direction, which is what an imposter actually shows.
	 */

	/**
	 * @brief Maps a direction to a point of the unit square.
	 * @tparam precision_t The type of floating point number. Default float.
	 * @param direction The direction. It does not have to be normalized.
	 * @return Vector< 2, precision_t > A point in [0, 1]².
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	Vector< 2, precision_t >
	octahedralEncode (const Vector< 3, precision_t > & direction) noexcept
		requires (std::is_floating_point_v< precision_t >)
	{
		constexpr auto One = static_cast< precision_t >(1);
		constexpr auto Half = static_cast< precision_t >(0.5);

		const auto norm = std::abs(direction[X]) + std::abs(direction[Y]) + std::abs(direction[Z]);

		if ( norm <= static_cast< precision_t >(0) )
		{
			return {Half, Half};
		}

		/* Project onto the octahedron |x| + |y| + |z| = 1. */
		auto projectedX = direction[X] / norm;
		auto projectedZ = direction[Z] / norm;

		/* ⚠️ The LOWER hemisphere is folded outwards, which is what makes the map continuous
		 * across the equator. Getting this fold wrong is the classic octahedral defect: the
		 * round trip still works for the upper half and fails only below, so a test that samples
		 * one hemisphere passes on broken code. */
		if ( direction[Y] < static_cast< precision_t >(0) )
		{
			const auto foldedX = (One - std::abs(projectedZ)) * (projectedX >= static_cast< precision_t >(0) ? One : -One);
			const auto foldedZ = (One - std::abs(projectedX)) * (projectedZ >= static_cast< precision_t >(0) ? One : -One);

			projectedX = foldedX;
			projectedZ = foldedZ;
		}

		/* [-1, 1] to [0, 1]. */
		return {projectedX * Half + Half, projectedZ * Half + Half};
	}

	/**
	 * @brief Maps a point of the unit square back to a direction.
	 * @tparam precision_t The type of floating point number. Default float.
	 * @param point A reference to a point in [0, 1]².
	 * @return Vector< 3, precision_t > A normalized direction.
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	Vector< 3, precision_t >
	octahedralDecode (const Vector< 2, precision_t > & point) noexcept
		requires (std::is_floating_point_v< precision_t >)
	{
		constexpr auto One = static_cast< precision_t >(1);
		constexpr auto Two = static_cast< precision_t >(2);

		/* [0, 1] back to [-1, 1]. */
		const auto squareX = point[X] * Two - One;
		const auto squareZ = point[Y] * Two - One;

		auto resultX = squareX;
		auto resultZ = squareZ;
		const auto resultY = One - std::abs(squareX) - std::abs(squareZ);

		/* Unfold the lower hemisphere, mirroring octahedralEncode(). */
		if ( resultY < static_cast< precision_t >(0) )
		{
			const auto unfoldedX = (One - std::abs(squareZ)) * (squareX >= static_cast< precision_t >(0) ? One : -One);
			const auto unfoldedZ = (One - std::abs(squareX)) * (squareZ >= static_cast< precision_t >(0) ? One : -One);

			resultX = unfoldedX;
			resultZ = unfoldedZ;
		}

		return Vector< 3, precision_t >{resultX, resultY, resultZ}.normalized();
	}

	/**
	 * @brief The three atlas cells a direction falls between, and how much of each to blend.
	 * @tparam precision_t The type of floating point number. Default float.
	 */
	template< typename precision_t = float >
	struct OctahedralBlend final
	{
		/** @brief The cell coordinates, each in [0, gridSize - 1]. */
		std::array< std::array< uint32_t, 2 >, 3 > cells{};
		/** @brief The weight of each cell. They sum to 1. */
		std::array< precision_t, 3 > weights{};
	};

	/**
	 * @brief Returns the three lattice cells around a point of the unit square, with their barycentric weights.
	 * @note The part octahedralBlend() and hemiOctahedralBlend() share: both maps put the cell CENTRES on the
	 * same (gridSize - 1) lattice, only the direction-to-square step differs.
	 * @tparam precision_t The type of floating point number. Default float.
	 * @param point A reference to a point of the unit square.
	 * @param gridSize The number of cells per side of the atlas. Values below 2 give a single cell.
	 * @return OctahedralBlend< precision_t >
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	OctahedralBlend< precision_t >
	octahedralLatticeBlend (const Vector< 2, precision_t > & point, uint32_t gridSize) noexcept
		requires (std::is_floating_point_v< precision_t >)
	{
		OctahedralBlend< precision_t > blend;

		if ( gridSize < 2 )
		{
			blend.cells.fill({0, 0});
			blend.weights = {static_cast< precision_t >(1), 0, 0};

			return blend;
		}

		/* The cell CENTRES sit on a (gridSize - 1) lattice, so the corner cells hold the extreme
		 * directions and the map covers the whole square with no half-cell margin. */
		const auto lattice = static_cast< precision_t >(gridSize - 1);

		const auto exactX = std::clamp(point[X], static_cast< precision_t >(0), static_cast< precision_t >(1)) * lattice;
		const auto exactZ = std::clamp(point[Y], static_cast< precision_t >(0), static_cast< precision_t >(1)) * lattice;

		const auto baseX = std::min(static_cast< uint32_t >(exactX), gridSize - 2);
		const auto baseZ = std::min(static_cast< uint32_t >(exactZ), gridSize - 2);

		const auto fractionX = exactX - static_cast< precision_t >(baseX);
		const auto fractionZ = exactZ - static_cast< precision_t >(baseZ);

		/* The square cell is cut along its diagonal; which triangle the point lands in decides
		 * which three corners surround it. */
		if ( fractionX + fractionZ <= static_cast< precision_t >(1) )
		{
			blend.cells = {{{baseX, baseZ}, {baseX + 1, baseZ}, {baseX, baseZ + 1}}};
			blend.weights = {static_cast< precision_t >(1) - fractionX - fractionZ, fractionX, fractionZ};
		}
		else
		{
			blend.cells = {{{baseX + 1, baseZ + 1}, {baseX, baseZ + 1}, {baseX + 1, baseZ}}};
			blend.weights = {fractionX + fractionZ - static_cast< precision_t >(1), static_cast< precision_t >(1) - fractionX, static_cast< precision_t >(1) - fractionZ};
		}

		return blend;
	}

	/**
	 * @brief Returns the three nearest atlas cells for a direction, with their blend weights.
	 * @note A square grid splits into triangles, so THREE cells surround any point and their
	 * barycentric coordinates are the natural weights. Blending only the nearest cell makes the
	 * imposter jump as the camera turns; blending four would need a bilinear weight over a quad
	 * that the diagonal already cut in two.
	 * @tparam precision_t The type of floating point number. Default float.
	 * @param direction The view direction. It does not have to be normalized.
	 * @param gridSize The number of cells per side of the atlas. Values below 2 give a single cell.
	 * @return OctahedralBlend< precision_t >
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	OctahedralBlend< precision_t >
	octahedralBlend (const Vector< 3, precision_t > & direction, uint32_t gridSize) noexcept
		requires (std::is_floating_point_v< precision_t >)
	{
		return octahedralLatticeBlend(octahedralEncode(direction), gridSize);
	}

	/**
	 * @brief Returns the direction an atlas cell must be rendered from.
	 * @note The exact inverse of octahedralBlend()'s lattice: the baker and the shader must agree
	 * on where a cell sits, or the imposter shows a neighbouring view.
	 * @tparam precision_t The type of floating point number. Default float.
	 * @param cellX The cell column, in [0, gridSize - 1].
	 * @param cellY The cell row, in [0, gridSize - 1].
	 * @param gridSize The number of cells per side of the atlas.
	 * @return Vector< 3, precision_t > A normalized direction.
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	Vector< 3, precision_t >
	octahedralCellDirection (uint32_t cellX, uint32_t cellY, uint32_t gridSize) noexcept
		requires (std::is_floating_point_v< precision_t >)
	{
		if ( gridSize < 2 )
		{
			return Vector< 3, precision_t >::positiveY();
		}

		const auto lattice = static_cast< precision_t >(gridSize - 1);

		return octahedralDecode(Vector< 2, precision_t >{
			static_cast< precision_t >(std::min(cellX, gridSize - 1)) / lattice,
			static_cast< precision_t >(std::min(cellY, gridSize - 1)) / lattice
		});
	}

	/**
	 * @brief Maps an UPPER-hemisphere direction to a point of the unit square (hemi-octahedral map).
	 * @note The imposter of a tree planted on the ground is only ever seen from above the horizon, so the
	 * atlas spends all its cells there: the upper half of the octahedron, |x| + |z| <= 1 - y, is a diamond,
	 * and turning it by 45° fills the whole square — u = x + z, v = z - x. Unlike the full map there is NO
	 * fold and NO 2-to-1 border: the square's edge is the horizon, every point of it a distinct direction.
	 * The same parametrisation as Ryan Brucks' octahedral imposters (Unreal Engine, "Octahedral Impostors",
	 * 2018), which introduced the hemi variant for foliage.
	 * @note A direction BELOW the horizon is clamped onto it (y set to 0): a tree seen from slightly below
	 * shows its horizon view.
	 * @tparam precision_t The type of floating point number. Default float.
	 * @param direction The direction. It does not have to be normalized.
	 * @return Vector< 2, precision_t > A point in [0, 1]².
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	Vector< 2, precision_t >
	hemiOctahedralEncode (const Vector< 3, precision_t > & direction) noexcept
		requires (std::is_floating_point_v< precision_t >)
	{
		constexpr auto Half = static_cast< precision_t >(0.5);

		const auto clampedY = std::max(direction[Y], static_cast< precision_t >(0));
		const auto norm = std::abs(direction[X]) + clampedY + std::abs(direction[Z]);

		if ( norm <= static_cast< precision_t >(0) )
		{
			return {Half, Half};
		}

		const auto projectedX = direction[X] / norm;
		const auto projectedZ = direction[Z] / norm;

		/* The diamond turned by 45°, then [-1, 1] to [0, 1]. */
		return {(projectedX + projectedZ) * Half + Half, (projectedZ - projectedX) * Half + Half};
	}

	/**
	 * @brief Maps a point of the unit square back to an upper-hemisphere direction.
	 * @tparam precision_t The type of floating point number. Default float.
	 * @param point A reference to a point in [0, 1]².
	 * @return Vector< 3, precision_t > A normalized direction, y >= 0.
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	Vector< 3, precision_t >
	hemiOctahedralDecode (const Vector< 2, precision_t > & point) noexcept
		requires (std::is_floating_point_v< precision_t >)
	{
		constexpr auto One = static_cast< precision_t >(1);
		constexpr auto Two = static_cast< precision_t >(2);
		constexpr auto Half = static_cast< precision_t >(0.5);

		const auto u = point[X] * Two - One;
		const auto v = point[Y] * Two - One;

		/* The inverse of the 45° turn: |x| + |z| = max(|u|, |v|) <= 1, so y never goes negative. */
		const auto resultX = (u - v) * Half;
		const auto resultZ = (u + v) * Half;
		const auto resultY = std::max(One - std::abs(resultX) - std::abs(resultZ), static_cast< precision_t >(0));

		return Vector< 3, precision_t >{resultX, resultY, resultZ}.normalized();
	}

	/**
	 * @brief Returns the three nearest hemi-octahedral atlas cells for a direction, with their blend weights.
	 * @tparam precision_t The type of floating point number. Default float.
	 * @param direction The view direction (from the object toward the eye). It does not have to be normalized.
	 * @param gridSize The number of cells per side of the atlas. Values below 2 give a single cell.
	 * @return OctahedralBlend< precision_t >
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	OctahedralBlend< precision_t >
	hemiOctahedralBlend (const Vector< 3, precision_t > & direction, uint32_t gridSize) noexcept
		requires (std::is_floating_point_v< precision_t >)
	{
		return octahedralLatticeBlend(hemiOctahedralEncode(direction), gridSize);
	}

	/**
	 * @brief Returns the direction a hemi-octahedral atlas cell must be rendered from.
	 * @note The exact inverse of hemiOctahedralBlend()'s lattice. The cells of the outer ring hold the horizon.
	 * @tparam precision_t The type of floating point number. Default float.
	 * @param cellX The cell column, in [0, gridSize - 1].
	 * @param cellY The cell row, in [0, gridSize - 1].
	 * @param gridSize The number of cells per side of the atlas.
	 * @return Vector< 3, precision_t > A normalized direction, y >= 0.
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	Vector< 3, precision_t >
	hemiOctahedralCellDirection (uint32_t cellX, uint32_t cellY, uint32_t gridSize) noexcept
		requires (std::is_floating_point_v< precision_t >)
	{
		if ( gridSize < 2 )
		{
			return Vector< 3, precision_t >::positiveY();
		}

		const auto lattice = static_cast< precision_t >(gridSize - 1);

		return hemiOctahedralDecode(Vector< 2, precision_t >{
			static_cast< precision_t >(std::min(cellX, gridSize - 1)) / lattice,
			static_cast< precision_t >(std::min(cellY, gridSize - 1)) / lattice
		});
	}

	/**
	 * @brief The camera frame an imposter cell is rendered with, and read back with.
	 * @tparam precision_t The type of floating point number. Default float.
	 */
	template< typename precision_t = float >
	struct ImposterCellFrame final
	{
		/** @brief The image's +X, in object space. */
		Vector< 3, precision_t > right;
		/** @brief The image's +Y, in object space. */
		Vector< 3, precision_t > up;
		/** @brief Toward the camera (the cell direction); the camera looks along its opposite. */
		Vector< 3, precision_t > back;
	};

	/**
	 * @brief Returns the camera frame of the view along a direction: right-handed, back = the direction, up = the
	 * world +Y made orthogonal to it.
	 * @note ⚠️ The baker renders a cell with this frame and the shader projects onto the cell's plane with it:
	 * they must build it the SAME way, from the SAME direction (the one hemiOctahedralCellDirection() returns),
	 * or the view turns in its cell. Near the pole the world up is parallel to the direction; the frame then
	 * takes -Z as its up, so the image of the top view keeps -Z (forward) at its top. The frames of the cells
	 * around the pole turn about it — a property of any frame field on a sphere, harmless because each cell
	 * is read with its own frame.
	 * @tparam precision_t The type of floating point number. Default float.
	 * @param direction The direction toward the camera. It does not have to be normalized.
	 * @return ImposterCellFrame< precision_t >
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	ImposterCellFrame< precision_t >
	imposterCellFrame (const Vector< 3, precision_t > & direction) noexcept
		requires (std::is_floating_point_v< precision_t >)
	{
		constexpr auto PoleThreshold = static_cast< precision_t >(0.9999);

		ImposterCellFrame< precision_t > frame;

		frame.back = direction.normalized();

		const auto reference = std::abs(frame.back[Y]) >= PoleThreshold ?
			Vector< 3, precision_t >::negativeZ() :
			Vector< 3, precision_t >::positiveY();

		frame.up = (reference - frame.back * Vector< 3, precision_t >::dotProduct(reference, frame.back)).normalized();
		frame.right = Vector< 3, precision_t >::crossProduct(frame.up, frame.back);

		return frame;
	}
}
