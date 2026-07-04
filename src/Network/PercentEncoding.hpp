/*
 * src/Network/PercentEncoding.hpp
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
#include <string_view>

namespace EmEn::Base::Network::PercentEncoding
{
	/**
	 * @brief The URI component whose reserved-character set drives the encoding.
	 * @note RFC 3986 gives each component a different "allowed unencoded" set; the
	 * unreserved set (ALPHA / DIGIT / '-' / '.' / '_' / '~') is common to all.
	 */
	enum class Component : uint8_t
	{
		Path,       /* pchar + '/' */
		Segment,    /* pchar (no '/': for a single path segment) */
		Query,      /* pchar + '/' + '?' */
		Fragment,   /* pchar + '/' + '?' */
		Userinfo    /* unreserved + sub-delims + ':' */
	};

	/**
	 * @brief Percent-decodes a string (RFC 3986 §2.1).
	 * @note A malformed escape ('%' not followed by two hex digits) is left verbatim
	 * rather than dropped — decoding never fails, never throws.
	 * @param input The possibly-encoded string.
	 * @return std::string The decoded bytes.
	 */
	[[nodiscard]]
	std::string decode (std::string_view input) noexcept;

	/**
	 * @brief Percent-encodes a string for a given URI component (RFC 3986 §2.1).
	 * @note Unreserved characters and the component's extra-allowed set pass through;
	 * everything else becomes %XX with uppercase hex (the §6.2.2.1 canonical form).
	 * @param input The decoded string.
	 * @param component The target component.
	 * @return std::string The encoded string.
	 */
	[[nodiscard]]
	std::string encode (std::string_view input, Component component) noexcept;
}
