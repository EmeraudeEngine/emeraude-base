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

/* NOTE: Fixture directory. CMake bakes the absolute path to resources/assets so the
 * test binary works from any working directory (ctest, IDE run configs, direct run).
 * The "./assets" fallback keeps a build without the definition usable from resources/. */
#ifdef EMERAUDE_TESTS_ASSETS_DIR
static const std::filesystem::path AssetsDirectory{EMERAUDE_TESTS_ASSETS_DIR};
#else
static const std::filesystem::path AssetsDirectory{"./assets"};
#endif

/* NOTE: Source images. */
static const std::filesystem::path FixedFont{AssetsDirectory / "fixed-font.tga"};
static const std::filesystem::path TrueTypeFont{AssetsDirectory / "Joystick.Bold.ttf"};
static const std::filesystem::path LargeRGB{AssetsDirectory / "1200x800-RBG.jpg"};

/* NOTE: ExtraLargeRGB switches between the 1200x800 image (default, keeps the suite
 * fast) and the heavy 8160x6144 image. Toggle it with the CMake option
 * EMERAUDE_TESTS_USE_EXTRA_LARGE_RGB (available when the test suite is built). The
 * matching dimensions are exposed so tests don't hard-code them. */
#ifdef EMERAUDE_TESTS_USE_EXTRA_LARGE_RGB
static const std::filesystem::path ExtraLargeRGB{AssetsDirectory / "8160x6144-RBG.jpg"};
static constexpr uint32_t ExtraLargeRGB_Width = 8160;
static constexpr uint32_t ExtraLargeRGB_Height = 6144;
#else
static const std::filesystem::path ExtraLargeRGB{LargeRGB};
static constexpr uint32_t ExtraLargeRGB_Width = 1200;
static constexpr uint32_t ExtraLargeRGB_Height = 800;
#endif
static const std::filesystem::path MediumGrayscale{AssetsDirectory / "512x512-Grayscale.png"};
static const std::filesystem::path MediumGrayscaleAlpha{AssetsDirectory / "512x512-GrayscaleAlpha.png"};
static const std::filesystem::path MediumPalette{AssetsDirectory / "512x512-Palette.png"};
static const std::filesystem::path MediumRGB{AssetsDirectory / "512x512-RGB.png"};
static const std::filesystem::path MediumRGBA{AssetsDirectory / "512x512-RGBA.png"};
static const std::filesystem::path SmallPatternRBG_1{AssetsDirectory / "126x126-RGB_pattern001.png"};
static const std::filesystem::path SmallPatternRBG_2{AssetsDirectory / "126x126-RGB_pattern002.png"};
static const std::filesystem::path SmallPatternRBG_3{AssetsDirectory / "126x126-RGB_pattern003.png"};
static const std::filesystem::path MediumPatternRBG_1{AssetsDirectory / "256x256-RGB_pattern001.png"};
static const std::filesystem::path SmallRGBA{AssetsDirectory / "64x64-RGBA.png"};
static const std::filesystem::path LargeRGB_RLE_Targa{AssetsDirectory / "1700x1280-RGB_RLE.tga"};
