/*
 * src/AnyValue.hpp
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
#include <type_traits>

/* Local inclusions. */
#include "Any.hpp"
#include "Logging/Logging.hpp"

namespace EmEn::Base
{
	/**
	 * @brief Reads an Observer notification's payload (a Base::Any) WITHOUT EVER THROWING.
	 * @note On a mismatch (or an empty payload) it answers nullptr and logs an error naming the caller, and the caller
	 * skips the notification (owner decision 2026-09-30, plan Ave Robustus). No RTTI (Base::Any, 2026-10-07): no type
	 * name in the message, the context names the site.
	 * @tparam value_t The expected payload type.
	 * @param data A reference to the payload.
	 * @param context Who reads it, for the log (a class id, a notification name).
	 * @return const std::remove_cvref_t< value_t > * The value, or nullptr when the payload holds another type.
	 */
	template< typename value_t >
	[[nodiscard]]
	const std::remove_cvref_t< value_t > *
	anyValue (const Any & data, const char * context) noexcept
	{
		const auto * value = data.get< value_t >();

		if ( value == nullptr ) [[unlikely]]
		{
			Logging::error(context, "A notification payload is not of the expected type (or is empty): the notification is ignored.");
		}

		return value;
	}
}
