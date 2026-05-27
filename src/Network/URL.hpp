/*
 * src/Network/URL.hpp
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

/* Local inclusions for inheritances. */
#include "URI.hpp"

namespace EmEn::Base::Network
{
	/**
	 * @brief Old class to define a URL.
	 * @deprecated Use URI directly instead.
	 * @extends EmEn::Base::Network::URI
	 */
	class URL final : public URI
	{
		public:

			/**
			 * @brief Constructs a default URL.
			 */
			URL () noexcept = default;

			/**
			 * @brief Constructs a URL from a string.
			 * @param path A reference to a string.
			 */
			explicit
			URL (const std::string & path) noexcept
				: URI{path}
			{

			}

			/**
			 * @brief Returns whether the URL is valid.
			 * @return bool.
			 */
			[[nodiscard]]
			bool
			isValid () const noexcept
			{
				if ( this->scheme().empty() )
				{
					return false;
				}

				if ( this->uriDomain().empty() )
				{
					return false;
				}

				return true;
			}

			/**
			 * @brief Returns whether the URL is valid.
			 * @param path A reference to a string.
			 * @return bool.
			 */
			[[nodiscard]]
			static
			bool
			isURL (const std::string & path) noexcept
			{
				return URL{path}.isValid();
			}
	};
}
