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

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "PixelFactory/Color.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::PixelFactory;

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
