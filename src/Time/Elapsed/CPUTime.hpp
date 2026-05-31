/*
 * src/Time/Elapsed/CPUTime.hpp
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

/* Local inclusions for inheritances. */
#include "Abstract.hpp"

/* Local inclusions for usages. */
#include "../Time.hpp"

namespace EmEn::Base::Time::Elapsed
{
	/**
	 * @brief Gets the duration in CPU time between two points.
	 * @note Internal precision is nanoseconds, sourced from the real process CPU clock
	 * (POSIX clock_gettime(CLOCK_PROCESS_CPUTIME_ID) / Windows GetProcessTimes).
	 * @extends EmEn::Base::Time::Elapsed::Abstract
	 */
	class CPUTime : public Abstract
	{
		public:

			/**
			 * @brief Constructs an elapsed CPU time structure.
			 */
			CPUTime () noexcept = default;

			/** @copydoc EmEn::Base::Time::Elapsed::Abstract::start() */
			void
			start () noexcept override
			{
				m_startTime = this->currentCPUNanoseconds();
			}

			/** @copydoc EmEn::Base::Time::Elapsed::Abstract::stop() */
			void
			stop () noexcept override
			{
				this->setDuration(this->currentCPUNanoseconds() - m_startTime);
			}

		protected:

			/**
			 * @brief Returns the current process CPU time in nanoseconds.
			 * @note Virtual seam: overridable so a test can inject a deterministic clock.
			 * @return uint64_t
			 */
			[[nodiscard]]
			virtual
			uint64_t
			currentCPUNanoseconds () const noexcept
			{
				return processCPUTimeNanoseconds();
			}

		private:

			uint64_t m_startTime{0};
	};
}
