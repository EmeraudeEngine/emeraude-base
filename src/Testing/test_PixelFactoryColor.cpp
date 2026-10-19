/*
 * src/Testing/test_PixelFactoryColor.cpp
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
#include <array>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "Math/Vector.hpp"
#include "PixelFactory/Color.hpp"
#include "PixelFactory/Gradient.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::PixelFactory;

/* 2026-09-26: ColorFromInteger's constraint was a COMMA expression that checked only the output type: a float input was
 * accepted. Both halves are enforced now. */
template< typename input_t, typename output_t >
concept AcceptsColorFromInteger = requires (input_t component) {
	ColorFromInteger< input_t, output_t >(component, component, component);
};

template< typename input_t, typename output_t >
concept AcceptsColorFromIntegerArray = requires (std::array< input_t, 3 > rgb, std::array< input_t, 4 > rgba) {
	ColorFromInteger< input_t, output_t >(rgb);
	ColorFromInteger< input_t, output_t >(rgba);
};

template< typename input_t, typename output_t >
concept AcceptsColorFromIntegerVector = requires (Math::Vector< 3, input_t > rgb, Math::Vector< 4, input_t > rgba) {
	ColorFromInteger< input_t, output_t >(rgb);
	ColorFromInteger< input_t, output_t >(rgba);
};

/* Each overload: a FLOAT input used to be accepted (only the output half was checked). */
static_assert(!AcceptsColorFromInteger< float, float >);
static_assert(!AcceptsColorFromIntegerArray< float, float >);
static_assert(!AcceptsColorFromIntegerVector< float, float >);
static_assert(!AcceptsColorFromInteger< uint8_t, int >);
static_assert(AcceptsColorFromInteger< uint8_t, float >);
static_assert(AcceptsColorFromIntegerArray< uint8_t, float >);
static_assert(AcceptsColorFromIntegerVector< uint8_t, float >);

/* Gradient< scale, colour >: both halves floating point — an integral scale used to be accepted. */
template< typename scale_t, typename color_t >
concept MakesGradient = requires { sizeof(Gradient< scale_t, color_t >); };

static_assert(!MakesGradient< int, float >);
static_assert(MakesGradient< float, float >);

TEST(PixelFactoryColor, ColorFromInteger)
{
	ASSERT_EQ(ColorFromInteger< uint8_t >(0, 0, 0, 255), Black);
	ASSERT_EQ(ColorFromInteger< uint8_t >(255, 255, 255, 255), White);
	ASSERT_EQ(ColorFromInteger< uint8_t >(255, 0, 0, 255), Red);
	ASSERT_EQ(ColorFromInteger< uint8_t >(0, 255, 0, 255), Green);
	ASSERT_EQ(ColorFromInteger< uint8_t >(0, 0, 255, 255), Blue);

	ASSERT_EQ(ColorFromInteger< uint16_t >(0, 0, 0, 65535), Black);
	ASSERT_EQ(ColorFromInteger< uint16_t >(65535, 65535, 65535, 65535), White);
	ASSERT_EQ(ColorFromInteger< uint16_t >(65535, 0, 0, 65535), Red);
	ASSERT_EQ(ColorFromInteger< uint16_t >(0, 65535, 0, 65535), Green);
	ASSERT_EQ(ColorFromInteger< uint16_t >(0, 0, 65535, 65535), Blue);

	ASSERT_EQ(ColorFromInteger< uint32_t >(0, 0, 0, 4294967295), Black);
	ASSERT_EQ(ColorFromInteger< uint32_t >(4294967295, 4294967295, 4294967295, 4294967295), White);
	ASSERT_EQ(ColorFromInteger< uint32_t >(4294967295, 0, 0, 4294967295), Red);
	ASSERT_EQ(ColorFromInteger< uint32_t >(0, 4294967295, 0, 4294967295), Green);
	ASSERT_EQ(ColorFromInteger< uint32_t >(0, 0, 4294967295, 4294967295), Blue);

	constexpr auto max_uint64_t{std::numeric_limits< uint64_t >::max()};

	ASSERT_EQ(ColorFromInteger< uint64_t >(0, 0, 0, max_uint64_t), Black);
	ASSERT_EQ(ColorFromInteger< uint64_t >(max_uint64_t, max_uint64_t, max_uint64_t, max_uint64_t), White);
	ASSERT_EQ(ColorFromInteger< uint64_t >(max_uint64_t, 0, 0, max_uint64_t), Red);
	ASSERT_EQ(ColorFromInteger< uint64_t >(0, max_uint64_t, 0, max_uint64_t), Green);
	ASSERT_EQ(ColorFromInteger< uint64_t >(0, 0, max_uint64_t, max_uint64_t), Blue);
}

TEST(PixelFactoryColor, IntegerComponentsAreRefusedAtCompileTime)
{
	/* The float constructor clamps each channel to [0, 1]: an 8-bit literal colour used to build white
	 * (Color< float >{255U, 140U, 40U} == White), silently. It no longer compiles. */
	static_assert(!std::is_constructible_v< Color< float >, unsigned int, unsigned int, unsigned int >);
	static_assert(!std::is_constructible_v< Color< float >, int, int, int, int >);
	static_assert(std::is_constructible_v< Color< float >, float, float, float >);
	static_assert(std::is_constructible_v< Color< float >, float, int, int >);

	SUCCEED();
}

TEST(PixelFactoryColor, ColorFromIntegerOfAnEightBitOrange)
{
	/* The input type is NAMED: ColorFromInteger(255U, 140U, 40U) would deduce unsigned int and divide by
	 * 4 294 967 295 — black. */
	const auto orange = ColorFromInteger< uint8_t >(255, 140, 40);

	ASSERT_NEAR(orange.red(), 1.0F, 1.0e-6F);
	ASSERT_NEAR(orange.green(), 140.0F / 255.0F, 1.0e-6F);
	ASSERT_NEAR(orange.blue(), 40.0F / 255.0F, 1.0e-6F);
	ASSERT_NEAR(orange.alpha(), 1.0F, 1.0e-6F);
}

TEST(PixelFactoryColor, UnitLuminanceChromaticity)
{
	/* White and every grey: unit luminance means (1, 1, 1). White is exact (the Rec.709 weights sum to 1). */
	const auto white = White.unitLuminanceChromaticity();

	ASSERT_EQ(white[Math::X], 1.0F);
	ASSERT_EQ(white[Math::Y], 1.0F);
	ASSERT_EQ(white[Math::Z], 1.0F);

	const auto grey = Color< float >{0.5F, 0.5F, 0.5F}.unitLuminanceChromaticity();

	ASSERT_NEAR(grey[Math::X], 1.0F, 1.0e-6F);
	ASSERT_NEAR(grey[Math::Y], 1.0F, 1.0e-6F);
	ASSERT_NEAR(grey[Math::Z], 1.0F, 1.0e-6F);

	/* A hue keeps its ratios and gets unit luminance: the fire orange of the demos. */
	const auto orange = ColorFromInteger< uint8_t >(255, 140, 40).unitLuminanceChromaticity();

	ASSERT_NEAR(0.2126F * orange[Math::X] + 0.7152F * orange[Math::Y] + 0.0722F * orange[Math::Z], 1.0F, 1.0e-5F);
	ASSERT_NEAR(orange[Math::Y] / orange[Math::X], 140.0F / 255.0F, 1.0e-5F);
	ASSERT_NEAR(orange[Math::Z] / orange[Math::X], 40.0F / 255.0F, 1.0e-5F);

	/* A primary exceeds 1: pure blue carries its whole luminance in a 7.22 % weight. */
	const auto blue = Blue.unitLuminanceChromaticity();

	ASSERT_NEAR(blue[Math::Z], 1.0F / 0.0722F, 1.0e-3F);
	ASSERT_EQ(blue[Math::X], 0.0F);

	/* Black emits nothing. */
	const auto black = Black.unitLuminanceChromaticity();

	ASSERT_EQ(black[Math::X], 0.0F);
	ASSERT_EQ(black[Math::Y], 0.0F);
	ASSERT_EQ(black[Math::Z], 0.0F);
}

/* 2026-10-19: operator+=/-= compared 'this' with the address of a scalar (they had never compiled) and the HSV setters
 * went through a conversion that rounded the sector, so its fractional part was always 0, mixed the 0-100 and 0-1
 * scales and swapped a channel in the last sector — no code had ever called them. */
TEST(PixelFactoryColor, addAndSubtractAScalarInPlace)
{
	Color< float > color{0.25F, 0.5F, 0.75F, 0.5F};

	color += 0.5F;

	EXPECT_FLOAT_EQ(color.red(), 0.75F);
	EXPECT_FLOAT_EQ(color.green(), 1.0F);
	EXPECT_FLOAT_EQ(color.blue(), 1.0F);
	EXPECT_FLOAT_EQ(color.alpha(), 0.5F);

	color -= 0.75F;

	EXPECT_FLOAT_EQ(color.red(), 0.0F);
	EXPECT_FLOAT_EQ(color.green(), 0.25F);
	EXPECT_FLOAT_EQ(color.blue(), 0.25F);
	EXPECT_FLOAT_EQ(color.alpha(), 0.5F);
}

TEST(PixelFactoryColor, hsvSettersFollowTheHexcone)
{
	const auto expectRGB = [] (const Color< double > & color, double red, double green, double blue) {
		EXPECT_NEAR(color.red(), red, 1.0e-12);
		EXPECT_NEAR(color.green(), green, 1.0e-12);
		EXPECT_NEAR(color.blue(), blue, 1.0e-12);
	};

	Color< double > color{1.0, 0.0, 0.0, 1.0};

	color.setHue(120.0);
	expectRGB(color, 0.0, 1.0, 0.0);

	color.setHue(240U);
	expectRGB(color, 0.0, 0.0, 1.0);

	/* Sector 5, halfway: (v, p, q). */
	color.setHue(330.0);
	expectRGB(color, 1.0, 0.0, 0.5);

	/* Any angle, taken modulo 360. */
	color.setHue(-240.0);
	expectRGB(color, 0.0, 1.0, 0.0);

	color.setHue(0.0);
	color.setSaturation(50.0);
	expectRGB(color, 1.0, 0.5, 0.5);

	color.setValue(50U);
	expectRGB(color, 0.5, 0.25, 0.25);

	EXPECT_NEAR(color.hue(), 0.0, 1.0e-9);
	EXPECT_NEAR(color.saturation(), 50.0, 1.0e-9);
	EXPECT_NEAR(color.value(), 50.0, 1.0e-9);

	/* Clamped to 0-100. */
	color.setSaturation(250.0);
	expectRGB(color, 0.5, 0.0, 0.0);
}

/* ShadesScale divided by its option: 0 (the default) gave NaN. */
TEST(PixelFactoryColor, shadesScaleWithoutAStepIsTheAverage)
{
	EXPECT_FLOAT_EQ(Color< float >::computeLuminance(0.3F, 0.6F, 0.9F, GrayscaleConversionMode::ShadesScale, 0), 0.6F);
	EXPECT_FLOAT_EQ(Color< float >::computeLuminance(0.3F, 0.6F, 0.9F, GrayscaleConversionMode::ShadesScale, -2), 0.6F);
}
