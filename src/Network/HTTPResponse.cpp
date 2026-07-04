/*
 * src/Network/HTTPResponse.cpp
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

#include "HTTPResponse.hpp"

/* STL inclusions. */
#include <sstream>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "String.hpp"

namespace EmEn::Base::Network
{
	bool
	HTTPResponse::isValid () const noexcept
	{
		/* NOTE: the reason phrase is optional (RFC 9112 §4) — only the status
		 * code qualifies the response. */
		return m_codeResponse >= 100 && m_codeResponse <= 599;
	}

	bool
	HTTPResponse::keepConnectionAlive () const noexcept
	{
		const auto connectionValue = String::toLower(this->value(Connection));

		switch ( m_version )
		{
			/* HTTP/1.1: persistent by default. */
			case Version::HTTP11 :
				return connectionValue != "close";

			/* HTTP/1.0: close by default. */
			case Version::HTTP10 :
				return connectionValue == "keep-alive";

			default :
				return false;
		}
	}

	bool
	HTTPResponse::parseFirstLine (const std::string & line) noexcept
	{
		/* NOTE: keep empty chunks — the reason phrase is optional (RFC 9112 §4),
		 * "HTTP/1.1 404" and "HTTP/1.1 404 " are both legal status lines. */
		const auto chunks = String::explode(line, ' ', true, 2);

		if ( chunks.size() < 2 )
		{
			Logging::error("Network::HTTPResponse", "parseFirstLine(), invalid HTTP status line : " + line);

			return false;
		}

		/* Protocol version. */
		this->setVersion(HTTPHeaders::parseVersion(chunks[0]));

		/* Status code: exactly what qualifies the response — bounds-checked. */
		const auto code = String::toNumber< int >(chunks[1]);

		if ( code < 100 || code > 599 )
		{
			Logging::error("Network::HTTPResponse", "parseFirstLine(), invalid HTTP status code : " + line);

			return false;
		}

		m_codeResponse = code;

		/* Optional reason phrase. */
		m_textResponse = chunks.size() == 3 ? chunks[2] : std::string{};

		return true;
	}

	std::string
	HTTPResponse::toString () const noexcept
	{
		/* NOTE: HTTP version 0.9 have no header in response. */
		if ( m_version == Version::HTTP09 )
		{
			return {};
		}

		std::stringstream output;

		output << HTTPHeaders::version(m_version) << ' ' << m_codeResponse << ' ' << m_textResponse << Separator;

		for ( const auto & [name, value] : m_headers )
		{
			output << name << ": " << value << Separator;
		}

		output << Separator;

		return output.str();
	}
}
