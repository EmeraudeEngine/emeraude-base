/*
 * src/Logging/Severity.hpp
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
#include <cstdint>
#include <string>

namespace EmEn::Base
{
	/**
	 * @brief The message severity enumeration.
	 * @note Shared logging primitive of the cascade: the engine re-exports it as
	 * EmEn::Severity and its Tracer sink uses it. Enumerator order is part of the
	 * contract — do not reorder.
	 */
	enum class Severity : uint8_t
	{
		Debug,
		Success,
		Info,
		Warning,
		Error,
		Fatal
	};

	static constexpr auto DebugString{"Debug"};
	static constexpr auto SuccessString{"Success"};
	static constexpr auto InfoString{"Info"};
	static constexpr auto WarningString{"Warning"};
	static constexpr auto ErrorString{"Error"};
	static constexpr auto FatalString{"Fatal"};

	/**
	 * @brief Returns a C-String version of the enum value.
	 * @param value The enum value.
	 * @return const char *
	 */
	[[nodiscard]]
	inline
	const char *
	to_cstring (Severity value) noexcept
	{
		switch ( value )
		{
			case Severity::Debug :
				return DebugString;

			case Severity::Info :
				return InfoString;

			case Severity::Success :
				return SuccessString;

			case Severity::Warning :
				return WarningString;

			case Severity::Error :
				return ErrorString;

			case Severity::Fatal :
				return FatalString;

			default:
				return "Unknown";
		}
	}

	/**
	 * @brief Returns a string version of the enum value.
	 * @param value The enum value.
	 * @return std::string
	 */
	[[nodiscard]]
	inline
	std::string
	to_string (Severity value)
	{
		return {to_cstring(value)};
	}
}
