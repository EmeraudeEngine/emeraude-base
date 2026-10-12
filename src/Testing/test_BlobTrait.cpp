/*
 * src/Testing/test_BlobTrait.cpp
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
#include <filesystem>
#include <string>

/* Local inclusions. */
#include "emeraude_platform.hpp"
#include "BlobTrait.hpp"
#include "IO/IO.hpp"

/* 2026-10-12 (rule C, engine todo windows-path-string-ansi-pass): a std::filesystem::path streamed into a blob (every
 * Trace* line of the engine) is written as UTF-8, quoted as std's operator<< quotes it. std's operator<< writes
 * path::string(): on Windows the ANSI code page, which THROWS (an abort with exceptions off) on a character outside it
 * and writes invalid UTF-8 for the others. */
namespace EmEn::Base
{
	TEST(BlobTrait, pathKeepsTheQuotedFormat)
	{
		BlobTrait blob;
		blob << IO::u8path("data/a b.json");

		ASSERT_EQ(blob.get(), "\"data/a b.json\"");
	}

	TEST(BlobTrait, pathInsideTheCodePageIsUTF8)
	{
		BlobTrait blob;
		blob << IO::u8path("J\xC3\xA9r\xC3\xB4me/x.png");

		ASSERT_EQ(blob.get(), "\"J\xC3\xA9r\xC3\xB4me/x.png\"");
	}

	TEST(BlobTrait, pathOutsideTheCodePageIsUTF8)
	{
		BlobTrait blob;
		blob << IO::u8path("\xCE\xA9mega-\xE6\x97\xA5\xE6\x9C\xAC/x.gltf");

		ASSERT_EQ(blob.get(), "\"\xCE\xA9mega-\xE6\x97\xA5\xE6\x9C\xAC/x.gltf\"");
	}

	TEST(BlobTrait, stringIsNotTakenForAPath)
	{
		BlobTrait blob;
		blob << std::string{"plain"} << ' ' << "text";

		ASSERT_EQ(blob.get(), "plain text");
	}

	TEST(BlobTrait, pathWithALoneSurrogate)
	{
		if constexpr ( IsWindows )
		{
			/* NOTE: NTFS allows a lone surrogate in a file name: U+FFFD, never a throw. */
			BlobTrait blob;
			blob << std::filesystem::path{std::wstring{L'a', static_cast< wchar_t >(0xDC00), L'b'}};

			ASSERT_EQ(blob.get(), "\"a\xEF\xBF\xBD" "b\"");
		}
		else
		{
			GTEST_SKIP() << "Windows only (a POSIX path holds bytes).";
		}
	}
}
