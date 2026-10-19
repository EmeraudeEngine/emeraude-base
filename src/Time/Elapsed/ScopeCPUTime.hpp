/*
 * src/Time/Elapsed/ScopeCPUTime.hpp
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
#include <ctime>

namespace EmEn::Base::Time::Elapsed
{
	/**
	 * @brief Gets the duration in CPU time of a specific scope.
	 * @note The internal precision is set to nanoseconds.
	 */
	class ScopeCPUTime final
	{
		public:

			/**
			 * @brief Construct a scope timer in nanoseconds.
			 * @param duration A reference where the duration will be accumulated in nanoseconds.
			 */
			explicit
			ScopeCPUTime (uint64_t & duration) noexcept
				: m_duration{duration}
			{

			}

			/** @brief Deleted copy constructor. */
			ScopeCPUTime (const ScopeCPUTime &) = delete;

			/** @brief Deleted move constructor. */
			ScopeCPUTime (ScopeCPUTime &&) = delete;

			/** @brief Deleted assignment operator. */
			ScopeCPUTime & operator= (const ScopeCPUTime &) = delete;

			/** @brief Deleted move assignment operator. */
			ScopeCPUTime & operator= (ScopeCPUTime &&) = delete;

			/** @brief Destructor. */
			~ScopeCPUTime ()
			{
				/* NOTE: CLOCKS_PER_SEC is 1000 (Windows) or 1000000 (POSIX); a clock that went back (or failed, -1) counts zero. */
				static_assert(1000000000 % CLOCKS_PER_SEC == 0, "CLOCKS_PER_SEC must divide one second in nanoseconds.");

				const auto ticks = std::clock() - m_start;

				if ( ticks > 0 )
				{
					m_duration += static_cast< uint64_t >(ticks) * static_cast< uint64_t >(1000000000 / CLOCKS_PER_SEC);
				}
			}

		private:

			uint64_t & m_duration;
			std::clock_t m_start{std::clock()};
	};
}
