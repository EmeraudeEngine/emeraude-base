/*
 * src/Logging/Logging.hpp
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
#include <functional>
#include <string_view>

/* Local inclusions. */
#include "Severity.hpp"

namespace EmEn::Base::Logging
{
	/**
	 * @brief The diagnostic sink signature.
	 * @note Deliberately identical to the engine Tracer::Sink so a consumer can wire
	 * base diagnostics straight into the Tracer with no adapter.
	 */
	using Sink = std::function< void (Severity severity, const char * tag, std::string_view message) >;

	/**
	 * @brief Installs the diagnostic sink, replacing the default one.
	 * @note Thread-safe. Pass an empty sink (e.g. nullptr) to restore the default cerr sink.
	 * The engine registers a sink here that forwards into its Tracer.
	 * @param sink The sink receiving every base diagnostic, or empty to reset.
	 * @return void
	 */
	void setSink (Sink sink) noexcept;

	/**
	 * @brief Emits a diagnostic message through the current sink.
	 * @note Thread-safe. With no installed sink, routes by severity to std::cout
	 * (Debug/Info/Success) or std::cerr (Warning/Error/Fatal).
	 * @param severity The message severity.
	 * @param tag A short category tag (must outlive the call; usually a string literal).
	 * @param message The message body.
	 * @return void
	 */
	void log (Severity severity, const char * tag, std::string_view message) noexcept;

	/** @brief Emits a Debug-severity diagnostic. @param tag The category tag. @param message The message body. @return void */
	inline void debug (const char * tag, std::string_view message) noexcept { log(Severity::Debug, tag, message); }

	/** @brief Emits an Info-severity diagnostic. @param tag The category tag. @param message The message body. @return void */
	inline void info (const char * tag, std::string_view message) noexcept { log(Severity::Info, tag, message); }

	/** @brief Emits a Success-severity diagnostic. @param tag The category tag. @param message The message body. @return void */
	inline void success (const char * tag, std::string_view message) noexcept { log(Severity::Success, tag, message); }

	/** @brief Emits a Warning-severity diagnostic. @param tag The category tag. @param message The message body. @return void */
	inline void warning (const char * tag, std::string_view message) noexcept { log(Severity::Warning, tag, message); }

	/** @brief Emits an Error-severity diagnostic. @param tag The category tag. @param message The message body. @return void */
	inline void error (const char * tag, std::string_view message) noexcept { log(Severity::Error, tag, message); }

	/** @brief Emits a Fatal-severity diagnostic. @param tag The category tag. @param message The message body. @return void */
	inline void fatal (const char * tag, std::string_view message) noexcept { log(Severity::Fatal, tag, message); }
}