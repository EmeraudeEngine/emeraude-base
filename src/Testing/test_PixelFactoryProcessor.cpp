/*
 * src/Testing/test_PixelFactoryProcessor.cpp
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
#include "PixelFactory/Pixmap.hpp"
#include "PixelFactory/Processor.hpp"
#include "ThreadPool.hpp"
#include "Time/Elapsed/PrintScopeRealTime.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::PixelFactory;
using namespace EmEn::Base::Time::Elapsed;

TEST(PixelFactoryProcessor, scale)
{
	PrintScopeRealTime globalStat{"Processor::scale(2.0F) [OVERALL]"};

	Pixmap< uint8_t > source;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	auto output = source;

	{
		PrintScopeRealTime localStat{"Processor::scale(2.0F)"};

		Processor< uint8_t > processor{output};

		ASSERT_TRUE(processor.scaleValue(2.0F));
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_scaleAllValue.png"}, true));
}

TEST(PixelFactoryProcessor, scaleRed)
{
	PrintScopeRealTime globalStat{"Processor::scale(1.5F, Channel::Red) [OVERALL]"};

	Pixmap< uint8_t > source;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	auto output = source;

	{
		PrintScopeRealTime localStat{"Processor::scale(1.5F, Channel::Red)"};

		Processor< uint8_t > processor{output};

		ASSERT_TRUE(processor.scaleValue(2.0F, Channel::Red));
	}

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_scaleRedValue.png"}, true));
}

TEST(PixelFactoryProcessor, drawing)
{
	PrintScopeRealTime globalStat{"Processor::drawXXX() [OVERALL]"};

	Pixmap< uint8_t > emptyImage{800, 600};

	ASSERT_EQ(emptyImage.width(), 800);
	ASSERT_EQ(emptyImage.height(), 600);
	ASSERT_EQ(emptyImage.colorCount(), 3);

	Processor< uint8_t > processor{emptyImage};

	{
		PrintScopeRealTime localState{"Processor::drawSegment({10, 16}, {85, 503}, LightBlue)"};

		ASSERT_TRUE(processor.drawSegment({10, 16}, {85, 503}, LightBlue));
	}

	{
		PrintScopeRealTime localState{"Processor::drawCircle({578, 250}, 58, DarkRed)"};

		ASSERT_TRUE(processor.drawCircle({578, 250}, 58, DarkRed));
	}

	{
		PrintScopeRealTime localState{"Processor::drawCircle({64, 128}, 800, Red)"};

		ASSERT_TRUE(processor.drawCircle({64, 128}, 500, Red));
	}

	{
		PrintScopeRealTime localState{"Processor::drawSquare({256, 502, 125, 98}, Green)"};

		ASSERT_TRUE(processor.drawSquare({256, 502, 125, 98}, Green));
	}

	{
		PrintScopeRealTime localState{"Processor::drawCross({64, 96, 256, 128}, Yellow)"};

		ASSERT_TRUE(processor.drawCross({64, 96, 256, 128}, Yellow));
	}

	{
		PrintScopeRealTime localState{"Processor::drawStraightCross({200, 350, 300, 350}, White)"};

		ASSERT_TRUE(processor.drawStraightCross({200, 350, 300, 350}, White));
	}

	ASSERT_EQ(emptyImage.width(), 800);
	ASSERT_EQ(emptyImage.height(), 600);
	ASSERT_EQ(emptyImage.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(emptyImage, {"./assets/tmp_drawing.png"}, true));
}

TEST(PixelFactoryProcessor, move)
{
	PrintScopeRealTime globalStat{"Processor::move(600, -200) [OVERALL]"};

	Pixmap< uint8_t > source;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::move(600, -200)"};

		Processor< uint8_t > processor{source};

		ASSERT_TRUE(processor.move(600, -400));
	}

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(source, {"./assets/tmp_moved.png"}, true));
}

TEST(PixelFactoryProcessor, shift)
{
	PrintScopeRealTime globalStat{"Processor::shift(600, -400) [OVERALL]"};

	Pixmap< uint8_t > source;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::shift(600, -400)"};

		Processor< uint8_t > processor{source};

		ASSERT_TRUE(processor.shift(600, -400));
	}

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(source, {"./assets/tmp_shifted.png"}, true));
}

TEST(PixelFactoryProcessor, shiftTextArea)
{
	PrintScopeRealTime globalStat{"Processor::shiftTextArea(100) [OVERALL]"};

	Pixmap< uint8_t > source;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::shiftTextArea(100)"};

		Processor< uint8_t > processor{source};

		ASSERT_TRUE(processor.shiftTextArea(-100));
	}

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(source, {"./assets/tmp_textShifted.png"}, true));
}

TEST(PixelFactoryProcessor, resizeNearestDown)
{
	PrintScopeRealTime globalStat{"Processor::resize(50%,Nearest) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::resize(50%,Nearest)"};

		output = Processor< uint8_t >::resize(source, ExtraLargeRGB_Width / 2, ExtraLargeRGB_Height / 2, FilteringMode::Nearest);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width / 2);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height / 2);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_resizeDownNearest.png"}, true));
}

TEST(PixelFactoryProcessor, resizeLinearDown)
{
	PrintScopeRealTime globalStat{"Processor::resize(50%,Linear) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::resize(50%,Linear)"};

		output = Processor< uint8_t >::resize(source, ExtraLargeRGB_Width / 2, ExtraLargeRGB_Height / 2, FilteringMode::Linear);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width / 2);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height / 2);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_resizeDownLinear.png"}, true));
}

TEST(PixelFactoryProcessor, resizeCubicDown)
{
	PrintScopeRealTime globalStat{"Processor::resize(50%,Cubic) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::resize(50%,Cubic)"};

		output = Processor< uint8_t >::resize(source, ExtraLargeRGB_Width / 2, ExtraLargeRGB_Height / 2, FilteringMode::Cubic);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width / 2);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height / 2);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_resizeDownCubic.png"}, true));
}

TEST(PixelFactoryProcessor, resizeNearestUp)
{
	PrintScopeRealTime globalStat{"Processor::resize(200%,Nearest) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(LargeRGB, source));

	ASSERT_EQ(source.width(), 1200);
	ASSERT_EQ(source.height(), 800);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::resize(200%,Nearest)"};

		output = Processor< uint8_t >::resize(source, ExtraLargeRGB_Width * 2, ExtraLargeRGB_Height * 2, FilteringMode::Nearest);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width * 2);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height * 2);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_resizeUpNearest.png"}, true));
}

TEST(PixelFactoryProcessor, resizeLinearUp)
{
	PrintScopeRealTime globalStat{"Processor::resize(200%,Linear) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(LargeRGB, source));

	ASSERT_EQ(source.width(), 1200);
	ASSERT_EQ(source.height(), 800);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::resize(200%,Linear)"};

		output = Processor< uint8_t >::resize(source, ExtraLargeRGB_Width * 2, ExtraLargeRGB_Height * 2, FilteringMode::Linear);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width * 2);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height * 2);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_resizeUpLinear.png"}, true));
}

TEST(PixelFactoryProcessor, resizeCubicUp)
{
	PrintScopeRealTime globalStat{"Processor::resize(200%,Cubic) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(LargeRGB, source));

	ASSERT_EQ(source.width(), 1200);
	ASSERT_EQ(source.height(), 800);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::resize(200%,Cubic)"};

		output = Processor< uint8_t >::resize(source, ExtraLargeRGB_Width * 2, ExtraLargeRGB_Height * 2, FilteringMode::Cubic);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width * 2);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height * 2);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_resizeUpCubic.png"}, true));
}

TEST(PixelFactoryProcessor, crop)
{
	PrintScopeRealTime globalStat{"Processor::crop() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	/* Crop a sub-region derived from the source size so it always stays in-bounds,
	 * whatever the selected image (small default or extra-large). A region overflowing
	 * the source would pass isIntersect() and then read out of bounds (segfault). */
	constexpr uint32_t cropOffsetX = 64;
	constexpr uint32_t cropOffsetY = 128;
	constexpr uint32_t cropWidth = ExtraLargeRGB_Width / 2;
	constexpr uint32_t cropHeight = ExtraLargeRGB_Height / 2;

	{
		PrintScopeRealTime stat{"Processor::crop()"};

		output = Processor< uint8_t >::crop(source, {cropOffsetX, cropOffsetY, cropWidth, cropHeight});
	}

	ASSERT_EQ(output.width(), cropWidth);
	ASSERT_EQ(output.height(), cropHeight);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_cropped.png"}, true));
}

TEST(PixelFactoryProcessor, extractChannelRed)
{
	PrintScopeRealTime globalStat{"Processor::extractChannel(Red) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::extractChannel(Red)"};

		output = Processor< uint8_t >::extractChannel(source, Channel::Red);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 1);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_extractChannelRed.png"}, true));
}

TEST(PixelFactoryProcessor, extractChannelGreen)
{
	PrintScopeRealTime globalStat{"Processor::extractChannel(Green) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::extractChannel(Green)"};

		output = Processor< uint8_t >::extractChannel(source, Channel::Green);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 1);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_extractChannelGreen.png"}, true));
}

TEST(PixelFactoryProcessor, extractChannelBlue)
{
	PrintScopeRealTime globalStat{"Processor::extractChannel(Blue) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::extractChannel(Blue)"};

		output = Processor< uint8_t >::extractChannel(source, Channel::Blue);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 1);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_extractChannelBlue.png"}, true));
}

TEST(PixelFactoryProcessor, toGrayscale)
{
	PrintScopeRealTime globalStat{"Processor::toGrayscale() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::toGrayscale()"};

		output = Processor< uint8_t >::toGrayscale(source);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 1);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_toGrayscale.png"}, true));
}

TEST(PixelFactoryProcessor, toRGB)
{
	PrintScopeRealTime globalStat{"Processor::toRGB() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(MediumRGBA, source));

	ASSERT_EQ(source.width(), 512);
	ASSERT_EQ(source.height(), 512);
	ASSERT_EQ(source.colorCount(), 4);

	{
		PrintScopeRealTime localStat{"Processor::toRGB()"};

		output = Processor< uint8_t >::toRGB(source);
	}

	ASSERT_EQ(output.width(), 512);
	ASSERT_EQ(output.height(), 512);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_toRGB.png"}, true));
}

TEST(PixelFactoryProcessor, toRGBA)
{
	PrintScopeRealTime globalStat{"Processor::toRGBA() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::toRGBA()"};

		output = Processor< uint8_t >::toRGBA(source, 0.5F);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 4);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_toRGBA.png"}, true));
}

TEST(PixelFactoryProcessor, mirrorX)
{
	PrintScopeRealTime globalStat{"Processor::mirror(X) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::mirror(X)"};

		output = Processor< uint8_t >::mirror(source, MirrorMode::X);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_mirrorX.png"}, true));
}

TEST(PixelFactoryProcessor, mirrorY)
{
	PrintScopeRealTime globalStat{"Processor::mirror(Y) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::mirror(Y)"};

		output = Processor< uint8_t >::mirror(source, MirrorMode::Y);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_mirrorY.png"}, true));
}

TEST(PixelFactoryProcessor, mirrorBoth)
{
	PrintScopeRealTime globalStat{"Processor::mirror(Both) [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::mirror(Both)"};

		output = Processor< uint8_t >::mirror(source, MirrorMode::Both);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_mirrorBoth.png"}, true));
}

TEST(PixelFactoryProcessor, extend)
{
	PrintScopeRealTime globalStat{"Processor::extend() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::extend()"};

		output = Processor< uint8_t >::extend(source, {16, 24, 32, 128}, Red);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width + 16 + 32);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height + 24 + 128);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_extend.png"}, true));
}

TEST(PixelFactoryProcessor, rotateQuarterTurn)
{
	PrintScopeRealTime globalStat{"Processor::rotateQuarterTurn() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::rotateQuarterTurn()"};

		output = Processor< uint8_t >::rotateQuarterTurn(source);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_rotated+90.png"}, true));
}

TEST(PixelFactoryProcessor, rotateHalfTurn)
{
	PrintScopeRealTime globalStat{"Processor::rotateHalfTurn() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::rotateHalfTurn()"};

		output = Processor< uint8_t >::rotateHalfTurn(source);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_rotated+180.png"}, true));
}

TEST(PixelFactoryProcessor, rotateThreeQuarterTurn)
{
	PrintScopeRealTime globalStat{"Processor::rotateThreeQuarterTurn() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::rotateThreeQuarterTurn()"};

		output = Processor< uint8_t >::rotateThreeQuarterTurn(source);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_rotated+270.png"}, true));
}

TEST(PixelFactoryProcessor, inverseColors)
{
	PrintScopeRealTime globalStat{"Processor::inverseColors() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::inverseColors()"};

		output = Processor< uint8_t >::inverseColors(source);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_inverseColors.png"}, true));
}

TEST(PixelFactoryProcessor, swapChannels)
{
	PrintScopeRealTime globalStat{"Processor::swapChannels() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(ExtraLargeRGB, source));

	ASSERT_EQ(source.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(source.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::swapChannels()"};

		output = Processor< uint8_t >::swapChannels(source);
	}

	ASSERT_EQ(output.width(), ExtraLargeRGB_Width);
	ASSERT_EQ(output.height(), ExtraLargeRGB_Height);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_swapChannels.png"}, true));
}

TEST(PixelFactoryProcessor, blit)
{
	Pixmap< uint8_t > source;

	ASSERT_TRUE(FileIO::read(MediumRGBA, source));
	ASSERT_EQ(source.width(), 512);
	ASSERT_EQ(source.height(), 512);
	ASSERT_EQ(source.colorCount(), 4);

	Pixmap< uint8_t > smallImage;

	ASSERT_TRUE(FileIO::read(SmallRGBA, smallImage));
	ASSERT_EQ(smallImage.width(), 64);
	ASSERT_EQ(smallImage.height(), 64);
	ASSERT_EQ(smallImage.colorCount(), 4);

	Processor< uint8_t > proc{source};

	Pixmap< uint8_t > blitImage;

	ASSERT_TRUE(proc.blit(smallImage, {64, 64}, {0, 0, 64, 64}));
	ASSERT_TRUE(proc.blit(smallImage, {64, 64}, {512-64, 512-64, 64, 64}));
	ASSERT_TRUE(proc.blit(smallImage, {64, 64}, {512-64, 0, 64, 64}));

	ASSERT_TRUE(FileIO::write(source, {"./assets/tmp_64x64-blit.png"}, true));
}

TEST(PixelFactoryProcessor, copy)
{
	Pixmap< uint8_t > source;

	ASSERT_TRUE(FileIO::read(LargeRGB, source));

	Processor< uint8_t > processor{source};

	{
		PrintScopeRealTime stat{"Processor::copy(pixmapSimple)"};

		Pixmap< uint8_t > chunk;

		ASSERT_TRUE(FileIO::read(MediumRGBA, chunk));

		ASSERT_TRUE(processor.copy(chunk, DrawPixelMode::Normal));
	}

	{
		PrintScopeRealTime stat{"Processor::copy(pixmapSimpleClip)"};

		Pixmap< uint8_t > chunk;

		ASSERT_TRUE(FileIO::read(SmallRGBA, chunk));

		ASSERT_TRUE(processor.copy(chunk, 600, 32, DrawPixelMode::Addition));
	}

	{
		PrintScopeRealTime stat{"Processor::copy(pixmap)"};

		Pixmap< uint8_t > chunk;

		ASSERT_TRUE(FileIO::read(MediumGrayscale, chunk));

		ASSERT_TRUE(processor.copy(chunk, {128, 128, 256, 256}, {400, 260, 256, 256}, DrawPixelMode::Multiply));
	}

	{
		PrintScopeRealTime stat{"Processor::copy(color)"};

		ASSERT_TRUE(processor.copy(Red, {640, 600, 200, 16}, DrawPixelMode::Overlay));
	}

	ASSERT_TRUE(FileIO::write(source, {"./assets/tmp_copy.png"}, true));
}

TEST(PixelFactoryProcessor, addAlphaChannel)
{
	PrintScopeRealTime globalStat{"Processor::addAlphaChannel() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(MediumRGB, source));

	ASSERT_EQ(source.width(), 512);
	ASSERT_EQ(source.height(), 512);
	ASSERT_EQ(source.colorCount(), 3);

	{
		PrintScopeRealTime localStat{"Processor::addAlphaChannel()"};

		ASSERT_TRUE(Processor< uint8_t >::addAlphaChannel(source, output));
	}

	ASSERT_EQ(output.width(), 512);
	ASSERT_EQ(output.height(), 512);
	ASSERT_EQ(output.colorCount(), 4);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_addAlphaChannel.png"}, true));
}

TEST(PixelFactoryProcessor, removeAlphaChannel)
{
	PrintScopeRealTime globalStat{"Processor::removeAlphaChannel() [OVERALL]"};

	Pixmap< uint8_t > source, output;

	ASSERT_TRUE(FileIO::read(MediumRGBA, source));

	ASSERT_EQ(source.width(), 512);
	ASSERT_EQ(source.height(), 512);
	ASSERT_EQ(source.colorCount(), 4);

	{
		PrintScopeRealTime localStat{"Processor::removeAlphaChannel()"};

		ASSERT_TRUE(Processor< uint8_t >::removeAlphaChannel(source, output));
	}

	ASSERT_EQ(output.width(), 512);
	ASSERT_EQ(output.height(), 512);
	ASSERT_EQ(output.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(output, {"./assets/tmp_removeAlphaChannel.png"}, true));
}

TEST(PixelFactoryProcessor, resizeCubicParallelMatchesSerial)
{
	/* Ave robustus! (A.5): the ThreadPool path of resizeCubic must produce byte-identical output to
	 * the serial path. This is the correctness guarantee that lets the parallelization land; it is
	 * also the race detector under ASan/UBSan. RGBA exercises all four channels. */
	Pixmap< uint8_t > source{640, 480, ChannelMode::RGBA};

	auto & data = source.data();

	for ( size_t index = 0; index < data.size(); ++index )
	{
		data[index] = static_cast< uint8_t >((index * 2654435761U) >> 24);
	}

	ThreadPool pool;

	const auto serial = Processor< uint8_t >::resize(source, 1280, 960, FilteringMode::Cubic, nullptr);
	const auto parallel = Processor< uint8_t >::resize(source, 1280, 960, FilteringMode::Cubic, &pool);

	ASSERT_EQ(serial.width(), parallel.width());
	ASSERT_EQ(serial.height(), parallel.height());
	ASSERT_EQ(serial.data().size(), parallel.data().size());
	EXPECT_EQ(serial.data(), parallel.data());
}

TEST(PixelFactoryProcessor, resizeLinearParallelMatchesSerial)
{
	/* Ave robustus! (A.5): FilteringMode::Linear ThreadPool path must equal the serial path (race-checked under ASan). */
	Pixmap< uint8_t > source{640, 480, ChannelMode::RGBA};

	auto & data = source.data();

	for ( size_t index = 0; index < data.size(); ++index )
	{
		data[index] = static_cast< uint8_t >((index * 2654435761U) >> 24);
	}

	ThreadPool pool;

	const auto serial = Processor< uint8_t >::resize(source, 1280, 960, FilteringMode::Linear, nullptr);
	const auto parallel = Processor< uint8_t >::resize(source, 1280, 960, FilteringMode::Linear, &pool);

	ASSERT_EQ(serial.width(), parallel.width());
	ASSERT_EQ(serial.height(), parallel.height());
	ASSERT_EQ(serial.data().size(), parallel.data().size());
	EXPECT_EQ(serial.data(), parallel.data());
}

TEST(PixelFactoryProcessor, resizeNearestParallelMatchesSerial)
{
	/* Ave robustus! (A.5): FilteringMode::Nearest ThreadPool path must equal the serial path (race-checked under ASan). */
	Pixmap< uint8_t > source{640, 480, ChannelMode::RGBA};

	auto & data = source.data();

	for ( size_t index = 0; index < data.size(); ++index )
	{
		data[index] = static_cast< uint8_t >((index * 2654435761U) >> 24);
	}

	ThreadPool pool;

	const auto serial = Processor< uint8_t >::resize(source, 1280, 960, FilteringMode::Nearest, nullptr);
	const auto parallel = Processor< uint8_t >::resize(source, 1280, 960, FilteringMode::Nearest, &pool);

	ASSERT_EQ(serial.width(), parallel.width());
	ASSERT_EQ(serial.height(), parallel.height());
	ASSERT_EQ(serial.data().size(), parallel.data().size());
	EXPECT_EQ(serial.data(), parallel.data());
}

TEST(PixelFactoryProcessor, mirrorYParallelMatchesSerial)
{
	/* Ave robustus! (A.5): the ThreadPool path of mirrorY must equal the serial path (race-checked
	 * under ASan), and mirroring twice must restore the original (it is a real horizontal flip). */
	Pixmap< uint8_t > source{640, 480, ChannelMode::RGBA};

	auto & data = source.data();

	for ( size_t index = 0; index < data.size(); ++index )
	{
		data[index] = static_cast< uint8_t >((index * 2654435761U) >> 24);
	}

	ThreadPool pool;

	const auto serial = Processor< uint8_t >::mirror(source, MirrorMode::Y, nullptr);
	const auto parallel = Processor< uint8_t >::mirror(source, MirrorMode::Y, &pool);

	ASSERT_EQ(serial.data().size(), parallel.data().size());
	EXPECT_EQ(serial.data(), parallel.data());

	const auto twice = Processor< uint8_t >::mirror(parallel, MirrorMode::Y, &pool);
	EXPECT_EQ(twice.data(), source.data());
}

TEST(PixelFactoryProcessor, toGrayscaleParallelMatchesSerial)
{
	/* Ave robustus! (A.5): the ThreadPool path of toGrayscale must equal the serial path (race-checked
	 * under ASan). Per-pixel luminance, output written via index-local pixelPointer(). */
	Pixmap< uint8_t > source{640, 480, ChannelMode::RGB};

	auto & data = source.data();

	for ( size_t index = 0; index < data.size(); ++index )
	{
		data[index] = static_cast< uint8_t >((index * 2654435761U) >> 24);
	}

	ThreadPool pool;

	const auto serial = Processor< uint8_t >::toGrayscale(source, GrayscaleConversionMode::LumaRec709, 0, nullptr);
	const auto parallel = Processor< uint8_t >::toGrayscale(source, GrayscaleConversionMode::LumaRec709, 0, &pool);

	ASSERT_EQ(serial.channelMode(), ChannelMode::Grayscale);
	ASSERT_EQ(serial.data().size(), parallel.data().size());
	EXPECT_EQ(serial.data(), parallel.data());
}

TEST(PixelFactoryProcessor, resizeCubicUniformImageStaysUniform)
{
	/* Ave robustus! (A.5): cubic interpolation of a constant must stay that constant. Independent
	 * guard for the gather-hoisting kernel refactor (catches a wrong tap index / channel offset).
	 * NOTE: only the interior is checked — safePixel() returns Black out of bounds (it is not an
	 * edge clamp), so the border output pixels legitimately overshoot (Catmull-Rom near the
	 * constant->Black step). That overshoot is pre-existing behaviour, identical before/after the
	 * refactor; interior pixels (whose full 4x4 neighbourhood is in bounds) must be exactly 200. */
	Pixmap< uint8_t > source{16, 16, ChannelMode::RGBA};

	auto & data = source.data();

	for ( auto & value : data )
	{
		value = 200;
	}

	const auto output = Processor< uint8_t >::resize(source, 40, 40, FilteringMode::Cubic);

	ASSERT_EQ(output.width(), 40);
	ASSERT_EQ(output.height(), 40);
	ASSERT_EQ(output.channelMode(), ChannelMode::RGBA);

	const auto & outputData = output.data();

	/* Interior region [8,31]^2 maps to a fully in-bounds source neighbourhood for a 16->40 upscale. */
	for ( size_t y = 8; y <= 31; ++y )
	{
		for ( size_t x = 8; x <= 31; ++x )
		{
			const auto pixelOffset = ((y * 40) + x) * 4;

			EXPECT_EQ(outputData[pixelOffset], 200);
			EXPECT_EQ(outputData[pixelOffset + 1], 200);
			EXPECT_EQ(outputData[pixelOffset + 2], 200);
			EXPECT_EQ(outputData[pixelOffset + 3], 200);
		}
	}
}

TEST(PixelFactoryProcessor, coloredStencilRespectsMask)
{
	/* Ave robustus! (Axis B): the colored-source stencil was a `return false` stub. Stencil semantics:
	 * grayscale mask = coverage, white passes, black blocks. Left half white -> filled with red;
	 * right half black -> left untouched. */
	Pixmap< uint8_t > mask{4, 2, ChannelMode::Grayscale};

	{
		auto & maskData = mask.data();
		const uint8_t pattern[8] = {255, 255, 0, 0, 255, 255, 0, 0};

		for ( size_t index = 0; index < maskData.size(); ++index )
		{
			maskData[index] = pattern[index];
		}
	}

	/* Force a known all-zero base (a fresh RGBA pixmap initialises alpha to opaque, not 0). */
	Pixmap< uint8_t > target{4, 2, ChannelMode::RGBA};

	for ( auto & value : target.data() )
	{
		value = 0;
	}

	Processor< uint8_t > processor{target};

	ASSERT_TRUE(processor.stencil(Red, Math::Space2D::AARectangle< uint32_t >{0, 0, 4, 2}, mask, DrawPixelMode::Replace));

	const auto & data = target.data();

	/* (col 0, row 0): white mask -> red written. */
	EXPECT_EQ(data[0], 255);
	EXPECT_EQ(data[1], 0);
	EXPECT_EQ(data[2], 0);
	EXPECT_EQ(data[3], 255);

	/* (col 2, row 0): black mask -> untouched (still zero). */
	EXPECT_EQ(data[8], 0);
	EXPECT_EQ(data[9], 0);
	EXPECT_EQ(data[10], 0);
	EXPECT_EQ(data[11], 0);
}
