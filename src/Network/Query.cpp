/*
 * src/Network/Query.cpp
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

#include "Query.hpp"

/* STL inclusions. */
#include <vector>

/* Local inclusions. */
#include "PercentEncoding.hpp"
#include "String.hpp"

namespace EmEn::Base::Network
{
	Query
	Query::fromString (const std::string & string) noexcept
	{
		Query query;

		const auto variables = String::explode(string, '&');

		for ( const auto & variable : variables )
		{
			if ( variable.empty() )
			{
				continue;
			}

			/* Split on the first '=' only: a value may legitimately contain '='.
			 * Keys and values are percent-decoded (stored decoded, re-encoded on output). */
			const auto separator = variable.find('=');

			if ( separator == std::string::npos )
			{
				query.addVariable(PercentEncoding::decode(variable), "");
			}
			else
			{
				query.addVariable(PercentEncoding::decode(variable.substr(0, separator)), PercentEncoding::decode(variable.substr(separator + 1)));
			}
		}

		return query;
	}

	std::ostream &
	operator<< (std::ostream & out, const Query & obj)
	{
		std::vector< std::string > variables;
		variables.reserve(obj.m_variables.size());

		for ( const auto & [key, value] : obj.m_variables )
		{
			auto encoded = PercentEncoding::encode(key, PercentEncoding::Component::Query);

			/* A value-less key is emitted bare (no trailing '='): matches how a
			 * flag-style query ("?y") round-trips. */
			if ( !value.empty() )
			{
				encoded += '=' + PercentEncoding::encode(value, PercentEncoding::Component::Query);
			}

			variables.emplace_back(std::move(encoded));
		}

		return out << String::implode(variables, '&');
	}
}
