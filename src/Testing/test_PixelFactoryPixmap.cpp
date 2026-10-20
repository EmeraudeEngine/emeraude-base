/*
 * src/Testing/test_PixelFactoryPixmap.cpp
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

/* STL inclusions. */
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

/* Local inclusions. */
#include "Constants.hpp"
#include "PixelFactory/Color.hpp"
#include "PixelFactory/FileIO.hpp"
#include "PixelFactory/Gradient.hpp"
#include "PixelFactory/Pixmap.hpp"
#include "PixelFactory/Types.hpp"
#include "Randomizer.hpp"
#include "Time/Elapsed/PrintScopeRealTime.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::PixelFactory;
using namespace EmEn::Base::Time::Elapsed;

TEST(PixelFactoryPixmap, constructor)
{
	PrintScopeRealTime stat{"Pixmap::Pixmap(1024, 768, ChannelMode::RGBA)"};

	Pixmap< uint8_t > image{1024, 768, ChannelMode::RGBA};

	ASSERT_EQ(image.width(), 1024);
	ASSERT_EQ(image.height(), 768);
	ASSERT_EQ(image.colorCount(), 4);
	ASSERT_TRUE(image.isValid());

	ASSERT_EQ(image.pixel(32, 32), Black);
}

TEST(PixelFactoryPixmap, readPNG_Palette)
{
	PrintScopeRealTime stat{"Pixmap::read(PNG_Palette)"};

	Pixmap< uint8_t > image;

	ASSERT_TRUE(FileIO::read(MediumPalette, image));

	ASSERT_EQ(image.width(), 512);
	ASSERT_EQ(image.height(), 512);
	ASSERT_EQ(image.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_512x512-Palette.png", true));
}

TEST(PixelFactoryPixmap, readPNG_Grayscale)
{
	PrintScopeRealTime stat{"Pixmap::read(PNG_Grayscale)"};

	Pixmap< uint8_t > image;

	ASSERT_TRUE(FileIO::read(MediumGrayscale, image));

	ASSERT_EQ(image.width(), 512);
	ASSERT_EQ(image.height(), 512);
	ASSERT_EQ(image.colorCount(), 1);

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_512x512-Grayscale.png", true));
}

TEST(PixelFactoryPixmap, readPNG_GrayscaleAlpha)
{
	PrintScopeRealTime stat{"Pixmap::read(PNG_GrayscaleAlpha)"};

	Pixmap< uint8_t > image;

	ASSERT_TRUE(FileIO::read(MediumGrayscaleAlpha, image));

	ASSERT_EQ(image.width(), 512);
	ASSERT_EQ(image.height(), 512);
	ASSERT_EQ(image.colorCount(), 2);

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_512x512-GrayscaleAlpha.png", true));
}

TEST(PixelFactoryPixmap, readPNG_RGB)
{
	PrintScopeRealTime stat{"Pixmap::read(PNG_RGB)"};

	Pixmap< uint8_t > image;

	ASSERT_TRUE(FileIO::read(MediumRGB, image));

	ASSERT_EQ(image.width(), 512);
	ASSERT_EQ(image.height(), 512);
	ASSERT_EQ(image.colorCount(), 3);

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_512x512-RGB.png", true));
}

TEST(PixelFactoryPixmap, readPNG_RGBA)
{
	PrintScopeRealTime stat{"Pixmap::read(PNG_RGBA)"};

	Pixmap< uint8_t > image;

	ASSERT_TRUE(FileIO::read(MediumRGBA, image));

	ASSERT_EQ(image.width(), 512);
	ASSERT_EQ(image.height(), 512);
	ASSERT_EQ(image.colorCount(), 4);

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_512x512-RGBA.png", true));
}

TEST(PixelFactoryPixmap, fillValue)
{
	Pixmap< uint8_t > image{340, 120, ChannelMode::RGBA};

	{
		PrintScopeRealTime stat{"Pixmap::fill(127)"};

		ASSERT_TRUE(image.fill(127));
	}

	ASSERT_EQ(image.pixel(32, 32), ColorFromInteger< uint8_t >(127, 127, 127, 255));

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillValue.png", true));
}

TEST(PixelFactoryPixmap, fillSequence)
{
	Randomizer< uint8_t > randomizer;

	const auto random = randomizer.vector(100, 0, 255);

	Pixmap< uint8_t > image{340, 120, ChannelMode::RGBA};

	{
		PrintScopeRealTime stat{"Pixmap::fill(sequence)"};

		ASSERT_TRUE(image.fill(random));
	}

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillSequence.png", true));
}

TEST(PixelFactoryPixmap, fillColor)
{
	const auto color = ColorFromInteger< uint8_t >(127, 255, 255, 255);

	Pixmap< uint8_t > image{340, 120, ChannelMode::RGBA};

	{
		PrintScopeRealTime stat{"Pixmap::fill(color)"};

		ASSERT_TRUE(image.fill(color));
	}

	ASSERT_EQ(image.pixel(32, 32), color);

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillColor.png", true));
}

TEST(PixelFactoryPixmap, fillPattern)
{
	Pixmap< uint8_t > pattern;

	ASSERT_TRUE(FileIO::read(SmallRGBA, pattern));
	ASSERT_EQ(pattern.width(), 64);
	ASSERT_EQ(pattern.height(), 64);
	ASSERT_EQ(pattern.colorCount(), 4);

	Pixmap< uint8_t > image{340, 120, ChannelMode::RGBA};

	{
		PrintScopeRealTime stat{"Pixmap::fill(pattern)"};

		ASSERT_TRUE(image.fill(pattern));
	}

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillPattern.png", true));
}

TEST(PixelFactoryPixmap, fillHorizontalGradient)
{
	Pixmap< uint8_t > image{340, 120, ChannelMode::RGBA};

	{
		PrintScopeRealTime stat{"Pixmap::fillHorizontal(gradient2)"};

		ASSERT_TRUE(image.fillHorizontal({Red, Green}));
	}

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillHorizontalGradient2.png", true));

	{
		PrintScopeRealTime stat{"Pixmap::fillHorizontal(gradient3)"};

		Gradient< float, float > gradient;
		gradient.addColorAt(0.0F, Red);
		gradient.addColorAt(0.5F, Blue);
		gradient.addColorAt(1.0F, Green);

		ASSERT_TRUE(image.fillHorizontal(gradient));
	}

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillHorizontalGradient3.png", true));
}

TEST(PixelFactoryPixmap, fillVerticalGradient)
{
	Pixmap< uint8_t > image{340, 120, ChannelMode::RGBA};

	{
		PrintScopeRealTime stat{"Pixmap::fillVertical(gradient2)"};

		ASSERT_TRUE(image.fillVertical({Green, Cyan}));
	}

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillVerticalGradient2.png", true));

	{
		PrintScopeRealTime stat{"Pixmap::fillVertical(gradient3)"};

		Gradient< float, float > gradient;
		gradient.addColorAt(0.0F, LightBlue);
		gradient.addColorAt(0.5F, DarkRed);
		gradient.addColorAt(1.0F, Grey);

		ASSERT_TRUE(image.fillVertical(gradient));
	}

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillVerticalGradient3.png", true));
}

TEST(PixelFactoryPixmap, fillChannelValue)
{
	Pixmap< uint8_t > image{256, 256, ChannelMode::RGB};

	{
		PrintScopeRealTime stat{"Pixmap::fillChannel(Red, 255)"};

		ASSERT_TRUE(image.fillChannel(Channel::Red, 255));

		ASSERT_EQ(image.pixel(32, 32), Red);

		ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillChannelRedValue.png", true));

		ASSERT_TRUE(image.fillChannel(Channel::Red, 0));
	}

	{
		PrintScopeRealTime stat{"Pixmap::fillChannel(Green, 255)"};

		ASSERT_TRUE(image.fillChannel(Channel::Green, 255));

		ASSERT_EQ(image.pixel(32, 32), Green);

		ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillChannelGreenValue.png", true));

		ASSERT_TRUE(image.fillChannel(Channel::Green, 0));
	}

	{
		PrintScopeRealTime stat{"Pixmap::fillChannel(Blue, 255)"};

		ASSERT_TRUE(image.fillChannel(Channel::Blue, 255));

		ASSERT_EQ(image.pixel(32, 32), Blue);

		ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillChannelBlueValue.png", true));

		ASSERT_TRUE(image.fillChannel(Channel::Blue, 0));
	}
}

TEST(PixelFactoryPixmap, copyChannelPacksOneChannelOfAnotherPixmap)
{
	constexpr uint32_t Width{4};
	constexpr uint32_t Height{3};
	constexpr size_t PixelCount{static_cast< size_t >(Width) * Height};

	/* Every channel of the source is distinct per pixel: R = i, G = i + 50, B = i + 100, A = i + 150. The packed
	 * pixmap starts at 7 everywhere and expects the source ALPHA in its GREEN, nothing else changed. */
	std::vector< uint8_t > sourceData(PixelCount * 4);
	std::vector< uint8_t > expected(PixelCount * 4, 7);

	for ( size_t pixel = 0; pixel < PixelCount; ++pixel )
	{
		const auto value = static_cast< uint8_t >(pixel);

		sourceData[(pixel * 4) + 0] = value;
		sourceData[(pixel * 4) + 1] = static_cast< uint8_t >(value + 50);
		sourceData[(pixel * 4) + 2] = static_cast< uint8_t >(value + 100);
		sourceData[(pixel * 4) + 3] = static_cast< uint8_t >(value + 150);
		expected[(pixel * 4) + 1] = static_cast< uint8_t >(value + 150);
	}

	const Pixmap< uint8_t > source{Width, Height, ChannelMode::RGBA, std::span< const uint8_t >{sourceData}};
	Pixmap< uint8_t > packed{Width, Height, ChannelMode::RGBA, std::span< const uint8_t >{std::vector< uint8_t >(PixelCount * 4, 7)}};

	ASSERT_TRUE(packed.copyChannel(Channel::Green, source, Channel::Alpha));
	EXPECT_EQ(packed.data(), expected);

	/* A grey-scale source has one channel, read as red. */
	const Pixmap< uint8_t > grey{Width, Height, ChannelMode::Grayscale, std::span< const uint8_t >{std::vector< uint8_t >(PixelCount, 200)}};

	ASSERT_TRUE(packed.copyChannel(Channel::Alpha, grey, Channel::Red));
	EXPECT_EQ(packed.pixelElement(3, 2, Channel::Alpha), 200);
	EXPECT_EQ(packed.pixelElement(3, 2, Channel::Green), static_cast< uint8_t >(11 + 150));
}

TEST(PixelFactoryPixmap, copyChannelRefusesWhatCannotBeCopied)
{
	Pixmap< uint8_t > packed{4, 3, ChannelMode::RGBA};
	ASSERT_TRUE(packed.fill(static_cast< uint8_t >(7)));

	Pixmap< uint8_t > otherSize{3, 4, ChannelMode::RGBA};
	ASSERT_TRUE(otherSize.fill(static_cast< uint8_t >(9)));

	Pixmap< uint8_t > grey{4, 3, ChannelMode::Grayscale};
	ASSERT_TRUE(grey.fill(static_cast< uint8_t >(9)));

	Pixmap< uint8_t > rgb{4, 3, ChannelMode::RGB};
	ASSERT_TRUE(rgb.fill(static_cast< uint8_t >(9)));

	const Pixmap< uint8_t > invalid;

	/* Another size, a source channel the source does not have, a target channel this pixmap does not have, an
	 * invalid source: refused, nothing written. */
	EXPECT_FALSE(packed.copyChannel(Channel::Red, otherSize, Channel::Red));
	EXPECT_FALSE(packed.copyChannel(Channel::Red, grey, Channel::Alpha));
	EXPECT_FALSE(rgb.copyChannel(Channel::Alpha, packed, Channel::Red));
	EXPECT_FALSE(packed.copyChannel(Channel::Red, invalid, Channel::Red));

	EXPECT_EQ(packed.pixelElement(0, 0, Channel::Red), 7);
	EXPECT_EQ(rgb.pixelElement(0, 0, Channel::Red), 9);
}

TEST(PixelFactoryPixmap, fillChannelSequence)
{
	Randomizer< uint8_t > randomizer;

	const auto random = randomizer.vector(100, 0, 255);

	Pixmap< uint8_t > image{256, 256, ChannelMode::RGBA};

	{
		PrintScopeRealTime stat{"Pixmap::fillChannel(sequence)"};

		ASSERT_TRUE(image.fillChannel(Channel::Alpha, random));
	}

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillChannelAlphaSequence.png", true));
}

TEST(PixelFactoryPixmap, fillChannelPattern)
{
	Pixmap< uint8_t > redChannel;
	Pixmap< uint8_t > greenChannel;
	Pixmap< uint8_t > blueChannel;
	Pixmap< uint8_t > alphaChannel;

	ASSERT_TRUE(FileIO::read(SmallPatternRBG_1, redChannel));
	ASSERT_TRUE(FileIO::read(SmallPatternRBG_2, greenChannel));
	ASSERT_TRUE(FileIO::read(SmallPatternRBG_3, blueChannel));
	ASSERT_TRUE(FileIO::read(MediumPatternRBG_1, alphaChannel));

	Pixmap< uint8_t > image{1024, 768, ChannelMode::RGBA};

	{
		PrintScopeRealTime stat{"Pixmap::fillChannel(PatternOnRed)"};

		ASSERT_TRUE(image.fillChannel(Channel::Red, redChannel));
	}

	{
		PrintScopeRealTime stat{"Pixmap::fillChannel(PatternOnGreen)"};

		ASSERT_TRUE(image.fillChannel(Channel::Green, greenChannel));
	}

	{
		PrintScopeRealTime stat{"Pixmap::fillChannel(PatternOnBlue)"};

		ASSERT_TRUE(image.fillChannel(Channel::Blue, blueChannel));
	}

	{
		PrintScopeRealTime stat{"Pixmap::fillChannelPattern(PatternOnAlpha)"};

		ASSERT_TRUE(image.fillChannel(Channel::Alpha, alphaChannel));
	}

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillChannelPattern.png", true));
}

TEST(PixelFactoryPixmap, fillChannelGradient)
{
	Pixmap< uint8_t > image{800, 600, ChannelMode::RGB};

	{
		PrintScopeRealTime stat{"Pixmap::fillVerticalChannel(Gradient2)"};

		ASSERT_TRUE(image.fillVerticalChannel(Channel::Red, {LightBlue, DarkMagenta}));
	}

	{
		PrintScopeRealTime stat{"Pixmap::fillHorizontalChannel(Gradient5)"};

		Gradient< float, float > gradient;
		gradient.addColorAt(0.0F, DarkGreen);
		gradient.addColorAt(0.25F, LightYellow);
		gradient.addColorAt(0.5F, White);
		gradient.addColorAt(0.75F, LightYellow);
		gradient.addColorAt(1.0F, DarkRed);

		ASSERT_TRUE(image.fillHorizontalChannel(Channel::Green, gradient));
	}

	{
		PrintScopeRealTime stat{"Pixmap::fillVerticalChannel(Gradient3)"};

		Gradient< float, float > gradient;
		gradient.addColorAt(0.0F, LightBlue);
		gradient.addColorAt(0.5F, DarkRed);
		gradient.addColorAt(1.0F, Grey);

		ASSERT_TRUE(image.fillVerticalChannel(Channel::Blue, gradient));
	}

	ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_fillChannelGradient.png", true));
}

TEST(PixelFactoryPixmap, noiseGeneration)
{
	Pixmap< uint8_t > image{1024, 1024, ChannelMode::RGB};

	{
		PrintScopeRealTime stat{"Pixmap::noise()"};

		ASSERT_TRUE(image.noise());

		ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_1024x1024-noise.png", true));
	}

	{
		PrintScopeRealTime stat{"Pixmap::perlinNoise(9.0F, false)"};

		ASSERT_TRUE(image.perlinNoise(9.0F, false));

		ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_1024x1024-perlinNoise.png", true));
	}

	{
		PrintScopeRealTime stat{"Pixmap::perlinNoise(9.0F, true)"};

		ASSERT_TRUE(image.perlinNoise(9.0F, true));

		ASSERT_TRUE(FileIO::write(image, AssetsDirectory / "tmp_1024x1024-perlinNoiseGray.png", true));
	}
}

TEST(PixelFactoryPixmap, addAlphaChannelGrayscale)
{
	/* Ave robustus! (A.5): regression for the Grayscale-case fall-through (former OOB read + wrong
	 * output). 2x1 grayscale -> GrayscaleAlpha, each gray followed by the requested alpha. */
	const std::array< uint8_t, 2 > grayData{10, 20};
	Pixmap< uint8_t > image{2, 1, ChannelMode::Grayscale, grayData};

	ASSERT_TRUE(image.addAlphaChannel(255));
	EXPECT_EQ(image.channelMode(), ChannelMode::GrayscaleAlpha);
	EXPECT_EQ(image.colorCount(), 2);
	EXPECT_EQ(image.data(), (std::vector< uint8_t >{10, 255, 20, 255}));
}

TEST(PixelFactoryPixmap, addAlphaChannelRGB)
{
	/* 2x1 RGB -> RGBA, each triplet followed by the requested alpha (copy-free in-place expand). */
	const std::array< uint8_t, 6 > rgbData{1, 2, 3, 4, 5, 6};
	Pixmap< uint8_t > image{2, 1, ChannelMode::RGB, rgbData};

	ASSERT_TRUE(image.addAlphaChannel(128));
	EXPECT_EQ(image.channelMode(), ChannelMode::RGBA);
	EXPECT_EQ(image.colorCount(), 4);
	EXPECT_EQ(image.data(), (std::vector< uint8_t >{1, 2, 3, 128, 4, 5, 6, 128}));
}

TEST(PixelFactoryPixmap, addAlphaChannelAlreadyHasAlpha)
{
	/* RGBA / GrayscaleAlpha already carry alpha: addAlphaChannel must be a no-op success. */
	const std::array< uint8_t, 8 > rgbaData{1, 2, 3, 4, 5, 6, 7, 8};
	Pixmap< uint8_t > image{2, 1, ChannelMode::RGBA, rgbaData};

	ASSERT_TRUE(image.addAlphaChannel(0));
	EXPECT_EQ(image.channelMode(), ChannelMode::RGBA);
	EXPECT_EQ(image.data(), (std::vector< uint8_t >{1, 2, 3, 4, 5, 6, 7, 8}));
}

/* Until 2026-10-17 the element count was computed in uint32 and WRAPPED: 65536x65536 grayscale (2^32 elements) allocated
 * NOTHING and initialize() returned true — width() and height() said 65536, the buffer was empty. Owner decision: a pixmap
 * beyond the uint32 element accounting of its getters is refused. Nothing is allocated by these checks. */
TEST(Pixmap, elementCountBeyondUint32IsRefused)
{
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	EXPECT_FALSE(pixmap.initialize(65536, 65536, PixelFactory::ChannelMode::Grayscale));
	EXPECT_FALSE(pixmap.isValid());
	EXPECT_TRUE(pixmap.data().empty());

	EXPECT_FALSE(pixmap.initialize(65000, 65000, PixelFactory::ChannelMode::RGB));
	EXPECT_FALSE(pixmap.isValid());

	const PixelFactory::Pixmap< uint8_t, uint32_t > constructed{70000, 70000, PixelFactory::ChannelMode::RGBA};

	EXPECT_FALSE(constructed.isValid());
	EXPECT_TRUE(constructed.data().empty());
}

TEST(Pixmap, elementAndPixelCountsAreExact)
{
	/* The guard must not change what a normal pixmap reports. */
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_TRUE(pixmap.initialize(4, 4, PixelFactory::ChannelMode::RGB));
	EXPECT_EQ(pixmap.elementCount(), 48U);
	EXPECT_EQ(pixmap.pixelCount(), 16U);
	EXPECT_EQ(pixmap.data().size(), 48U);
}
