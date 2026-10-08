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

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "Constants.hpp"
#include "PixelFactory/FileIO.hpp"
#include "PixelFactory/TextProcessor.hpp"
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
