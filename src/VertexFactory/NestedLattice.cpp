/*
 * src/VertexFactory/NestedLattice.cpp
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

#include "NestedLattice.hpp"

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "StaticVector.hpp"

namespace
{
	using EmEn::Base::VertexFactory::MaxNestedLatticeLevels;
	using EmEn::Base::VertexFactory::MaxNestedLatticeNodes;
	using EmEn::Base::VertexFactory::NestedLatticeCounts;
	using EmEn::Base::VertexFactory::NestedLatticeVertex;
	using EmEn::Base::VertexFactory::NestedLatticeWindow;

	constexpr auto TracerTag{"VertexFactory::NestedLattice"};

	/**
	 * @brief A checked window: its ends, the size of its own cells and how many of them a side takes.
	 */
	struct Level final
	{
		uint32_t firstX{0};
		uint32_t firstY{0};
		uint32_t endX{0};
		uint32_t endY{0};
		uint32_t step{1};
		uint32_t quads{0};
	};

	/**
	 * @brief The checked windows of a nested lattice, finest first, and what their lattices hold.
	 */
	struct Plan final
	{
		EmEn::Base::StaticVector< Level, MaxNestedLatticeLevels > storage;
		/** @brief Every level's square of nodes, holes included: the size of the node table. */
		size_t nodeCount{0};

		/**
		 * @brief Returns the checked levels.
		 * @return std::span< const Level >
		 */
		[[nodiscard]]
		std::span< const Level >
		levels () const noexcept
		{
			return {storage.data(), storage.size()};
		}
	};

	/**
	 * @brief A lattice node, in the finest coordinates.
	 */
	struct Node final
	{
		uint32_t x{0};
		uint32_t y{0};
	};

	/**
	 * @brief The vertices of a cell's corners, named as the lattice is read: X to the right, Y downward.
	 */
	struct Corners final
	{
		uint32_t topLeft{0};
		uint32_t bottomLeft{0};
		uint32_t bottomRight{0};
		uint32_t topRight{0};
	};

	/**
	 * @brief Logs why the windows are refused.
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

	/**
	 * @brief Checks one window alone and turns it into a level.
	 * @param window The window.
	 * @param rank Its rank, below MaxNestedLatticeLevels.
	 * @param isLast Whether no window follows it.
	 * @param level Receives the level.
	 * @return bool
	 */
	[[nodiscard]]
	bool
	prepareLevel (const NestedLatticeWindow & window, size_t rank, bool isLast, Level & level) noexcept
	{
		const auto step = uint32_t{1} << static_cast< uint32_t >(rank);
		/* A window another level surrounds is made of whole cells of that level. */
		const auto alignment = isLast ? step : step << 1U;

		if ( window.cells < step || window.cells % alignment != 0 || window.firstX % alignment != 0 || window.firstY % alignment != 0 )
		{
			return refuse(
				"buildNestedLattice(), window " + std::to_string(rank) + " (first " + std::to_string(window.firstX) + ", " + std::to_string(window.firstY) +
				", " + std::to_string(window.cells) + " cells) must hold at least one cell of " + std::to_string(step) +
				" and have its corner and its side on multiples of " + std::to_string(alignment) + " !"
			);
		}

		constexpr auto LastCoordinate = uint64_t{std::numeric_limits< uint32_t >::max()};

		if ( uint64_t{window.firstX} + window.cells > LastCoordinate || uint64_t{window.firstY} + window.cells > LastCoordinate )
		{
			return refuse("buildNestedLattice(), window " + std::to_string(rank) + " ends past the last coordinate !");
		}

		level.firstX = window.firstX;
		level.firstY = window.firstY;
		level.endX = window.firstX + window.cells;
		level.endY = window.firstY + window.cells;
		level.step = step;
		level.quads = window.cells / step;

		return true;
	}

	/**
	 * @brief Checks how consecutive levels sit in one another.
	 * @param levels The levels, each checked alone.
	 * @return bool
	 */
	[[nodiscard]]
	bool
	checkNesting (std::span< const Level > levels) noexcept
	{
		for ( size_t rank = 0; rank + 1 < levels.size(); ++rank )
		{
			const auto & inner = levels[rank];
			const auto & outer = levels[rank + 1];

			if ( inner.firstX < outer.firstX || inner.firstY < outer.firstY || inner.endX > outer.endX || inner.endY > outer.endY || (inner.endX - inner.firstX) >= (outer.endX - outer.firstX) )
			{
				return refuse("buildNestedLattice(), window " + std::to_string(rank) + " must lie inside window " + std::to_string(rank + 1) + " and be smaller !");
			}

			if ( rank + 2 >= levels.size() )
			{
				continue;
			}

			/* A side shared with the next window is the outer border: the level after the next one has no cell there.
			 * Otherwise that level would meet cells four times smaller than its own, which a fan around ONE node cannot
			 * close. */
			const auto & beyond = levels[rank + 2];

			if (
				(inner.firstX == outer.firstX && outer.firstX != beyond.firstX) || (inner.endX == outer.endX && outer.endX != beyond.endX) ||
				(inner.firstY == outer.firstY && outer.firstY != beyond.firstY) || (inner.endY == outer.endY && outer.endY != beyond.endY)
			)
			{
				return refuse("buildNestedLattice(), window " + std::to_string(rank) + " shares a side with window " + std::to_string(rank + 1) + " that is not the outer border !");
			}
		}

		return true;
	}

	/**
	 * @brief Checks the windows and prepares their levels.
	 * @param windows The windows, finest first.
	 * @param plan Receives the levels and the size of the node table.
	 * @return bool False when a pre-condition of buildNestedLattice() is broken (one error logged).
	 */
	[[nodiscard]]
	bool
	preparePlan (std::span< const NestedLatticeWindow > windows, Plan & plan) noexcept
	{
		if ( windows.empty() || windows.size() > MaxNestedLatticeLevels )
		{
			return refuse("buildNestedLattice(), 1 to " + std::to_string(MaxNestedLatticeLevels) + " windows are expected, " + std::to_string(windows.size()) + " given !");
		}

		plan.storage.clear();
		plan.nodeCount = 0;

		for ( size_t rank = 0; rank < windows.size(); ++rank )
		{
			Level level;

			if ( !prepareLevel(windows[rank], rank, rank + 1 == windows.size(), level) )
			{
				return false;
			}

			/* The side is checked alone first: its square then fits 64 bits. */
			const auto side = uint64_t{level.quads} + 1;

			if ( side > MaxNestedLatticeNodes || (side * side) > MaxNestedLatticeNodes - plan.nodeCount )
			{
				return refuse("buildNestedLattice(), the lattices hold more than " + std::to_string(MaxNestedLatticeNodes) + " nodes !");
			}

			plan.nodeCount += static_cast< size_t >(side * side);
			/* In capacity: at most MaxNestedLatticeLevels windows come here. */
			plan.storage.push_back(level);
		}

		return checkNesting(plan.levels());
	}

	/**
	 * @brief Counts what the levels give.
	 * @param levels The checked levels.
	 * @return NestedLatticeCounts
	 */
	[[nodiscard]]
	NestedLatticeCounts
	countLevels (std::span< const Level > levels) noexcept
	{
		NestedLatticeCounts counts;

		for ( size_t rank = 0; rank < levels.size(); ++rank )
		{
			const auto & level = levels[rank];
			const auto quads = size_t{level.quads};

			if ( rank == 0 )
			{
				counts.vertexCount += (quads + 1) * (quads + 1);
				counts.triangleCount += 2 * quads * quads;

				continue;
			}

			/* The finer window is a hole of whole cells: its nodes, border included, are the finer levels' vertices, and
			 * each cell along a side of the hole that is not the level's own border takes a third triangle. */
			const auto & hole = levels[rank - 1];
			const auto holeQuads = size_t{(hole.endX - hole.firstX) / level.step};
			const auto openSides =
				size_t{hole.firstX != level.firstX ? 1U : 0U} + size_t{hole.endX != level.endX ? 1U : 0U} +
				size_t{hole.firstY != level.firstY ? 1U : 0U} + size_t{hole.endY != level.endY ? 1U : 0U};

			counts.vertexCount += ((quads + 1) * (quads + 1)) - ((holeQuads + 1) * (holeQuads + 1));
			counts.triangleCount += (2 * ((quads * quads) - (holeQuads * holeQuads))) + (holeQuads * openSides);
		}

		return counts;
	}

	/**
	 * @brief Appends one triangle to a triangle list.
	 * @param indices A writable reference to the triangle list.
	 * @param vertexA The first vertex.
	 * @param vertexB The second vertex.
	 * @param vertexC The third vertex.
	 */
	void
	pushTriangle (std::vector< uint32_t > & indices, uint32_t vertexA, uint32_t vertexB, uint32_t vertexC) noexcept
	{
		indices.push_back(vertexA);
		indices.push_back(vertexB);
		indices.push_back(vertexC);
	}

	/**
	 * @brief Builds the vertices of every level and the table that gives the vertex of each lattice node, then cuts the
	 * cells with it.
	 * @note It reads the plan it is built from: the plan outlives it.
	 */
	class Builder final
	{
		public:

			/**
			 * @brief Constructs the builder.
			 * @param plan A reference to the checked plan.
			 */
			explicit
			Builder (const Plan & plan) noexcept
				: m_levels{plan.levels()},
				m_table(plan.nodeCount, 0U)
			{

			}

			/**
			 * @brief Gives every node its vertex, finest level first: a node the finer window holds, border included,
			 * takes the finer level's vertex.
			 * @param vertices A writable reference to the vertices, empty.
			 */
			void
			buildVertices (std::vector< NestedLatticeVertex > & vertices) noexcept
			{
				size_t offset = 0;

				m_tableOffsets.clear();

				for ( size_t rank = 0; rank < m_levels.size(); ++rank )
				{
					const auto & level = m_levels[rank];
					const auto side = size_t{level.quads} + 1;

					/* In capacity: one per level, MaxNestedLatticeLevels at most. */
					m_tableOffsets.push_back(offset);

					for ( uint32_t y = 0; y <= level.quads; ++y )
					{
						for ( uint32_t x = 0; x <= level.quads; ++x )
						{
							const Node node{.x = level.firstX + (x * level.step), .y = level.firstY + (y * level.step)};
							const auto slot = offset + x + (y * side);

							if ( rank > 0 && this->isInFinerWindow(rank, node) )
							{
								m_table[slot] = this->vertexAt(rank - 1, node);

								continue;
							}

							m_table[slot] = static_cast< uint32_t >(vertices.size());
							vertices.push_back({.level = static_cast< uint32_t >(rank), .x = x, .y = y});
						}
					}

					offset += side * side;
				}
			}

			/**
			 * @brief Cuts every cell of every level outside the finer window.
			 * @pre buildVertices() was called.
			 * @param indices A writable reference to the triangle list, empty.
			 */
			void
			buildTriangles (std::vector< uint32_t > & indices) const noexcept
			{
				for ( size_t rank = 0; rank < m_levels.size(); ++rank )
				{
					const auto & level = m_levels[rank];

					for ( uint32_t y = 0; y < level.quads; ++y )
					{
						for ( uint32_t x = 0; x < level.quads; ++x )
						{
							this->buildCell(rank, {.x = level.firstX + (x * level.step), .y = level.firstY + (y * level.step)}, indices);
						}
					}
				}
			}

		private:

			/**
			 * @brief Returns whether a node of a level lies in the finer window, border included.
			 * @param rank The level, above 0.
			 * @param node The node.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isInFinerWindow (size_t rank, const Node & node) const noexcept
			{
				const auto & finer = m_levels[rank - 1];

				return node.x >= finer.firstX && node.x <= finer.endX && node.y >= finer.firstY && node.y <= finer.endY;
			}

			/**
			 * @brief Returns the vertex of a node of a level's lattice.
			 * @pre The node is one of that level's lattice, already given its vertex.
			 * @param rank The level.
			 * @param node The node.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			vertexAt (size_t rank, const Node & node) const noexcept
			{
				const auto & level = m_levels[rank];
				const auto side = size_t{level.quads} + 1;
				const auto x = size_t{(node.x - level.firstX) / level.step};
				const auto y = size_t{(node.y - level.firstY) / level.step};

				return m_table[m_tableOffsets[rank] + x + (y * side)];
			}

			/**
			 * @brief Cuts one cell of a level, unless the finer window covers it.
			 * @param rank The level.
			 * @param first The cell's first corner.
			 * @param indices A writable reference to the triangle list.
			 */
			void
			buildCell (size_t rank, const Node & first, std::vector< uint32_t > & indices) const noexcept
			{
				const auto & level = m_levels[rank];
				const Node next{.x = first.x + level.step, .y = first.y + level.step};

				if ( rank > 0 )
				{
					const auto & finer = m_levels[rank - 1];

					if ( first.x >= finer.firstX && next.x <= finer.endX && first.y >= finer.firstY && next.y <= finer.endY )
					{
						return;
					}
				}

				const Corners corners{
					.topLeft = this->vertexAt(rank, first),
					.bottomLeft = this->vertexAt(rank, {.x = first.x, .y = next.y}),
					.bottomRight = this->vertexAt(rank, next),
					.topRight = this->vertexAt(rank, {.x = next.x, .y = first.y})
				};

				if ( rank > 0 && this->buildCellAlongFinerWindow(rank, first, corners, indices) )
				{
					return;
				}

				pushTriangle(indices, corners.topLeft, corners.bottomLeft, corners.bottomRight);
				pushTriangle(indices, corners.topLeft, corners.bottomRight, corners.topRight);
			}

			/**
			 * @brief Cuts a cell that lies along the finer window: the middle of the side they share is a node of the finer
			 * lattice, so the cell becomes a fan of three triangles around it, turning like the two triangles of a plain
			 * cell. Without it that node would end two finer sides in the middle of one coarse side: a T-junction.
			 * @pre The cell is outside the finer window. It then shares at most one side with it (the window is a square).
			 * @param rank The level, above 0.
			 * @param first The cell's first corner.
			 * @param corners The cell's corners.
			 * @param indices A writable reference to the triangle list.
			 * @return bool False when the cell shares no side with the finer window: nothing is appended.
			 */
			[[nodiscard]]
			bool
			buildCellAlongFinerWindow (size_t rank, const Node & first, const Corners & corners, std::vector< uint32_t > & indices) const noexcept
			{
				const auto & level = m_levels[rank];
				const auto & finer = m_levels[rank - 1];
				const auto half = level.step / 2U;
				const Node next{.x = first.x + level.step, .y = first.y + level.step};
				const auto besideOnY = first.y >= finer.firstY && next.y <= finer.endY;
				const auto besideOnX = first.x >= finer.firstX && next.x <= finer.endX;

				if ( besideOnY && first.x == finer.endX )
				{
					/* The window is on the left: the node cuts the side topLeft-bottomLeft. */
					const auto middle = this->vertexAt(rank - 1, {.x = first.x, .y = first.y + half});

					pushTriangle(indices, middle, corners.bottomLeft, corners.bottomRight);
					pushTriangle(indices, middle, corners.bottomRight, corners.topRight);
					pushTriangle(indices, middle, corners.topRight, corners.topLeft);

					return true;
				}

				if ( besideOnX && next.y == finer.firstY )
				{
					/* The window is below: the node cuts the side bottomLeft-bottomRight. */
					const auto middle = this->vertexAt(rank - 1, {.x = first.x + half, .y = next.y});

					pushTriangle(indices, middle, corners.bottomRight, corners.topRight);
					pushTriangle(indices, middle, corners.topRight, corners.topLeft);
					pushTriangle(indices, middle, corners.topLeft, corners.bottomLeft);

					return true;
				}

				if ( besideOnY && next.x == finer.firstX )
				{
					/* The window is on the right: the node cuts the side bottomRight-topRight. */
					const auto middle = this->vertexAt(rank - 1, {.x = next.x, .y = first.y + half});

					pushTriangle(indices, middle, corners.topRight, corners.topLeft);
					pushTriangle(indices, middle, corners.topLeft, corners.bottomLeft);
					pushTriangle(indices, middle, corners.bottomLeft, corners.bottomRight);

					return true;
				}

				if ( besideOnX && first.y == finer.endY )
				{
					/* The window is above: the node cuts the side topRight-topLeft. */
					const auto middle = this->vertexAt(rank - 1, {.x = first.x + half, .y = first.y});

					pushTriangle(indices, middle, corners.topLeft, corners.bottomLeft);
					pushTriangle(indices, middle, corners.bottomLeft, corners.bottomRight);
					pushTriangle(indices, middle, corners.bottomRight, corners.topRight);

					return true;
				}

				return false;
			}

			std::span< const Level > m_levels;
			std::vector< uint32_t > m_table;
			EmEn::Base::StaticVector< size_t, MaxNestedLatticeLevels > m_tableOffsets;
	};
}

namespace EmEn::Base::VertexFactory
{
	std::optional< NestedLatticeCounts >
	countNestedLattice (std::span< const NestedLatticeWindow > windows) noexcept
	{
		Plan plan;

		if ( !preparePlan(windows, plan) )
		{
			return std::nullopt;
		}

		return countLevels(plan.levels());
	}

	bool
	buildNestedLattice (std::span< const NestedLatticeWindow > windows, NestedLatticeMesh & mesh) noexcept
	{
		mesh.vertices.clear();
		mesh.indices.clear();

		Plan plan;

		if ( !preparePlan(windows, plan) )
		{
			return false;
		}

		const auto counts = countLevels(plan.levels());

		mesh.vertices.reserve(counts.vertexCount);
		mesh.indices.reserve(counts.triangleCount * 3);

		Builder builder{plan};
		builder.buildVertices(mesh.vertices);
		builder.buildTriangles(mesh.indices);

		/* The count is what a caller sizes its budget on: the two must agree. */
		if ( mesh.vertices.size() != counts.vertexCount || mesh.indices.size() != counts.triangleCount * 3 )
		{
			mesh.vertices.clear();
			mesh.indices.clear();

			return refuse("buildNestedLattice(), the lattice built does not hold what was counted !");
		}

		return true;
	}
}
