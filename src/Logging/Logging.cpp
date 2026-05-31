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
			std::cerr << '[' << (tag != nullptr ? tag : "") << "] (" << to_cstring(severity) << ") " << message << '\n';
		}

		/* Function-local statics (no globally-accessible mutable state, no static
		 * init-order issues): the sink and its mutex are created on first use. */
		std::shared_mutex &
		sinkMutex () noexcept
		{
			static std::shared_mutex mutex;

			return mutex;
		}

		Sink &
		activeSink () noexcept
		{
			static Sink sink;

			return sink;
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