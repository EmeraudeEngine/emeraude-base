/*
 * src/Testing/test_VertexFactoryShelterMap.cpp
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
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/* Local inclusions. */
#include "DiagnosticsCapture.hpp"
#include "Logging/Severity.hpp"
#include "VertexFactory/ShelterMap.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::VertexFactory;

namespace
{
	constexpr auto ShelterTag{"VertexFactory::ShelterMap"};

	/**
	 * @brief A grid of heights drawn in text: '~' is water (one metre under the level 0), any other character land
	 * (one metre above it).
	 */
	struct Drawing final
	{
		std::vector< float > heights;
		size_t columns{0};
		size_t rows{0};

		explicit
		Drawing (const std::vector< std::string_view > & lines)
			: columns{lines.empty() ? 0 : lines.front().size()},
			  rows{lines.size()}
		{
			for ( const auto line : lines )
			{
				EXPECT_EQ(line.size(), columns);

				for ( const auto character : line )
				{
					heights.push_back(character == '~' ? -1.0F : 1.0F);
				}
			}
		}

		[[nodiscard]]
		HeightCells
		cells () const noexcept
		{
			return {.heights = heights, .columns = columns, .rows = rows};
		}

		[[nodiscard]]
		bool
		isWater (size_t column, size_t row) const noexcept
		{
			return heights[column + (row * columns)] < 0.0F;
		}
	};

	/** @brief Builds the map of a drawing, which must be accepted without a word. */
	[[nodiscard]]
	std::vector< uint8_t >
	mapOf (const Drawing & drawing, const ShelterMapOptions & options)
	{
		std::vector< uint8_t > map;
		bool built = false;

		const auto diagnostics = Testing::captureDiagnostics([&] {
			built = buildShelterMap(drawing.cells(), options, map);
		});

		EXPECT_TRUE(built);
		EXPECT_EQ(diagnostics.countWithoutDebug(), 0U);
		EXPECT_EQ(map.size(), drawing.columns * drawing.rows);

		return map;
	}

	/** @brief Checks a grid is refused with one error, the map left empty. */
	void
	expectRefused (const HeightCells & cells, const ShelterMapOptions & options)
	{
		std::vector< uint8_t > map{1, 2, 3};
		bool built = true;

		const auto diagnostics = Testing::captureDiagnostics([&] {
			built = buildShelterMap(cells, options, map);
		});

		EXPECT_FALSE(built);
		EXPECT_TRUE(map.empty());
		EXPECT_EQ(diagnostics.count(Severity::Error, ShelterTag), 1U);
		EXPECT_EQ(diagnostics.countWithoutDebug(), 1U);
	}

	/** @brief An island of 24 x 24 cells of land in a sea, with a lake of 12 x 12 in its middle. */
	[[nodiscard]]
	std::vector< std::string >
	islandWithLake ()
	{
		std::vector< std::string > lines(40, std::string(40, '~'));

		for ( size_t row = 8; row < 32; ++row )
		{
			for ( size_t column = 8; column < 32; ++column )
			{
				const bool lake = row >= 14 && row < 26 && column >= 14 && column < 26;

				lines[row][column] = lake ? '~' : '#';
			}
		}

		return lines;
	}

	/** @brief Turns owned lines into the views a drawing takes. */
	[[nodiscard]]
	std::vector< std::string_view >
	viewsOf (const std::vector< std::string > & lines)
	{
		return {lines.cbegin(), lines.cend()};
	}

	constexpr size_t LakeCentre{20};

	/** @brief Counts the water cells of the island's drawing that are not what they must be: the lake sheltered, the
	 * sea around the island open. */
	[[nodiscard]]
	size_t
	countWrongIslandWater (const Drawing & drawing, const std::vector< uint8_t > & map)
	{
		size_t wrong = 0;

		for ( size_t row = 0; row < drawing.rows; ++row )
		{
			for ( size_t column = 0; column < drawing.columns; ++column )
			{
				const bool lake = row >= 14 && row < 26 && column >= 14 && column < 26;

				if ( drawing.isWater(column, row) && map[column + (row * drawing.columns)] != (lake ? ShelteredWater : OpenWater) )
				{
					++wrong;
				}
			}
		}

		return wrong;
	}

	/** @brief The island and its lake, a channel cut from the lake to the sea through the island's rim (columns 26 to
	 * 31) over the given rows. */
	[[nodiscard]]
	std::vector< std::string >
	islandWithChannel (const std::vector< size_t > & channelRows)
	{
		auto lines = islandWithLake();

		for ( const auto row : channelRows )
		{
			for ( size_t column = 26; column < 32; ++column )
			{
				lines[row][column] = '~';
			}
		}

		return lines;
	}

	/** @brief What a run of cells shows: how often the openness rises again, and how many values lie in between. */
	struct Profile final
	{
		size_t rises{0};
		size_t between{0};
	};

	/** @brief Reads the profile of a run of cells. */
	[[nodiscard]]
	Profile
	profileOf (std::span< const uint8_t > values)
	{
		Profile profile;
		uint8_t previous = OpenWater;

		for ( const auto value : values )
		{
			if ( value > previous )
			{
				++profile.rises;
			}

			if ( value != OpenWater && value != ShelteredWater )
			{
				++profile.between;
			}

			previous = value;
		}

		return profile;
	}

	/** @brief A corridor of water three cells wide and 60 long (row 4, columns 3 to 62), open to the sea at its left
	 * end only. */
	[[nodiscard]]
	std::vector< std::string >
	corridor ()
	{
		std::vector< std::string > lines(9, std::string(70, '#'));

		for ( auto & line : lines )
		{
			line.replace(0, 3, "~~~");
		}

		for ( size_t row = 3; row < 6; ++row )
		{
			lines[row].replace(3, 60, std::string(60, '~'));
		}

		return lines;
	}

	constexpr size_t CorridorRow{4};
	constexpr size_t CorridorEnd{63};
}

TEST(VertexFactoryShelterMap, ABrokenPreConditionIsRefused)
{
	const std::vector< float > heights(16, -1.0F);

	expectRefused({.heights = heights, .columns = 0, .rows = 4}, {});
	expectRefused({.heights = heights, .columns = 4, .rows = 0}, {});
	expectRefused({.heights = heights, .columns = 4, .rows = 5}, {});
	expectRefused({.heights = heights, .columns = MaxShelterMapSide + 1, .rows = 1}, {});
	expectRefused({.heights = heights, .columns = 4, .rows = 4}, {.waterLevel = std::numeric_limits< float >::quiet_NaN()});
	expectRefused({.heights = heights, .columns = 4, .rows = 4}, {.minimumPassage = -1.0F});
	expectRefused({.heights = heights, .columns = 4, .rows = 4}, {.minimumPassage = std::numeric_limits< float >::infinity()});
	expectRefused({.heights = heights, .columns = 4, .rows = 4}, {.fade = std::numeric_limits< float >::quiet_NaN()});
	expectRefused({.heights = heights, .columns = 4, .rows = 4}, {.fade = MaxShelterMapReach + 1.0F});
}

TEST(VertexFactoryShelterMap, WaterEverywhereIsOpenAndSoIsAGridWithoutWater)
{
	const Drawing sea{{"~~~~", "~~~~", "~~~~"}};
	const Drawing land{{"####", "####", "####"}};

	for ( const auto value : mapOf(sea, {.minimumPassage = 6.0F, .fade = 4.0F}) )
	{
		EXPECT_EQ(value, OpenWater);
	}

	for ( const auto value : mapOf(land, {}) )
	{
		EXPECT_EQ(value, OpenWater);
	}

	/* A single cell is a grid. */
	EXPECT_EQ(mapOf(Drawing{{"~"}}, {}).front(), OpenWater);
}

TEST(VertexFactoryShelterMap, ALakeInsideAnIslandIsShelteredTheSeaAroundItIsOpen)
{
	const auto lines = islandWithLake();
	const Drawing drawing{viewsOf(lines)};

	for ( const auto passage : {0.0F, 8.0F} )
	{
		const auto map = mapOf(drawing, {.minimumPassage = passage, .fade = 6.0F});

		EXPECT_EQ(countWrongIslandWater(drawing, map), 0U) << "passage " << passage;

		/* The land takes the water nearest to it: the sea's on the island's rim, the lake's around the lake. */
		EXPECT_EQ(map[8 + (8 * drawing.columns)], OpenWater);
		EXPECT_EQ(map[13 + (LakeCentre * drawing.columns)], ShelteredWater);
		EXPECT_EQ(map[9 + (LakeCentre * drawing.columns)], OpenWater);
	}
}

TEST(VertexFactoryShelterMap, EveryWaterOfAGridWhoseBorderIsLandIsSheltered)
{
	const Drawing drawing{{
		"########",
		"#~~~~~~#",
		"#~~~~~~#",
		"#~~~~~~#",
		"########"
	}};

	const auto map = mapOf(drawing, {});

	for ( const auto value : map )
	{
		EXPECT_EQ(value, ShelteredWater);
	}
}

TEST(VertexFactoryShelterMap, APassageNarrowerThanTheThresholdKeepsTheOpenWaterOut)
{
	/* A channel two cells wide from the lake to the sea. */
	const auto lines = islandWithChannel({19, 20});
	const Drawing drawing{viewsOf(lines)};
	const auto centre = LakeCentre + (LakeCentre * drawing.columns);

	/* Any passage lets the open water in: the lake is the sea's. */
	EXPECT_EQ(mapOf(drawing, {})[centre], OpenWater);

	/* A passage of two cells is narrower than six: the lake stays sheltered, the channel's mouth is open. */
	const auto map = mapOf(drawing, {.minimumPassage = 6.0F});

	EXPECT_EQ(map[centre], ShelteredWater);
	EXPECT_EQ(map[14 + (14 * drawing.columns)], ShelteredWater);
	EXPECT_EQ(map[31 + (19 * drawing.columns)], OpenWater);
	EXPECT_EQ(map[33 + (19 * drawing.columns)], OpenWater);
}

TEST(VertexFactoryShelterMap, APassageWiderThanTheThresholdLetsTheOpenWaterIn)
{
	/* The same channel ten cells wide: wider than six, the open water enters the lake. */
	const auto lines = islandWithChannel({15, 16, 17, 18, 19, 20, 21, 22, 23, 24});
	const Drawing drawing{viewsOf(lines)};

	EXPECT_EQ(mapOf(drawing, {.minimumPassage = 6.0F})[LakeCentre + (LakeCentre * drawing.columns)], OpenWater);
}

TEST(VertexFactoryShelterMap, TheOpennessFallsAlongTheFadeAndNeverRisesAgain)
{
	const auto lines = corridor();
	const Drawing drawing{viewsOf(lines)};
	const auto map = mapOf(drawing, {.minimumPassage = 8.0F, .fade = 20.0F});
	const auto profile = profileOf(std::span{map}.subspan(CorridorRow * drawing.columns, CorridorEnd));

	EXPECT_EQ(profile.rises, 0U);
	EXPECT_EQ(map[CorridorRow * drawing.columns], OpenWater);
	EXPECT_EQ(map[(CorridorEnd - 1) + (CorridorRow * drawing.columns)], ShelteredWater);

	/* Twenty cells of fade: most of them hold a value in between. */
	EXPECT_GE(profile.between, 12U);
	EXPECT_LE(profile.between, 20U);
}

TEST(VertexFactoryShelterMap, WithoutAFadeTheLimitIsSharp)
{
	const auto lines = corridor();
	const Drawing drawing{viewsOf(lines)};
	const auto map = mapOf(drawing, {.minimumPassage = 8.0F});
	const auto profile = profileOf(std::span{map}.subspan(CorridorRow * drawing.columns, CorridorEnd));

	EXPECT_EQ(profile.rises, 0U);
	EXPECT_EQ(profile.between, 0U);
	EXPECT_EQ(map[CorridorRow * drawing.columns], OpenWater);
	EXPECT_EQ(map[(CorridorEnd - 1) + (CorridorRow * drawing.columns)], ShelteredWater);
}

TEST(VertexFactoryShelterMap, WaterDoesNotPassBetweenTwoLandCellsThatTouchByACorner)
{
	/* A lake closed by a wall drawn in diagonal: its land cells touch by their corners only. */
	const Drawing drawing{{
		"~~~~~~~~~",
		"~~~~#~~~~",
		"~~~#~#~~~",
		"~~#~~~#~~",
		"~#~~~~~#~",
		"~~#~~~#~~",
		"~~~#~#~~~",
		"~~~~#~~~~",
		"~~~~~~~~~"
	}};

	const auto map = mapOf(drawing, {});

	EXPECT_EQ(map[4 + (4 * drawing.columns)], ShelteredWater);
	EXPECT_EQ(map[4 + (2 * drawing.columns)], ShelteredWater);
	EXPECT_EQ(map[0], OpenWater);
	EXPECT_EQ(map[4 + (0 * drawing.columns)], OpenWater);
}

TEST(VertexFactoryShelterMap, AHeightThatIsNotANumberIsLand)
{
	Drawing drawing{{
		"~~~~~~~",
		"~#####~",
		"~#~~~#~",
		"~#####~",
		"~~~~~~~"
	}};

	for ( auto & height : drawing.heights )
	{
		if ( height > 0.0F )
		{
			height = std::numeric_limits< float >::quiet_NaN();
		}
	}

	const auto map = mapOf(drawing, {});

	EXPECT_EQ(map[3 + (2 * drawing.columns)], ShelteredWater);
	EXPECT_EQ(map[0], OpenWater);
}
