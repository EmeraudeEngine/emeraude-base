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
#include <mutex>
#include <sstream>
#include <string>
#include <streambuf>
#include <string_view>
#include <utility>
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
	 * @brief Puts the default Logging sink back in a death-test child, first statement of the EXPECT_DEATH block.
	 * @note EXPECT_DEATH matches the child's stderr, where only the default sink writes: the suite's guard
	 * (src/Testing/main.cpp) would turn the fatal message into a test failure the parent never reads. The child dies,
	 * so nothing has to be restored.
	 */
	inline
	void
	useDefaultSinkInDeathTest () noexcept
	{
		static_cast< void >(Logging::setSink(nullptr));
	}

	/**
	 * @brief Captures the Logging sink, std::cout and std::cerr from its construction to stop() (or its destruction).
	 * @note Declared FIRST in a test body: it is then destroyed LAST, after every object of the body (a server, a pool)
	 * whose threads may still log; its destructor puts everything back even when an ASSERT_* left the body early.
	 * @note The sink comes back as it was (the suite's guard, src/Testing/main.cpp): Logging::setSink() returns it.
	 * @note Messages from other threads (a test server, a worker) are recorded under a mutex. The streams are
	 * process-wide: one capture at a time (gtest runs the bodies one after the other).
	 */
	class DiagnosticsCapture final
	{
		public:

			/** @brief Starts the capture. */
			DiagnosticsCapture () noexcept
				: m_previousOutput{std::cout.rdbuf(m_standardOutput.rdbuf())},
				m_previousError{std::cerr.rdbuf(m_standardError.rdbuf())},
				m_previousSink{Logging::setSink([this] (Severity severity, const char * tag, std::string_view message) {
					const std::scoped_lock lock{m_access};

					m_messages.push_back({tag != nullptr ? tag : "", std::string{message}, severity});
				})}
			{

			}

			/** @brief Deleted copy constructor (the sink captures this object). */
			DiagnosticsCapture (const DiagnosticsCapture &) = delete;

			/** @brief Deleted move constructor (the sink captures this object). */
			DiagnosticsCapture (DiagnosticsCapture &&) = delete;

			/** @brief Deleted copy assignment. */
			DiagnosticsCapture & operator= (const DiagnosticsCapture &) = delete;

			/** @brief Deleted move assignment. */
			DiagnosticsCapture & operator= (DiagnosticsCapture &&) = delete;

			/** @brief Puts the sink and the streams back if stop() did not. */
			~DiagnosticsCapture ()
			{
				this->restore();
			}

			/**
			 * @brief Ends the capture (idempotent) and returns what it recorded.
			 * @return CapturedDiagnostics
			 */
			[[nodiscard]]
			CapturedDiagnostics
			stop () noexcept
			{
				this->restore();

				return this->snapshot();
			}

			/**
			 * @brief Returns what was recorded so far; the capture goes on (until stop() or the destruction).
			 * @note A test body asserts on it at its end: what the body's objects log while they are destroyed afterwards
			 * stays captured (never on the terminal, never a guard failure).
			 * @return CapturedDiagnostics
			 */
			[[nodiscard]]
			CapturedDiagnostics
			snapshot () noexcept
			{
				CapturedDiagnostics captured;

				{
					const std::scoped_lock lock{m_access};

					captured.messages = m_messages;
				}

				captured.standardOutput = m_standardOutput.str();
				captured.standardError = m_standardError.str();

				return captured;
			}

		private:

			/** @brief Puts the previous sink, then the streams, back once. */
			void
			restore () noexcept
			{
				if ( !m_active )
				{
					return;
				}

				m_active = false;

				static_cast< void >(Logging::setSink(std::move(m_previousSink)));
				std::cerr.rdbuf(m_previousError);
				std::cout.rdbuf(m_previousOutput);
			}

			std::mutex m_access;
			std::vector< LoggedMessage > m_messages;
			std::ostringstream m_standardOutput;
			std::ostringstream m_standardError;
			std::streambuf * m_previousOutput;
			std::streambuf * m_previousError;
			Logging::Sink m_previousSink;
			bool m_active{true};
	};

	/**
	 * @brief Runs an action with the Logging sink, std::cout and std::cerr captured; all three are restored before
	 * returning, so the test asserts on the result with the previous sink (the suite's guard) back in place.
	 * @note A raw stream write (std::cerr << ...) in base code bypasses the sink: neither an application nor a fuzz
	 * target can filter it. A test asserts an empty standardError to keep a refusal on the Logging path.
	 * @param action The code under test.
	 * @return CapturedDiagnostics
	 */
	template< typename action_t >
	[[nodiscard]]
	CapturedDiagnostics
	captureDiagnostics (const action_t & action)
	{
		DiagnosticsCapture capture;

		action();

		return capture.stop();
	}
}
