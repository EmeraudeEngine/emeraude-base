/*
 * src/Network/HTTPHeaders.hpp
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

#pragma once

/* STL inclusions. */
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace EmEn::Base::Network
{
	/**
	 * @brief The HTTPHeaders class
	 */
	class HTTPHeaders
	{
		public:

			/**
			 * @brief The HTTP protocol version enumeration.
			 */
			enum class Version : std::uint8_t
			{
				HTTP09,
				HTTP10,
				HTTP11,
				HTTP20,
				HTTP30
			};

			static constexpr auto HTTP09{"HTTP/0.9"};
			static constexpr auto HTTP10{"HTTP/1.0"};
			static constexpr auto HTTP11{"HTTP/1.1"};
			static constexpr auto HTTP20{"HTTP/2.0"};
			static constexpr auto HTTP30{"HTTP/3.0"};

			static constexpr auto HeaderSeparator{"\r\n\r\n"};
			static constexpr auto Separator{"\r\n"};

			/**
			 * @brief Case-insensitive hash for header field names.
			 * @note HTTP field names are case-insensitive (RFC 9110 §5.1) — servers
			 * legitimately send 'content-length' as well as 'Content-Length'.
			 */
			struct CaseInsensitiveHash final
			{
				[[nodiscard]]
				size_t
				operator() (const std::string & key) const noexcept
				{
					/* FNV-1a over lowercased bytes. */
					size_t hash = 14695981039346656037ULL;

					for ( const auto character : key )
					{
						hash ^= static_cast< size_t >(std::tolower(static_cast< unsigned char >(character)));
						hash *= 1099511628211ULL;
					}

					return hash;
				}
			};

			/** @brief Case-insensitive equality for header field names. */
			struct CaseInsensitiveEqual final
			{
				[[nodiscard]]
				bool
				operator() (const std::string & lhs, const std::string & rhs) const noexcept
				{
					if ( lhs.size() != rhs.size() )
					{
						return false;
					}

					for ( size_t index = 0; index < lhs.size(); ++index )
					{
						if ( std::tolower(static_cast< unsigned char >(lhs[index])) != std::tolower(static_cast< unsigned char >(rhs[index])) )
						{
							return false;
						}
					}

					return true;
				}
			};

			/**
			 * @brief Copy constructor.
			 * @param copy A reference to the copied instance.
			 */
			HTTPHeaders (const HTTPHeaders & copy) noexcept = default;

			/**
			 * @brief Move constructor.
			 * @param copy A reference to the copied instance.
			 */
			HTTPHeaders (HTTPHeaders && copy) noexcept = default;

			/**
			 * @brief Copy assignment.
			 * @param copy A reference to the copied instance.
			 * @return HTTPHeaders &
			 */
			HTTPHeaders & operator= (const HTTPHeaders & copy) noexcept = default;

			/**
			 * @brief Move assignment.
			 * @param copy A reference to the copied instance.
			 * @return HTTPHeaders &
			 */
			HTTPHeaders & operator= (HTTPHeaders && copy) noexcept = default;
			
			/**
			 * @brief Destructs the HTTP headers.
			 */
			virtual ~HTTPHeaders () = default;

			/**
			 * @brief Sets the HTTP version in use.
			 * @param version The HTTP version enum.
			 * @return void
			 */
			void
			setVersion (Version version) noexcept
			{
				m_version = version;
			}

			/**
			 * @brief Returns the HTTP version in use.
			 * @return Version
			 */
			[[nodiscard]]
			Version
			version () const noexcept
			{
				return m_version;
			}

			/**
			 * @brief Adds a new header line.
			 * @param key The name of the header.
			 * @param value The value of the header.
			 * @return void
			 */
			void
			add (const std::string & key, const std::string & value) noexcept
			{
				m_headers.emplace(key, value);
			}

			/**
			 * @brief Returns the value of a header.
			 * @param key The name of the header.
			 * @return std::string
			 */
			[[nodiscard]]
			std::string value (const std::string & key) const noexcept;

			/**
			 * @brief Parses raw headers to the map data.
			 * @param rawHeaders A reference to string.
			 * @return bool
			 */
			[[nodiscard]]
			bool parse (const std::string & rawHeaders) noexcept;

			/**
			 * @brief Returns whether the headers are valid.
			 * @return bool
			 */
			[[nodiscard]]
			virtual bool isValid () const noexcept = 0;

			/**
			 * @brief Returns a string representation of the HTTP headers.
			 * @return std::string
			 */
			[[nodiscard]]
			virtual std::string toString () const noexcept = 0;

			/**
			 * @brief Returns the string of an HTTP protocol version.
			 * @param version
			 * @return const char *
			 */
			[[nodiscard]]
			static const char * version (Version version) noexcept;

			/**
			 * @brief Returns the HTTP protocol version from a string.
			 * @param version A reference to a string.
			 * @return Version
			 */
			[[nodiscard]]
			static Version parseVersion (const std::string & version) noexcept;

		protected:

			/**
			 * @brief Constructs an HTTP header.
			 * @param version The HTTP version.
			 */
			explicit
			HTTPHeaders (Version version = Version::HTTP09) noexcept
				: m_version{version}
			{

			}

			/**
			 * @brief Parses the first line of HTTP Headers.
			 * @param line A reference to the string.
			 * @return bool
			 */
			virtual bool parseFirstLine (const std::string & line) noexcept = 0;

			Version m_version;
			std::unordered_map< std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual > m_headers;
	};
}
