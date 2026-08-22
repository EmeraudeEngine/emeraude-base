/*
 * src/Locale.hpp
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

namespace EmEn::Base::Locale
{
	/**
	 * @brief Forces the C locale's NUMERIC category to "C" and reports whether it had drifted.
	 *
	 * @note **Why this exists.** `LC_NUMERIC` decides the decimal separator used by the whole C
	 * numeric I/O family — `printf("%f")`, `strtod()`, `atof()`, `std::to_string()`, `std::stof()`.
	 * A third-party library that calls `setlocale(LC_ALL, "")` switches it to the user's locale,
	 * and in `fr_BE`, `fr_FR` or `de_DE` that separator is a **COMMA**. Every float the process
	 * then writes to a scene file, a settings entry, a cache key or a generated shader silently
	 * changes format, and `strtod("1.5")` stops parsing at the dot — a corruption that no test
	 * catches on an `en_US` or `C` machine. GTK's `gtk_init()` does exactly that call, and the
	 * CEF/GTK stack pulls it into the process (measured 2026-08-22: the process ran with
	 * `LC_CTYPE=fr_BE.UTF-8` while `LC_NUMERIC` was reset to `C` **by Chromium itself** — correct,
	 * but by luck rather than by contract).
	 *
	 * @note Only `LC_NUMERIC` is forced: `LC_CTYPE`, `LC_TIME`, `LC_MESSAGES` and the rest stay on
	 * the user's locale, so text, dates and messages remain localised.
	 *
	 * @note C++ streams are NOT concerned either way — an `std::ostream` carries a copy of the
	 * global `std::locale` (the classic one unless `std::locale::global()` is called), never the C
	 * locale. This function is about the C family only, which is what the cascade uses for float
	 * serialisation.
	 *
	 * @note Call it **early**, and again **after initialising any library known to touch the
	 * locale** — a single call at startup cannot protect against a `setlocale()` performed later
	 * (CEF is initialised after the engine, and Chromium loads GTK lazily). The structural answer,
	 * for new code, is to not depend on the locale at all: `std::to_chars()` / `std::from_chars()`
	 * are locale-independent by specification.
	 *
	 * @return bool True when the category had drifted away from "C" and was restored, which is
	 * worth a warning naming whatever ran in between. False when it was already "C" (the normal
	 * case) or when the platform refused the change.
	 */
	[[nodiscard]]
	bool enforceNumericC () noexcept;
}
