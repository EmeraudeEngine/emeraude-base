/*
 * src/Testing/test_Time.cpp
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

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Time/Time.hpp"
#include "Time/Elapsed/CPUTime.hpp"

namespace EmEn::Base::Time
{
	namespace
	{
		/**
		 * @brief Deterministic CPUTime: scripts the CPU-clock source so the
		 * nanosecond duration / unit conversion can be asserted with no dependency
		 * on real timing (zero flakiness).
		 */
		class ScriptedCPUTime final : public Elapsed::CPUTime
		{
			public:

				explicit ScriptedCPUTime (std::vector< uint64_t > values) noexcept
					: m_values{std::move(values)}
				{

				}

			protected:

				[[nodiscard]]
				uint64_t
				currentCPUNanoseconds () const noexcept override
				{
					const std::size_t index = m_index < m_values.size() ? m_index : m_values.size() - 1;
					++m_index;

					return m_values[index];
				}

			private:

				std::vector< uint64_t > m_values;
				mutable std::size_t m_index{0};
		};
	}

	TEST(TimeElapsedCPUTime, deterministicNsConversion)
	{
		/* start() reads 5.0 s, stop() reads 5.1 s → delta = 100 ms.
		 * Guards the historical bug where raw clock() ticks were stored as nanoseconds. */
		ScriptedCPUTime cpuTime{{5'000'000'000ULL, 5'100'000'000ULL}};

		cpuTime.start();
		cpuTime.stop();

		EXPECT_EQ(cpuTime.duration(), 100'000'000ULL);
		EXPECT_DOUBLE_EQ(cpuTime.microseconds(), 100'000.0);
		EXPECT_DOUBLE_EQ(cpuTime.milliseconds(), 100.0);
		EXPECT_DOUBLE_EQ(cpuTime.seconds(), 0.1);
	}

	TEST(TimeElapsedCPUTime, realClockIsMonotonic)
	{
		const auto first = processCPUTimeNanoseconds();

		volatile double sink = 0.0;
		for ( int i = 0; i < 2'000'000; ++i )
		{
			sink += static_cast< double >(i) * 0.5;
		}
		(void)sink;

		const auto second = processCPUTimeNanoseconds();

		/* Process CPU time never decreases — safe lower bound, not timing-sensitive. */
		EXPECT_GE(second, first);
	}
}