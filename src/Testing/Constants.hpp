/*
 * src/Testing/Constants.hpp
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

#pragma once

/* STL inclusions. */
#include <cstdint>
#include <filesystem>

/* NOTE: Source images. */
static const std::filesystem::path FixedFont{"./assets/fixed-font.tga"};
static const std::filesystem::path TrueTypeFont{"./assets/Joystick.Bold.ttf"};
static const std::filesystem::path LargeRGB{"./assets/1200x800-RBG.jpg"};

/* NOTE: ExtraLargeRGB switches between the 1200x800 image (default, keeps the suite
 * fast) and the heavy 8160x6144 image. Toggle it with the CMake option
 * EMERAUDE_TESTS_USE_EXTRA_LARGE_RGB (available when the test suite is built). The
 * matching dimensions are exposed so tests don't hard-code them. */
#ifdef EMERAUDE_TESTS_USE_EXTRA_LARGE_RGB
static const std::filesystem::path ExtraLargeRGB{"./assets/8160x6144-RBG.jpg"};
static constexpr uint32_t ExtraLargeRGB_Width = 8160;
static constexpr uint32_t ExtraLargeRGB_Height = 6144;
#else
static const std::filesystem::path ExtraLargeRGB{LargeRGB};
static constexpr uint32_t ExtraLargeRGB_Width = 1200;
static constexpr uint32_t ExtraLargeRGB_Height = 800;
#endif
static const std::filesystem::path MediumGrayscale{"./assets/512x512-Grayscale.png"};
static const std::filesystem::path MediumGrayscaleAlpha{"./assets/512x512-GrayscaleAlpha.png"};
static const std::filesystem::path MediumPalette{"./assets/512x512-Palette.png"};
static const std::filesystem::path MediumRGB{"./assets/512x512-RGB.png"};
static const std::filesystem::path MediumRGBA{"./assets/512x512-RGBA.png"};
static const std::filesystem::path SmallPatternRBG_1{"./assets/126x126-RGB_pattern001.png"};
static const std::filesystem::path SmallPatternRBG_2{"./assets/126x126-RGB_pattern002.png"};
static const std::filesystem::path SmallPatternRBG_3{"./assets/126x126-RGB_pattern003.png"};
static const std::filesystem::path MediumPatternRBG_1{"./assets/256x256-RGB_pattern001.png"};
static const std::filesystem::path SmallRGBA{"./assets/64x64-RGBA.png"};
static const std::filesystem::path LargeRGB_RLE_Targa{"./assets/1700x1280-RGB_RLE.tga"};
