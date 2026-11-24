/*
 * src/VertexFactory/ShelterMap.cpp
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

#include "ShelterMap.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Logging/Logging.hpp"

namespace
{
	using namespace EmEn::Base::VertexFactory;

	constexpr auto TracerTag{"VertexFactory::ShelterMap"};

	/** @brief The chamfer cost of a move to a cell that shares a side. */
	constexpr uint32_t Straight{5};

	/** @brief The chamfer cost of a move to a cell that shares a corner. */
	constexpr uint32_t Diagonal{7};

	/** @brief A distance nothing reached. Half the range: a move added to it does not wrap. */
	constexpr uint32_t Unreached{std::numeric_limits< uint32_t >::max() / 2U};

	/** @brief How far beyond half the narrowest passage the water is still fully open, in chamfer units (two cells):
	 * the chamfer distance of a coast's own cells overshoots the half passage by up to that. */
	constexpr uint32_t CoastMargin{2U * Straight};

	/**
	 * @brief Logs why a grid is refused.
	 * @param reason The reason.
	 * @return bool Always false.
	 */
	[[nodiscard]]
	bool
	refuse (const std::string & reason) noexcept
	{
		EmEn::Base::Logging::error(TracerTag, reason);

		return false;
	}

	/** @brief A move from a cell to one of its eight neighbours. */
	struct Move final
	{
		int32_t column{0};
		int32_t row{0};
		uint32_t cost{0};
	};

	/** @brief The four moves by a side. */
	constexpr std::array< Move, 4 > SideMoves{{{.column = -1, .row = 0, .cost = Straight}, {.column = 1, .row = 0, .cost = Straight}, {.column = 0, .row = -1, .cost = Straight}, {.column = 0, .row = 1, .cost = Straight}}};

	/** @brief The eight moves: by a side, then by a corner. */
	constexpr std::array< Move, 8 > Moves{{
		{.column = -1, .row = 0, .cost = Straight}, {.column = 1, .row = 0, .cost = Straight}, {.column = 0, .row = -1, .cost = Straight}, {.column = 0, .row = 1, .cost = Straight},
		{.column = -1, .row = -1, .cost = Diagonal}, {.column = 1, .row = -1, .cost = Diagonal}, {.column = -1, .row = 1, .cost = Diagonal}, {.column = 1, .row = 1, .cost = Diagonal}
	}};

	/**
	 * @brief Builds one shelter map: the working state of buildShelterMap(), one stage a method.
	 */
	class ShelterBuilder final
	{
		public:

			/**
			 * @brief Constructs the builder over a checked grid.
			 * @param cells The grid of heights.
			 * @param options The checked options.
			 */
			ShelterBuilder (const HeightCells & cells, const ShelterMapOptions & options) noexcept
				: m_columns{cells.columns},
				  m_rows{cells.rows},
				  m_halfPassage{static_cast< uint32_t >(std::lround(options.minimumPassage * 0.5F * static_cast< float >(Straight)))},
				  m_fade{static_cast< uint32_t >(std::lround(options.fade * static_cast< float >(Straight)))},
				  m_water(cells.columns * cells.rows, uint8_t{0}),
				  m_open(cells.columns * cells.rows, uint8_t{0}),
				  m_distance(cells.columns * cells.rows, Unreached)
			{
				for ( size_t index = 0; index < m_water.size(); ++index )
				{
					const auto height = cells.heights[index];

					if ( std::isfinite(height) && height < options.waterLevel )
					{
						m_water[index] = 1;
					}
					else
					{
						m_distance[index] = 0;
					}
				}
			}

			/**
			 * @brief Runs every stage.
			 * @param map Receives the openness of each cell.
			 */
			void
			build (std::vector< uint8_t > & map) noexcept
			{
				this->measureLandDistance();
				this->spreadOpenWater();
				this->measureDistanceFromOpenWater();
				this->writeWater(map);
				this->spreadOverLand(map);
			}

		private:

			/**
			 * @brief Returns the neighbour of a cell by a move, or the cell count when the move leaves the grid.
			 * @param index The cell.
			 * @param move The move.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			neighbour (size_t index, const Move & move) const noexcept
			{
				const auto column = static_cast< int64_t >(index % m_columns) + move.column;
				const auto row = static_cast< int64_t >(index / m_columns) + move.row;

				if ( column < 0 || row < 0 || std::cmp_greater_equal(column, m_columns) || std::cmp_greater_equal(row, m_rows) )
				{
					return m_water.size();
				}

				return static_cast< size_t >(column) + (static_cast< size_t >(row) * m_columns);
			}

			/**
			 * @brief Returns whether water passes from a cell to its neighbour by a move.
			 * @note A move by a corner needs one of the two cells beside it to be water: between two land cells that
			 * touch by a corner there is no way.
			 * @param index The cell.
			 * @param move The move.
			 * @param target The neighbour, from neighbour().
			 * @return bool
			 */
			[[nodiscard]]
			bool
			waterPasses (size_t index, const Move & move, size_t target) const noexcept
			{
				if ( target >= m_water.size() || m_water[target] == 0 )
				{
					return false;
				}

				if ( move.column == 0 || move.row == 0 )
				{
					return true;
				}

				const auto beside = this->neighbour(index, Move{.column = move.column, .row = 0, .cost = 0});
				const auto below = this->neighbour(index, Move{.column = 0, .row = move.row, .cost = 0});

				return m_water[beside] != 0 || m_water[below] != 0;
			}

			/**
			 * @brief Lowers a cell's distance to what a neighbour's allows.
			 * @param index The cell.
			 * @param move The move to the neighbour.
			 */
			void
			relaxFromNeighbour (size_t index, const Move & move) noexcept
			{
				const auto source = this->neighbour(index, move);

				if ( source < m_distance.size() )
				{
					m_distance[index] = std::min(m_distance[index], m_distance[source] + move.cost);
				}
			}

			/**
			 * @brief Gives every water cell its chamfer distance to the land, in two sweeps.
			 * @note The grid's surroundings are water: its border is no land.
			 */
			void
			measureLandDistance () noexcept
			{
				static constexpr std::array< Move, 4 > Before{{{.column = -1, .row = 0, .cost = Straight}, {.column = 0, .row = -1, .cost = Straight}, {.column = -1, .row = -1, .cost = Diagonal}, {.column = 1, .row = -1, .cost = Diagonal}}};
				static constexpr std::array< Move, 4 > After{{{.column = 1, .row = 0, .cost = Straight}, {.column = 0, .row = 1, .cost = Straight}, {.column = 1, .row = 1, .cost = Diagonal}, {.column = -1, .row = 1, .cost = Diagonal}}};

				for ( size_t index = 0; index < m_distance.size(); ++index )
				{
					for ( const auto & move : Before )
					{
						this->relaxFromNeighbour(index, move);
					}
				}

				for ( size_t index = m_distance.size(); index-- > 0; )
				{
					for ( const auto & move : After )
					{
						this->relaxFromNeighbour(index, move);
					}
				}
			}

			/**
			 * @brief Marks a cell open and queues it, when the open water enters it: a water cell farther from the
			 * land than half the narrowest passage, not marked yet.
			 * @param index The cell, or the cell count (no cell).
			 */
			void
			openCell (size_t index) noexcept
			{
				if ( index >= m_water.size() || m_water[index] == 0 || m_open[index] != 0 || m_distance[index] <= m_halfPassage )
				{
					return;
				}

				m_open[index] = 1;
				m_queue.push_back(static_cast< uint32_t >(index));
			}

			/**
			 * @brief Spreads the open water from the border, cell to cell by their sides.
			 */
			void
			spreadOpenWater () noexcept
			{
				m_queue.clear();

				for ( size_t column = 0; column < m_columns; ++column )
				{
					this->openCell(column);
					this->openCell(column + ((m_rows - 1) * m_columns));
				}

				for ( size_t row = 0; row < m_rows; ++row )
				{
					this->openCell(row * m_columns);
					this->openCell((m_columns - 1) + (row * m_columns));
				}

				/* NOTE: A work queue: it grows while it is walked (openCell()), each cell enters it once. */
				size_t head = 0;

				while ( head < m_queue.size() )
				{
					const size_t index = m_queue[head];

					++head;

					for ( const auto & move : SideMoves )
					{
						this->openCell(this->neighbour(index, move));
					}
				}
			}

			/**
			 * @brief Gives a water cell of the border the distance of one move from the sea beyond it, unless it is
			 * open already.
			 * @param index The cell.
			 * @param bucket The bucket of that distance.
			 */
			void
			reachFromBeyond (size_t index, std::vector< uint32_t > & bucket) noexcept
			{
				if ( m_water[index] == 0 || m_distance[index] <= Straight )
				{
					return;
				}

				m_distance[index] = Straight;
				bucket.push_back(static_cast< uint32_t >(index));
			}

			/**
			 * @brief Gives every water cell near the open water its chamfer distance to it THROUGH the water, as far
			 * as the fade ends.
			 * @note Dial's buckets: the distances are small integers, one bucket a distance.
			 */
			void
			measureDistanceFromOpenWater () noexcept
			{
				const size_t limit = static_cast< size_t >(m_halfPassage) + CoastMargin + m_fade;

				std::vector< std::vector< uint32_t > > buckets(limit + 1);

				std::ranges::transform(m_open, m_distance.begin(), [] (uint8_t open) noexcept {
					return open != 0 ? uint32_t{0} : Unreached;
				});

				/* The open cells are the sources: the queue of the spread holds each of them once. */
				buckets.front() = std::move(m_queue);

				/* So is the sea beyond the border, one cell from the border's own water: a strip of water along the
				 * border too narrow to hold open cells is still the edge of that sea. */
				if ( Straight <= limit )
				{
					for ( size_t column = 0; column < m_columns; ++column )
					{
						this->reachFromBeyond(column, buckets[Straight]);
						this->reachFromBeyond(column + ((m_rows - 1) * m_columns), buckets[Straight]);
					}

					for ( size_t row = 0; row < m_rows; ++row )
					{
						this->reachFromBeyond(row * m_columns, buckets[Straight]);
						this->reachFromBeyond((m_columns - 1) + (row * m_columns), buckets[Straight]);
					}
				}

				for ( size_t distance = 0; distance < buckets.size(); ++distance )
				{
					/* NOTE: A bucket is only ever appended to by a smaller distance: it is complete when it is walked. */
					for ( const size_t index : buckets[distance] )
					{
						if ( m_distance[index] != distance )
						{
							continue;
						}

						for ( const auto & move : Moves )
						{
							const auto target = this->neighbour(index, move);
							const auto reached = distance + move.cost;

							if ( reached > limit || !this->waterPasses(index, move, target) || m_distance[target] <= reached )
							{
								continue;
							}

							m_distance[target] = static_cast< uint32_t >(reached);
							buckets[reached].push_back(static_cast< uint32_t >(target));
						}
					}
				}
			}

			/**
			 * @brief Returns the openness of a water cell from its distance to the open water.
			 * @param distance The distance, in chamfer units.
			 * @return uint8_t
			 */
			[[nodiscard]]
			uint8_t
			openness (uint32_t distance) const noexcept
			{
				const auto full = m_halfPassage + CoastMargin;

				if ( distance <= full )
				{
					return OpenWater;
				}

				if ( m_fade == 0 || distance >= full + m_fade )
				{
					return ShelteredWater;
				}

				const auto along = static_cast< float >(distance - full) / static_cast< float >(m_fade);
				const auto sheltered = along * along * (3.0F - (2.0F * along));

				return static_cast< uint8_t >(std::lround((1.0F - sheltered) * static_cast< float >(OpenWater)));
			}

			/**
			 * @brief Writes the openness of the water cells.
			 * @param map Receives them; the land cells are left sheltered.
			 */
			void
			writeWater (std::vector< uint8_t > & map) const noexcept
			{
				map.assign(m_water.size(), ShelteredWater);

				for ( size_t index = 0; index < m_water.size(); ++index )
				{
					if ( m_water[index] != 0 )
					{
						map[index] = this->openness(m_distance[index]);
					}
				}
			}

			/**
			 * @brief Gives every land cell the openness of the water cell nearest to it, by sides.
			 * @note A grid without water is open everywhere.
			 * @param map The map, its water cells written.
			 */
			void
			spreadOverLand (std::vector< uint8_t > & map) noexcept
			{
				/* NOTE: m_open is reused: a cell whose value is settled. */
				m_queue.clear();

				for ( size_t index = 0; index < m_water.size(); ++index )
				{
					m_open[index] = m_water[index];

					if ( m_water[index] != 0 )
					{
						m_queue.push_back(static_cast< uint32_t >(index));
					}
				}

				if ( m_queue.empty() )
				{
					map.assign(m_water.size(), OpenWater);

					return;
				}

				/* NOTE: A work queue again: the land cells join it as they are reached. */
				size_t head = 0;

				while ( head < m_queue.size() )
				{
					const size_t index = m_queue[head];

					++head;

					for ( const auto & move : SideMoves )
					{
						const auto target = this->neighbour(index, move);

						if ( target >= m_open.size() || m_open[target] != 0 )
						{
							continue;
						}

						m_open[target] = 1;
						map[target] = map[index];
						m_queue.push_back(static_cast< uint32_t >(target));
					}
				}
			}

			size_t m_columns;
			size_t m_rows;
			uint32_t m_halfPassage;
			uint32_t m_fade;
			std::vector< uint8_t > m_water;
			std::vector< uint8_t > m_open;
			std::vector< uint32_t > m_distance;
			std::vector< uint32_t > m_queue;
	};
}

namespace EmEn::Base::VertexFactory
{
	bool
	buildShelterMap (const HeightCells & cells, const ShelterMapOptions & options, std::vector< uint8_t > & map) noexcept
	{
		map.clear();

		if ( cells.columns == 0 || cells.rows == 0 || cells.columns > MaxShelterMapSide || cells.rows > MaxShelterMapSide )
		{
			return refuse("A shelter map takes 1 to " + std::to_string(MaxShelterMapSide) + " columns and rows, not " + std::to_string(cells.columns) + " x " + std::to_string(cells.rows) + ".");
		}

		if ( cells.heights.size() != cells.columns * cells.rows )
		{
			return refuse("A shelter map of " + std::to_string(cells.columns) + " x " + std::to_string(cells.rows) + " cells was given " + std::to_string(cells.heights.size()) + " heights.");
		}

		if ( !std::isfinite(options.waterLevel) )
		{
			return refuse("The water level of a shelter map is not a number.");
		}

		const auto outOfReach = [] (float cellCount) noexcept {
			return !std::isfinite(cellCount) || cellCount < 0.0F || cellCount > MaxShelterMapReach;
		};

		if ( outOfReach(options.minimumPassage) || outOfReach(options.fade) )
		{
			return refuse("The narrowest passage and the fade of a shelter map go from 0 to " + std::to_string(static_cast< int >(MaxShelterMapReach)) + " cells.");
		}

		ShelterBuilder{cells, options}.build(map);

		return true;
	}
}
