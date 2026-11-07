/*
 * src/Network/HTTPHeaders.cpp
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

#include "HTTPHeaders.hpp"

/* STL inclusions. */
#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "Network/PercentEncoding.hpp"
#include "String.hpp"

namespace EmEn::Base::Network
{
	namespace
	{
		constexpr auto Tag{"Network::HTTPHeaders"};

		/**
		 * @brief Returns whether a character is a RFC 9110 § 5.6.2 token character.
		 * @param character The character.
		 * @return bool
		 */
		[[nodiscard]]
		bool
		isTokenChar (char character) noexcept
		{
			if ( std::isalnum(static_cast< unsigned char >(character)) != 0 )
			{
				return true;
			}

			constexpr std::string_view Specials{"!#$%&'*+-.^_`|~"};

			return Specials.find(character) != std::string_view::npos;
		}
	}

	bool
	HTTPHeaders::isFieldNameValid (std::string_view name) noexcept
	{
		return !name.empty() && std::ranges::all_of(name, isTokenChar);
	}

	bool
	HTTPHeaders::isFieldValueValid (std::string_view value) noexcept
	{
		/* ⚠️ A message is built by concatenation ("name: value\r\n"), so a CR or LF inside a value ends the line early and
		 * injects everything after it — header injection, and with a body, message splitting. Every other C0 control
		 * (and DEL) is refused too; only HTAB is legal in a field value (RFC 9110 § 5.5) and a reason phrase
		 * (RFC 9112 § 4). */
		return std::ranges::none_of(value, [] (char character) {
			const auto byte = static_cast< unsigned char >(character);

			if ( byte == '\t' )
			{
				return false;
			}

			return byte < 0x20 || byte == 0x7F;
		});
	}

	bool
	HTTPHeaders::add (const std::string & key, const std::string & value) noexcept
	{
		if ( !isFieldNameValid(key) || !isFieldValueValid(value) )
		{
			/* NOTE: The name is escaped: a refused name may hold the very CR / LF the check is for. */
			Logging::error(Tag, "add(), the field '" + PercentEncoding::encode(key, PercentEncoding::Component::Userinfo) + "' has an invalid name or value, refused.");

			return false;
		}

		m_headers.emplace(key, value);

		return true;
	}

	bool
	HTTPHeaders::set (const std::string & key, const std::string & value) noexcept
	{
		if ( !isFieldNameValid(key) || !isFieldValueValid(value) )
		{
			/* NOTE: The name is escaped: a refused name may hold the very CR / LF the check is for. */
			Logging::error(Tag, "set(), the field '" + PercentEncoding::encode(key, PercentEncoding::Component::Userinfo) + "' has an invalid name or value, refused.");

			return false;
		}

		m_headers.insert_or_assign(key, value);

		return true;
	}

	std::string
	HTTPHeaders::value (const std::string & key) const noexcept
	{
		if ( const auto headerIt = m_headers.find(key); headerIt != m_headers.cend() )
		{
			return headerIt->second;
		}

		return {};
	}

	bool
	HTTPHeaders::parse (const std::string & rawHeaders) noexcept
	{
		auto lines = String::explode(rawHeaders, Separator, false);

		if ( empty(lines) )
		{
			Logging::error(Tag, "parse(), empty HTTP header !");

			return false;
		}

		/* NOTE: Check the first and remove it. */
		if ( this->parseFirstLine(lines[0]) )
		{
			lines.erase(lines.begin());
		}
		else
		{
			Logging::error(Tag, "parse(), unable to identify the HTTP header !");

			return false;
		}

		/* NOTE: Parse each header. */
		size_t errors = 0;

		for ( const auto & line : lines )
		{
			/* NOTE: split on the FIRST colon manually. String::explode() with keepEmpty=false
			 * drops an empty tail, which used to turn a legal empty field value ("X-Cache:",
			 * RFC 9110 §5.5) into a parse error that rejected the whole response. */
			const auto separator = line.find(':');

			if ( separator == std::string::npos || separator == 0 )
			{
				Logging::warning(Tag, "parse(), unable to parse header line : " + line);

				errors++;

				continue;
			}

			/* NOTE: A field add() refuses (a name that is not a token, a control character in the value) is a parse
			 * error: the message is refused (RFC 9110 § 5.5 allows it; owner decision 2026-11-07, strict). */
			if ( !this->add(String::trim(line.substr(0, separator)), String::trim(line.substr(separator + 1))) )
			{
				errors++;
			}
		}

		return errors == 0;
	}

	const char *
	HTTPHeaders::version (Version version) noexcept
	{
		switch ( version )
		{
			case Version::HTTP09 :
				return HTTP09;

			case Version::HTTP10 :
				return HTTP10;

			case Version::HTTP11 :
				return HTTP11;

			case Version::HTTP20 :
				return HTTP20;

			case Version::HTTP30 :
				return HTTP30;
		}

		return HTTP09;
	}

	HTTPHeaders::Version
	HTTPHeaders::parseVersion (const std::string & version) noexcept
	{
		if ( version == HTTP09 )
		{
			return Version::HTTP09;
		}

		if ( version == HTTP10 )
		{
			return Version::HTTP10;
		}

		if ( version == HTTP11 )
		{
			return Version::HTTP11;
		}

		if ( version == HTTP20 )
		{
			return Version::HTTP20;
		}

		if ( version == HTTP30 )
		{
			return Version::HTTP30;
		}

		return Version::HTTP09;
	}
}
