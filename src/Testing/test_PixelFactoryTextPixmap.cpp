/*
 * src/Testing/test_PixelFactoryTextPixmap.cpp
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

/* STL inclusions. */
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "Constants.hpp"
#include "Math/Space2D/AARectangle.hpp"
#include "PixelFactory/Color.hpp"
#include "PixelFactory/FileIO.hpp"
#include "PixelFactory/Font.hpp"
#include "PixelFactory/Pixmap.hpp"
#include "PixelFactory/Processor.hpp"
#include "PixelFactory/TextProcessor.hpp"
#include "PixelFactory/Types.hpp"
#include "Time/Elapsed/PrintScopeRealTime.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::PixelFactory;
using namespace EmEn::Base::Time::Elapsed;

/* 2026-10-08: both blocks below were commented out: the TrueType font rendered no glyph, and TextProcessor's pixmap
 * constructor read a Pixmap::area() that does not exist (that template constructor was never instantiated). */
TEST(PixelFactoryTextProcessor, write)
{
	Pixmap< uint8_t > image{1024, 1024, ChannelMode::RGB};

	{
		PrintScopeRealTime stat{"Processor::fill(pattern)"};

		Pixmap< uint8_t > pattern;

		ASSERT_TRUE(FileIO::read(SmallPatternRBG_2, pattern));

		ASSERT_TRUE(image.fill(Processor< uint8_t >::toRGB(pattern)));
	}

	Font< uint8_t > fixedFont;

	EXPECT_TRUE(fixedFont.readFile(TrueTypeFont, 48, true));

	{
		PrintScopeRealTime stat{"TextProcessor::write(fixedFont)"};

		TextProcessor< uint8_t > textProcessor{image};
		textProcessor.setFont(fixedFont, 48);
		textProcessor.setFontColor(DarkRed);
		textProcessor.setDrawMode(DrawPixelMode::Normal);
		textProcessor.setRectangle({32, 32, 1024 - 64, 512 - 64});
		textProcessor.setLineSpace(8);

		EXPECT_TRUE(textProcessor.write(LoremIpsum));
	}

	Font< uint8_t > trueTypeFont;

	EXPECT_TRUE(trueTypeFont.readFile(TrueTypeFont, 32, false));

	{
		PrintScopeRealTime stat{"TextProcessor::write(trueTypeFont)"};

		TextProcessor< uint8_t > textProcessor{image};
		textProcessor.setFont(trueTypeFont, 32);
		textProcessor.setFontColor(Green);
		textProcessor.setDrawMode(DrawPixelMode::Normal);
		textProcessor.setRectangle({64, 64 + 512, 1024 - 128, 512 - 128});
		textProcessor.setLineSpace(4);

		EXPECT_TRUE(textProcessor.write(LoremIpsum));
	}

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_textPixmap.png", true));
}

/* The pixmap font (the coverage pass, 2026-10-17). */
namespace
{
	[[nodiscard]]
	Pixmap< uint8_t >
	loadCharsMap ()
	{
		Pixmap< uint8_t > charsMap;
		static_cast< void >(FileIO::read(FixedFont, charsMap));

		return charsMap;
	}
}

TEST(PixelFactoryFont, proportionalGlyphsKeepTheirAspectAtEverySize)
{
	/* The engine's FontResource parses the default font at 16, 24 and 32 px, proportional. Below the 32-px cell every
	 * glyph used to take the CELL's width: 'i' and '.' came out 16 x 16, as wide as 'W'. */
	const auto charsMap = loadCharsMap();
	ASSERT_TRUE(charsMap.isValid());

	Font< uint8_t > font;
	ASSERT_TRUE(font.parsePixmap(charsMap, 32U, false));
	ASSERT_TRUE(font.parsePixmap(charsMap, 24U, false));
	ASSERT_TRUE(font.parsePixmap(charsMap, 16U, false));

	const auto * reference = font.glyphs(32U);
	ASSERT_NE(reference, nullptr);

	for ( const uint32_t size : {24U, 16U} )
	{
		const auto * glyphs = font.glyphs(size);
		ASSERT_NE(glyphs, nullptr);

		for ( const char character : {'i', 'l', '.', 'm', 'W', 'A', '1'} )
		{
			const auto code = static_cast< uint8_t >(character);
			const auto expected = static_cast< double >(reference->glyph(code).width()) * static_cast< double >(size) / 32.0;

			EXPECT_EQ(glyphs->glyph(code).height(), size) << character;
			EXPECT_NEAR(static_cast< double >(glyphs->glyph(code).width()), expected, 1.0) << character << " at " << size << " px";
		}

		EXPECT_LT(glyphs->glyph(static_cast< uint8_t >('.')).width(), glyphs->glyph(static_cast< uint8_t >('W')).width()) << size;
	}
}

TEST(PixelFactoryFont, fixedWidthGlyphsKeepTheCellWidth)
{
	const auto charsMap = loadCharsMap();
	ASSERT_TRUE(charsMap.isValid());

	Font< uint8_t > font;
	ASSERT_TRUE(font.parsePixmap(charsMap, 16U, true));

	const auto * glyphs = font.glyphs(16U);
	ASSERT_NE(glyphs, nullptr);
	EXPECT_TRUE(glyphs->isFixedWidth());

	for ( const char character : {'i', '.', 'W'} )
	{
		EXPECT_EQ(glyphs->glyph(static_cast< uint8_t >(character)).width(), 16U) << character;
	}
}

TEST(PixelFactoryFont, refusalsAndClosestSize)
{
	Font< uint8_t > font;

	/* No size loaded yet. */
	EXPECT_EQ(font.glyphs(16U), nullptr);

	/* An empty map, a colour map, dimensions that are not a multiple of 16. */
	EXPECT_FALSE(font.parsePixmap(Pixmap< uint8_t >{}, 16U, false));
	EXPECT_FALSE(font.parsePixmap(Pixmap< uint8_t >{32U, 32U, ChannelMode::RGB}, 16U, false));
	EXPECT_FALSE(font.parsePixmap(Pixmap< uint8_t >{100U, 100U, ChannelMode::Grayscale}, 16U, false));

	/* The closest loaded size, the smaller one on a tie. */
	const auto charsMap = loadCharsMap();
	ASSERT_TRUE(font.parsePixmap(charsMap, 16U, false));
	ASSERT_TRUE(font.parsePixmap(charsMap, 32U, false));
	EXPECT_EQ(font.glyphs(20U)->height(), 16U);
	EXPECT_EQ(font.glyphs(30U)->height(), 32U);
	EXPECT_EQ(font.glyphs(24U)->height(), 16U);
	EXPECT_EQ(font.glyphs(8U)->height(), 16U);
	EXPECT_EQ(font.glyphs(100U)->height(), 32U);

	/* The file type comes from the extension. */
	Font< uint8_t > fromFile;
	EXPECT_TRUE(fromFile.readFile(FixedFont, 16U, false));
	EXPECT_FALSE(fromFile.readFile(std::filesystem::path{"font.xyz"}, 16U, false));
}

/* The text layout in pixels (2026-10-18). */
namespace
{
	/** @brief Writes a text with a font size on a black 256 x 128 grayscale pixmap, the area starting at a column. */
	[[nodiscard]]
	Pixmap< uint8_t >
	writeText (const Font< uint8_t > & font, uint32_t size, const std::string & text, int32_t left, int32_t areaWidth)
	{
		Pixmap< uint8_t > target{256U, 128U, ChannelMode::Grayscale};
		static_cast< void >(target.fill(Black));

		TextProcessor< uint8_t > processor{target};
		processor.setFont(font, size);
		processor.setFontColor(White);
		processor.setRectangle({left, 0, areaWidth, 128});
		static_cast< void >(processor.write(text));

		return target;
	}

	/** @brief Returns whether a band of rows holds ink at or after a column. */
	[[nodiscard]]
	bool
	hasInk (const Pixmap< uint8_t > & target, uint32_t firstRow, uint32_t lastRow, uint32_t fromColumn)
	{
		for ( uint32_t row = firstRow; row <= lastRow; ++row )
		{
			for ( uint32_t column = fromColumn; column < target.width(); ++column )
			{
				if ( target.pixelElement(column, row, Channel::Red) > 0 )
				{
					return true;
				}
			}
		}

		return false;
	}
}

TEST(PixelFactoryTextProcessor, proportionalFontAdvancesByEachGlyph)
{
	/* "il" equals 'i' at the area's left plus 'l' one glyph width and one letter spacing further: the pen used to move
	 * by the widest glyph's cell, a proportional font came out letter-spaced like a monospaced one. */
	const auto charsMap = loadCharsMap();
	ASSERT_TRUE(charsMap.isValid());

	Font< uint8_t > font;
	ASSERT_TRUE(font.parsePixmap(charsMap, 32U, false));

	const auto * glyphs = font.glyphs(32U);
	ASSERT_NE(glyphs, nullptr);
	EXPECT_EQ(glyphs->letterSpacing(), 2U);

	const auto widthI = static_cast< int32_t >(glyphs->glyph(static_cast< uint8_t >('i')).width());
	const auto spacing = static_cast< int32_t >(glyphs->letterSpacing());
	ASSERT_LT(widthI + spacing, static_cast< int32_t >(glyphs->widestChar()));

	const auto both = writeText(font, 32U, "il", 0, 256);
	auto expected = writeText(font, 32U, "i", 0, 256);
	{
		TextProcessor< uint8_t > processor{expected};
		processor.setFont(font, 32U);
		processor.setFontColor(White);
		processor.setRectangle({widthI + spacing, 0, 256 - (widthI + spacing), 128});
		ASSERT_TRUE(processor.write("l"));
	}

	EXPECT_TRUE(both.data() == expected.data());
}

TEST(PixelFactoryTextProcessor, fixedWidthFontKeepsItsGrid)
{
	const auto charsMap = loadCharsMap();
	ASSERT_TRUE(charsMap.isValid());

	Font< uint8_t > font;
	ASSERT_TRUE(font.parsePixmap(charsMap, 32U, true));
	EXPECT_EQ(font.glyphs(32U)->letterSpacing(), 0U);

	const auto both = writeText(font, 32U, "il", 0, 256);
	auto expected = writeText(font, 32U, "i", 0, 256);
	{
		TextProcessor< uint8_t > processor{expected};
		processor.setFont(font, 32U);
		processor.setFontColor(White);
		processor.setRectangle({32, 0, 256 - 32, 128});
		ASSERT_TRUE(processor.write("l"));
	}

	EXPECT_TRUE(both.data() == expected.data());
}

TEST(PixelFactoryTextProcessor, wordsWrapByTheirPixelWidth)
{
	const auto charsMap = loadCharsMap();
	ASSERT_TRUE(charsMap.isValid());

	Font< uint8_t > font;
	ASSERT_TRUE(font.parsePixmap(charsMap, 32U, false));

	const auto * glyphs = font.glyphs(32U);
	const auto widthM = glyphs->glyph(static_cast< uint8_t >('m')).width();
	const auto spacing = glyphs->letterSpacing();
	const auto wordWidth = (3U * widthM) + (2U * spacing);

	/* The area holds one "mmm" but not "mmm mmm": the second word, the LAST of the text, goes whole to the second row
	 * (the last word used to be cut where the row ended). */
	const auto areaWidth = static_cast< int32_t >(wordWidth + 8U);
	const auto target = writeText(font, 32U, "mmm mmm", 0, areaWidth);

	EXPECT_TRUE(hasInk(target, 0U, 31U, 0U));
	EXPECT_FALSE(hasInk(target, 0U, 31U, wordWidth)) << "the first row holds more than its word";
	EXPECT_TRUE(hasInk(target, 32U, 63U, 0U)) << "the second word did not wrap";
	EXPECT_FALSE(hasInk(target, 32U, 63U, wordWidth));
	EXPECT_FALSE(hasInk(target, 64U, 127U, 0U));

	/* A word wider than a whole row is cut where the row ends instead. */
	const auto narrow = writeText(font, 32U, "mmm", 0, static_cast< int32_t >(widthM + spacing + widthM));

	EXPECT_TRUE(hasInk(narrow, 0U, 31U, 0U));
	EXPECT_TRUE(hasInk(narrow, 32U, 63U, 0U)) << "the third 'm' did not go to the second row";
	EXPECT_FALSE(hasInk(narrow, 64U, 127U, 0U));
}

/* The kerning (2026-10-27, caution-points § PixelFactory): a TrueType font's 'kern' pairs move the pen ("AV" closes up). */
TEST(PixelFactoryFont, trueTypeFontReadsItsKerningPairs)
{
	Font< uint8_t > proportional;
	ASSERT_TRUE(proportional.readFile(TrueTypeFont, 32U, false));

	const auto * glyphs = proportional.glyphs(32U);
	ASSERT_NE(glyphs, nullptr);

	/* The test font's 'kern' table: AV -407 / 2048 em, nothing for HH. */
	EXPECT_LT(glyphs->kerning(static_cast< uint8_t >('A'), static_cast< uint8_t >('V')), 0);
	EXPECT_EQ(glyphs->kerning(static_cast< uint8_t >('H'), static_cast< uint8_t >('H')), 0);
	EXPECT_GT(glyphs->kerningPairCount(), 0U);

	/* A fixed-width font keeps its grid, and a pixmap font has no pair data. */
	Font< uint8_t > fixed;
	ASSERT_TRUE(fixed.readFile(TrueTypeFont, 32U, true));
	ASSERT_NE(fixed.glyphs(32U), nullptr);
	EXPECT_EQ(fixed.glyphs(32U)->kerningPairCount(), 0U);

	const auto charsMap = loadCharsMap();
	ASSERT_TRUE(charsMap.isValid());

	Font< uint8_t > pixmapFont;
	ASSERT_TRUE(pixmapFont.parsePixmap(charsMap, 32U, false));
	ASSERT_NE(pixmapFont.glyphs(32U), nullptr);
	EXPECT_EQ(pixmapFont.glyphs(32U)->kerningPairCount(), 0U);
}

TEST(PixelFactoryTextProcessor, aKernedPairMovesThePen)
{
	/* "AV" equals 'A' at the area's left plus 'V' one 'A' width and one (negative) pair adjustment further. */
	Font< uint8_t > font;
	ASSERT_TRUE(font.readFile(TrueTypeFont, 32U, false));

	const auto * glyphs = font.glyphs(32U);
	ASSERT_NE(glyphs, nullptr);

	const auto widthA = static_cast< int32_t >(glyphs->glyph(static_cast< uint8_t >('A')).width());
	const auto pair = glyphs->kerning(static_cast< uint8_t >('A'), static_cast< uint8_t >('V'));
	ASSERT_LT(pair, 0);

	const auto both = writeText(font, 32U, "AV", 0, 256);
	auto expected = writeText(font, 32U, "A", 0, 256);
	{
		TextProcessor< uint8_t > processor{expected};
		processor.setFont(font, 32U);
		processor.setFontColor(White);
		processor.setRectangle({widthA + pair, 0, 256 - (widthA + pair), 128});
		ASSERT_TRUE(processor.write("V"));
	}

	EXPECT_TRUE(both.data() == expected.data());
}
