/*
 * src/VertexFactory/NestedLattice.hpp
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
#include <optional>
#include <span>
#include <vector>

namespace EmEn::Base::VertexFactory
{
	/** @brief The most windows a nested lattice takes: level 15 has cells of 32 768 finest cells. */
	constexpr size_t MaxNestedLatticeLevels{16};

	/** @brief The most lattice nodes (every level's square of nodes, holes included) a nested lattice may hold. */
	constexpr size_t MaxNestedLatticeNodes{size_t{1} << 26U};

	/**
	 * @brief One square window of a nested lattice, in cells of the FINEST lattice.
	 * @note The windows of a nested lattice are given finest first: window k is drawn with square cells of 2^k finest
	 * cells (its own cells), everywhere the window k - 1 does not cover it.
	 */
	struct NestedLatticeWindow final
	{
		/** @brief The first finest cell of the window on X. */
		uint32_t firstX{0};
		/** @brief The first finest cell of the window on Y. */
		uint32_t firstY{0};
		/** @brief The side of the window, in finest cells. */
		uint32_t cells{0};
	};

	/**
	 * @brief A vertex of a nested lattice: one node of one level's lattice.
	 * @note The node sits at the finest coordinates (window.firstX + x × 2^level, window.firstY + y × 2^level) of
	 * that level's window. A node belongs to ONE vertex, the finest level's whose window holds it.
	 */
	struct NestedLatticeVertex final
	{
		/** @brief The level (the window's rank, 0 the finest). */
		uint32_t level{0};
		/** @brief The node's column in the level's own cells, from the window's first corner. */
		uint32_t x{0};
		/** @brief The node's row in the level's own cells, from the window's first corner. */
		uint32_t y{0};
	};

	/**
	 * @brief What a nested lattice holds.
	 */
	struct NestedLatticeCounts final
	{
		size_t vertexCount{0};
		size_t triangleCount{0};
	};

	/**
	 * @brief The triangulation of a nested lattice: its vertices and a triangle list over them.
	 */
	struct NestedLatticeMesh final
	{
		std::vector< NestedLatticeVertex > vertices;
		std::vector< uint32_t > indices;
	};

	/**
	 * @brief Counts the vertices and the triangles buildNestedLattice() gives for these windows, without building.
	 * @note Cold path: the windows are checked like buildNestedLattice() checks them.
	 * @param windows The windows, finest first (see buildNestedLattice()).
	 * @return std::optional< NestedLatticeCounts > The counts, or nothing when the windows are refused (one error is
	 * logged on "VertexFactory::NestedLattice").
	 */
	[[nodiscard]]
	std::optional< NestedLatticeCounts > countNestedLattice (std::span< const NestedLatticeWindow > windows) noexcept;

	/**
	 * @brief Triangulates square windows nested in one another, each drawn with cells twice as large as the one it
	 * surrounds, into ONE watertight triangle list: a regular lattice whose resolution is the finest in the first
	 * window and halves from window to window.
	 *
	 * Where a level meets the finer window it surrounds, each of its cells along that window is cut into a fan of three
	 * triangles around the finer lattice's node in the middle of the shared side, so no side of a triangle ends in the
	 * middle of another one (no T-junction, hence no crack once the nodes take a height). Every other cell is cut into
	 * two triangles along the diagonal (x, y)-(x + 1, y + 1), and every triangle turns the same way:
	 * (x, y) -> (x, y + 1) -> (x + 1, y + 1). A node the windows share is ONE vertex (the finest level's), so two
	 * levels never give the same point two positions.
	 *
	 * A single window gives the plain regular grid: its nodes in row-major order (index = x + y × (cells + 1)) and, cell
	 * by cell in the same order, the triangles (x, y), (x, y + 1), (x + 1, y + 1) then (x, y), (x + 1, y + 1), (x + 1, y).
	 *
	 * @pre 1 to MaxNestedLatticeLevels windows, finest first; window k has at least one cell of its own (2^k finest
	 * cells) and neither end passes 2^32 - 1.
	 * @pre The first corner and the side of window k are multiples of 2^k, and of 2^(k + 1) when a window follows (a
	 * window is made of whole cells of the level around it).
	 * @pre Window k lies inside window k + 1 and is strictly smaller.
	 * @pre A window shares a side with the next one only on the OUTER border: where window k touches a side of
	 * window k + 1, window k + 1 touches the same side of window k + 2 (otherwise a level would meet cells four times
	 * smaller than its own).
	 * @pre The lattices hold at most MaxNestedLatticeNodes nodes together.
	 * @post On success every index is below vertices.size(), and the sizes are the ones countNestedLattice() gives.
	 * @param windows The windows, finest first.
	 * @param mesh Receives the vertices and the triangle list; left EMPTY on a refusal.
	 * @return bool False when a pre-condition is broken: nothing is built and one error is logged on
	 * "VertexFactory::NestedLattice".
	 */
	[[nodiscard]]
	bool buildNestedLattice (std::span< const NestedLatticeWindow > windows, NestedLatticeMesh & mesh) noexcept;
}
