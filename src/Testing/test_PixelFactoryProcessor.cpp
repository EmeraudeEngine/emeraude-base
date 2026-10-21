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

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <numbers>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Constants.hpp"
#include "Math/Space2D/AARectangle.hpp"
#include "Math/Vector.hpp"
#include "PixelFactory/Color.hpp"
#include "PixelFactory/FileIO.hpp"
#include "PixelFactory/IOCommon.hpp"
#include "PixelFactory/Pixmap.hpp"
#include "PixelFactory/Processor.hpp"
#include "PixelFactory/Reduction.hpp"
#include "PixelFactory/Types.hpp"
#include "TemporaryPath.hpp"
#include "ThreadPool.hpp"
#include "Time/Elapsed/PrintScopeRealTime.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::PixelFactory;
using namespace EmEn::Base::Time::Elapsed;

namespace
{
	/**
	 * @brief Resamples a texture filled with 200 to 16 x 16 and answers whether every value stayed 200.
	 * @param source The 200-filled source.
	 * @param wrap The addressing.
	 * @return bool
	 */
	[[nodiscard]]
	bool
	resamplesToTheSameConstant (const Pixmap< uint8_t > & source, bool wrap) noexcept
	{
		Pixmap< uint8_t > upscaled;

		return Processor< uint8_t >::resampleTexture(source, 16, 16, upscaled, wrap) && upscaled.width() == 16 && upscaled.height() == 16 &&
			std::ranges::all_of(upscaled.data(), [] (uint8_t value) { return value == 200; });
	}
}

TEST(PixelFactoryProcessor, resampleTextureKeepsAConstantTextureConstantUpToItsEdges)
{
	const Pixmap< uint8_t > source{8, 8, ChannelMode::RGBA, std::span< const uint8_t >{std::vector< uint8_t >(size_t{8} * 8 * 4, 200)}};

	/* resize() reads Black past the border: its edges darken. A texture resample wraps (or clamps): none does. */
	EXPECT_TRUE(resamplesToTheSameConstant(source, true));
	EXPECT_TRUE(resamplesToTheSameConstant(source, false));
}

TEST(PixelFactoryProcessor, resampleTextureAlignsTexelCentres)
{
	/* A 2-texel ramp, clamped, doubled: centre alignment reads the source at -0.25, 0.25, 0.75, 1.25 — a result
	 * symmetric around the middle (each pair sums to 255). resize() maps the corners and loses that symmetry. */
	Pixmap< uint8_t > source{2, 1, ChannelMode::Grayscale};
	source.setPixelElement(0, 0, Channel::Red, 0);
	source.setPixelElement(1, 0, Channel::Red, 255);

	Pixmap< uint8_t > upscaled;
	ASSERT_TRUE(Processor< uint8_t >::resampleTexture(source, 4, 1, upscaled, false));

	const auto & data = upscaled.data();
	ASSERT_EQ(data.size(), 4U);
	EXPECT_EQ(data[0], 0);
	EXPECT_EQ(data[3], 255);
	EXPECT_NEAR(static_cast< int >(data[1]) + static_cast< int >(data[2]), 255, 1);
	EXPECT_LT(data[1], data[2]);
}

TEST(PixelFactoryProcessor, resampleTextureWrapsARepeatingTexture)
{
	/* Only the first texel is lit. Doubled, the last destination texel reads the source at 3.25, between the last
	 * texel and, wrapped around, the first one: Catmull-Rom gives ~58 with repeat addressing, 0 with clamping. */
	Pixmap< uint8_t > source{4, 1, ChannelMode::Grayscale};
	ASSERT_TRUE(source.fill(static_cast< uint8_t >(0)));
	source.setPixelElement(0, 0, Channel::Red, 255);

	Pixmap< uint8_t > wrapped;
	ASSERT_TRUE(Processor< uint8_t >::resampleTexture(source, 8, 1, wrapped, true));

	Pixmap< uint8_t > clamped;
	ASSERT_TRUE(Processor< uint8_t >::resampleTexture(source, 8, 1, clamped, false));

	EXPECT_GT(wrapped.data()[7], 32);
	EXPECT_LT(clamped.data()[7], 16);
}

TEST(PixelFactoryProcessor, resampleTextureRefusesAnInvalidRequest)
{
	const Pixmap< uint8_t > invalid;
	const Pixmap< uint8_t > source{4, 4, ChannelMode::RGB};
	Pixmap< uint8_t > destination;

	EXPECT_FALSE(Processor< uint8_t >::resampleTexture(invalid, 8, 8, destination));
	EXPECT_FALSE(Processor< uint8_t >::resampleTexture(source, 0, 8, destination));
	EXPECT_FALSE(Processor< uint8_t >::resampleTexture(source, 8, 0, destination));
}

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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_scaleAllValue.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_scaleRedValue.png", true));
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

	ASSERT_TRUE(FileIO::write(emptyImage, AssetsDirectory / "tmp_drawing.png", true));
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

	ASSERT_TRUE(FileIO::write(source, AssetsDirectory / "tmp_moved.png", true));
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

	ASSERT_TRUE(FileIO::write(source, AssetsDirectory / "tmp_shifted.png", true));
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

	ASSERT_TRUE(FileIO::write(source, AssetsDirectory / "tmp_textShifted.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_resizeDownNearest.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_resizeDownLinear.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_resizeDownCubic.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_resizeUpNearest.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_resizeUpLinear.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_resizeUpCubic.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_cropped.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_extractChannelRed.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_extractChannelGreen.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_extractChannelBlue.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_toGrayscale.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_toRGB.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_toRGBA.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_mirrorX.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_mirrorY.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_mirrorBoth.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_extend.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_rotated+90.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_rotated+180.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_rotated+270.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_inverseColors.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_swapChannels.png", true));
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

	ASSERT_TRUE(FileIO::write(source, AssetsDirectory / "tmp_64x64-blit.png", true));
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

	ASSERT_TRUE(FileIO::write(source, AssetsDirectory / "tmp_copy.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_addAlphaChannel.png", true));
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

	ASSERT_TRUE(FileIO::write(output, AssetsDirectory / "tmp_removeAlphaChannel.png", true));
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

TEST(PixelFactoryProcessor, pixmapStencilRespectsMask)
{
	/* Ave robustus! (Axis B): the pixmap-source stencil used to ignore its mask (FIXME) and misuse
	 * blendPixel(index,row,...). It now copies the source through the mask: white passes, black blocks.
	 * Source = solid red; mask left-white/right-black; target zeroed -> left half red, right untouched. */
	Pixmap< uint8_t > source{4, 2, ChannelMode::RGBA};

	{
		auto & sourceData = source.data();

		for ( size_t pixel = 0; pixel < 8; ++pixel )
		{
			sourceData[(pixel * 4) + 0] = 255;
			sourceData[(pixel * 4) + 1] = 0;
			sourceData[(pixel * 4) + 2] = 0;
			sourceData[(pixel * 4) + 3] = 255;
		}
	}

	Pixmap< uint8_t > mask{4, 2, ChannelMode::Grayscale};

	{
		auto & maskData = mask.data();
		const uint8_t pattern[8] = {255, 255, 0, 0, 255, 255, 0, 0};

		for ( size_t index = 0; index < maskData.size(); ++index )
		{
			maskData[index] = pattern[index];
		}
	}

	const auto checkResult = [] (const Pixmap< uint8_t > & out) {
		const auto & data = out.data();

		/* (col 0): white mask -> red copied. */
		EXPECT_EQ(data[0], 255);
		EXPECT_EQ(data[1], 0);
		EXPECT_EQ(data[2], 0);
		EXPECT_EQ(data[3], 255);

		/* (col 2): black mask -> untouched (zero). */
		EXPECT_EQ(data[8], 0);
		EXPECT_EQ(data[9], 0);
		EXPECT_EQ(data[10], 0);
		EXPECT_EQ(data[11], 0);
	};

	/* Core overload (explicit source/destination clips). */
	{
		Pixmap< uint8_t > target{4, 2, ChannelMode::RGBA};

		for ( auto & value : target.data() )
		{
			value = 0;
		}

		Processor< uint8_t > processor{target};

		ASSERT_TRUE(processor.stencil(source, Math::Space2D::AARectangle< uint32_t >{0, 0, 4, 2}, Math::Space2D::AARectangle< uint32_t >{0, 0, 4, 2}, mask, DrawPixelMode::Replace));
		checkResult(target);
	}

	/* Convenience overload (full source -> clip); also forces instantiation of the formerly
	 * uncompilable forwarding call. */
	{
		Pixmap< uint8_t > target{4, 2, ChannelMode::RGBA};

		for ( auto & value : target.data() )
		{
			value = 0;
		}

		Processor< uint8_t > processor{target};

		ASSERT_TRUE(processor.stencil(source, Math::Space2D::AARectangle< uint32_t >{0, 0, 4, 2}, mask, DrawPixelMode::Replace, 1.0F));
		checkResult(target);
	}
}

namespace
{
	/** @brief The mean of channel 0 of a pixmap, in [0, 1]. */
	double
	channelMean (const Pixmap< uint8_t > & pixmap) noexcept
	{
		const auto & data = pixmap.data();
		const auto stride = static_cast< size_t >(pixmap.colorCount());

		double sum = 0.0;

		for ( size_t index = 0; index < data.size(); index += stride )
		{
			sum += data[index];
		}

		return sum / (255.0 * static_cast< double >(data.size() / stride));
	}
}

/* A 2:1 reduction of an even pixmap is exactly the 2 × 2 box average (rounded). */
TEST(PixelFactoryProcessor, downsampleHalvesAsTheTwoByTwoBox)
{
	Pixmap< uint8_t > source;
	ASSERT_TRUE(source.initialize(4, 2, ChannelMode::Grayscale));

	const std::array< uint8_t, 8 > values{0, 255, 10, 20, 255, 0, 30, 41};
	std::copy(values.begin(), values.end(), source.data().begin());

	Pixmap< uint8_t > target;
	ASSERT_TRUE(Processor< uint8_t >::downsample(source, 2, 1, target));

	EXPECT_EQ(target.data()[0], 128);
	EXPECT_EQ(target.data()[1], 25);
}

/* THE mip property: the mean survives every level, down to 1 × 1, on a SPARSE mask whose width is odd at every
 * level (1022 → 511 → 255 …). resize(Linear) reads the corner pixel for a 1 × 1 target and sent a 23 % leaf mask to
 * 0.001 at 1 × 4 — distant pines lost their needles. */
TEST(PixelFactoryProcessor, downsampleKeepsTheMeanDownToOnePixelOnANonPowerOfTwoMask)
{
	Pixmap< uint8_t > mask;
	ASSERT_TRUE(mask.initialize(1022, 2048, ChannelMode::Grayscale));

	/* Thin diagonal needles on black, about a quarter covered, none in the corner. */
	for ( uint32_t y = 0; y < mask.height(); ++y )
	{
		for ( uint32_t x = 0; x < mask.width(); ++x )
		{
			const bool needle = x > 8 && y > 8 && ((x + y) % 8) < 2;

			mask.data()[static_cast< size_t >(y) * mask.width() + x] = needle ? 255 : 0;
		}
	}

	const auto baseMean = channelMean(mask);

	ASSERT_GT(baseMean, 0.2);

	auto level = mask;

	while ( level.width() > 1 || level.height() > 1 )
	{
		Pixmap< uint8_t > next;

		ASSERT_TRUE(Processor< uint8_t >::downsample(level, std::max(level.width() / 2, 1U), std::max(level.height() / 2, 1U), next));

		/* The truncating halving of an odd width drops no pixel: the footprint is 2.x wide. Rounding to 8 bits
		 * costs at most half a code value per level. */
		EXPECT_NEAR(channelMean(next), baseMean, 2.0 / 255.0) << "at " << next.width() << " x " << next.height();

		level = std::move(next);
	}
}

/* Every channel is filtered, and an enlarging request is refused. */
TEST(PixelFactoryProcessor, downsampleFiltersEveryChannelAndRefusesToEnlarge)
{
	Pixmap< uint8_t > source;
	ASSERT_TRUE(source.initialize(3, 3, ChannelMode::RGBA));

	for ( size_t index = 0; index < source.data().size(); index += 4 )
	{
		source.data()[index] = 90;
		source.data()[index + 1] = 180;
		source.data()[index + 2] = 30;
		source.data()[index + 3] = 255;
	}

	Pixmap< uint8_t > target;
	ASSERT_TRUE(Processor< uint8_t >::downsample(source, 1, 1, target));

	EXPECT_EQ(target.data()[0], 90);
	EXPECT_EQ(target.data()[1], 180);
	EXPECT_EQ(target.data()[2], 30);
	EXPECT_EQ(target.data()[3], 255);

	EXPECT_FALSE(Processor< uint8_t >::downsample(source, 4, 3, target));
}

namespace
{
	/**
	 * @brief Answers whether every destination pixel of a kernel has weights summing to 1, taps inside the source, and a
	 * span within maximumSpan().
	 * @param sourceSize The source size.
	 * @param destinationSize The destination size.
	 * @param filter The filter.
	 * @return bool
	 */
	bool
	isNormalisedAndInside (size_t sourceSize, size_t destinationSize, ReductionFilter filter) noexcept
	{
		const ReductionKernel kernel{sourceSize, destinationSize, filter};

		if ( !kernel.isValid() || kernel.destinationSize() != destinationSize )
		{
			return false;
		}

		for ( size_t target = 0; target < destinationSize; ++target )
		{
			const auto taps = kernel.taps(target);
			double sum = 0.0;

			for ( const auto & tap : taps )
			{
				sum += static_cast< double >(tap.weight);
			}

			const auto [lowest, highest] = std::ranges::minmax_element(taps, {}, &ReductionTap::index);

			if ( std::abs(sum - 1.0) > 1.0e-5 || highest->index >= sourceSize || highest->index - lowest->index + 1 > kernel.maximumSpan() )
			{
				return false;
			}
		}

		return true;
	}
}

/* Every kernel's weights sum to 1 for every destination pixel, the taps stay inside the source (the Kaiser mirrors
 * them), and one destination pixel reads at most maximumSpan() consecutive rows. */
TEST(PixelFactoryProcessor, reductionKernelIsNormalisedAndStaysInside)
{
	for ( const auto filter : {ReductionFilter::Box, ReductionFilter::Kaiser} )
	{
		for ( const auto & [sourceSize, destinationSize] : std::array< std::pair< size_t, size_t >, 7 >{{{1, 1}, {2, 1}, {3, 1}, {5, 2}, {1022, 511}, {511, 255}, {64, 32}}} )
		{
			EXPECT_TRUE(isNormalisedAndInside(sourceSize, destinationSize, filter)) << sourceSize << " -> " << destinationSize;
		}
	}
}

namespace
{
	/**
	 * @brief Answers whether a kernel is the identity: one tap of weight 1 on its own pixel, for every pixel.
	 * @param kernel The kernel.
	 * @return bool
	 */
	bool
	isIdentity (const ReductionKernel & kernel) noexcept
	{
		for ( size_t target = 0; target < kernel.destinationSize(); ++target )
		{
			const auto taps = kernel.taps(target);

			if ( taps.size() != 1 || taps.front().index != target || taps.front().weight != 1.0F )
			{
				return false;
			}
		}

		return kernel.isValid();
	}
}

/* An unchanged size is the identity, for both filters; a null or enlarging size is refused. */
TEST(PixelFactoryProcessor, reductionKernelIdentityAndRefusals)
{
	EXPECT_TRUE(isIdentity(ReductionKernel{7, 7, ReductionFilter::Box}));
	EXPECT_TRUE(isIdentity(ReductionKernel{7, 7, ReductionFilter::Kaiser}));

	EXPECT_FALSE((ReductionKernel{0, 0, ReductionFilter::Kaiser}).isValid());
	EXPECT_FALSE((ReductionKernel{4, 5, ReductionFilter::Kaiser}).isValid());
}

/* NVTT's filter: 1 at the centre, 0 at and past its half-width of 3, a sinc's zero at every other integer. */
TEST(PixelFactoryProcessor, kaiserFilterShape)
{
	EXPECT_DOUBLE_EQ(ReductionKernel::kaiser(0.0), 1.0);
	EXPECT_NEAR(ReductionKernel::kaiser(1.0), 0.0, 1.0e-12);
	EXPECT_NEAR(ReductionKernel::kaiser(2.0), 0.0, 1.0e-12);
	EXPECT_NEAR(ReductionKernel::kaiser(3.0), 0.0, 1.0e-12);
	EXPECT_EQ(ReductionKernel::kaiser(-4.5), 0.0);
	EXPECT_LT(ReductionKernel::kaiser(1.5), 0.0);
}

/* The sRGB byte table round-trips through the encoder, and matches the transfer function. */
TEST(PixelFactoryProcessor, sRGBByteTableRoundTrips)
{
	for ( int value = 0; value <= 255; ++value )
	{
		const auto byte = static_cast< uint8_t >(value);

		EXPECT_EQ(linearToSRGBByte(sRGBByteToLinear(byte)), byte);
		EXPECT_NEAR(sRGBByteToLinear(byte), sRGBToLinear(static_cast< float >(value) / 255.0F), 1.0e-7F);
	}
}

/* The bucketed encoder is round(encode × 255) exactly, and refuses what is outside [0, 1]. */
TEST(PixelFactoryProcessor, sRGBByteEncoderIsTheNearestCode)
{
	for ( int step = 0; step <= 4000; ++step )
	{
		const auto linear = static_cast< float >(step) / 4000.0F;

		EXPECT_EQ(linearToSRGBByte(linear), static_cast< uint8_t >(std::lround(linearToSRGB(linear) * 255.0F))) << linear;
	}

	EXPECT_EQ(linearToSRGBByte(-1.0F), 0);
	EXPECT_EQ(linearToSRGBByte(2.0F), 255);
	EXPECT_EQ(linearToSRGBByte(std::numeric_limits< float >::quiet_NaN()), 0);
	EXPECT_EQ(linearToSRGB(std::numeric_limits< float >::quiet_NaN()), 0.0F);
}

namespace
{
	/**
	 * @brief Reduces a pixmap by halves down to 1 × 1 with the Kaiser and counts the pixels that are not the constant.
	 * @param source The constant RGBA source.
	 * @param constant The four values.
	 * @param sRGB Whether the colour is filtered in linear light.
	 * @return size_t The pixels that changed, every level together (SIZE_MAX on a refused reduction).
	 */
	size_t
	changedPixelsDownTheChain (const Pixmap< uint8_t > & source, const std::array< uint8_t, 4 > & constant, bool sRGB) noexcept
	{
		size_t changed = 0;
		auto level = source;

		while ( level.width() > 1 || level.height() > 1 )
		{
			Pixmap< uint8_t > next;

			if ( !Processor< uint8_t >::downsample(level, std::max(level.width() / 2, 1U), std::max(level.height() / 2, 1U), next, ReductionFilter::Kaiser, sRGB) )
			{
				return std::numeric_limits< size_t >::max();
			}

			for ( size_t index = 0; index < next.data().size(); index += 4 )
			{
				if ( !std::ranges::equal(std::span{next.data()}.subspan(index, 4), constant) )
				{
					++changed;
				}
			}

			level = std::move(next);
		}

		return changed;
	}
}

/* A constant image stays EXACTLY constant through the Kaiser, odd sizes and edges included, sRGB or not. */
TEST(PixelFactoryProcessor, downsampleKaiserKeepsAConstant)
{
	constexpr std::array< uint8_t, 4 > Constant{17, 128, 250, 99};

	Pixmap< uint8_t > source;
	ASSERT_TRUE(source.initialize(37, 23, ChannelMode::RGBA));

	for ( size_t index = 0; index < source.data().size(); index += 4 )
	{
		std::ranges::copy(Constant, std::span{source.data()}.subspan(index, 4).begin());
	}

	EXPECT_EQ(changedPixelsDownTheChain(source, Constant, false), 0U);
	EXPECT_EQ(changedPixelsDownTheChain(source, Constant, true), 0U);
}

namespace
{
	/**
	 * @brief The 1022 × 2048 needle mask of the mip tests: thin diagonal needles, about a quarter covered.
	 * @param on The value of a needle.
	 * @return Pixmap< pixel_t >
	 */
	template< typename pixel_t >
	Pixmap< pixel_t >
	needleMask (pixel_t on) noexcept
	{
		Pixmap< pixel_t > mask;

		if ( !mask.initialize(1022, 2048, ChannelMode::Grayscale) )
		{
			return mask;
		}

		for ( uint32_t y = 0; y < mask.height(); ++y )
		{
			for ( uint32_t x = 0; x < mask.width(); ++x )
			{
				const bool needle = x > 8 && y > 8 && ((x + y) % 8) < 2;

				mask.data()[(static_cast< size_t >(y) * mask.width()) + x] = needle ? on : pixel_t{0};
			}
		}

		return mask;
	}

	/**
	 * @brief The worst distance between the mean of every level of a Kaiser chain and a reference mean.
	 * @param mask The base level.
	 * @param reference The reference mean, in [0, 1].
	 * @param scale The value of 1 (255 for bytes, 1 for floats).
	 * @return double (infinity on a refused reduction)
	 */
	template< typename pixel_t >
	double
	worstKaiserMeanDrift (const Pixmap< pixel_t > & mask, double reference, double scale) noexcept
	{
		double worst = 0.0;
		auto level = mask;

		while ( level.width() > 1 || level.height() > 1 )
		{
			Pixmap< pixel_t > next;

			if ( !Processor< pixel_t >::downsample(level, std::max(level.width() / 2, 1U), std::max(level.height() / 2, 1U), next, ReductionFilter::Kaiser) )
			{
				return std::numeric_limits< double >::infinity();
			}

			double sum = 0.0;

			for ( const auto value : next.data() )
			{
				sum += static_cast< double >(value);
			}

			worst = std::max(worst, std::abs((sum / (scale * static_cast< double >(next.data().size()))) - reference));
			level = std::move(next);
		}

		return worst;
	}
}

/* The Kaiser's weights spread every source pixel evenly (the mean is kept in floating point), but its ringing is
 * CLAMPED on bytes: on the worst case — a binary needle mask, 0 / 255 at the Nyquist — the negative lobes clipped at 0
 * raise the mean of the first level (0.247 → 0.273), then it holds down to 1 × 1. */
TEST(PixelFactoryProcessor, downsampleKaiserKeepsTheMeanWithinABound)
{
	const auto mask = needleMask< uint8_t >(255);
	const auto baseMean = channelMean(mask);

	ASSERT_GT(baseMean, 0.2);

	EXPECT_LT(worstKaiserMeanDrift(needleMask< float >(1.0F), baseMean, 1.0), 0.0005);
	EXPECT_LT(worstKaiserMeanDrift(mask, baseMean, 255.0), 8.0 / 255.0);
}

namespace
{
	/**
	 * @brief The amplitude of one frequency in a row of a grey-scale pixmap: the projection on its sine and cosine over
	 * whole periods.
	 * @param pixmap The one-row pixmap.
	 * @param period The period, in pixels.
	 * @param first The first pixel of the window.
	 * @param count The window's length, a whole number of periods.
	 * @return double
	 */
	double
	amplitude (const Pixmap< uint8_t > & pixmap, double period, size_t first, size_t count) noexcept
	{
		const auto window = std::span{pixmap.data()}.subspan(first, count);
		double mean = 0.0;

		for ( const auto value : window )
		{
			mean += value;
		}

		mean /= static_cast< double >(count);

		double sine = 0.0;
		double cosine = 0.0;

		for ( size_t index = 0; index < count; ++index )
		{
			const auto phase = 2.0 * std::numbers::pi * static_cast< double >(first + index) / period;
			const auto value = static_cast< double >(window[index]) - mean;

			sine += value * std::sin(phase);
			cosine += value * std::cos(phase);
		}

		return 2.0 * std::hypot(sine, cosine) / static_cast< double >(count);
	}

	/**
	 * @brief A 256 × 1 grey-scale sinusoid of amplitude 100 around 127.5.
	 * @param period The period, in pixels.
	 * @return Pixmap< uint8_t >
	 */
	Pixmap< uint8_t >
	sinusoid (double period) noexcept
	{
		Pixmap< uint8_t > pixmap;

		if ( !pixmap.initialize(256, 1, ChannelMode::Grayscale) )
		{
			return pixmap;
		}

		for ( size_t index = 0; index < 256; ++index )
		{
			const auto value = 127.5 + (100.0 * std::sin(2.0 * std::numbers::pi * static_cast< double >(index) / period));

			pixmap.data()[index] = static_cast< uint8_t >(std::lround(value));
		}

		return pixmap;
	}

	/**
	 * @brief Halves a 256 × 1 pixmap and measures one frequency of the result.
	 * @param source The source.
	 * @param filter The filter.
	 * @param period The period to measure, in destination pixels.
	 * @param count The window's length (from pixel 8), a whole number of periods.
	 * @return double
	 */
	double
	halvedAmplitude (const Pixmap< uint8_t > & source, ReductionFilter filter, double period, size_t count) noexcept
	{
		Pixmap< uint8_t > target;

		if ( !Processor< uint8_t >::downsample(source, 128, 1, target, filter) )
		{
			return -1.0;
		}

		return amplitude(target, period, 8, count);
	}
}

/* THE reason for the Kaiser: sharper AND less aliasing than the box. A detail below the new Nyquist keeps more of its
 * contrast; a detail above it (which the reduced image cannot hold) folds back as a weaker false pattern. */
TEST(PixelFactoryProcessor, downsampleKaiserIsSharperAndAliasesLessThanTheBox)
{
	/* Period 8 source pixels → 4 destination pixels (half the new Nyquist): 28 whole periods away from the edges. */
	const auto passband = sinusoid(8.0);
	const auto passBox = halvedAmplitude(passband, ReductionFilter::Box, 4.0, 112);
	const auto passKaiser = halvedAmplitude(passband, ReductionFilter::Kaiser, 4.0, 112);

	/* Period 2.5 source pixels (1.6 × the new Nyquist) folds to period 5 destination pixels: 22 whole periods. */
	const auto stopband = sinusoid(2.5);
	const auto aliasBox = halvedAmplitude(stopband, ReductionFilter::Box, 5.0, 110);
	const auto aliasKaiser = halvedAmplitude(stopband, ReductionFilter::Kaiser, 5.0, 110);

	/* The box: cos(π/8) = 0.924 of the contrast kept, |cos(0.4 π)| = 0.309 folded back. */
	EXPECT_NEAR(passBox, 92.4, 1.5);
	EXPECT_NEAR(aliasBox, 30.9, 1.5);

	EXPECT_GT(passKaiser, passBox + 3.0) << "Kaiser " << passKaiser << " vs box " << passBox;
	EXPECT_LT(aliasKaiser, aliasBox * 0.25) << "Kaiser " << aliasKaiser << " vs box " << aliasBox;
}

namespace
{
	/**
	 * @brief A 2 × 2 RGBA checkerboard of 0 and 254 on every channel, alpha included.
	 * @return Pixmap< uint8_t >
	 */
	Pixmap< uint8_t >
	checkerboard () noexcept
	{
		Pixmap< uint8_t > pixmap;

		if ( !pixmap.initialize(2, 2, ChannelMode::RGBA) )
		{
			return pixmap;
		}

		for ( size_t pixel = 0; pixel < 4; ++pixel )
		{
			const uint8_t value = ((pixel + (pixel / 2)) % 2) == 0 ? 0 : 254;

			std::ranges::fill(std::span{pixmap.data()}.subspan(pixel * 4, 4), value);
		}

		return pixmap;
	}

	/**
	 * @brief Reduces a pixmap to 1 × 1 and returns its pixel.
	 * @param source The source.
	 * @param filter The filter.
	 * @param sRGB Whether the colour is filtered in linear light.
	 * @return std::array< uint8_t, 4 > (the unused channels 0)
	 */
	std::array< uint8_t, 4 >
	reducedToOnePixel (const Pixmap< uint8_t > & source, ReductionFilter filter, bool sRGB) noexcept
	{
		std::array< uint8_t, 4 > pixel{};
		Pixmap< uint8_t > target;

		if ( Processor< uint8_t >::downsample(source, 1, 1, target, filter, sRGB) )
		{
			std::ranges::copy(target.data(), pixel.begin());
		}

		return pixel;
	}
}

/* sRGB: the colour channels are averaged in LINEAR light — black and 254 average to 187 (linear 0.4956), not to 127
 * — and the alpha stays linear. Without the flag, everything is averaged as stored. (254, not 255: the stored mean of
 * 0 and 255 is a 127.5 tie, which a Kaiser weight one ulp off 0.5 rounds either way.) */
TEST(PixelFactoryProcessor, downsampleFiltersSRGBColourInLinearLight)
{
	const auto source = checkerboard();

	for ( const auto filter : {ReductionFilter::Box, ReductionFilter::Kaiser} )
	{
		EXPECT_EQ(reducedToOnePixel(source, filter, false), (std::array< uint8_t, 4 >{127, 127, 127, 127}));
		EXPECT_EQ(reducedToOnePixel(source, filter, true), (std::array< uint8_t, 4 >{187, 187, 187, 127}));
	}
}

/* A grey-scale image without alpha: its only channel is colour. A floating-point pixmap is linear by convention: sRGB is
 * refused. */
TEST(PixelFactoryProcessor, downsampleSRGBGreyScaleAndFloatRefusal)
{
	Pixmap< uint8_t > grey;
	ASSERT_TRUE(grey.initialize(2, 1, ChannelMode::Grayscale));
	grey.data()[0] = 0;
	grey.data()[1] = 254;

	EXPECT_EQ(reducedToOnePixel(grey, ReductionFilter::Box, true).front(), 187);

	Pixmap< float > hdr;
	ASSERT_TRUE(hdr.initialize(2, 2, ChannelMode::RGBA));

	Pixmap< float > hdrMip;
	EXPECT_FALSE(Processor< float >::downsample(hdr, 1, 1, hdrMip, ReductionFilter::Kaiser, true));
	EXPECT_TRUE(Processor< float >::downsample(hdr, 1, 1, hdrMip, ReductionFilter::Kaiser, false));
}

/* The Kaiser's edges are MIRRORED: a white left column never reaches the right edge (a wrapping kernel would bleed it
 * there), and the ringing next to the hard edge is clamped to the byte range. */
TEST(PixelFactoryProcessor, downsampleKaiserMirrorsItsEdges)
{
	Pixmap< uint8_t > source;
	ASSERT_TRUE(source.initialize(32, 1, ChannelMode::Grayscale));

	std::ranges::fill(source.data(), static_cast< uint8_t >(0));
	source.data()[0] = 255;
	source.data()[1] = 255;

	Pixmap< uint8_t > target;
	ASSERT_TRUE(Processor< uint8_t >::downsample(source, 16, 1, target, ReductionFilter::Kaiser));

	EXPECT_GT(target.data()[0], 200);
	EXPECT_EQ(target.data()[15], 0);
	EXPECT_EQ(target.data()[14], 0);
}

namespace
{
	/** @brief A 4 x 3 grayscale pixmap whose pixel values are their own index (0 … 11). */
	Pixmap< uint8_t >
	indexedPixmap ()
	{
		std::array< uint8_t, 12 > data{};
		uint8_t value = 0;

		for ( auto & element : data )
		{
			element = value++;
		}

		return Pixmap< uint8_t >{4, 3, ChannelMode::Grayscale, std::span< const uint8_t >{data}};
	}

	/** @brief Shifts a copy of the indexed pixmap and returns its pixels. */
	std::vector< uint8_t >
	shiftedPixels (int32_t xDirection, int32_t yDirection)
	{
		auto pixmap = indexedPixmap();

		Processor< uint8_t > processor{pixmap};

		EXPECT_TRUE(processor.shift(xDirection, yDirection));

		return {pixmap.data().begin(), pixmap.data().end()};
	}
}

/* Ave Robustus II warning pass (2026-10-08): a negative X shift computed an unsigned row chunk of ~4 GB
 * (memcpy overrun), and a negative Y shift went the POSITIVE way (std::abs). Pixels wrap around, so a
 * negative shift must equal the complementary positive one. */
TEST(PixelFactoryProcessor, shiftNegativeEqualsComplement)
{
	EXPECT_EQ(shiftedPixels(-1, 0), shiftedPixels(3, 0));
	EXPECT_EQ(shiftedPixels(0, -1), shiftedPixels(0, 2));
	EXPECT_EQ(shiftedPixels(-5, -4), shiftedPixels(3, 2));
	EXPECT_NE(shiftedPixels(0, -1), shiftedPixels(0, 1));
}

/* A distance of the whole height or more used to wrap the remaining height below zero (memcpy overrun),
 * and std::abs(INT32_MIN) overflowed. */
TEST(PixelFactoryProcessor, shiftTextAreaWholeHeightOrMore)
{
	for ( const int32_t distance : {3, -3, 1000, -1000, std::numeric_limits< int32_t >::min(), std::numeric_limits< int32_t >::max()} )
	{
		auto pixmap = indexedPixmap();

		Processor< uint8_t > processor{pixmap};

		EXPECT_TRUE(processor.shiftTextArea(distance)) << "distance " << distance;
		EXPECT_EQ(pixmap.width(), 4U);
		EXPECT_EQ(pixmap.height(), 3U);
	}
}

namespace
{
	/** @brief A 5 x 4 RGB pixmap whose elements are their own index (0 … 59), in any precision. */
	template< typename pixel_data_t >
	Pixmap< pixel_data_t >
	indexedRGB ()
	{
		std::vector< pixel_data_t > data(5U * 4U * 3U);
		uint32_t value = 0;

		for ( auto & element : data )
		{
			element = static_cast< pixel_data_t >(value++);
		}

		/* cppcheck-suppress returnDanglingLifetime ; the Pixmap COPIES the span into its own storage. */
		return Pixmap< pixel_data_t >{5, 4, ChannelMode::RGB, std::span< const pixel_data_t >{data}};
	}

	/** @brief The elements of a pixmap as floats, to compare precisions. */
	template< typename pixel_data_t >
	std::vector< float >
	elementsOf (const Pixmap< pixel_data_t > & pixmap)
	{
		std::vector< float > elements;
		elements.reserve(pixmap.data().size());

		for ( const auto element : pixmap.data() )
		{
			elements.push_back(static_cast< float >(element));
		}

		return elements;
	}

	/** @brief Runs every row-copy operation of the processor on an indexed pixmap of the given precision. */
	template< typename pixel_data_t >
	std::vector< std::vector< float > >
	rowCopyResults ()
	{
		std::vector< std::vector< float > > results;

		const auto runOnCopy = [&results] (const auto & operation) {
			auto pixmap = indexedRGB< pixel_data_t >();
			Processor< pixel_data_t > processor{pixmap};

			EXPECT_TRUE(operation(processor));

			results.push_back(elementsOf(pixmap));
		};

		runOnCopy([] (auto & processor) { return processor.move(2, 1); });
		runOnCopy([] (auto & processor) { return processor.move(-1, -2); });
		runOnCopy([] (auto & processor) { return processor.shift(2, 1); });
		runOnCopy([] (auto & processor) { return processor.shift(-1, -3); });
		runOnCopy([] (auto & processor) { return processor.shiftTextArea(1); });
		runOnCopy([] (auto & processor) { return processor.shiftTextArea(-2); });

		{
			const auto source = indexedRGB< pixel_data_t >();
			Pixmap< pixel_data_t > target{5, 4, ChannelMode::RGB};
			const Processor< pixel_data_t > processor{target};

			EXPECT_TRUE(processor.blit(source, {1, 1, 3, 2}, {0, 2, 3, 2}));

			results.push_back(elementsOf(target));
		}

		{
			Pixmap< pixel_data_t > cropped;

			EXPECT_TRUE(Processor< pixel_data_t >::crop(indexedRGB< pixel_data_t >(), {1, 1, 3, 2}, cropped));

			results.push_back(elementsOf(cropped));
		}

		results.push_back(elementsOf(Processor< pixel_data_t >::mirror(indexedRGB< pixel_data_t >(), MirrorMode::X)));

		return results;
	}
}

/* Ave Robustus II (2026-10-08): move / shift / shiftTextArea / blit / crop / mirrorX mixed byte counts (pitch(),
 * bytes(), sizeof) with element offsets: right for 8-bit pixmaps only — a float or 16-bit pixmap was read and written
 * sizeof(pixel_data_t) times too far (out of bounds) and the swap buffer was sized in bytes. Every precision must give
 * exactly the 8-bit result. */
TEST(PixelFactoryProcessor, everyPrecisionMatchesEightBit)
{
	const auto reference = rowCopyResults< uint8_t >();
	const auto sixteenBit = rowCopyResults< uint16_t >();
	const auto floatingPoint = rowCopyResults< float >();

	ASSERT_EQ(reference.size(), sixteenBit.size());
	ASSERT_EQ(reference.size(), floatingPoint.size());

	for ( size_t index = 0; index < reference.size(); ++index )
	{
		EXPECT_EQ(reference[index], sixteenBit[index]) << "operation #" << index;
		EXPECT_EQ(reference[index], floatingPoint[index]) << "operation #" << index;
	}
}

/* The swap buffer kept the previous operation's pixels where move() copies nothing: the second move showed the
 * ORIGINAL image in the vacated columns. */
TEST(PixelFactoryProcessor, moveVacatesWithEmptyPixels)
{
	auto pixmap = indexedRGB< uint8_t >();
	Processor< uint8_t > processor{pixmap};

	ASSERT_TRUE(processor.move(1, 0));
	ASSERT_TRUE(processor.move(1, 0));

	/* Positive X moves the content by two pixels (move()'s own convention): the two vacated columns are empty. */
	for ( size_t row = 0; row < 4; ++row )
	{
		for ( size_t element = 0; element < size_t{2} * 3U; ++element )
		{
			EXPECT_EQ(pixmap.data()[(row * 5U * 3U) + element], 0) << "row " << row << " element " << element;
		}
	}
}

/* move() took std::abs() of an int32_t direction: INT32_MIN overflowed (UB). Any extreme direction moves everything
 * out of the canvas and leaves it empty. */
TEST(PixelFactoryProcessor, moveExtremeDirections)
{
	for ( const int32_t direction : {std::numeric_limits< int32_t >::min(), std::numeric_limits< int32_t >::max(), 5, -5} )
	{
		auto horizontal = indexedRGB< uint8_t >();
		Processor< uint8_t > horizontalProcessor{horizontal};

		EXPECT_TRUE(horizontalProcessor.move(direction, 0)) << "direction " << direction;
		EXPECT_TRUE(std::ranges::all_of(horizontal.data(), [] (uint8_t element) { return element == 0; })) << "direction " << direction;

		auto vertical = indexedRGB< uint8_t >();
		Processor< uint8_t > verticalProcessor{vertical};

		EXPECT_TRUE(verticalProcessor.move(0, direction)) << "direction " << direction;
		EXPECT_TRUE(std::ranges::all_of(vertical.data(), [] (uint8_t element) { return element == 0; })) << "direction " << direction;
	}
}

/* The channel conversions keep the alpha (the coverage pass, 2026-10-18). */
namespace
{
	/** @brief A 2 x 2 pixmap with every pixel set from the given components. */
	template< typename pixel_data_t >
	[[nodiscard]]
	Pixmap< pixel_data_t >
	uniform (ChannelMode mode, std::initializer_list< pixel_data_t > components)
	{
		Pixmap< pixel_data_t > pixmap{2U, 2U, mode};

		for ( size_t pixelIndex = 0; pixelIndex < pixmap.pixelCount(); ++pixelIndex )
		{
			std::ranges::copy(components, pixmap.pixelPointer(pixelIndex));
		}

		return pixmap;
	}
}

TEST(PixelFactoryProcessor, channelConversionsKeepTheAlpha)
{
	/* toRGBA() overwrote a GrayscaleAlpha source's alpha (every texture the engine loads goes through it: a gray + alpha
	 * MASK texture rendered opaque); toGrayscale() returned a GrayscaleAlpha unchanged, two channels. */
	const auto grayAlpha = uniform< uint8_t >(ChannelMode::GrayscaleAlpha, {120, 30});
	const auto rgb = uniform< uint8_t >(ChannelMode::RGB, {200, 100, 50});
	const auto rgba = uniform< uint8_t >(ChannelMode::RGBA, {200, 100, 50, 64});

	const auto fromGrayAlpha = Processor< uint8_t >::toRGBA(grayAlpha);
	ASSERT_EQ(fromGrayAlpha.channelMode(), ChannelMode::RGBA);
	EXPECT_EQ(fromGrayAlpha.pixelPointer(3)[3], 30);
	EXPECT_EQ(fromGrayAlpha.pixelPointer(3)[0], 120);

	/* The opacity is the alpha of a source WITHOUT one. */
	const auto fromRGB = Processor< uint8_t >::toRGBA(rgb, 0.5F);
	EXPECT_NEAR(fromRGB.pixelPointer(0)[3], 128, 1);

	const auto gray = Processor< uint8_t >::toGrayscale(grayAlpha);
	ASSERT_EQ(gray.channelMode(), ChannelMode::Grayscale);
	EXPECT_EQ(gray.pixelPointer(1)[0], 120);

	const auto grayAlphaFromRGBA = Processor< uint8_t >::toGrayscaleAlpha(rgba);
	ASSERT_EQ(grayAlphaFromRGBA.channelMode(), ChannelMode::GrayscaleAlpha);
	EXPECT_EQ(grayAlphaFromRGBA.pixelPointer(2)[0], Processor< uint8_t >::toGrayscale(rgba).pixelPointer(2)[0]);
	EXPECT_EQ(grayAlphaFromRGBA.pixelPointer(2)[1], 64);
	EXPECT_EQ(Processor< uint8_t >::toGrayscaleAlpha(rgb).pixelPointer(0)[1], 255);
}

TEST(PixelFactoryProcessor, readOptionsConvertWithoutLosingTheAlpha)
{
	const auto grayAlpha = uniform< uint8_t >(ChannelMode::GrayscaleAlpha, {120, 30});
	const auto rgba = uniform< uint8_t >(ChannelMode::RGBA, {200, 100, 50, 64});

	{
		auto pixmap = rgba;
		ASSERT_TRUE(applyReadOptions(pixmap, ReadOptions{.targetChannelMode = TargetChannelMode::GrayscaleAlpha}));
		ASSERT_EQ(pixmap.channelMode(), ChannelMode::GrayscaleAlpha);
		EXPECT_EQ(pixmap.pixelPointer(0)[1], 64);
	}

	{
		auto pixmap = grayAlpha;
		ASSERT_TRUE(applyReadOptions(pixmap, ReadOptions{.targetChannelMode = TargetChannelMode::Grayscale}));
		EXPECT_EQ(pixmap.channelMode(), ChannelMode::Grayscale);
	}

	{
		auto pixmap = grayAlpha;
		ASSERT_TRUE(applyReadOptions(pixmap, ReadOptions{.targetChannelMode = TargetChannelMode::RGBA}));
		ASSERT_EQ(pixmap.channelMode(), ChannelMode::RGBA);
		EXPECT_EQ(pixmap.pixelPointer(0)[3], 30);
	}

	/* The premultiplication divides by the type's full scale (255 for every integer type: 40000 x 0.5 gave 28270). */
	{
		auto pixmap = uniform< uint16_t >(ChannelMode::RGBA, {40000, 20000, 10000, 32768});
		ASSERT_TRUE(applyReadOptions(pixmap, ReadOptions{.premultiplyAlpha = true}));
		EXPECT_NEAR(pixmap.pixelPointer(0)[0], 20000, 1);
		EXPECT_NEAR(pixmap.pixelPointer(0)[2], 5000, 1);
	}

	{
		auto pixmap = rgba;
		ASSERT_TRUE(applyReadOptions(pixmap, ReadOptions{.premultiplyAlpha = true}));
		EXPECT_EQ(pixmap.pixelPointer(0)[0], 50);
	}
}

TEST(PixelFactoryProcessor, aGrayAlphaPNGReadAsRGBAKeepsItsAlpha)
{
	/* The engine's texture path: a gray + alpha PNG read with targetChannelMode = RGBA (the glTF sample
	 * TextureLinearInterpolationTest's MASK labels rendered as an opaque black panel). */
	const auto path = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_gray_alpha.png");

	Pixmap< uint8_t > source{4U, 4U, ChannelMode::GrayscaleAlpha};

	for ( size_t pixelIndex = 0; pixelIndex < source.pixelCount(); ++pixelIndex )
	{
		source.pixelPointer(pixelIndex)[0] = 200;
		source.pixelPointer(pixelIndex)[1] = static_cast< uint8_t >(pixelIndex * 16U);
	}

	ASSERT_TRUE(FileIO::write(source, path, true));

	Pixmap< uint8_t > loaded;
	ASSERT_TRUE(FileIO::read(path, loaded, ReadOptions{.targetChannelMode = TargetChannelMode::RGBA}));
	ASSERT_EQ(loaded.channelMode(), ChannelMode::RGBA);

	for ( size_t pixelIndex = 0; pixelIndex < loaded.pixelCount(); ++pixelIndex )
	{
		EXPECT_EQ(loaded.pixelPointer(pixelIndex)[3], static_cast< uint8_t >(pixelIndex * 16U)) << pixelIndex;
	}

	std::error_code errorCode;
	std::filesystem::remove(path, errorCode);
}

namespace
{
	/**
	 * @brief Returns whether a pixel holds a color, at 8-bit precision.
	 * @param pixmap A reference to the pixmap.
	 * @param coordX The X coordinate.
	 * @param coordY The Y coordinate.
	 * @param color The expected color.
	 * @return bool
	 */
	bool
	pixelIs (const Pixmap< uint8_t > & pixmap, uint32_t coordX, uint32_t coordY, const Color< float > & color) noexcept
	{
		const auto pixel = pixmap.pixel(coordX, coordY);

		return std::abs(pixel.red() - color.red()) < 0.01F && std::abs(pixel.green() - color.green()) < 0.01F && std::abs(pixel.blue() - color.blue()) < 0.01F;
	}
}

/* 2026-10-19: blit(source) and copy(source, clip) called a Pixmap::area() that does not exist, copy(source, position)
 * an overload that does not exist, and extend() returned the pixmap as its bool: none had ever compiled (no code
 * instantiated them). */
TEST(PixelFactoryProcessor, blitAndCopyAWholePixmap)
{
	Pixmap< uint8_t > source{2, 2, ChannelMode::RGB};
	source.fill(Red);

	Pixmap< uint8_t > target{4, 4, ChannelMode::RGB};
	target.fill(Black);

	const Processor< uint8_t > processor{target};

	ASSERT_TRUE(processor.blit(source));
	EXPECT_TRUE(pixelIs(target, 0, 0, Red));
	EXPECT_TRUE(pixelIs(target, 1, 1, Red));
	EXPECT_TRUE(pixelIs(target, 2, 2, Black));

	ASSERT_TRUE(processor.copy(source, Math::Space2D::AARectangle< uint32_t >{2, 2, 2, 2}));
	EXPECT_TRUE(pixelIs(target, 3, 3, Red));
	EXPECT_TRUE(pixelIs(target, 2, 1, Black));

	target.fill(Black);

	/* One column left of the target: only the source's second column lands, at x = 0. */
	ASSERT_TRUE(processor.copy(source, Math::Vector< 2, int32_t >{-1, 2}));
	EXPECT_TRUE(pixelIs(target, 0, 2, Red));
	EXPECT_TRUE(pixelIs(target, 0, 3, Red));
	EXPECT_TRUE(pixelIs(target, 1, 2, Black));

	/* INT32_MIN: no std::abs() overflow, refused (the source is not in the target). */
	EXPECT_FALSE(processor.copy(source, Math::Vector< 2, int32_t >{std::numeric_limits< int32_t >::min(), 0}));
}

/* The borders are [left, top, right, bottom], row 0 at the top. */
TEST(PixelFactoryProcessor, extendPlacesTheBorders)
{
	Pixmap< uint8_t > source{2, 2, ChannelMode::RGB};
	source.fill(Red);

	const auto black = Processor< uint8_t >::extend(source, std::array< uint32_t, 4 >{1, 2, 3, 4});

	ASSERT_EQ(black.width(), 6U);
	ASSERT_EQ(black.height(), 8U);
	EXPECT_TRUE(pixelIs(black, 1, 2, Red));
	EXPECT_TRUE(pixelIs(black, 2, 3, Red));
	EXPECT_TRUE(pixelIs(black, 0, 2, Black));
	EXPECT_TRUE(pixelIs(black, 3, 2, Black));
	EXPECT_TRUE(pixelIs(black, 1, 1, Black));
	EXPECT_TRUE(pixelIs(black, 1, 4, Black));

	const auto colored = Processor< uint8_t >::extend(source, std::array< uint32_t, 4 >{1, 2, 3, 4}, Green);

	ASSERT_EQ(colored.width(), 6U);
	ASSERT_EQ(colored.height(), 8U);
	EXPECT_TRUE(pixelIs(colored, 1, 2, Red));
	EXPECT_TRUE(pixelIs(colored, 0, 2, Green));
	EXPECT_TRUE(pixelIs(colored, 5, 3, Green));
	EXPECT_TRUE(pixelIs(colored, 1, 0, Green));
	EXPECT_TRUE(pixelIs(colored, 1, 7, Green));
}

/* resize(ratio) truncated source.width() * ratio into a size_t with no check: a NaN, a negative or a huge ratio was
 * undefined behaviour. */
TEST(PixelFactoryProcessor, resizeByRatioRefusesAnUnusableRatio)
{
	Pixmap< uint8_t > source{4, 6, ChannelMode::RGB};
	source.fill(Red);

	const auto half = Processor< uint8_t >::resize(source, 0.5F);

	EXPECT_EQ(half.width(), 2U);
	EXPECT_EQ(half.height(), 3U);

	for ( const auto ratio : {0.0F, -1.0F, 0.1F, std::numeric_limits< float >::quiet_NaN(), std::numeric_limits< float >::infinity(), 3.0e9F} )
	{
		Pixmap< uint8_t > output;

		EXPECT_FALSE(Processor< uint8_t >::resize(source, ratio, output)) << "ratio " << ratio;
		EXPECT_FALSE(Processor< uint8_t >::resize(source, ratio).isValid()) << "ratio " << ratio;
	}
}

namespace
{
	using RGBA = std::array< uint8_t, 4 >;

	/** @brief An RGBA pixmap whose pixels are given by a function of their position. */
	[[nodiscard]]
	Pixmap< uint8_t >
	makeRGBA (uint32_t width, uint32_t height, const std::function< RGBA (size_t x, size_t y) > & pixelOf)
	{
		std::vector< uint8_t > bytes;
		bytes.reserve(size_t{width} * height * 4);

		for ( size_t y = 0; y < height; ++y )
		{
			for ( size_t x = 0; x < width; ++x )
			{
				const auto pixel = pixelOf(x, y);
				bytes.insert(bytes.end(), pixel.begin(), pixel.end());
			}
		}

		return Pixmap< uint8_t >{width, height, ChannelMode::RGBA, std::span< const uint8_t >{bytes}};
	}

	[[nodiscard]]
	RGBA
	pixelAt (const Pixmap< uint8_t > & pixmap, size_t x, size_t y)
	{
		const auto offset = ((y * pixmap.width()) + x) * 4;
		const auto & data = pixmap.data();

		return {data[offset], data[offset + 1], data[offset + 2], data[offset + 3]};
	}

	/** @brief Returns whether every pixel matches what the function expects at its position. */
	[[nodiscard]]
	bool
	everyPixelIs (const Pixmap< uint8_t > & pixmap, const std::function< RGBA (size_t x, size_t y) > & expected)
	{
		for ( size_t y = 0; y < pixmap.height(); ++y )
		{
			for ( size_t x = 0; x < pixmap.width(); ++x )
			{
				if ( pixelAt(pixmap, x, y) != expected(x, y) )
				{
					return false;
				}
			}
		}

		return true;
	}

	/** @brief A cut-out leaf: the left half visible green, the right half transparent BLACK. */
	[[nodiscard]]
	RGBA
	halfLeaf (size_t x, size_t /*y*/)
	{
		return x < 4 ? RGBA{30, 200, 40, 255} : RGBA{0, 0, 0, 0};
	}
}

TEST(PixelFactoryProcessor, fillTransparentColourGivesTheHolesTheVisibleColour)
{
	/* What Sponza's cypress stores: once filled, every RGB is the green and the alpha is untouched. */
	auto pixmap = makeRGBA(8, 8, halfLeaf);

	ASSERT_TRUE(Processor< uint8_t >::fillTransparentColour(pixmap, 128));
	EXPECT_TRUE(everyPixelIs(pixmap, [] (size_t x, size_t /*y*/) { return x < 4 ? RGBA{30, 200, 40, 255} : RGBA{30, 200, 40, 0}; }));
}

TEST(PixelFactoryProcessor, fillTransparentColourKeepsTheColourOfABoxFilteredLevel)
{
	/* The defect it removes: the box-filtered 1 x 1 level of the raw leaf halves its green, the filled one keeps it. */
	const auto raw = makeRGBA(8, 8, halfLeaf);
	auto filled = makeRGBA(8, 8, halfLeaf);
	ASSERT_TRUE(Processor< uint8_t >::fillTransparentColour(filled, 128));

	Pixmap< uint8_t > rawMip;
	Pixmap< uint8_t > filledMip;
	ASSERT_TRUE(Processor< uint8_t >::downsample(raw, 1, 1, rawMip));
	ASSERT_TRUE(Processor< uint8_t >::downsample(filled, 1, 1, filledMip));

	EXPECT_EQ(pixelAt(rawMip, 0, 0)[1], 100);
	EXPECT_EQ(pixelAt(filledMip, 0, 0)[1], 200);
}

namespace
{
	/** @brief Red on the left columns, blue on the right ones, a transparent gap between. */
	[[nodiscard]]
	RGBA
	redGapBlue (size_t x, size_t /*y*/)
	{
		if ( x < 2 )
		{
			return RGBA{250, 10, 10, 255};
		}

		return x >= 6 ? RGBA{10, 10, 250, 255} : RGBA{0, 0, 0, 0};
	}
}

TEST(PixelFactoryProcessor, fillTransparentColourLeavesTheVisiblePixels)
{
	auto pixmap = makeRGBA(8, 4, redGapBlue);

	ASSERT_TRUE(Processor< uint8_t >::fillTransparentColour(pixmap, 128));
	EXPECT_TRUE(everyPixelIs(pixmap, [&pixmap] (size_t x, size_t y) {
		/* The visible pixels as they were; a hole keeps its alpha 0 (its colour is checked below). */
		return x < 2 || x >= 6 ? redGapBlue(x, y) : RGBA{pixelAt(pixmap, x, y)[0], pixelAt(pixmap, x, y)[1], pixelAt(pixmap, x, y)[2], 0};
	}));
}

TEST(PixelFactoryProcessor, fillTransparentColourTakesTheNearestColour)
{
	auto pixmap = makeRGBA(8, 4, redGapBlue);

	ASSERT_TRUE(Processor< uint8_t >::fillTransparentColour(pixmap, 128));

	/* Each hole leans to its own side, on every row. */
	bool leansToItsSide = true;

	for ( size_t y = 0; y < 4; ++y )
	{
		const auto nearRed = pixelAt(pixmap, 2, y);
		const auto nearBlue = pixelAt(pixmap, 5, y);

		leansToItsSide = leansToItsSide && nearRed[0] > nearRed[2] && nearBlue[2] > nearBlue[0];
	}

	EXPECT_TRUE(leansToItsSide);
}

namespace
{
	/** @brief 5 x 3: one visible pixel (alpha 200) at the origin, one at alpha 100 in the far corner, holes elsewhere. */
	[[nodiscard]]
	RGBA
	twoPixels (size_t x, size_t y)
	{
		if ( x == 0 && y == 0 )
		{
			return RGBA{100, 150, 200, 200};
		}

		return x == 4 && y == 2 ? RGBA{7, 7, 7, 100} : RGBA{0, 0, 0, 0};
	}
}

TEST(PixelFactoryProcessor, fillTransparentColourTreatsAPixelBelowTheThresholdAsAHole)
{
	/* A cut-out threshold (128): the alpha-100 pixel is a hole and takes the visible colour; odd sizes work. */
	auto pixmap = makeRGBA(5, 3, twoPixels);

	ASSERT_TRUE(Processor< uint8_t >::fillTransparentColour(pixmap, 128));
	EXPECT_EQ(pixelAt(pixmap, 4, 2), (RGBA{100, 150, 200, 100}));
	EXPECT_EQ(pixelAt(pixmap, 2, 1), (RGBA{100, 150, 200, 0}));
}

TEST(PixelFactoryProcessor, fillTransparentColourKeepsASemiTransparentPixelAboveTheThreshold)
{
	/* A graded-alpha threshold (1: only the fully transparent pixels are holes): the alpha-100 pixel keeps its colour. */
	auto pixmap = makeRGBA(5, 3, twoPixels);

	ASSERT_TRUE(Processor< uint8_t >::fillTransparentColour(pixmap, 1));
	EXPECT_EQ(pixelAt(pixmap, 4, 2), (RGBA{7, 7, 7, 100}));
	EXPECT_EQ(pixelAt(pixmap, 0, 0), (RGBA{100, 150, 200, 200}));
}

TEST(PixelFactoryProcessor, fillTransparentColourLeavesAFullOrAnEmptyMask)
{
	/* Nothing to fill (every pixel visible) and nothing to fill it with (every pixel a hole): unchanged, true. */
	const auto opaque = [] (size_t /*x*/, size_t /*y*/) { return RGBA{1, 2, 3, 255}; };
	const auto holes = [] (size_t /*x*/, size_t /*y*/) { return RGBA{9, 8, 7, 0}; };
	auto full = makeRGBA(4, 4, opaque);
	auto empty = makeRGBA(4, 4, holes);

	EXPECT_TRUE(Processor< uint8_t >::fillTransparentColour(full, 128) && everyPixelIs(full, opaque));
	EXPECT_TRUE(Processor< uint8_t >::fillTransparentColour(empty, 128) && everyPixelIs(empty, holes));
}

TEST(PixelFactoryProcessor, fillTransparentColourRefusesBadInput)
{
	/* A zero threshold, a pixmap without alpha, an invalid pixmap: refused. */
	auto rgba = makeRGBA(4, 4, halfLeaf);
	Pixmap< uint8_t > rgb{4, 4, ChannelMode::RGB};
	Pixmap< uint8_t > invalid;

	EXPECT_FALSE(Processor< uint8_t >::fillTransparentColour(rgba, 0));
	EXPECT_FALSE(Processor< uint8_t >::fillTransparentColour(rgb, 128));
	EXPECT_FALSE(Processor< uint8_t >::fillTransparentColour(invalid, 128));
}
