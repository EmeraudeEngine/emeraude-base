/*
 * src/Testing/test_VertexFactoryNestedLattice.cpp
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
 */

#include <gtest/gtest.h>

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "DiagnosticsCapture.hpp"
#include "Logging/Severity.hpp"
#include "VertexFactory/NestedLattice.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::VertexFactory;

namespace
{
	constexpr auto LatticeTag{"VertexFactory::NestedLattice"};

	using Windows = std::vector< NestedLatticeWindow >;

	/** @brief A lattice node in the finest coordinates. */
	struct Node final
	{
		int64_t x{0};
		int64_t y{0};
	};

	/** @brief What the triangles of a lattice are made of. */
	struct TriangleCensus final
	{
		/** @brief How many times each side is drawn, from its first vertex to its second. */
		std::unordered_map< uint64_t, uint32_t > directedSides;
		/** @brief Twice the area the triangles cover, in finest cells. */
		int64_t doubledArea{0};
		/** @brief Triangles naming a vertex that does not exist. */
		size_t wrongIndices{0};
		/** @brief Triangles that are flat or turn the other way. */
		size_t wrongTriangles{0};
	};

	/** @brief What the sides of the triangles of a lattice are. */
	struct SideCensus final
	{
		/** @brief Sides drawn twice the same way. */
		size_t repeated{0};
		/** @brief Sides inside the lattice no other triangle meets: a T-junction or a hole. */
		size_t open{0};
		/** @brief The length of the sides no other triangle meets that lie on the outer border, in finest cells. */
		int64_t borderLength{0};
	};

	/** @brief Packs two 32-bit values into one key. */
	[[nodiscard]]
	uint64_t
	key (uint32_t high, uint32_t low) noexcept
	{
		return (uint64_t{high} << 32U) | uint64_t{low};
	}

	/**
	 * @brief Returns where every vertex stands in the finest coordinates.
	 * @param windows The windows the lattice was built from.
	 * @param mesh The lattice.
	 * @param outOfLattice Receives how many vertices name a level or a node the windows do not have.
	 * @return std::vector< Node > One node per vertex.
	 */
	[[nodiscard]]
	std::vector< Node >
	placeVertices (std::span< const NestedLatticeWindow > windows, const NestedLatticeMesh & mesh, size_t & outOfLattice)
	{
		std::vector< Node > nodes;

		nodes.reserve(mesh.vertices.size());
		outOfLattice = 0;

		for ( const auto & vertex : mesh.vertices )
		{
			if ( vertex.level >= windows.size() )
			{
				++outOfLattice;
				nodes.push_back({.x = -1, .y = -1});

				continue;
			}

			const auto & window = windows[vertex.level];
			const auto offsetX = int64_t{vertex.x} << vertex.level;
			const auto offsetY = int64_t{vertex.y} << vertex.level;

			if ( offsetX > int64_t{window.cells} || offsetY > int64_t{window.cells} )
			{
				++outOfLattice;
			}

			nodes.push_back({.x = int64_t{window.firstX} + offsetX, .y = int64_t{window.firstY} + offsetY});
		}

		return nodes;
	}

	/** @brief Returns how many vertices stand on a node another vertex already stands on. */
	[[nodiscard]]
	size_t
	countRepeatedNodes (const std::vector< Node > & nodes)
	{
		std::unordered_set< uint64_t > distinct;
		size_t repeated = 0;

		for ( const auto & node : nodes )
		{
			if ( !distinct.insert(key(static_cast< uint32_t >(node.x), static_cast< uint32_t >(node.y))).second )
			{
				++repeated;
			}
		}

		return repeated;
	}

	/** @brief Goes through the triangles of a lattice: their sides, their winding, the area they cover. */
	[[nodiscard]]
	TriangleCensus
	takeTriangleCensus (const NestedLatticeMesh & mesh, const std::vector< Node > & nodes)
	{
		TriangleCensus census;

		census.directedSides.reserve(mesh.indices.size());

		for ( size_t index = 0; index + 2 < mesh.indices.size(); index += 3 )
		{
			const auto vertexA = mesh.indices[index];
			const auto vertexB = mesh.indices[index + 1];
			const auto vertexC = mesh.indices[index + 2];

			if ( vertexA >= nodes.size() || vertexB >= nodes.size() || vertexC >= nodes.size() )
			{
				++census.wrongIndices;

				continue;
			}

			const auto & nodeA = nodes[vertexA];
			const auto & nodeB = nodes[vertexB];
			const auto & nodeC = nodes[vertexC];
			/* (x, y) -> (x, y + 1) -> (x + 1, y + 1) is negative here. */
			const auto cross = ((nodeB.x - nodeA.x) * (nodeC.y - nodeA.y)) - ((nodeB.y - nodeA.y) * (nodeC.x - nodeA.x));

			if ( cross >= 0 )
			{
				++census.wrongTriangles;
			}

			census.doubledArea -= cross;

			++census.directedSides[key(vertexA, vertexB)];
			++census.directedSides[key(vertexB, vertexC)];
			++census.directedSides[key(vertexC, vertexA)];
		}

		return census;
	}

	/** @brief Goes through the sides of a lattice: each one has its opposite, unless it lies on the outer border. */
	[[nodiscard]]
	SideCensus
	takeSideCensus (const TriangleCensus & triangles, const std::vector< Node > & nodes, const NestedLatticeWindow & outer)
	{
		const auto firstX = int64_t{outer.firstX};
		const auto firstY = int64_t{outer.firstY};
		const auto endX = firstX + int64_t{outer.cells};
		const auto endY = firstY + int64_t{outer.cells};
		SideCensus census;

		for ( const auto & [side, count] : triangles.directedSides )
		{
			if ( count != 1 )
			{
				++census.repeated;
			}

			const auto from = static_cast< uint32_t >(side >> 32U);
			const auto to = static_cast< uint32_t >(side & 0xFFFFFFFFU);

			if ( triangles.directedSides.contains(key(to, from)) )
			{
				continue;
			}

			const auto & nodeA = nodes[from];
			const auto & nodeB = nodes[to];
			const auto alongX = nodeA.y == nodeB.y && (nodeA.y == firstY || nodeA.y == endY);
			const auto alongY = nodeA.x == nodeB.x && (nodeA.x == firstX || nodeA.x == endX);

			if ( alongX )
			{
				census.borderLength += std::max(nodeA.x, nodeB.x) - std::min(nodeA.x, nodeB.x);
			}
			else if ( alongY )
			{
				census.borderLength += std::max(nodeA.y, nodeB.y) - std::min(nodeA.y, nodeB.y);
			}
			else
			{
				++census.open;
			}
		}

		return census;
	}

	/**
	 * @brief Builds a nested lattice and checks it holds what the count announces, one vertex per node.
	 * @param windows The windows, finest first.
	 * @param mesh Receives the lattice.
	 * @param nodes Receives where every vertex stands.
	 */
	void
	expectCountedLattice (const Windows & windows, NestedLatticeMesh & mesh, std::vector< Node > & nodes)
	{
		ASSERT_TRUE(buildNestedLattice(windows, mesh));
		ASSERT_EQ(mesh.indices.size() % 3, 0U);

		const auto counts = countNestedLattice(windows);

		if ( !counts.has_value() )
		{
			ADD_FAILURE() << "the count refuses windows the build accepts";

			return;
		}

		EXPECT_EQ(mesh.vertices.size(), counts->vertexCount);
		EXPECT_EQ(mesh.indices.size(), counts->triangleCount * 3);

		size_t outOfLattice = 0;

		nodes = placeVertices(windows, mesh, outOfLattice);

		EXPECT_EQ(outOfLattice, 0U);
		EXPECT_EQ(countRepeatedNodes(nodes), 0U) << "a node has two vertices";
	}

	/** @brief Checks the triangles name existing vertices, turn one way and cover the outer window exactly once. */
	void
	expectSoundTriangles (const TriangleCensus & triangles, const NestedLatticeWindow & outer)
	{
		EXPECT_EQ(triangles.wrongIndices, 0U);
		EXPECT_EQ(triangles.wrongTriangles, 0U) << "flat or reversed triangles";
		EXPECT_EQ(triangles.doubledArea, 2 * int64_t{outer.cells} * int64_t{outer.cells});
	}

	/** @brief Checks every side is met by exactly one other triangle, unless it lies on the outer border. */
	void
	expectClosedSides (const SideCensus & sides, const NestedLatticeWindow & outer)
	{
		EXPECT_EQ(sides.repeated, 0U) << "sides drawn twice the same way";
		EXPECT_EQ(sides.open, 0U) << "sides inside the lattice no other triangle meets (a T-junction or a hole)";
		/* The sides without an opposite are the outer border, whole (finer cells where a finer window touches it). */
		EXPECT_EQ(sides.borderLength, 4 * int64_t{outer.cells});
	}

	/**
	 * @brief Builds a nested lattice and checks everything a traced surface needs of it: the counts announced, one
	 * vertex per node, no degenerate triangle, one winding, the whole outer window covered exactly once, and every side
	 * of every triangle met by exactly one other triangle unless it lies on the outer border (no T-junction, no hole).
	 * @param windows The windows, finest first.
	 * @param mesh Receives the lattice, for further checks.
	 */
	void
	expectWatertight (const Windows & windows, NestedLatticeMesh & mesh)
	{
		std::vector< Node > nodes;

		expectCountedLattice(windows, mesh, nodes);

		if ( nodes.size() != mesh.vertices.size() || windows.empty() )
		{
			return;
		}

		const auto & outer = windows.back();
		const auto triangles = takeTriangleCensus(mesh, nodes);

		expectSoundTriangles(triangles, outer);
		expectClosedSides(takeSideCensus(triangles, nodes, outer), outer);
	}

	/** @brief Returns the vertices of a lattice as {level, x, y} rows. */
	[[nodiscard]]
	std::vector< std::array< uint32_t, 3 > >
	rowsOf (const NestedLatticeMesh & mesh)
	{
		std::vector< std::array< uint32_t, 3 > > rows;

		rows.reserve(mesh.vertices.size());

		for ( const auto & vertex : mesh.vertices )
		{
			rows.push_back({vertex.level, vertex.x, vertex.y});
		}

		return rows;
	}

	/** @brief Returns the nodes of a plain grid of `cells` cells per side as {0, x, y} rows, in row-major order. */
	[[nodiscard]]
	std::vector< std::array< uint32_t, 3 > >
	plainGridRows (uint32_t cells)
	{
		std::vector< std::array< uint32_t, 3 > > rows;

		for ( uint32_t y = 0; y <= cells; ++y )
		{
			for ( uint32_t x = 0; x <= cells; ++x )
			{
				rows.push_back({0U, x, y});
			}
		}

		return rows;
	}

	/**
	 * @brief Returns the triangle list of a plain grid of `cells` cells per side: the cells in row-major order, each cut
	 * along (x, y)-(x + 1, y + 1) into (x, y), (x, y + 1), (x + 1, y + 1) then (x, y), (x + 1, y + 1), (x + 1, y).
	 */
	[[nodiscard]]
	std::vector< uint32_t >
	plainGridIndices (uint32_t cells)
	{
		const auto side = cells + 1;
		std::vector< uint32_t > indices;

		for ( uint32_t y = 0; y < cells; ++y )
		{
			for ( uint32_t x = 0; x < cells; ++x )
			{
				const auto topLeft = x + (y * side);
				const auto topRight = (x + 1) + (y * side);
				const auto bottomLeft = x + ((y + 1) * side);
				const auto bottomRight = (x + 1) + ((y + 1) * side);

				indices.insert(indices.end(), {topLeft, bottomLeft, bottomRight, topLeft, bottomRight, topRight});
			}
		}

		return indices;
	}

	/** @brief Returns the index of the vertex of a level's node, or nothing. */
	[[nodiscard]]
	std::optional< uint32_t >
	vertexOf (const NestedLatticeMesh & mesh, const std::array< uint32_t, 3 > & row) noexcept
	{
		for ( size_t index = 0; index < mesh.vertices.size(); ++index )
		{
			const auto & vertex = mesh.vertices[index];

			if ( vertex.level == row[0] && vertex.x == row[1] && vertex.y == row[2] )
			{
				return static_cast< uint32_t >(index);
			}
		}

		return std::nullopt;
	}

	/**
	 * @brief Returns how many cells of the finest window are NOT cut along (x, y)-(x + 1, y + 1), drawn both ways.
	 * @param mesh The lattice.
	 * @param cells The side of the finest window, in its cells.
	 */
	[[nodiscard]]
	size_t
	countCellsWithoutTheGridDiagonal (const NestedLatticeMesh & mesh, uint32_t cells)
	{
		std::unordered_set< uint64_t > directedSides;

		for ( size_t index = 0; index + 2 < mesh.indices.size(); index += 3 )
		{
			directedSides.insert(key(mesh.indices[index], mesh.indices[index + 1]));
			directedSides.insert(key(mesh.indices[index + 1], mesh.indices[index + 2]));
			directedSides.insert(key(mesh.indices[index + 2], mesh.indices[index]));
		}

		size_t missing = 0;

		for ( uint32_t y = 0; y < cells; ++y )
		{
			for ( uint32_t x = 0; x < cells; ++x )
			{
				const auto topLeft = vertexOf(mesh, {0U, x, y});
				const auto bottomRight = vertexOf(mesh, {0U, x + 1, y + 1});

				if ( !topLeft.has_value() || !bottomRight.has_value() || !directedSides.contains(key(*topLeft, *bottomRight)) || !directedSides.contains(key(*bottomRight, *topLeft)) )
				{
					++missing;
				}
			}
		}

		return missing;
	}

	/**
	 * @brief Returns how many vertices of the second level stand in the finest window, border included.
	 * @param windows The windows, two at least.
	 * @param mesh The lattice.
	 */
	[[nodiscard]]
	size_t
	countCoarseVerticesInTheFinestWindow (const Windows & windows, const NestedLatticeMesh & mesh)
	{
		const auto & finest = windows.front();
		const auto & coarse = windows[1];
		size_t inside = 0;

		for ( const auto & vertex : mesh.vertices )
		{
			if ( vertex.level != 1 )
			{
				continue;
			}

			const auto x = coarse.firstX + (vertex.x * 2U);
			const auto y = coarse.firstY + (vertex.y * 2U);

			if ( x >= finest.firstX && x <= finest.firstX + finest.cells && y >= finest.firstY && y <= finest.firstY + finest.cells )
			{
				++inside;
			}
		}

		return inside;
	}

	/** @brief Returns one window more than a lattice takes, each well nested in the next. */
	[[nodiscard]]
	Windows
	moreWindowsThanALatticeTakes ()
	{
		Windows windows;

		for ( uint32_t rank = 0; rank <= MaxNestedLatticeLevels; ++rank )
		{
			windows.push_back({.firstX = 0, .firstY = 0, .cells = uint32_t{1} << (rank + 2U)});
		}

		return windows;
	}

	/** @brief Checks windows are refused by the build, which then leaves the mesh empty, and by the count. */
	void
	expectRefused (const Windows & windows)
	{
		NestedLatticeMesh mesh;

		mesh.vertices.push_back({});
		mesh.indices.push_back(0);

		EXPECT_FALSE(buildNestedLattice(windows, mesh));
		EXPECT_TRUE(mesh.vertices.empty());
		EXPECT_TRUE(mesh.indices.empty());
		EXPECT_FALSE(countNestedLattice(windows).has_value());
	}
}

TEST(VertexFactoryNestedLattice, ASingleWindowIsThePlainRegularGrid)
{
	const Windows windows{{.firstX = 0, .firstY = 0, .cells = 4}};
	NestedLatticeMesh mesh;

	ASSERT_TRUE(buildNestedLattice(windows, mesh));

	/* The nodes in row-major order; the cells in the same order, each along (x, y)-(x + 1, y + 1), both triangles
	 * turning the same way. */
	EXPECT_EQ(rowsOf(mesh), plainGridRows(4U));
	EXPECT_EQ(mesh.indices, plainGridIndices(4U));

	const auto counts = countNestedLattice(windows).value_or(NestedLatticeCounts{});

	EXPECT_EQ(counts.vertexCount, 25U);
	EXPECT_EQ(counts.triangleCount, 32U);
}

TEST(VertexFactoryNestedLattice, NestedWindowsAreWatertight)
{
	const std::vector< Windows > cases{
		/* One window. */
		{{.firstX = 0, .firstY = 0, .cells = 8}},
		/* Two levels, centred. */
		{{.firstX = 4, .firstY = 4, .cells = 8}, {.firstX = 0, .firstY = 0, .cells = 16}},
		/* A ring one cell thick. */
		{{.firstX = 2, .firstY = 2, .cells = 4}, {.firstX = 0, .firstY = 0, .cells = 8}},
		/* Three levels, centred. */
		{{.firstX = 12, .firstY = 12, .cells = 8}, {.firstX = 8, .firstY = 8, .cells = 16}, {.firstX = 0, .firstY = 0, .cells = 32}},
		/* Three levels, off centre and not on the same centre. */
		{{.firstX = 6, .firstY = 18, .cells = 4}, {.firstX = 4, .firstY = 16, .cells = 8}, {.firstX = 0, .firstY = 0, .cells = 32}},
		/* Every window against the outer border on one side. */
		{{.firstX = 0, .firstY = 12, .cells = 8}, {.firstX = 0, .firstY = 8, .cells = 16}, {.firstX = 0, .firstY = 0, .cells = 32}},
		/* Every window in the outer border's corner. */
		{{.firstX = 0, .firstY = 0, .cells = 8}, {.firstX = 0, .firstY = 0, .cells = 16}, {.firstX = 0, .firstY = 0, .cells = 32}},
		/* Only the last two windows share sides (the far corner). */
		{{.firstX = 20, .firstY = 20, .cells = 8}, {.firstX = 16, .firstY = 16, .cells = 16}, {.firstX = 0, .firstY = 0, .cells = 32}},
		/* Four levels, the outer window away from the origin. */
		{{.firstX = 40, .firstY = 48, .cells = 4}, {.firstX = 36, .firstY = 44, .cells = 12}, {.firstX = 32, .firstY = 40, .cells = 24}, {.firstX = 16, .firstY = 16, .cells = 64}}
	};

	for ( size_t index = 0; index < cases.size(); ++index )
	{
		SCOPED_TRACE("case " + std::to_string(index));

		NestedLatticeMesh mesh;

		expectWatertight(cases[index], mesh);
	}
}

TEST(VertexFactoryNestedLattice, TheFinestWindowKeepsTheGridDiagonalAndSharedNodesAreTheFinestLevels)
{
	const Windows windows{{.firstX = 4, .firstY = 4, .cells = 8}, {.firstX = 0, .firstY = 0, .cells = 16}};
	NestedLatticeMesh mesh;

	expectWatertight(windows, mesh);

	/* Every cell of the finest window is cut along (x, y)-(x + 1, y + 1). */
	EXPECT_EQ(countCellsWithoutTheGridDiagonal(mesh, 8U), 0U);

	/* The coarse level owns no node of the finer window, border included: its nodes there are the finest level's. */
	EXPECT_EQ(countCoarseVerticesInTheFinestWindow(windows, mesh), 0U);

	/* 81 finest nodes, 81 coarse ones less the 25 the window holds; 128 finest triangles, 2 × (64 - 16) coarse ones
	 * and one more for each of the 16 cells along the window. */
	EXPECT_EQ(mesh.vertices.size(), 81U + 56U);
	EXPECT_EQ(mesh.indices.size() / 3, 128U + 96U + 16U);
}

TEST(VertexFactoryNestedLattice, TheCountIsWhatALargeLatticeHolds)
{
	/* A 2048-cell window at cells of 4, a 416-cell window at cells of 2 and a 208-cell window at the finest cells. */
	const Windows windows{{.firstX = 920, .firstY = 920, .cells = 208}, {.firstX = 816, .firstY = 816, .cells = 416}, {.firstX = 0, .firstY = 0, .cells = 2048}};
	const auto counts = countNestedLattice(windows).value_or(NestedLatticeCounts{});

	EXPECT_EQ(counts.triangleCount, 86528U + 65312U + 503072U);
	EXPECT_EQ(counts.vertexCount, (209U * 209U) + ((209U * 209U) - (105U * 105U)) + ((513U * 513U) - (105U * 105U)));

	NestedLatticeMesh mesh;

	expectWatertight(windows, mesh);
}

TEST(VertexFactoryNestedLattice, BrokenWindowsAreRefusedAndNothingIsBuilt)
{
	EmEn::Base::Testing::DiagnosticsCapture diagnosticsCapture;

	std::vector< Windows > cases{
		/* No window. */
		{},
		/* A window without a cell. */
		{{.firstX = 0, .firstY = 0, .cells = 0}},
		/* A surrounded window that is not made of whole cells of the next level. */
		{{.firstX = 1, .firstY = 2, .cells = 4}, {.firstX = 0, .firstY = 0, .cells = 16}},
		{{.firstX = 2, .firstY = 2, .cells = 5}, {.firstX = 0, .firstY = 0, .cells = 16}},
		/* A last window that is not made of whole cells of its own. */
		{{.firstX = 2, .firstY = 2, .cells = 4}, {.firstX = 0, .firstY = 0, .cells = 9}},
		/* A window that leaves the next one. */
		{{.firstX = 12, .firstY = 12, .cells = 8}, {.firstX = 0, .firstY = 0, .cells = 16}},
		/* A window as large as the next one. */
		{{.firstX = 0, .firstY = 0, .cells = 16}, {.firstX = 0, .firstY = 0, .cells = 16}},
		/* A side shared with the next window that is not the outer border. */
		{{.firstX = 4, .firstY = 8, .cells = 8}, {.firstX = 4, .firstY = 4, .cells = 24}, {.firstX = 0, .firstY = 0, .cells = 64}},
		/* A window ending past the last coordinate. */
		{{.firstX = 4294967292U, .firstY = 0, .cells = 8}},
		/* More nodes than a lattice may hold. */
		{{.firstX = 0, .firstY = 0, .cells = 16384}}
	};

	cases.push_back(moreWindowsThanALatticeTakes());

	for ( size_t index = 0; index < cases.size(); ++index )
	{
		SCOPED_TRACE("case " + std::to_string(index));

		expectRefused(cases[index]);
	}

	/* One error for each refusal, of the build and of the count. */
	const auto captured = diagnosticsCapture.snapshot();

	EXPECT_EQ(captured.count(Severity::Error, LatticeTag), 2U * cases.size());
	EXPECT_EQ(captured.countWithoutDebug(), 2U * cases.size());
	EXPECT_TRUE(captured.standardError.empty()) << captured.standardError;
}
