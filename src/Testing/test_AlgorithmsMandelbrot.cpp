/*
 * src/Testing/test_AlgorithmsMandelbrot.cpp
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
#include <cstdint>
#include <string>
#include <vector>

/* Local inclusions. */
#include "Algorithms/Mandelbrot.hpp"
#include "PixelFactory/Pixmap.hpp"

using namespace EmEn::Base;

/* 2026-10-19: no translation unit included Mandelbrot.hpp: its size_t/uint32_t conversions and empty documentation had
 * never compiled under the warnings. The origin is inside the set (every iteration), a far point escapes at once. */
TEST(AlgorithmsMandelbrot, insideTheSetReachesTheIterationLimit)
{
	Algorithms::Mandelbrot< double > mandelbrot{4, 2};

	std::vector< int32_t > iterations(8, -1);

	mandelbrot.setColorizationMethod([&iterations] (PixelFactory::Pixmap< uint8_t > & /*pixmap*/, uint32_t x, uint32_t y, int32_t value, int32_t /*maxIterations*/) {
		iterations[(y * 4) + x] = value;
	});

	/* Columns map -2, -1, 0, 1 on the real axis, rows 0 and 1 on the imaginary one (c = -2 sits on |z| = 2, the strict
	 * escape test lets it out). */
	mandelbrot.execute(-2.0, 2.0, 0.0, 2.0, 64);

	EXPECT_EQ(iterations[2], 64) << "c = 0";
	EXPECT_EQ(iterations[1], 64) << "c = -1";
	EXPECT_LT(iterations[3], 64) << "c = 1";
	EXPECT_LT(iterations[7], 64) << "c = 1 + i";
	EXPECT_EQ(mandelbrot.pixmap().width(), 4U);
	EXPECT_EQ(mandelbrot.pixmap().height(), 2U);
}
