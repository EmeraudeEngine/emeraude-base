/*
 * src/PixelFactory/Reduction.cpp
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

#include "Reduction.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numbers>
#include <ranges>
#include <span>

/* Local inclusions. */
#include "PixelFactory/Types.hpp"

namespace EmEn::Base::PixelFactory
{
	namespace
	{
		/**
		 * @brief The modified Bessel function of the first kind, order zero: its power series, summed until a term falls
		 * below 1e-6 of the sum (NVTT's bessel0()). Bounded: the terms (x/2)^2k / (k!)^2 shrink super-exponentially.
		 * @param x The argument, in [0, KaiserAlpha].
		 * @return double
		 */
		[[nodiscard]]
		double
		bessel0 (double x) noexcept
		{
			constexpr double EpsilonRatio{1.0e-6};
			/* The term count for x ≤ 4 is below 15: the cap is a bound, never reached. */
			constexpr int MaxTerms{64};

			const auto halfX = 0.5 * x;
			auto sum = 1.0;
			auto power = 1.0;

			for ( int term = 1; term <= MaxTerms; ++term )
			{
				power *= halfX / static_cast< double >(term);

				const auto squared = power * power;

				sum += squared;

				if ( squared <= sum * EpsilonRatio )
				{
					break;
				}
			}

			return sum;
		}

		/**
		 * @brief Returns the weight of one Kaiser tap: the filter averaged over the source pixel [offset, offset + 1) from the
		 * centre (NVTT's Filter::sampleBox()), in destination units.
		 * @param offset The source pixel's left edge, relative to the centre, in source pixels.
		 * @param scale The destination size over the source size.
		 * @return double
		 */
		[[nodiscard]]
		double
		kaiserTapWeight (double offset, double scale) noexcept
		{
			auto weight = 0.0;

			for ( int sample = 0; sample < ReductionKernel::KaiserBoxSamples; ++sample )
			{
				weight += ReductionKernel::kaiser((offset + ((static_cast< double >(sample) + 0.5) / static_cast< double >(ReductionKernel::KaiserBoxSamples))) * scale);
			}

			return weight / static_cast< double >(ReductionKernel::KaiserBoxSamples);
		}

		/** @brief The lowest exponent of a bucket: below 2^-13 (1.2e-4), every value encodes to 0 (the first threshold is 1.5e-4). */
		constexpr int LowestBucketExponent{-13};
		/** @brief 2^LowestBucketExponent. */
		constexpr float LowestBucketValue{1.0F / 8192.0F};
		/** @brief The mantissa bits indexing a bucket inside its exponent: 32 buckets per octave, at most 3 thresholds each. */
		constexpr uint32_t BucketMantissaBits{5};
		constexpr size_t BucketCount{static_cast< size_t >(-LowestBucketExponent) << BucketMantissaBits};

		/**
		 * @brief Returns the bucket of a linear value, from its float bits: (exponent, top mantissa bits).
		 * @pre linear in [2^LowestBucketExponent, 1).
		 * @param linear The value.
		 * @return size_t
		 */
		[[nodiscard]]
		size_t
		bucketOf (float linear) noexcept
		{
			const auto bits = std::bit_cast< uint32_t >(linear);
			const auto exponent = static_cast< int >(bits >> 23U) - 127;

			return (static_cast< size_t >(exponent - LowestBucketExponent) << BucketMantissaBits) | ((bits >> (23U - BucketMantissaBits)) & ((1U << BucketMantissaBits) - 1U));
		}

		/** @brief The sRGB byte tables: the 256 decoded values, the 255 linear thresholds halfway between consecutive bytes
		 * (in the encoded space), and for each bucket the byte of its lowest value. */
		struct SRGBTables final
		{
			std::array< float, 256 > decode{};
			std::array< float, 255 > thresholds{};
			std::array< uint8_t, BucketCount > bucketStart{};
		};

		[[nodiscard]]
		const SRGBTables &
		sRGBTables () noexcept
		{
			static const SRGBTables tables = [] {
				SRGBTables result;

				auto code = 0.0F;

				for ( auto & linear : result.decode )
				{
					linear = sRGBToLinear(code / 255.0F);
					code += 1.0F;
				}

				code = 0.5F;

				for ( auto & threshold : result.thresholds )
				{
					threshold = sRGBToLinear(code / 255.0F);
					code += 1.0F;
				}

				size_t bucket = 0;

				for ( auto & start : result.bucketStart )
				{
					const auto exponent = static_cast< uint32_t >(static_cast< int >(bucket >> BucketMantissaBits) + LowestBucketExponent + 127);
					const auto lowest = std::bit_cast< float >((exponent << 23U) | (static_cast< uint32_t >(bucket & ((1U << BucketMantissaBits) - 1U)) << (23U - BucketMantissaBits)));

					start = static_cast< uint8_t >(std::ranges::upper_bound(result.thresholds, lowest) - result.thresholds.begin());
					++bucket;
				}

				return result;
			}();

			return tables;
		}
	}

	ReductionKernel::ReductionKernel (size_t sourceSize, size_t destinationSize, ReductionFilter filter) noexcept
	{
		if ( sourceSize == 0 || destinationSize == 0 || destinationSize > sourceSize )
		{
			return;
		}

		m_offsets.reserve(destinationSize + 1);
		m_offsets.push_back(0);

		if ( destinationSize == sourceSize )
		{
			this->buildIdentity(destinationSize);
		}
		else if ( filter == ReductionFilter::Box )
		{
			this->buildBox(sourceSize, destinationSize);
		}
		else
		{
			this->buildKaiser(sourceSize, destinationSize);
		}
	}

	void
	ReductionKernel::closeTarget () noexcept
	{
		const auto first = m_taps.begin() + static_cast< std::ptrdiff_t >(m_offsets.back());
		const auto [lowest, highest] = std::ranges::minmax_element(first, m_taps.end(), {}, &ReductionTap::index);

		m_maximumSpan = std::max(m_maximumSpan, highest->index - lowest->index + 1);
		m_offsets.push_back(m_taps.size());
	}

	void
	ReductionKernel::buildIdentity (size_t size) noexcept
	{
		m_taps.reserve(size);

		for ( size_t target = 0; target < size; ++target )
		{
			m_taps.push_back({target, 1.0F});
			this->closeTarget();
		}
	}

	void
	ReductionKernel::buildBox (size_t sourceSize, size_t destinationSize) noexcept
	{
		const auto scale = static_cast< double >(destinationSize) / static_cast< double >(sourceSize);
		const auto inverseScale = static_cast< double >(sourceSize) / static_cast< double >(destinationSize);

		m_taps.reserve(destinationSize * (static_cast< size_t >(std::ceil(inverseScale)) + 1));

		/* Every source pixel the footprint [target, target + 1) × inverseScale overlaps, weighted by the overlap. */
		for ( size_t target = 0; target < destinationSize; ++target )
		{
			const auto start = static_cast< double >(target) * inverseScale;
			const auto end = static_cast< double >(target + 1) * inverseScale;
			const auto last = std::min(static_cast< size_t >(std::ceil(end)), sourceSize);

			for ( auto index = static_cast< size_t >(std::floor(start)); index < last; ++index )
			{
				const auto overlap = std::min(end, static_cast< double >(index + 1)) - std::max(start, static_cast< double >(index));

				if ( overlap > 0.0 )
				{
					m_taps.push_back({index, static_cast< float >(overlap * scale)});
				}
			}

			this->closeTarget();
		}
	}

	void
	ReductionKernel::buildKaiser (size_t sourceSize, size_t destinationSize) noexcept
	{
		const auto scale = static_cast< double >(destinationSize) / static_cast< double >(sourceSize);
		const auto inverseScale = static_cast< double >(sourceSize) / static_cast< double >(destinationSize);

		/* NVTT's PolyphaseKernel: the filter spans ±KaiserWidth destination pixels, i.e. ±KaiserWidth × inverseScale source
		 * pixels; each tap is the filter averaged over its source pixel [k, k + 1). */
		const auto halfWidth = static_cast< double >(KaiserWidth) * inverseScale;
		const auto windowSize = static_cast< size_t >(std::ceil(halfWidth * 2.0)) + 1;
		const auto size = static_cast< int64_t >(sourceSize);

		/* The image repeats as mirrored copies, the edge pixel repeated (index −1 reads 0). */
		const auto mirror = [size] (int64_t index) noexcept {
			const auto period = 2 * size;
			const auto folded = ((index % period) + period) % period;

			return static_cast< size_t >(folded < size ? folded : period - 1 - folded);
		};

		m_taps.reserve(destinationSize * windowSize);

		for ( size_t target = 0; target < destinationSize; ++target )
		{
			const auto first = m_taps.size();
			const auto center = (static_cast< double >(target) + 0.5) * inverseScale;
			const auto left = static_cast< int64_t >(std::floor(center - halfWidth));
			auto total = 0.0;

			for ( size_t step = 0; step < windowSize; ++step )
			{
				const auto pixel = left + static_cast< int64_t >(step);
				const auto weight = kaiserTapWeight(static_cast< double >(pixel) - center, scale);

				total += weight;

				this->addTap(first, mirror(pixel), weight);
			}

			/* The window always holds the centre's pixel, whose weight is positive: the total is never zero. */
			const auto normalization = static_cast< float >(1.0 / total);

			for ( auto & tap : std::span{m_taps}.subspan(first) )
			{
				tap.weight *= normalization;
			}

			this->closeTarget();
		}
	}

	void
	ReductionKernel::addTap (size_t first, size_t index, double weight) noexcept
	{
		/* NOTE: The filter is exactly 0 past its window: such a tap reads nothing. */
		if ( !(std::abs(weight) > 0.0) )
		{
			return;
		}

		/* A mirrored tap may land on a pixel already read: merge it. */
		const auto taps = std::span{m_taps}.subspan(first);

		if ( const auto existing = std::ranges::find(taps, index, &ReductionTap::index); existing != taps.end() )
		{
			existing->weight += static_cast< float >(weight);

			return;
		}

		m_taps.push_back({index, static_cast< float >(weight)});
	}

	double
	ReductionKernel::kaiser (double x) noexcept
	{
		const auto t = x / static_cast< double >(KaiserWidth);
		const auto inside = 1.0 - (t * t);

		if ( inside < 0.0 )
		{
			return 0.0;
		}

		const auto argument = std::numbers::pi * x * static_cast< double >(KaiserStretch);
		const auto sinc = std::abs(argument) < 1.0e-9 ? 1.0 : std::sin(argument) / argument;

		static const auto normalization = bessel0(static_cast< double >(KaiserAlpha));

		return sinc * bessel0(static_cast< double >(KaiserAlpha) * std::sqrt(inside)) / normalization;
	}

	float
	sRGBToLinear (float encoded) noexcept
	{
		if ( encoded <= 0.04045F )
		{
			return encoded / 12.92F;
		}

		return std::pow((encoded + 0.055F) / 1.055F, 2.4F);
	}

	float
	linearToSRGB (float linear) noexcept
	{
		/* NOTE: The negated comparison sends NaN to 0. */
		if ( !(linear > 0.0F) )
		{
			return 0.0F;
		}

		if ( linear >= 1.0F )
		{
			return 1.0F;
		}

		if ( linear <= 0.0031308F )
		{
			return linear * 12.92F;
		}

		return (1.055F * std::pow(linear, 1.0F / 2.4F)) - 0.055F;
	}

	const std::array< float, 256 > &
	sRGBByteDecodeTable () noexcept
	{
		return sRGBTables().decode;
	}

	float
	sRGBByteToLinear (uint8_t encoded) noexcept
	{
		return std::span< const float, 256 >{sRGBTables().decode}[encoded];
	}

	uint8_t
	linearToSRGBByte (float linear) noexcept
	{
		/* NOTE: The negated comparison sends NaN to 0. */
		if ( !(linear > 0.0F) )
		{
			return 0;
		}

		if ( linear >= 1.0F )
		{
			return 255;
		}

		const auto & tables = sRGBTables();
		const std::span< const float > thresholds{tables.thresholds};

		/* The byte is the count of thresholds at or below the value: from the byte of the value's bucket, at most three
		 * thresholds further (32 buckets per octave), never past 255. */
		size_t byte = linear < LowestBucketValue ? 0 : std::span< const uint8_t >{tables.bucketStart}[bucketOf(linear)];

		while ( byte < thresholds.size() && thresholds[byte] <= linear )
		{
			++byte;
		}

		return static_cast< uint8_t >(byte);
	}
}
