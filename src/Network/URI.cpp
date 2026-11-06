/*
 * src/Network/URI.cpp
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

#include "URI.hpp"

/* STL inclusions. */
#include <cctype>
#include <sstream>
#include <string>

/* Local inclusions. */
#include "IO/IO.hpp"
#include "Logging/Logging.hpp"
#include "Network/Query.hpp"
#include "Network/URIDomain.hpp"
#include "PercentEncoding.hpp"

namespace EmEn::Base::Network
{
	namespace
	{
		constexpr auto Tag{"Network::URI"};

		/**
		 * @brief Returns whether a string is a syntactically valid scheme (RFC 3986 §3.1).
		 * @param scheme The candidate scheme.
		 * @return bool
		 */
		bool
		isValidScheme (const std::string & scheme) noexcept
		{
			if ( scheme.empty() || std::isalpha(static_cast< unsigned char >(scheme.front())) == 0 )
			{
				return false;
			}

			for ( const auto character : scheme )
			{
				const auto byte = static_cast< unsigned char >(character);

				if ( std::isalnum(byte) == 0 && character != '+' && character != '-' && character != '.' )
				{
					return false;
				}
			}

			return true;
		}
	}

	std::string
	URI::toLowerASCII (const std::string & string) noexcept
	{
		std::string output{string};

		for ( auto & character : output )
		{
			character = static_cast< char >(std::tolower(static_cast< unsigned char >(character)));
		}

		return output;
	}

	URI::URI (const std::string & rawURI) noexcept
	{
		if ( !this->parseRawString(rawURI) )
		{
			Logging::warning(Tag, "URI(), empty string, the URI stays empty.");
		}
	}

	URI &
	URI::operator= (const std::string & rawURI) noexcept
	{
		if ( !this->parseRawString(rawURI) )
		{
			Logging::warning(Tag, "URI::operator=(), empty string, the URI keeps its previous value.");
		}

		return *this;
	}

	bool
	URI::parseRawString (const std::string & rawString) noexcept
	{
		if ( rawString.empty() )
		{
			return false;
		}

		/* RFC 3986 Appendix B decomposition, done by hand (no std::regex — it trips a
		 * libstdc++ -Wmaybe-uninitialized false positive under the sanitizer build,
		 * and is heavyweight for a fixed grammar). Order: fragment, query, scheme,
		 * authority, path — each delimiter splits off its component. */
		std::string remainder{rawString};

		/* Fragment: everything after the first '#'. */
		std::string fragment;
		bool hasFragment = false;

		if ( const auto hash = remainder.find('#'); hash != std::string::npos )
		{
			hasFragment = true;
			fragment = remainder.substr(hash + 1);
			remainder.erase(hash);
		}

		/* Query: everything after the first '?' (already fragment-free). */
		std::string query;
		bool hasQuery = false;

		if ( const auto question = remainder.find('?'); question != std::string::npos )
		{
			hasQuery = true;
			query = remainder.substr(question + 1);
			remainder.erase(question);
		}

		/* Scheme: a ':' that comes before any '/', and before the remainder start. */
		std::string scheme;

		if ( const auto colon = remainder.find(':'); colon != std::string::npos )
		{
			const auto slash = remainder.find('/');

			if ( slash == std::string::npos || colon < slash )
			{
				scheme = remainder.substr(0, colon);
				remainder.erase(0, colon + 1);
			}
		}

		/* Authority: present iff the remainder starts with "//"; it runs up to the
		 * next '/' (path), the rest being the path. */
		std::string authority;
		bool hasAuthority = false;

		if ( remainder.starts_with("//") )
		{
			hasAuthority = true;
			remainder.erase(0, 2);

			const auto pathStart = remainder.find('/');

			if ( pathStart == std::string::npos )
			{
				authority = remainder;
				remainder.clear();
			}
			else
			{
				authority = remainder.substr(0, pathStart);
				remainder.erase(0, pathStart);
			}
		}

		/* Whatever remains is the path. */
		std::string path{remainder};

		/* Scheme: valid → normalized; captured-but-invalid → not a scheme, fold it
		 * back into the path (a relative reference such as "2:3" has no scheme). */
		if ( !scheme.empty() && isValidScheme(scheme) )
		{
			m_scheme = toLowerASCII(scheme);
		}
		else
		{
			m_scheme.clear();

			if ( !scheme.empty() )
			{
				path = scheme + ':' + path;
			}
		}

		/* Authority (host normalized to lowercase inside URIDomain). */
		if ( hasAuthority )
		{
			m_uriDomain = URIDomain{authority};
		}
		else
		{
			m_uriDomain = URIDomain{};
		}

		/* Path and query: the encoded bytes kept for the wire, the decoded views derived from them. */
		this->assignEncodedPath(path);

		if ( hasQuery )
		{
			this->assignEncodedQuery(query);
		}
		else
		{
			this->assignEncodedQuery({});
		}

		if ( hasFragment )
		{
			m_fragment = PercentEncoding::decode(fragment);
		}
		else
		{
			m_fragment.clear();
		}

		return true;
	}

	std::string
	URI::removeDotSegments (const std::string & path) noexcept
	{
		/* RFC 3986 §5.2.4 — the canonical input/output-buffer algorithm. */
		std::string input{path};
		std::string output;

		while ( !input.empty() )
		{
			if ( input.starts_with("../") )
			{
				input.erase(0, 3);
			}
			else if ( input.starts_with("./") )
			{
				input.erase(0, 2);
			}
			else if ( input.starts_with("/./") )
			{
				input.erase(0, 2);
			}
			else if ( input == "/." )
			{
				input = "/";
			}
			else if ( input.starts_with("/../") )
			{
				input.erase(0, 3);

				const auto lastSlash = output.find_last_of('/');

				output.erase(lastSlash == std::string::npos ? 0 : lastSlash);
			}
			else if ( input == "/.." )
			{
				input = "/";

				const auto lastSlash = output.find_last_of('/');

				output.erase(lastSlash == std::string::npos ? 0 : lastSlash);
			}
			else if ( input == "." || input == ".." )
			{
				input.clear();
			}
			else
			{
				/* Move the first path segment (including a leading '/') to output. */
				const auto nextSlash = input.find('/', input.front() == '/' ? 1 : 0);

				if ( nextSlash == std::string::npos )
				{
					output += input;
					input.clear();
				}
				else
				{
					output += input.substr(0, nextSlash);
					input.erase(0, nextSlash);
				}
			}
		}

		return output;
	}

	void
	URI::assignEncodedPath (const std::string & encodedPath) noexcept
	{
		/* NOTE: dot-segments are removed on the ENCODED path: a "%2F" stays inside its segment, as the server sees it. */
		m_encodedPath = PercentEncoding::encodePreservingEscapes(encodedPath, PercentEncoding::Component::Path);

		if ( !m_encodedPath.empty() && m_encodedPath.front() == '/' )
		{
			m_encodedPath = removeDotSegments(m_encodedPath);
		}

		/* NOTE: UTF-8 (the decoded bytes): IO::u8path(), never the narrow path conversion (the ANSI code page on Windows);
		 * read back through IO::toGenericU8String(). */
		m_path = IO::u8path(PercentEncoding::decode(m_encodedPath));
	}

	void
	URI::assignEncodedQuery (const std::string & encodedQuery) noexcept
	{
		m_encodedQuery = PercentEncoding::encodePreservingEscapes(encodedQuery, PercentEncoding::Component::Query);
		m_query = Query::fromString(m_encodedQuery);
	}

	void
	URI::setPath (const std::filesystem::path & path) noexcept
	{
		m_path = path;
		m_encodedPath = PercentEncoding::encode(IO::toGenericU8String(m_path), PercentEncoding::Component::Path);
	}

	void
	URI::setQuery (const Query & query) noexcept
	{
		std::stringstream encoded;
		encoded << query;

		m_query = query;
		m_encodedQuery = encoded.str();
	}

	std::string
	URI::mergePath (const std::string & referencePath) const noexcept
	{
		/* RFC 3986 §5.2.3. */
		if ( !m_uriDomain.empty() && m_encodedPath.empty() )
		{
			return '/' + referencePath;
		}

		const auto lastSlash = m_encodedPath.find_last_of('/');

		if ( lastSlash == std::string::npos )
		{
			return referencePath;
		}

		return m_encodedPath.substr(0, lastSlash + 1) + referencePath;
	}

	URI
	URI::resolve (const URI & base, const std::string & reference) noexcept
	{
		URI ref;
		ref.parseRawString(reference);

		URI target;

		/* RFC 3986 §5.2.2 — Transform References, on the ENCODED path and query (the decoded views follow). */
		if ( !ref.m_scheme.empty() )
		{
			target.m_scheme = ref.m_scheme;
			target.m_uriDomain = ref.m_uriDomain;
			target.assignEncodedPath(removeDotSegments(ref.m_encodedPath));
			target.assignEncodedQuery(ref.m_encodedQuery);
		}
		else
		{
			target.m_scheme = base.m_scheme;

			if ( !ref.m_uriDomain.empty() )
			{
				target.m_uriDomain = ref.m_uriDomain;
				target.assignEncodedPath(removeDotSegments(ref.m_encodedPath));
				target.assignEncodedQuery(ref.m_encodedQuery);
			}
			else
			{
				target.m_uriDomain = base.m_uriDomain;

				const auto & referencePath = ref.m_encodedPath;

				if ( referencePath.empty() )
				{
					target.assignEncodedPath(base.m_encodedPath);
					target.assignEncodedQuery(ref.m_encodedQuery.empty() ? base.m_encodedQuery : ref.m_encodedQuery);
				}
				else
				{
					if ( referencePath.front() == '/' )
					{
						target.assignEncodedPath(removeDotSegments(referencePath));
					}
					else
					{
						target.assignEncodedPath(removeDotSegments(base.mergePath(referencePath)));
					}

					target.assignEncodedQuery(ref.m_encodedQuery);
				}
			}
		}

		target.m_fragment = ref.m_fragment;

		return target;
	}

	std::string
	URI::resource () const noexcept
	{
		std::stringstream string;

		if ( m_encodedPath.empty() )
		{
			string << '/';
		}
		else
		{
			string << m_encodedPath;
		}

		if ( !m_encodedQuery.empty() )
		{
			string << '?' << m_encodedQuery;
		}

		return string.str();
	}

	std::string
	URI::redacted () const noexcept
	{
		std::stringstream string;

		if ( !m_scheme.empty() )
		{
			string << m_scheme << ':';
		}

		if ( !m_uriDomain.empty() )
		{
			string << "//" << m_uriDomain.host();
		}

		string << m_encodedPath;

		if ( !m_encodedQuery.empty() )
		{
			string << "?<redacted>";
		}

		return string.str();
	}

	std::ostream &
	operator<< (std::ostream & out, const URI & obj)
	{
		if ( !obj.m_scheme.empty() )
		{
			out << obj.m_scheme << ':';
		}

		if ( !obj.m_uriDomain.empty() )
		{
			out << "//" << obj.m_uriDomain;
		}

		out << obj.m_encodedPath;

		if ( !obj.m_encodedQuery.empty() )
		{
			out << '?' << obj.m_encodedQuery;
		}

		if ( !obj.m_fragment.empty() )
		{
			out << '#' << PercentEncoding::encode(obj.m_fragment, PercentEncoding::Component::Fragment);
		}

		return out;
	}
}
