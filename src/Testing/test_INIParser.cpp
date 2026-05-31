/*
 * src/Testing/test_INIParser.cpp
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
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

/* Local inclusions. */
#include "INIParser.hpp"

namespace EmEn::Base
{
	namespace
	{
		/* noexcept filesystem helpers (the throwing overloads would terminate under -fno-exceptions). */
		std::filesystem::path
		tempFile (const char * name) noexcept
		{
			std::error_code errorCode;

			return std::filesystem::temp_directory_path(errorCode) / name;
		}

		void
		removeQuietly (const std::filesystem::path & path) noexcept
		{
			std::error_code errorCode;

			std::filesystem::remove(path, errorCode);
		}

		/* Writes raw INI content to a temp file and returns its path (empty on failure). */
		std::filesystem::path
		writeIni (const char * name, std::string_view content) noexcept
		{
			const auto path = tempFile(name);

			std::ofstream file{path, std::ios::out | std::ios::trunc};

			if ( !file.is_open() )
			{
				return {};
			}

			file << content;

			return path;
		}
	}

	/* ===== Nominal parsing ===== */

	TEST(INIParser, parseSectionsAndTypedValues)
	{
		const auto path = writeIni("emeraude_base_ini_nominal.ini",
			"# leading comment\n"
			"@ header line\n"
			"version = 1.0\n"
			"\n"
			"[Graphics]\n"
			"width = 1920\n"
			"fullscreen = true\n"
			"gamma = 2.2\n"
			"name = hotel\n");
		ASSERT_FALSE(path.empty());

		INIParser parser;
		ASSERT_TRUE(parser.read(path));

		/* Key written before any [section] lands in the implicit "main" section. */
		EXPECT_EQ(parser.section("main").variable("version").asString(), std::string{"1.0"});

		const auto & graphics = parser.section("Graphics");
		EXPECT_EQ(graphics.variable("width").asInteger(), 1920);
		EXPECT_TRUE(graphics.variable("fullscreen").asBoolean());
		EXPECT_FLOAT_EQ(graphics.variable("gamma").asFloat(), 2.2F);
		EXPECT_DOUBLE_EQ(graphics.variable("gamma").asDouble(), 2.2);
		EXPECT_EQ(graphics.variable("name").asString(), std::string{"hotel"});

		/* A missing variable is undefined, never a throw. */
		EXPECT_TRUE(graphics.variable("absent").isUndefined());
		EXPECT_FALSE(graphics.variable("width").isUndefined());

		removeQuietly(path);
	}

	TEST(INIParser, trimsKeysAndValues)
	{
		const auto path = writeIni("emeraude_base_ini_trim.ini",
			"   spaced_key   =   spaced value   \n");
		ASSERT_FALSE(path.empty());

		INIParser parser;
		ASSERT_TRUE(parser.read(path));

		EXPECT_EQ(parser.section("main").variable("spaced_key").asString(), std::string{"spaced value"});

		removeQuietly(path);
	}

	TEST(INIParser, commentsAndHeadersAreIgnored)
	{
		const auto path = writeIni("emeraude_base_ini_comments.ini",
			"# this is a comment = not a definition\n"
			"@ this is a header = also ignored\n"
			"real = value\n");
		ASSERT_FALSE(path.empty());

		INIParser parser;
		ASSERT_TRUE(parser.read(path));

		const auto & main = parser.section("main");
		EXPECT_EQ(main.variable("real").asString(), std::string{"value"});
		/* A '=' inside a comment/header line must NOT create a variable. */
		EXPECT_EQ(main.variables().size(), 1U);

		removeQuietly(path);
	}

	TEST(INIParser, writeThenReadRoundTrip)
	{
		const auto path = tempFile("emeraude_base_ini_roundtrip.ini");
		removeQuietly(path);

		{
			INIParser config;
			config.section("main").addVariable("version", INIVariable{std::string{"1.0"}});
			config.section("Graphics").addVariable("width", INIVariable{1920});
			config.section("Graphics").addVariable("fullscreen", INIVariable{true});
			ASSERT_TRUE(config.write(path));
		}

		INIParser reloaded;
		ASSERT_TRUE(reloaded.read(path));
		EXPECT_EQ(reloaded.section("main").variable("version").asString(), std::string{"1.0"});
		EXPECT_EQ(reloaded.section("Graphics").variable("width").asInteger(), 1920);
		EXPECT_TRUE(reloaded.section("Graphics").variable("fullscreen").asBoolean());

		removeQuietly(path);
	}

	TEST(INIParser, missingFileReturnsFalse)
	{
		INIParser parser;
		EXPECT_FALSE(parser.read("/nonexistent/emeraude_base_ini_missing.ini"));
	}

	/* ===== Classification correctness (regression guard for getLineType) ===== */

	/* A key may legitimately contain '[', '#' or '@'. The line type must be decided by the
	 * FIRST non-whitespace character, not by the first special character found anywhere —
	 * otherwise a definition whose key contains a marker is silently dropped (and, for '[',
	 * mis-parsed into a spurious section). */
	TEST(INIParser, keyContainingBracketIsADefinition)
	{
		const auto path = writeIni("emeraude_base_ini_bracket_key.ini",
			"arr[0] = 5\n"
			"after = ok\n");
		ASSERT_FALSE(path.empty());

		INIParser parser;
		ASSERT_TRUE(parser.read(path));

		const auto & main = parser.section("main");
		EXPECT_EQ(main.variable("arr[0]").asInteger(), 5);
		EXPECT_FALSE(main.variable("arr[0]").isUndefined());
		/* The bracketed key must NOT have been turned into a section named "0". */
		EXPECT_TRUE(parser.section("0").variable("arr[0]").isUndefined());
		EXPECT_TRUE(parser.section("0").variables().empty());
		/* The following line must still land in "main", not in a hijacked section. */
		EXPECT_EQ(main.variable("after").asString(), std::string{"ok"});

		removeQuietly(path);
	}

	TEST(INIParser, keyContainingAtSignIsADefinition)
	{
		const auto path = writeIni("emeraude_base_ini_at_key.ini",
			"user@host = admin\n");
		ASSERT_FALSE(path.empty());

		INIParser parser;
		ASSERT_TRUE(parser.read(path));

		EXPECT_EQ(parser.section("main").variable("user@host").asString(), std::string{"admin"});

		removeQuietly(path);
	}

	/* Special characters inside the VALUE (after '=') are preserved verbatim — no inline
	 * comment stripping. Works regardless of getLineType because '=' precedes them. */
	TEST(INIParser, valueContainingSpecialCharsIsPreserved)
	{
		const auto path = writeIni("emeraude_base_ini_special_value.ini",
			"color = #ff8800\n"
			"path = C:/users/[guest]\n");
		ASSERT_FALSE(path.empty());

		INIParser parser;
		ASSERT_TRUE(parser.read(path));

		const auto & main = parser.section("main");
		EXPECT_EQ(main.variable("color").asString(), std::string{"#ff8800"});
		EXPECT_EQ(main.variable("path").asString(), std::string{"C:/users/[guest]"});

		removeQuietly(path);
	}
}
