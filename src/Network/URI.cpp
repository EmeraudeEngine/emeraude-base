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
#include "PercentEncoding.hpp"

namespace EmEn::Base::Network
{
	namespace
	{
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

		/* Path: percent-decode, then remove dot-segments for an absolute path. */
		auto decodedPath = PercentEncoding::decode(path);

		if ( !decodedPath.empty() && decodedPath.front() == '/' )
		{
			decodedPath = removeDotSegments(decodedPath);
		}

		m_path = decodedPath;

		/* Query and fragment (both percent-decoded on the way in). */
		if ( hasQuery )
		{
			m_query = Query::fromString(query);
		}
		else
		{
			m_query = Query{};
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

	std::string
	URI::mergePath (const std::string & referencePath) const noexcept
	{
		/* RFC 3986 §5.2.3. */
		if ( !m_uriDomain.empty() && m_path.empty() )
		{
			return '/' + referencePath;
		}

		const auto basePath = m_path.generic_string();
		const auto lastSlash = basePath.find_last_of('/');

		if ( lastSlash == std::string::npos )
		{
			return referencePath;
		}

		return basePath.substr(0, lastSlash + 1) + referencePath;
	}

	URI
	URI::resolve (const URI & base, const std::string & reference) noexcept
	{
		URI ref;
		ref.parseRawString(reference);

		URI target;

		/* RFC 3986 §5.2.2 — Transform References. */
		if ( !ref.m_scheme.empty() )
		{
			target.m_scheme = ref.m_scheme;
			target.m_uriDomain = ref.m_uriDomain;
			target.m_path = removeDotSegments(ref.m_path.generic_string());
			target.m_query = ref.m_query;
		}
		else
		{
			target.m_scheme = base.m_scheme;

			if ( !ref.m_uriDomain.empty() )
			{
				target.m_uriDomain = ref.m_uriDomain;
				target.m_path = removeDotSegments(ref.m_path.generic_string());
				target.m_query = ref.m_query;
			}
			else
			{
				target.m_uriDomain = base.m_uriDomain;

				const auto referencePath = ref.m_path.generic_string();

				if ( referencePath.empty() )
				{
					target.m_path = base.m_path;
					target.m_query = ref.m_query.empty() ? base.m_query : ref.m_query;
				}
				else
				{
					if ( referencePath.front() == '/' )
					{
						target.m_path = removeDotSegments(referencePath);
					}
					else
					{
						target.m_path = removeDotSegments(base.mergePath(referencePath));
					}

					target.m_query = ref.m_query;
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

		if ( m_path.empty() )
		{
			string << '/';
		}
		else
		{
			string << PercentEncoding::encode(m_path.generic_string(), PercentEncoding::Component::Path);
		}

		if ( !m_query.empty() )
		{
			string << '?' << m_query;
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

		out << PercentEncoding::encode(obj.m_path.generic_string(), PercentEncoding::Component::Path);

		if ( !obj.m_query.empty() )
		{
			out << '?' << obj.m_query;
		}

		if ( !obj.m_fragment.empty() )
		{
			out << '#' << PercentEncoding::encode(obj.m_fragment, PercentEncoding::Component::Fragment);
		}

		return out;
	}
}
