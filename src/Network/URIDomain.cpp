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

		/* userinfo — everything before the first '@' (host never contains one). */
		if ( const auto at = rawString.find('@'); at != std::string::npos )
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
					Logging::error(Tag, "URIDomain(), port '" + port + "' out of range, ignored.");
				}
			}
			else
			{
				Logging::error(Tag, "URIDomain(), non-numeric port '" + port + "', ignored.");
			}
		}

		/* Host is case-insensitive (RFC 3986 §6.2.2.1) and percent-decoded. A
		 * bracketed IP-literal keeps its brackets and is not further decoded. */
		if ( !host.empty() && host.front() == '[' )
		{
			m_hostname = Hostname::fromString(toLowerASCII(host));
		}
		else
		{
			m_hostname = Hostname::fromString(toLowerASCII(PercentEncoding::decode(host)));
		}
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
