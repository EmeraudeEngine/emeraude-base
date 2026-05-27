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
#include <ctime>

/* Local inclusions for inheritances. */
#include "Abstract.hpp"

namespace EmEn::Base::Time::Elapsed
{
	/**
	 * @brief Gets the duration in CPU time between two point in milliseconds.
	 * @note The internal precision is set to nanoseconds.
	 * @extends EmEn::Base::Time::Elapsed::Abstract
	 */
	class CPUTime final : public Abstract
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
				m_startTime = std::clock();
			}

			/** @copydoc EmEn::Base::Time::Elapsed::Abstract::stop() */
			void
			stop () noexcept override
			{
				this->setDuration(std::clock() - m_startTime);
			}

		private:

			std::clock_t m_startTime{};
	};
}
