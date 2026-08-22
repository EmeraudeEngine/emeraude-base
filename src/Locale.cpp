/*
 * src/Locale.cpp
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

#include "Locale.hpp"

/* STL inclusions. */
#include <clocale>
#include <cstring>

namespace EmEn::Base::Locale
{
	bool
	enforceNumericC () noexcept
	{
		/* A null return would mean the category cannot be queried at all; treat it as drifted so the
		 * caller reports something rather than trusting an unknown state. */
		const auto * current = std::setlocale(LC_NUMERIC, nullptr);
		const auto drifted = current == nullptr || std::strcmp(current, "C") != 0;

		if ( !drifted )
		{
			return false;
		}

		/* "C" is guaranteed to exist by the standard; a null here means the platform refused, and
		 * there is nothing further this function can do about it. */
		return std::setlocale(LC_NUMERIC, "C") != nullptr;
	}
}
