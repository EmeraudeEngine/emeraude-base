/*
 * src/VertexFactory/ShelterMap.hpp
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
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace EmEn::Base::VertexFactory
{
	/** @brief The most cells a side of a shelter map may hold. */
	constexpr size_t MaxShelterMapSide{4096};

	/** @brief The widest passage and the longest fade a shelter map takes, in cells. */
	constexpr float MaxShelterMapReach{1024.0F};

	/** @brief The openness of a cell of the open water. */
	constexpr uint8_t OpenWater{255};

	/** @brief The openness of a cell the open water does not reach. */
	constexpr uint8_t ShelteredWater{0};

	/**
	 * @brief A grid of heights: the height of the cell (column, row) is heights[column + row × columns].
	 */
	struct HeightCells final
	{
		/** @brief The heights, row after row. */
		std::span< const float > heights;
		/** @brief The number of cells in a row. */
		size_t columns{0};
		/** @brief The number of rows. */
		size_t rows{0};
	};

	/**
	 * @brief The knobs of a shelter map.
	 */
	struct ShelterMapOptions final
	{
		/** @brief The level of the water: a cell whose height is UNDER it is water; any other is land, a height that is
		 * not a number included. */
		float waterLevel{0.0F};
		/** @brief The narrowest passage the open water enters, in cells (the passage's width). 0: any passage. */
		float minimumPassage{0.0F};
		/** @brief The distance, in cells, over which the openness falls from open to sheltered once the open water has
		 * gone as far as it can. 0: a sharp limit. */
		float fade{0.0F};
	};

	/**
	 * @brief Tells, for each cell of a grid of heights, how OPEN the water is there: whether the sea that surrounds the
	 * grid reaches it, or land shelters it.
	 *
	 * The grid is taken to lie in the open water: what is beyond its border is the sea, one cell from the border's own
	 * water. From the water cells of the border the open water spreads, cell to cell by their sides, through every
	 * water cell that stands farther from the land than half the narrowest passage — so it does not enter a passage
	 * narrower than that. What it reached is OPEN, and so is the water within that same half passage (plus two cells)
	 * of it or of the sea beyond the border: the strip along a coast, the mouth of a passage it did not enter. Farther
	 * in, the openness falls to SHELTERED over the fade. A basin the
	 * water does not join to the border is sheltered: a lake inside an island, and every water of a grid whose border
	 * is all land.
	 *
	 * Distances are chamfer distances (5 for a side, 7 for a diagonal): within 8 % of the straight line. Water never
	 * passes between two land cells that touch by a corner: a wall drawn in diagonal holds.
	 *
	 * A LAND cell takes the openness of the water cell nearest to it (by sides), so that a filtered read of the map
	 * along a shore mixes values of that shore alone. A grid without any water is open everywhere.
	 *
	 * @pre 1 to MaxShelterMapSide columns and rows, and as many heights as cells.
	 * @pre A water level that is a number; a passage and a fade from 0 to MaxShelterMapReach.
	 * @post On success the map holds one value per cell, in the grid's order: OpenWater, ShelteredWater, or in between
	 * along a fade.
	 * @param cells The grid of heights.
	 * @param options The water level, the narrowest passage and the fade.
	 * @param map Receives the openness of each cell; left EMPTY on a refusal.
	 * @return bool False when a pre-condition is broken: nothing is built and one error is logged on
	 * "VertexFactory::ShelterMap".
	 */
	[[nodiscard]]
	bool buildShelterMap (const HeightCells & cells, const ShelterMapOptions & options, std::vector< uint8_t > & map) noexcept;
}
