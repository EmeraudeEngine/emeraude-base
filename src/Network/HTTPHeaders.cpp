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

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "String.hpp"

namespace EmEn::Base::Network
{
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
			Logging::error("Network::HTTPHeaders", "parse(), empty HTTP header !");

			return false;
		}

		/* NOTE: Check the first and remove it. */
		if ( this->parseFirstLine(lines[0]) )
		{
			lines.erase(lines.begin());
		}
		else
		{
			Logging::error("Network::HTTPHeaders", "parse(), unable to identify the HTTP header !");

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
				Logging::warning("Network::HTTPHeaders", "parse(), unable to parse header line : " + line);

				errors++;

				continue;
			}

			this->add(String::trim(line.substr(0, separator)), String::trim(line.substr(separator + 1)));
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
