/*
 * src/Testing/test_Platform.cpp
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
 */

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <string_view>

/* Local inclusions. */
#include "emeraude_platform.hpp"

using namespace EmEn;

/* Ave robustus! (Axis B): compiler-identity detection — PLATFORM_COMPILER + version, previously
 * missing from the platform header. */
TEST(Platform, compilerIdentity)
{
	const std::string_view compiler{PlatformCompiler};

	EXPECT_TRUE(compiler == "GCC" || compiler == "Clang" || compiler == "MSVC");

	/* The build had to come from a real compiler, so the major version is at least 1. */
	EXPECT_GT(PlatformCompilerVersionMajor, 0);
	EXPECT_GE(PlatformCompilerVersionMinor, 0);
	EXPECT_GE(PlatformCompilerVersionPatch, 0);
}
