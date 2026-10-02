/*
 * src/Randomizer.hpp
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
#include <array>
#include <cstdint>
#include <random>
#include <vector>

/* Local inclusions for usages. */
#include "PortableRandom.hpp"

namespace EmEn::Base
{
	/**
	 * @brief The randomizer class.
	 * @tparam number_t The type of integral number. Default int32_t.
	 * @tparam generator_t The type of generator. Default std::mt19937.
	 */
	template< typename number_t = int32_t, typename generator_t = std::mt19937 >
	requires (std::is_arithmetic_v< number_t > && !std::is_same_v< number_t, bool > && PortableRandom::FullRangeGenerator< generator_t >)
	class Randomizer final
	{
		public:

			/**
			 * @brief Constructs a randomizer fully random.
			 */
			Randomizer ()
			   : m_generator(m_device())
			{

			}

			/**
			 * @brief Constructs a randomizer with a seed to reproduce results.
			 * @note The same seed gives the same numbers on every platform (PortableRandom.hpp: std's distributions
			 * did not, 2026-10-02).
			 * @param seed A fixed value.
			 */
			explicit Randomizer (typename generator_t::result_type seed) noexcept
				: m_generator(seed)
			{

			}

			/**
			 * @brief Returns a random number between a specified range.
			 * @param min The minimum number (included).
			 * @param max The maximum number (included for an integer, excluded for a real).
			 * @return number_t
			 */
			[[nodiscard]]
			number_t
			value (number_t min, number_t max) noexcept
			{
				if ( max < min )
				{
					std::swap(min, max);
				}

				return this->draw(min, max);
			}

			/**
			 * @brief Returns an STL vector of random number between a specified range.
			 * @param count The number of desired numbers.
			 * @param min The minimum number.
			 * @param max The maximum number.
			 * @return std::vector< number_t >
			 */
			[[nodiscard]]
			std::vector< number_t >
			vector (size_t count, number_t min, number_t max)
			{
				std::vector< number_t > range(count);

				if ( max < min )
				{
					std::swap(min, max);
				}

				for ( auto & value : range )
				{
					value = this->draw(min, max);
				}

				return range;
			}

			/**
			 * @brief Returns an STL array of random number between a specified range.
			 * @tparam count_t The number of desired numbers.
			 * @param min The minimum number.
			 * @param max The maximum number.
			 * @return number_t
			 */
			template< size_t count_t >
			[[nodiscard]]
			std::array< number_t, count_t >
			array (number_t min, number_t max) noexcept
			{
				std::array< number_t, count_t > range{};

				if ( max < min )
				{
					std::swap(min, max);
				}

				for ( auto & value : range )
				{
					value = this->draw(min, max);
				}

				return range;
			}

		private:

			/**
			 * @brief One draw in [min, max] (an integer) or [min, max) (a real), the same on every platform.
			 * @param min The minimum number.
			 * @param max The maximum number.
			 * @return number_t
			 */
			[[nodiscard]]
			number_t
			draw (number_t min, number_t max) noexcept
			{
				if constexpr ( std::is_integral_v< number_t > )
				{
					return PortableRandom::uniformInteger< number_t >(m_generator, min, max);
				}
				else
				{
					return PortableRandom::uniformReal< number_t >(m_generator, min, max);
				}
			}

			std::random_device m_device;
			generator_t m_generator;
	};
}
