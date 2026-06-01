/*
 * src/Testing/test_MD5AnimParser.cpp
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
#include <sstream>
#include <string>

/* Local inclusions. */
#include "Animation/MD5AnimParser.hpp"

/* Robustness characterization for the MD5 animation parser (Ave robustus! — A.3, I/O boundary
 * hardening). The parser consumes untrusted .md5anim text; malformed/truncated input must
 * degrade gracefully (empty clip), never crash. Verified meaningfully under ASan/UBSan via
 * ctest — e.g. the empty-joint-name regression below was undefined behaviour (front()/back()
 * on an empty string) before the size>=2 guard. */

namespace EmEn::Base::Animation
{
	namespace
	{
		using Parser = MD5AnimParser< float >;
	}

	TEST(MD5AnimParser, emptyStreamYieldsEmptyClip)
	{
		std::istringstream stream{""};

		const auto clip = Parser::parseStream(stream, "test");

		EXPECT_TRUE(clip.empty());
	}

	TEST(MD5AnimParser, truncatedHierarchyEmptyJointNameDoesNotCrash)
	{
		/* numJoints announces one joint, but the joint line is blank: `hs >> name` leaves
		 * `name` empty, so name.front()/back() used to be UB. The parse must complete. */
		const std::string md5 =
			"numFrames 0\n"
			"numJoints 1\n"
			"frameRate 24\n"
			"numAnimatedComponents 0\n"
			"hierarchy {\n"
			"\n"
			"}\n";

		std::istringstream stream{md5};

		const auto clip = Parser::parseStream(stream, "truncated");

		/* No frames -> no channels; the contract verified under ASan/UBSan is "no crash". */
		EXPECT_TRUE(clip.empty());
	}

	TEST(MD5AnimParser, singleQuoteJointNameDoesNotCrash)
	{
		/* A one-character `"` token: front()==back()=='"' but size<2, so the quote-strip must
		 * be skipped (otherwise substr(1, size-2) underflows the length). */
		const std::string md5 =
			"numFrames 0\n"
			"numJoints 1\n"
			"frameRate 24\n"
			"numAnimatedComponents 0\n"
			"hierarchy {\n"
			"\"\n"
			"}\n";

		std::istringstream stream{md5};

		const auto clip = Parser::parseStream(stream, "weird");

		EXPECT_TRUE(clip.empty());
	}
}