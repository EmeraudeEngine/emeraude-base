/*
 * src/FastJSON.cpp
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

#include "FastJSON.hpp"

/* STL inclusions. */
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

/* Local inclusions. */
#include "Logging/Logging.hpp"

namespace EmEn::Base::FastJSON
{
	namespace
	{
		/**
		 * @brief Returns whether the JSON text nests deeper than `limit` levels of [] / {}.
		 * @note String contents (between unescaped quotes) are skipped so brackets inside
		 * strings do not inflate the depth. Cheap O(n) pre-check run BEFORE jsoncpp: jsoncpp's
		 * CharReader leaks its internal tree on its own stackLimit-abort path for hostile
		 * deeply-nested input, so we reject such input before it ever reaches the parser.
		 */
		bool
		exceedsNestingDepth (std::string_view json, int limit) noexcept
		{
			int depth = 0;
			bool inString = false;
			bool escaped = false;

			for ( const char character : json )
			{
				if ( inString )
				{
					if ( escaped )
					{
						escaped = false;
					}
					else if ( character == '\\' )
					{
						escaped = true;
					}
					else if ( character == '"' )
					{
						inString = false;
					}

					continue;
				}

				switch ( character )
				{
					case '"' :
						inString = true;
						break;

					case '[' :
					case '{' :
						++depth;

						/* jsoncpp's CharReader throws RuntimeError when depth REACHES stackLimit (not just
						 * beyond it); under -fno-exceptions that terminates. Reject at the limit so the
						 * throwing path is never reached (found by the JSON-SFX fuzzer, Ave robustus! A.3). */
						if ( depth >= limit )
						{
							return true;
						}
						break;

					case ']' :
					case '}' :
						if ( depth > 0 )
						{
							--depth;
						}
						break;

					default:
						break;
				}
			}

			return false;
		}
	}

	std::optional< Json::Value >
	getRootFromString (const std::string & json, int stackLimit, bool quiet)
	{
		/* Reject pathologically deep input before handing it to jsoncpp (whose CharReader
		 * leaks on its stackLimit-abort path). */
		if ( exceedsNestingDepth(json, stackLimit) )
		{
			if ( !quiet )
			{
				Logging::warning("FastJSON", "JSON rejected: nesting depth exceeds the stack limit.");
			}

			return std::nullopt;
		}

		Json::CharReaderBuilder builder{};
		builder["collectComments"] = false;
		builder["allowComments"] = false;
		builder["allowTrailingCommas"] = false;
		builder["strictRoot"] = true;
		builder["allowDroppedNullPlaceholders"] = false;
		builder["allowNumericKeys"] = false;
		builder["allowSingleQuotes"] = false;
		builder["stackLimit"] = stackLimit;
		builder["failIfExtra"] = true;
		builder["rejectDupKeys"] = true;
		builder["allowSpecialFloats"] = true;
		builder["skipBom"] = true;

		const std::unique_ptr< Json::CharReader > reader{builder.newCharReader()};

		Json::Value root;
		std::string errors;

		if ( !reader->parse(json.data(), json.data() + json.size(), &root, &errors) )
		{
			if ( !quiet )
			{
				Logging::warning("FastJSON", "unable to parse JSON string: " + errors);
			}

			return std::nullopt;
		}

		return root;
	}

	std::optional< Json::Value >
	getRootFromFile (const std::filesystem::path & filepath, int stackLimit, bool quiet)
	{
		std::ifstream file{filepath, std::ifstream::binary};

		if ( !file.is_open() )
		{
			if ( !quiet )
			{
				Logging::error("FastJSON", "unable to open the file " + filepath.string());
			}

			return std::nullopt;
		}

		/* Read the whole file, then parse through the single guarded string path. */
		const std::string content{std::istreambuf_iterator< char >{file}, std::istreambuf_iterator< char >{}};

		return getRootFromString(content, stackLimit, quiet);
	}

	std::string
	stringify (const Json::Value & root)
	{
		Json::StreamWriterBuilder builder{};
		builder["commentStyle"] = "None";
		builder["indentation"] = "";
		builder["enableYAMLCompatibility"] = false;
		builder["dropNullPlaceholders"] = false;
		builder["useSpecialFloats"] = false;
		builder["precision"] = 5;
		builder["precisionType"] = "significant";
		builder["emitUTF8"] = true;

		return Json::writeString(builder, root);
	}
}
