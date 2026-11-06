/*
 * src/Network/URI.hpp
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
#include <filesystem>
#include <sstream>
#include <string>

/* Local inclusions. */
#include "Query.hpp"
#include "URIDomain.hpp"

namespace EmEn::Base::Network
{
	/**
	 * @brief The URI class.
	 * @note [protocol:][//[user-info@]host[:port]][path]
	 * @note Two views of the path and of the query (owner decision, 2026-11-06): the ENCODED bytes, kept as parsed and
	 * re-emitted unchanged by resource() and operator<< (a presigned URL signs them: "%2F" inside an object key, the
	 * parameters' order and duplicates, the escapes' hex case), and the DECODED view, path() and query(), for
	 * inspection only (a "%2F" there is a '/', the query a sorted map that keeps one value per key). The encoded bytes
	 * only ever lose what is illegal in their component: PercentEncoding::encodePreservingEscapes().
	 */
	class URI
	{
		public:

			static constexpr auto HttpProtocol{"http:"};
			static constexpr auto SecureHttpProtocol{"https:"};

			/**
			 * @brief Constructs a default URI.
			 */
			URI () noexcept = default;

			/**
			 * @brief Constructs a URI from a raw string.
			 * @note An empty string is refused: the URI stays empty and a warning goes through Logging (tag
			 * "Network::URI"), never straight to std::cerr.
			 * @param rawURI A reference to a string.
			 */
			explicit URI (const std::string & rawURI) noexcept;

			/**
			 * @brief Assignment operator for std::string.
			 * @note An empty string is refused: the URI KEEPS its previous value and a warning goes through Logging
			 * (tag "Network::URI"), never straight to std::cerr.
			 * @param rawURI A reference of a string for URI.
			 * @return URI &
			 */
			URI & operator= (const std::string & rawURI) noexcept;

			/**
			 * @brief Sets the scheme of the URI.
			 * @param scheme A reference to a string.
			 */
			void
			setScheme (const std::string & scheme) noexcept
			{
				/* RFC 3986 §6.2.2.1: the scheme is case-insensitive and normalized to
				 * lowercase. Syntactic validation happens at parse time. */
				m_scheme = toLowerASCII(scheme);
			}

			/**
			 * @brief Sets the URI domain.
			 * @param uriDomain A reference to an URIDomain.
			 */
			void
			setURIDomain (const URIDomain & uriDomain) noexcept
			{
				m_uriDomain = uriDomain;
			}

			/**
			 * @brief Sets the path of the URI from its DECODED form.
			 * @note The encoded path becomes its percent-encoding (PercentEncoding::Component::Path).
			 * @param path A reference to a filesystem path (UTF-8).
			 */
			void setPath (const std::filesystem::path & path) noexcept;

			/**
			 * @brief Sets the query of the URI from its decoded variables.
			 * @note The encoded query becomes the variables' encoding, in the map's (sorted) order.
			 * @param query A reference to a query.
			 */
			void setQuery (const Query & query) noexcept;

			/**
			 * @brief Sets the fragment of the URI.
			 * @param fragment A reference to a string.
			 */
			void
			setFragment (const std::string & fragment) noexcept
			{
				m_fragment = fragment;
			}

			/**
			 * @brief Returns the scheme part.
			 * @return const std::string &
			 */
			[[nodiscard]]
			const std::string &
			scheme () const noexcept
			{
				return m_scheme;
			}

			/**
			 * @brief Returns the domain part.
			 * @return const URIDomain &
			 */
			[[nodiscard]]
			const URIDomain &
			uriDomain () const noexcept
			{
				return m_uriDomain;
			}

			/**
			 * @brief Returns the path part, DECODED (inspection: a "%2F" is a '/' here).
			 * @return const std::filesystem::path &
			 */
			[[nodiscard]]
			const std::filesystem::path &
			path () const noexcept
			{
				return m_path;
			}

			/**
			 * @brief Returns the query part, DECODED (inspection: sorted, one value per key).
			 * @return const Query &
			 */
			[[nodiscard]]
			const Query &
			query () const noexcept
			{
				return m_query;
			}

			/**
			 * @brief Returns the path as it goes on the wire (the parsed bytes, or the encoding of setPath()).
			 * @return const std::string &
			 */
			[[nodiscard]]
			const std::string &
			encodedPath () const noexcept
			{
				return m_encodedPath;
			}

			/**
			 * @brief Returns the query as it goes on the wire, without its '?' (the parsed bytes, or the encoding of
			 * setQuery()).
			 * @return const std::string &
			 */
			[[nodiscard]]
			const std::string &
			encodedQuery () const noexcept
			{
				return m_encodedQuery;
			}

			/**
			 * @brief Returns the fragment part.
			 * @return const std::string &
			 */
			[[nodiscard]]
			const std::string &
			fragment () const noexcept
			{
				return m_fragment;
			}

			/**
			 * @brief Returns the resource part, the HTTP request target: the encoded path, then '?' and the encoded query.
			 * @return std::string
			 */
			[[nodiscard]]
			std::string resource () const noexcept;

			/**
			 * @brief Returns the URI for a trace: scheme, host, port and path only.
			 * @note The userinfo (a password), the query (a presigned URL carries its credentials there, e.g. an S3
			 * X-Amz-Security-Token) and the fragment are left out; a query is replaced by "?<redacted>" so the trace
			 * still says there was one. Use it in every log line; keep operator<< for what goes on the wire.
			 * @return std::string
			 */
			[[nodiscard]]
			std::string redacted () const noexcept;

			/**
			 * @brief Returns whether the URI is empty.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			empty () const noexcept
			{
				if ( m_scheme.empty() )
				{
					return true;
				}

				if ( m_uriDomain.empty() )
				{
					return true;
				}

				return false;
			}

			/**
			 * @brief Returns whether a string parses as a URI with both a scheme and a host — a URL a client can reach.
			 * @note The former Network::URL::isURL() (URL was folded into URI on 2026-10-08). Equivalent to
			 * `!URI{path}.empty()`.
			 * @param path The string to test.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			isURL (const std::string & path) noexcept
			{
				return !URI{path}.empty();
			}

			/**
			 * @brief Resolves a URI reference against a base URI (RFC 3986 §5).
			 * @note This is the algorithm the HTTPS client uses to turn a redirect
			 * Location (which may be absolute, absolute-path, or relative) into an
			 * absolute URI. The base MUST be absolute (have a scheme).
			 * @param base The base URI.
			 * @param reference The reference (absolute or relative) as a raw string.
			 * @return URI The resolved (target) URI.
			 */
			[[nodiscard]]
			static URI resolve (const URI & base, const std::string & reference) noexcept;

			/**
			 * @brief Removes the '.' and '..' segments of a path (RFC 3986 §5.2.4).
			 * @param path The input path.
			 * @return std::string
			 */
			[[nodiscard]]
			static std::string removeDotSegments (const std::string & path) noexcept;

		private:

			/**
			 * @brief Lowercases the ASCII letters of a string (scheme/host normalization).
			 * @param string The input.
			 * @return std::string
			 */
			[[nodiscard]]
			static std::string toLowerASCII (const std::string & string) noexcept;

			/**
			 * @brief Parses a raw URI/relative-reference string (RFC 3986 Appendix B).
			 * @param rawString The raw string.
			 * @return bool True when the string is non-empty (the grammar always matches).
			 */
			bool parseRawString (const std::string & rawString) noexcept;

			/**
			 * @brief Merges a reference path onto this (base) path (RFC 3986 §5.2.3), both ENCODED.
			 * @param referencePath The encoded reference path.
			 * @return std::string
			 */
			[[nodiscard]]
			std::string mergePath (const std::string & referencePath) const noexcept;

			/**
			 * @brief Sets the path from its encoded bytes: illegal bytes escaped, dot-segments removed from an absolute
			 * path, and the decoded view derived from them.
			 * @param encodedPath The encoded path as parsed.
			 */
			void assignEncodedPath (const std::string & encodedPath) noexcept;

			/**
			 * @brief Sets the query from its encoded bytes (without the '?'): illegal bytes escaped, and the decoded view
			 * derived from them.
			 * @param encodedQuery The encoded query as parsed.
			 */
			void assignEncodedQuery (const std::string & encodedQuery) noexcept;

			/**
			 * @brief STL streams printable object.
			 * @param out A reference to the stream output.
			 * @param obj A reference to the object to print.
			 * @return std::ostream &
			 */
			friend std::ostream & operator<< (std::ostream & out, const URI & obj);

			std::string m_scheme;
			URIDomain m_uriDomain;
			std::filesystem::path m_path;
			std::string m_encodedPath;
			Query m_query;
			std::string m_encodedQuery;
			std::string m_fragment;
	};

	/**
	 * @brief Stringifies the object.
	 * @param obj A reference to the object to print.
	 * @return std::string
	 */
	inline
	std::string
	to_string (const URI & obj) noexcept
	{
		std::stringstream output;

		output << obj;

		return output.str();
	}
}
