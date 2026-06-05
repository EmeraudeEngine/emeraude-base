/*
 * src/Logging/Logging.cpp
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

#include "Logging.hpp"

/* STL inclusions. */
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <utility>

namespace EmEn::Base::Logging
{
	namespace
	{
		void
		defaultSink (Severity severity, const char * tag, std::string_view message) noexcept
		{
			/* Warning/Error/Fatal -> stderr; Debug/Info/Success -> stdout. */
			std::ostream & stream = (severity == Severity::Warning || severity == Severity::Error || severity == Severity::Fatal) ? std::cerr : std::cout;

			stream << '[' << (tag != nullptr ? tag : "") << "] (" << to_cstring(severity) << ") " << message << '\n';
		}

		/* Function-local statics created on first use (no static *init*-order issues).
		 *
		 * They are intentionally "immortal": allocated once on first use and never freed.
		 * This dodges the static *destruction*-order fiasco — a global Tracer held by a
		 * unique_ptr is destroyed at exit() and its destructor calls setSink(nullptr).
		 * These statics are constructed lazily, i.e. *after* the Tracer was registered for
		 * destruction, so reverse-order teardown would otherwise destroy the mutex before
		 * ~Tracer() runs; locking the dead mutex then throws out of a noexcept function and
		 * calls std::terminate(). Leaking them (one mutex + one std::function, reclaimed by
		 * the OS at process exit) keeps the references valid for the whole shutdown. */
		std::shared_mutex &
		sinkMutex () noexcept
		{
			// NOLINTNEXTLINE(cppcoreguidelines-owning-memory,bugprone-unhandled-exception-at-new): immortal singleton, leaked on purpose.
			static auto * mutex = new std::shared_mutex;

			return *mutex;
		}

		Sink &
		activeSink () noexcept
		{
			// NOLINTNEXTLINE(cppcoreguidelines-owning-memory,bugprone-unhandled-exception-at-new): immortal singleton, leaked on purpose.
			static auto * sink = new Sink;

			return *sink;
		}
	}

	void
	setSink (Sink sink) noexcept
	{
		const std::unique_lock lock{sinkMutex()};

		activeSink() = std::move(sink);
	}

	void
	log (Severity severity, const char * tag, std::string_view message) noexcept
	{
		/* Copy the sink under a shared lock, then invoke it outside the lock: the callback
		 * never runs while the mutex is held (no deadlock on re-entrant setSink, no contention). */
		Sink sink;

		{
			const std::shared_lock lock{sinkMutex()};

			sink = activeSink();
		}

		if ( sink )
		{
			sink(severity, tag, message);
		}
		else
		{
			defaultSink(severity, tag, message);
		}
	}
}