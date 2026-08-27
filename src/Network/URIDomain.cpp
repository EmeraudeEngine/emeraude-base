/*
 * src/Network/URIDomain.cpp
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

#include "URIDomain.hpp"

/* STL inclusions. */
#include <algorithm>
#include <ranges>

/* STL inclusions. */
#include <cctype>
#include <sstream>
#include <string>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "PercentEncoding.hpp"
#include "String.hpp"

namespace EmEn::Base::Network
{
	namespace
	{
		constexpr auto Tag{"Network::URIDomain"};

		/**
		 * @brief Returns whether every character of a string is an ASCII digit.
		 * @param string The string.
		 * @return bool
		 */
		/**
		 * @brief Returns whether a decoded host is a legal one: a bracketed IP literal, or a
		 * hostname made of unreserved DNS characters only.
		 * @note Refuses CR/LF/NUL/space/'@'/':' — the bytes that turn a host into an injection
		 * into the CONNECT line, the Host: header or SNI (see the caller).
		 */
		bool
		isValidHost (const std::string & host) noexcept
		{
			if ( host.empty() )
			{
				return true;
			}

			if ( host.front() == '[' )
			{
				if ( host.back() != ']' || host.size() < 4 )
				{
					return false;
				}

				return std::ranges::all_of(host.begin() + 1, host.end() - 1, [] (char character) {
					return std::isxdigit(static_cast< unsigned char >(character)) != 0 || character == ':' || character == '.' || character == '%';
				});
			}

			if ( host.size() > 253 )
			{
				return false;
			}

			return std::ranges::all_of(host, [] (char character) {
				return std::isalnum(static_cast< unsigned char >(character)) != 0 || character == '.' || character == '-' || character == '_';
			});
		}

		bool
		isAllDigits (const std::string & string) noexcept
		{
			if ( string.empty() )
			{
				return false;
			}

			for ( const auto character : string )
			{
				if ( std::isdigit(static_cast< unsigned char >(character)) == 0 )
				{
					return false;
				}
			}

			return true;
		}

		/**
		 * @brief Lowercases the ASCII letters of a string (host normalization).
		 * @param string The input.
		 * @return std::string
		 */
		std::string
		toLowerASCII (const std::string & string) noexcept
		{
			std::string output{string};

			for ( auto & character : output )
			{
				character = static_cast< char >(std::tolower(static_cast< unsigned char >(character)));
			}

			return output;
		}
	}

	URIDomain::URIDomain (std::string rawString) noexcept
	{
		/* rawString is the RFC 3986 authority: [ userinfo "@" ] host [ ":" port ].
		 * The leading "//" is handled by URI, not here. */
		if ( rawString.empty() )
		{
			return;
		}

		/* userinfo — everything before the LAST '@' (RFC 3986 §3.2: the host itself never
		 * contains one, but userinfo may carry an encoded '@'). */
		if ( const auto at = rawString.rfind('@'); at != std::string::npos )
		{
			this->parseUserInfos(rawString.substr(0, at));

			rawString.erase(0, at + 1);
		}

		/* host + optional port, with IPv6 literals kept whole inside brackets. */
		std::string host;
		std::string port;

		if ( !rawString.empty() && rawString.front() == '[' )
		{
			const auto close = rawString.find(']');

			if ( close == std::string::npos )
			{
				Logging::error(Tag, "URIDomain(), unterminated IPv6 literal in '" + rawString + "'.");

				return;
			}

			host = rawString.substr(0, close + 1);

			const auto remainder = rawString.substr(close + 1);

			if ( !remainder.empty() )
			{
				if ( remainder.front() != ':' )
				{
					Logging::error(Tag, "URIDomain(), junk after the IPv6 literal in '" + rawString + "'.");

					return;
				}

				port = remainder.substr(1);
			}
		}
		else if ( const auto colon = rawString.rfind(':'); colon != std::string::npos && isAllDigits(rawString.substr(colon + 1)) )
		{
			host = rawString.substr(0, colon);
			port = rawString.substr(colon + 1);
		}
		else if ( const auto trailing = rawString.rfind(':'); trailing != std::string::npos && trailing + 1 == rawString.size() && rawString.find('[') == std::string::npos )
		{
			/* "host:" — same case, isAllDigits("") being false. */
			host = rawString.substr(0, trailing);
		}
		else
		{
			host = rawString;
		}

		/* Port range validation (0-65535); an out-of-range value is dropped. */
		if ( !port.empty() )
		{
			if ( isAllDigits(port) )
			{
				const auto value = String::toNumber< unsigned long >(port);

				if ( value <= 65535 )
				{
					m_port = static_cast< uint32_t >(value);
				}
				else
				{
					/* NOTE: a declared-but-invalid port must not silently become the scheme's
					 * default port downstream — it is recorded as such. */
					m_portDeclaredInvalid = true;

					Logging::error(Tag, "URIDomain(), port '" + port + "' out of range, ignored.");
				}
			}
			else
			{
				m_portDeclaredInvalid = true;

				Logging::error(Tag, "URIDomain(), non-numeric port '" + port + "', ignored.");
			}
		}

		/* Host is case-insensitive (RFC 3986 §6.2.2.1) and percent-decoded. A
		 * bracketed IP-literal keeps its brackets and is not further decoded. */
		const auto decodedHost = host.empty() || host.front() == '[' ? host : PercentEncoding::decode(host);

		/* ⚠️ The decoded host reaches a CONNECT request line, a Host: header, SNI and the resolver.
		 * Percent-decoding can produce CR, LF, NUL, spaces or a second ':' — i.e. request smuggling
		 * and header injection — so anything that is not a legal host is refused here, once, for
		 * every consumer. */
		if ( !isValidHost(decodedHost) )
		{
			Logging::error(Tag, "URIDomain(), illegal host '" + PercentEncoding::encode(decodedHost, PercentEncoding::Component::Userinfo) + "' refused.");

			return;
		}

		m_hostname = Hostname::fromString(toLowerASCII(decodedHost));
	}

	void
	URIDomain::parseUserInfos (const std::string & string) noexcept
	{
		/* NOTE: non-standard ";AUTH=..." options are preserved for backward
		 * compatibility; the standard userinfo is "user[:password]". */
		auto chunks = String::explode(string, ';');

		for ( size_t index = 1; index < chunks.size(); index++ )
		{
			const auto option = String::explode(chunks[index], '=');

			if ( option.size() != 2 )
			{
				Logging::warning(Tag, "parseUserInfos(), invalid option '" + chunks[index] + "'.");

				continue;
			}

			this->addOption(PercentEncoding::decode(option[0]), PercentEncoding::decode(option[1]));
		}

		chunks = String::explode(chunks[0], ':');

		switch ( chunks.size() )
		{
			case 2 :
				this->setPassword(PercentEncoding::decode(chunks[1]));

				[[fallthrough]];

			case 1 :
				this->setUsername(PercentEncoding::decode(chunks[0]));
				break;

			default :
				Logging::warning(Tag, "parseUserInfos(), multiple ':' in user information.");

				break;
		}
	}

	std::string
	URIDomain::userinfo () const noexcept
	{
		std::stringstream output;

		output << PercentEncoding::encode(m_username, PercentEncoding::Component::Userinfo);

		if ( !m_password.empty() )
		{
			output << ':' << PercentEncoding::encode(m_password, PercentEncoding::Component::Userinfo);
		}

		if ( !m_options.empty() )
		{
			for ( const auto & [name, value] : m_options )
			{
				output << ';' << name << '=' << value;
			}
		}

		return output.str();
	}

	std::string
	URIDomain::host () const noexcept
	{
		std::stringstream string;

		string << m_hostname.name();

		if ( m_port > 0 )
		{
			string << ':' << m_port;
		}

		return string.str();
	}

	std::ostream &
	operator<< (std::ostream & out, const URIDomain & obj)
	{
		if ( !obj.m_username.empty() )
		{
			out << obj.userinfo() << '@';
		}

		out << obj.m_hostname.name();

		if ( obj.m_port > 0 )
		{
			out << ':' << obj.m_port;
		}

		return out;
	}
}
