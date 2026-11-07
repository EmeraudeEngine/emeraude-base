/*
 * src/Testing/DiagnosticsCapture.hpp
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
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "Logging/Severity.hpp"

namespace EmEn::Base::Testing
{
	/** @brief One message received by the Logging sink. */
	struct LoggedMessage
	{
		std::string tag;
		std::string message;
		Severity severity{Severity::Debug};
	};

	/** @brief What a piece of code sent to the Logging sink, and what it wrote straight to std::cout / std::cerr. */
	struct CapturedDiagnostics
	{
		std::vector< LoggedMessage > messages;
		std::string standardOutput;
		std::string standardError;

		/**
		 * @brief Returns how many messages of a severity were logged on a tag.
		 * @param severity The severity.
		 * @param tag The tag.
		 * @return std::size_t
		 */
		[[nodiscard]]
		std::size_t
		count (Severity severity, std::string_view tag) const noexcept
		{
			return static_cast< std::size_t >(std::count_if(messages.cbegin(), messages.cend(), [severity, tag] (const LoggedMessage & logged) {
				return logged.severity == severity && logged.tag == tag;
			}));
		}
	};

	/**
	 * @brief Runs an action with the Logging sink, std::cout and std::cerr captured; all three are restored before
	 * returning, so the test asserts on the result with the default sink back in place.
	 * @note A raw stream write (std::cerr << ...) in base code bypasses the sink: neither an application nor a fuzz
	 * target can filter it. A test asserts an empty standardError to keep a refusal on the Logging path.
	 * @note Not thread-safe: the sink and the stream buffers are process-wide (gtest runs a test body on one thread).
	 * @param action The code under test.
	 * @return CapturedDiagnostics
	 */
	template< typename action_t >
	[[nodiscard]]
	CapturedDiagnostics
	captureDiagnostics (const action_t & action)
	{
		CapturedDiagnostics captured;
		const std::ostringstream standardOutput;
		const std::ostringstream standardError;
		auto * const previousOutput = std::cout.rdbuf(standardOutput.rdbuf());
		auto * const previousError = std::cerr.rdbuf(standardError.rdbuf());

		Logging::setSink([&captured] (Severity severity, const char * tag, std::string_view message) {
			captured.messages.push_back({tag != nullptr ? tag : "", std::string{message}, severity});
		});

		action();

		Logging::setSink(nullptr); /* reset before returning (the lambda captures a local) */
		std::cerr.rdbuf(previousError);
		std::cout.rdbuf(previousOutput);

		captured.standardOutput = standardOutput.str();
		captured.standardError = standardError.str();

		return captured;
	}
}
