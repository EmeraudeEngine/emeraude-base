/*
 * src/Network/HTTPRequest.cpp
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

#include "HTTPRequest.hpp"

/* STL inclusions. */
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "Network/HTTPHeaders.hpp"
#include "Network/URI.hpp"
#include "String.hpp"

namespace EmEn::Base::Network
{
	namespace
	{
		constexpr auto Tag{"Network::HTTPRequest"};
	}

	HTTPRequest::HTTPRequest (const std::string & rawHeaders) noexcept
	{
		if ( !this->parse(rawHeaders) )
		{
			Logging::error(Tag, "HTTPRequest(), unable to parse the HTTP request.");
		}
	}

	bool
	HTTPRequest::isValid () const noexcept
	{
		if ( m_method == Method::NONE )
		{
			return false;
		}

		/* NOTE: A parsed request target is usually in origin-form ("/upload", RFC 9112 § 3.2.1): a path without a scheme,
		 * which URI::empty() calls empty. Either an absolute URI or a path qualifies the request. */
		if ( m_resource.empty() && m_resource.path().empty() )
		{
			return false;
		}

		return true;
	}

	bool
	HTTPRequest::parseFirstLine (const std::string & line) noexcept
	{
		const auto chunks = String::explode(line, ' ');

		if ( chunks.size() < 2 )
		{
			Logging::error(Tag, "parseFirstLine(), invalid HTTP request line '" + line + "'.");

			return false;
		}

		/* Command */
		m_method = HTTPRequest::parseMethod(chunks[0]);

		if ( m_method == Method::NONE )
		{
			Logging::error(Tag, "parseFirstLine(), invalid method in the request line '" + line + "'.");

			return false;
		}

		/* URL */
		m_resource = chunks[1];

		/* (Protocol version) */
		if ( chunks.size() > 2 )
		{
			this->setVersion(HTTPHeaders::parseVersion(chunks[2]));
		}

		return true;
	}

	std::string
	HTTPRequest::toString () const noexcept
	{
		/* NOTE: method() is nullptr for Method::NONE: streaming it was undefined behaviour (2026-11-07). An invalid
		 * request is never written. */
		if ( !this->isValid() )
		{
			Logging::error(Tag, "toString(), the request has no method or no resource, nothing written.");

			return {};
		}

		std::stringstream string;

		/* NOTE: The request target is the URI's ENCODED path and query, re-emitted as parsed (URI::resource()): the
		 * decoded path put a raw space or a '/' for "%2F" on the request line and the query was dropped (2026-11-07).
		 * ONE leading slash: " /" was written before a path that already starts with one ("GET //a/b", 2026-10-12), and a
		 * relative path gets one. */
		auto target = m_resource.resource();

		if ( target.empty() || target.front() != '/' )
		{
			target.insert(target.begin(), '/');
		}

		/* NOTE: HTTP version 0.9 have no header in response. */
		if ( m_version == Version::HTTP09 )
		{
			string << HTTPRequest::method(m_method) << ' ' << target << Separator;
		}
		else
		{
			string << HTTPRequest::method(m_method) << ' ' << target << ' ' << HTTPHeaders::version(m_version) << Separator;
		}

		for ( const auto & header : m_headers )
		{
			string << header.first << ": " << header.second << Separator;
		}

		string << Separator;

		return string.str();
	}

	const char *
	HTTPRequest::method (Method method) noexcept
	{
		switch ( method )
		{
			case Method::GET :
				return "GET";

			case Method::HEAD :
				return "HEAD";

			case Method::POST :
				return "POST";

			case Method::OPTIONS :
				return "OPTIONS";

			case Method::CONNECT :
				return "CONNECT";

			case Method::TRACE :
				return "TRACE";

			case Method::PUT :
				return "PUT";

			case Method::PATCH :
				return "PATCH";

			case Method::DELETE :
				return "DELETE";

			default:
				return nullptr;
		}
	}

	HTTPRequest::Method
	HTTPRequest::parseMethod (const std::string & method) noexcept
	{
		if ( method == GET )
		{
			return Method::GET;
		}

		if ( method == HEAD )
		{
			return Method::HEAD;
		}

		if ( method == POST )
		{
			return Method::POST;
		}

		if ( method == OPTIONS )
		{
			return Method::OPTIONS;
		}

		if ( method == CONNECT )
		{
			return Method::CONNECT;
		}

		if ( method == TRACE )
		{
			return Method::TRACE;
		}

		if ( method == PUT )
		{
			return Method::PUT;
		}

		if ( method == PATCH )
		{
			return Method::PATCH;
		}

		if ( method == DELETE )
		{
			return Method::DELETE;
		}

		return Method::NONE;
	}
}
