/*
 * src/PortableRandom.hpp
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
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

/**
 * @brief Seeded random numbers that are the SAME on every platform.
 * @note The C++ standard fixes the sequence and the seeding of std::mt19937 / std::mt19937_64, but NOT the algorithms
 * of std::uniform_int_distribution, std::uniform_real_distribution, std::shuffle, nor what std::default_random_engine
 * is: libstdc++, libc++ and MSVC turn one seed into different numbers. A seeded world (a terrain's noise, a forest's
 * groves) generated through them differed on Linux, macOS and Windows (measured 2026-10-02: citadel's terrain by up to
 * 1 m). Everything seeded goes through these functions; std's distributions are only for draws that are random anyway
 * (a std::random_device seed).
 */
namespace EmEn::Base::PortableRandom
{
	/**
	 * @brief A generator whose every output bit is uniform over a full 32 or 64-bit range (std::mt19937,
	 * std::mt19937_64). Its result type may be wider (std::mt19937's is uint_fast32_t: 64 bits on Linux): the range
	 * decides.
	 */
	template< typename generator_t >
	concept FullRangeGenerator = std::uniform_random_bit_generator< generator_t > &&
		generator_t::min() == 0 &&
		(static_cast< uint64_t >(generator_t::max()) == UINT64_C(0xFFFFFFFF) || static_cast< uint64_t >(generator_t::max()) == UINT64_C(0xFFFFFFFFFFFFFFFF));

	/**
	 * @brief Draws 32 uniform bits (a 64-bit generator gives its high half).
	 * @param generator A reference to the generator.
	 * @return uint32_t
	 */
	template< FullRangeGenerator generator_t >
	[[nodiscard]]
	uint32_t
	draw32 (generator_t & generator) noexcept
	{
		if constexpr ( static_cast< uint64_t >(generator_t::max()) == UINT64_C(0xFFFFFFFF) )
		{
			return static_cast< uint32_t >(generator());
		}
		else
		{
			return static_cast< uint32_t >(static_cast< uint64_t >(generator()) >> 32U);
		}
	}

	/**
	 * @brief Draws 64 uniform bits (a 32-bit generator: two draws, the first one high).
	 * @param generator A reference to the generator.
	 * @return uint64_t
	 */
	template< FullRangeGenerator generator_t >
	[[nodiscard]]
	uint64_t
	draw64 (generator_t & generator) noexcept
	{
		if constexpr ( static_cast< uint64_t >(generator_t::max()) == UINT64_C(0xFFFFFFFF) )
		{
			/* Two statements: the order of the draws is fixed. */
			const auto high = static_cast< uint64_t >(generator()) & UINT64_C(0xFFFFFFFF);
			const auto low = static_cast< uint64_t >(generator()) & UINT64_C(0xFFFFFFFF);

			return (high << 32U) | low;
		}
		else
		{
			return static_cast< uint64_t >(generator());
		}
	}

	/**
	 * @brief Returns a uniform integer in [minimum, maximum] (swapped when reversed), unbiased.
	 * @note Rejection of the 2^n mod range lowest draws, then a modulo — the method of OpenBSD's arc4random_uniform()
	 * (D. Lemire, "Fast Random Integer Generation in an Interval", ACM TOMACS 2019, § 2, as reference; no code taken).
	 * A type up to 32 bits takes 32-bit draws, a 64-bit type 64-bit draws.
	 * @tparam integer_t An integral type (not bool).
	 * @param generator A reference to the generator.
	 * @param minimum The lower bound, included.
	 * @param maximum The upper bound, included.
	 * @return integer_t
	 */
	template< std::integral integer_t, FullRangeGenerator generator_t >
	requires (!std::is_same_v< integer_t, bool >)
	[[nodiscard]]
	integer_t
	uniformInteger (generator_t & generator, integer_t minimum, integer_t maximum) noexcept
	{
		using unsigned_t = std::make_unsigned_t< integer_t >;

		if ( maximum < minimum )
		{
			std::swap(minimum, maximum);
		}

		/* Modular: well defined for a signed type too (C++20). */
		const auto span = static_cast< unsigned_t >(static_cast< unsigned_t >(maximum) - static_cast< unsigned_t >(minimum));

		if constexpr ( sizeof(integer_t) <= sizeof(uint32_t) )
		{
			const auto range = static_cast< uint32_t >(static_cast< uint32_t >(span) + 1U);

			/* The full 32-bit range: every draw is valid. */
			if ( range == 0U )
			{
				return static_cast< integer_t >(static_cast< unsigned_t >(static_cast< unsigned_t >(minimum) + static_cast< unsigned_t >(draw32(generator))));
			}

			const auto threshold = static_cast< uint32_t >(static_cast< uint32_t >(0U - range) % range);
			auto draw = draw32(generator);

			while ( draw < threshold )
			{
				draw = draw32(generator);
			}

			return static_cast< integer_t >(static_cast< unsigned_t >(static_cast< unsigned_t >(minimum) + static_cast< unsigned_t >(draw % range)));
		}
		else
		{
			const auto range = static_cast< uint64_t >(static_cast< uint64_t >(span) + 1U);

			if ( range == 0U )
			{
				return static_cast< integer_t >(static_cast< unsigned_t >(static_cast< unsigned_t >(minimum) + static_cast< unsigned_t >(draw64(generator))));
			}

			const auto threshold = static_cast< uint64_t >(static_cast< uint64_t >(0U - range) % range);
			auto draw = draw64(generator);

			while ( draw < threshold )
			{
				draw = draw64(generator);
			}

			return static_cast< integer_t >(static_cast< unsigned_t >(static_cast< unsigned_t >(minimum) + static_cast< unsigned_t >(draw % range)));
		}
	}

	/**
	 * @brief Returns a uniform real number in [minimum, maximum) (swapped when reversed).
	 * @note The unit fraction from the draw's high bits: 24 for a float, 53 for a double from two draws
	 * (M. Matsumoto and T. Nishimura's genrand_res53(), mt19937ar.c, as reference; no code taken), then mapped onto
	 * the bounds; a result rounded up to the maximum is pulled back below it.
	 * @tparam real_t A floating-point type (a long double gets the double's 53 bits).
	 * @param generator A reference to the generator.
	 * @param minimum The lower bound, included.
	 * @param maximum The upper bound, excluded.
	 * @return real_t The minimum when the bounds are equal or not finite.
	 */
	template< std::floating_point real_t, FullRangeGenerator generator_t >
	[[nodiscard]]
	real_t
	uniformReal (generator_t & generator, real_t minimum, real_t maximum) noexcept
	{
		if ( maximum < minimum )
		{
			std::swap(minimum, maximum);
		}

		if ( !std::isfinite(minimum) || !std::isfinite(maximum) || !(maximum > minimum) )
		{
			return minimum;
		}

		real_t unit;

		if constexpr ( std::is_same_v< real_t, float > )
		{
			unit = static_cast< float >(draw32(generator) >> 8U) * 0x1.0p-24F;
		}
		else
		{
			/* Two statements: the order of the draws is fixed. */
			const auto high = static_cast< uint64_t >(draw32(generator) >> 5U);
			const auto low = static_cast< uint64_t >(draw32(generator) >> 6U);

			unit = static_cast< real_t >(static_cast< double >((high << 26U) | low) * 0x1.0p-53);
		}

		/* ⚠️ One operation per statement: clang contracts a * b + c inside ONE expression into a fused multiply-add
		 * by default (-ffp-contract=on, arm64), which rounds once instead of twice — macOS would differ from Linux in
		 * the last bit. Across statements it does not. */
		const auto width = maximum - minimum;
		real_t value;

		if ( std::isfinite(width) )
		{
			const auto offset = width * unit;

			value = minimum + offset;
		}
		else
		{
			/* A width past the type's range (from −max to +max): interpolate instead. */
			const auto fromMinimum = minimum * (static_cast< real_t >(1) - unit);
			const auto fromMaximum = maximum * unit;

			value = fromMinimum + fromMaximum;
		}

		return value < maximum ? value : std::nextafter(maximum, minimum);
	}

	/**
	 * @brief A portable stand-in for std::uniform_real_distribution: the same call shape (distribution(generator)),
	 * uniformReal() inside — a seeded call site changes its type only.
	 * @tparam real_t A floating-point type.
	 */
	template< std::floating_point real_t >
	class UniformReal final
	{
		public:

			/**
			 * @brief Constructs the distribution.
			 * @param minimum The lower bound, included.
			 * @param maximum The upper bound, excluded.
			 */
			constexpr
			UniformReal (real_t minimum, real_t maximum) noexcept
				: m_minimum{minimum},
				m_maximum{maximum}
			{

			}

			/**
			 * @brief Draws a number in [minimum, maximum).
			 * @param generator A reference to the generator.
			 * @return real_t
			 */
			template< FullRangeGenerator generator_t >
			[[nodiscard]]
			real_t
			operator() (generator_t & generator) const noexcept
			{
				return uniformReal< real_t >(generator, m_minimum, m_maximum);
			}

		private:

			real_t m_minimum;
			real_t m_maximum;
	};

	/**
	 * @brief A portable stand-in for std::uniform_int_distribution (see UniformReal).
	 * @tparam integer_t An integral type (not bool).
	 */
	template< std::integral integer_t >
	requires (!std::is_same_v< integer_t, bool >)
	class UniformInteger final
	{
		public:

			/**
			 * @brief Constructs the distribution.
			 * @param minimum The lower bound, included.
			 * @param maximum The upper bound, included.
			 */
			constexpr
			UniformInteger (integer_t minimum, integer_t maximum) noexcept
				: m_minimum{minimum},
				m_maximum{maximum}
			{

			}

			/**
			 * @brief Draws a number in [minimum, maximum].
			 * @param generator A reference to the generator.
			 * @return integer_t
			 */
			template< FullRangeGenerator generator_t >
			[[nodiscard]]
			integer_t
			operator() (generator_t & generator) const noexcept
			{
				return uniformInteger< integer_t >(generator, m_minimum, m_maximum);
			}

		private:

			integer_t m_minimum;
			integer_t m_maximum;
	};

	/**
	 * @brief Shuffles a range: Fisher-Yates from its end, each swap partner by uniformInteger() (32-bit draws while the
	 * range has at most 2^32 elements, so a 256-entry permutation takes 255 draws).
	 * @param range The range (random access).
	 * @param generator A reference to the generator.
	 */
	template< std::ranges::random_access_range range_t, FullRangeGenerator generator_t >
	void
	shuffle (range_t & range, generator_t & generator) noexcept
	{
		auto first = std::ranges::begin(range);
		const auto count = static_cast< uint64_t >(std::ranges::distance(range));

		if ( count < 2U )
		{
			return;
		}

		for ( auto index = count - 1U; index > 0U; --index )
		{
			const auto other = count <= UINT64_C(0xFFFFFFFF) ?
				static_cast< uint64_t >(uniformInteger< uint32_t >(generator, 0U, static_cast< uint32_t >(index))) :
				uniformInteger< uint64_t >(generator, 0U, index);

			std::ranges::iter_swap(first + static_cast< std::ranges::range_difference_t< range_t > >(index), first + static_cast< std::ranges::range_difference_t< range_t > >(other));
		}
	}
}
