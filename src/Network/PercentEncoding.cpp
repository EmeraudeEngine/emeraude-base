/*
 * src/Network/PercentEncoding.cpp
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

#include "PercentEncoding.hpp"

/* STL inclusions. */
#include <cctype>
#include <cstddef>

namespace EmEn::Base::Network::PercentEncoding
{
	namespace
	{
		/**
		 * @brief Returns the value of a hexadecimal digit, or -1 when not one.
		 * @param character The character.
		 * @return int
		 */
		int
		hexValue (char character) noexcept
		{
			if ( character >= '0' && character <= '9' )
			{
				return character - '0';
			}

			if ( character >= 'a' && character <= 'f' )
			{
				return character - 'a' + 10;
			}

			if ( character >= 'A' && character <= 'F' )
			{
				return character - 'A' + 10;
			}

			return -1;
		}

		/**
		 * @brief Returns whether a character is RFC 3986 unreserved (allowed everywhere).
		 * @param character The character.
		 * @return bool
		 */
		bool
		isUnreserved (unsigned char character) noexcept
		{
			return std::isalnum(character) != 0 || character == '-' || character == '.' || character == '_' || character == '~';
		}

		/**
		 * @brief Returns whether a character is allowed unencoded in the given component.
		 * @param character The character.
		 * @param component The URI component.
		 * @return bool
		 */
		bool
		isAllowed (unsigned char character, Component component) noexcept
		{
			if ( isUnreserved(character) )
			{
				return true;
			}

			/* NOTE: A key or value of a query is delimited by '&' and '=', and form decoders (S3 among them) read a
			 * literal '+' as a space: a presigned URL whose token holds "%2B" or "%3D" broke once re-emitted. */
			if ( component == Component::QueryVariable && (character == '&' || character == '=' || character == '+' || character == ';') )
			{
				return false;
			}

			/* sub-delims (RFC 3986 §2.2): "!$&'()*+,;=" — allowed in every component below. */
			switch ( character )
			{
				case '!' : case '$' : case '&' : case '\'' : case '(' : case ')' :
				case '*' : case '+' : case ',' : case ';' : case '=' :
					return true;

				default :
					break;
			}

			switch ( component )
			{
				case Component::Path :
					/* pchar ( ':' '@' ) + '/'. */
					return character == ':' || character == '@' || character == '/';

				case Component::Segment :
					/* pchar without '/'. */
					return character == ':' || character == '@';

				case Component::Query :
				case Component::QueryVariable :
				case Component::Fragment :
					/* pchar + '/' + '?'. */
					return character == ':' || character == '@' || character == '/' || character == '?';

				case Component::Userinfo :
					/* unreserved + sub-delims + ':'. */
					return character == ':';
			}

			return false;
		}

		/**
		 * @brief Appends the %XX escape of a byte (uppercase hex, RFC 3986 §6.2.2.1).
		 * @param output The string to append to.
		 * @param byte The byte.
		 */
		void
		appendEscape (std::string & output, unsigned char byte) noexcept
		{
			/* A nibble (0-15) as an uppercase hexadecimal digit. */
			constexpr auto hexDigit = [] (unsigned int nibble) noexcept {
				return static_cast< char >(nibble < 10U ? '0' + nibble : 'A' + (nibble - 10U));
			};

			output.push_back('%');
			output.push_back(hexDigit(static_cast< unsigned int >(byte) >> 4U));
			output.push_back(hexDigit(static_cast< unsigned int >(byte) & 0x0FU));
		}
	}

	std::string
	decode (std::string_view input) noexcept
	{
		std::string output;
		output.reserve(input.size());

		for ( size_t index = 0; index < input.size(); ++index )
		{
			if ( input[index] == '%' && index + 2 < input.size() )
			{
				const auto high = hexValue(input[index + 1]);
				const auto low = hexValue(input[index + 2]);

				if ( high >= 0 && low >= 0 )
				{
					output.push_back(static_cast< char >((high << 4) | low));
					index += 2;

					continue;
				}
			}

			/* Malformed escape or ordinary byte: copy verbatim. */
			output.push_back(input[index]);
		}

		return output;
	}

	std::string
	encode (std::string_view input, Component component) noexcept
	{
		std::string output;
		output.reserve(input.size());

		for ( const auto character : input )
		{
			const auto byte = static_cast< unsigned char >(character);

			if ( isAllowed(byte, component) )
			{
				output.push_back(character);
			}
			else
			{
				appendEscape(output, byte);
			}
		}

		return output;
	}

	std::string
	encodePreservingEscapes (std::string_view input, Component component) noexcept
	{
		std::string output;
		output.reserve(input.size());

		for ( size_t index = 0; index < input.size(); ++index )
		{
			const auto byte = static_cast< unsigned char >(input[index]);

			if ( input[index] == '%' && index + 2 < input.size() && hexValue(input[index + 1]) >= 0 && hexValue(input[index + 2]) >= 0 )
			{
				/* A well-formed escape: kept as written (its hex case included). */
				output.append(input.substr(index, 3));
				index += 2;

				continue;
			}

			if ( isAllowed(byte, component) )
			{
				output.push_back(input[index]);
			}
			else
			{
				appendEscape(output, byte);
			}
		}

		return output;
	}
}
