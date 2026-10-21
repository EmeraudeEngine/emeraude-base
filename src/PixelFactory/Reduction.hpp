/*
 * src/PixelFactory/Reduction.hpp
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
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

/* Forward declarations. */
namespace EmEn::Base::PixelFactory
{
	enum class ReductionFilter : uint8_t;
}

namespace EmEn::Base::PixelFactory
{
	/** @brief One tap of a reduction kernel: a source pixel and its weight. */
	struct ReductionTap final
	{
		/** @brief The source pixel's index along the axis, in [0, source size). */
		size_t index{0};
		/** @brief The tap's weight. The weights of one destination pixel sum to 1; a Kaiser weight may be negative. */
		float weight{0.0F};
	};

	/**
	 * @brief The one-axis kernel of a reduction (Processor::downsample()): for every destination pixel, the source pixels
	 * it reads and their weights, normalised to sum to 1. Separable: one kernel for the columns, one for the rows.
	 * @note Box: the exact area-weighted footprint (a fractional pixel weighted by the fraction covered) — the mean is kept
	 * exactly, and no tap reaches past the edge.
	 * @note Kaiser: the polyphase kernel of NVIDIA Texture Tools (I. Castaño, src/nvimage/Filter.cpp, public domain,
	 * https://github.com/castano/nvidia-texture-tools): a Kaiser-windowed sinc, width 3 destination pixels, alpha 4,
	 * stretch 1, each tap the filter averaged over the source pixel's box (32 sub-samples). A tap past the edge reads
	 * the MIRRORED pixel; the mirror repeats the edge pixel (index −1 reads 0, as VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
	 * does) where NVTT's skips it (−1 reads 1). Taps landing on the same pixel are merged.
	 * @note An unchanged size (source = destination) is the identity, for both filters.
	 */
	class ReductionKernel final
	{
		public:

			/** @brief The Kaiser filter's half-width, in destination pixels (NVTT's KaiserFilter(3)). */
			static constexpr float KaiserWidth{3.0F};
			/** @brief The Kaiser window's alpha (NVTT's default). */
			static constexpr float KaiserAlpha{4.0F};
			/** @brief The sinc's stretch (NVTT's default). */
			static constexpr float KaiserStretch{1.0F};
			/** @brief The sub-samples averaged over a source pixel's box (NVTT's PolyphaseKernel default). */
			static constexpr int KaiserBoxSamples{32};

			/**
			 * @brief Builds the kernel of one axis.
			 * @param sourceSize The source size along the axis.
			 * @param destinationSize The destination size along the axis, in [1, sourceSize].
			 * @param filter The filter.
			 */
			ReductionKernel (size_t sourceSize, size_t destinationSize, ReductionFilter filter) noexcept;

			/**
			 * @brief Returns whether the kernel was built: false for a null size or an enlarging one.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isValid () const noexcept
			{
				return !m_offsets.empty();
			}

			/**
			 * @brief Returns the destination size along the axis.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			destinationSize () const noexcept
			{
				return m_offsets.empty() ? 0 : m_offsets.size() - 1;
			}

			/**
			 * @brief Returns the taps of a destination pixel.
			 * @pre The kernel is valid and target < destinationSize().
			 * @param target The destination pixel's index along the axis.
			 * @return std::span< const ReductionTap >
			 */
			[[nodiscard]]
			std::span< const ReductionTap >
			taps (size_t target) const noexcept
			{
				return std::span< const ReductionTap >{m_taps}.subspan(m_offsets[target], m_offsets[target + 1] - m_offsets[target]);
			}

			/**
			 * @brief Returns the widest span of source pixels one destination pixel reads (highest index − lowest + 1):
			 * a ring of that many filtered rows holds every row a destination row needs.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			maximumSpan () const noexcept
			{
				return m_maximumSpan;
			}

			/**
			 * @brief Evaluates the Kaiser-windowed sinc of NVTT (KaiserFilter::evaluate()).
			 * @param x The distance to the centre, in destination pixels.
			 * @return double 1 at the centre, 0 at and past ±KaiserWidth.
			 */
			[[nodiscard]]
			static double kaiser (double x) noexcept;

		private:

			/**
			 * @brief Ends the taps of one destination pixel: records its offset and widens the maximum span.
			 * @pre The pixel has at least one tap.
			 */
			void closeTarget () noexcept;

			/**
			 * @brief Builds the identity kernel: one tap of weight 1 per pixel.
			 * @param size The size along the axis.
			 */
			void buildIdentity (size_t size) noexcept;

			/**
			 * @brief Builds the exact area-weighted box kernel.
			 * @param sourceSize The source size.
			 * @param destinationSize The destination size, below the source size.
			 */
			void buildBox (size_t sourceSize, size_t destinationSize) noexcept;

			/**
			 * @brief Builds NVTT's Kaiser polyphase kernel, the edges mirrored.
			 * @param sourceSize The source size.
			 * @param destinationSize The destination size, below the source size.
			 */
			void buildKaiser (size_t sourceSize, size_t destinationSize) noexcept;

			/**
			 * @brief Adds a tap to the current destination pixel, merged with an existing tap on the same source pixel; a null
			 * weight adds nothing.
			 * @param first The index of the current pixel's first tap.
			 * @param index The source pixel.
			 * @param weight The weight, not yet normalised.
			 */
			void addTap (size_t first, size_t index, double weight) noexcept;

			std::vector< ReductionTap > m_taps;
			std::vector< size_t > m_offsets;
			size_t m_maximumSpan{0};
	};

	/**
	 * @brief Decodes an sRGB-encoded value to linear light (IEC 61966-2-1).
	 * @param encoded The encoded value, in [0, 1].
	 * @return float The linear value, in [0, 1].
	 */
	[[nodiscard]]
	float sRGBToLinear (float encoded) noexcept;

	/**
	 * @brief Encodes a linear-light value to sRGB (IEC 61966-2-1).
	 * @param linear The linear value; clamped to [0, 1] (NaN gives 0).
	 * @return float The encoded value, in [0, 1].
	 */
	[[nodiscard]]
	float linearToSRGB (float linear) noexcept;

	/**
	 * @brief Returns the table of the 256 sRGB bytes decoded to linear light (built once, thread-safe), for a loop that
	 * decodes many values: index it by the byte.
	 * @return const std::array< float, 256 > &
	 */
	[[nodiscard]]
	const std::array< float, 256 > & sRGBByteDecodeTable () noexcept;

	/**
	 * @brief Decodes an sRGB byte to linear light, through a table of the 256 values.
	 * @param encoded The encoded byte.
	 * @return float The linear value, in [0, 1].
	 */
	[[nodiscard]]
	float sRGBByteToLinear (uint8_t encoded) noexcept;

	/**
	 * @brief Encodes a linear-light value to the nearest sRGB byte — round(linearToSRGB(linear) × 255) — through the 255
	 * linear thresholds between consecutive bytes (a binary search, no pow()).
	 * @param linear The linear value; below 0 gives 0, above 1 gives 255, NaN gives 0.
	 * @return uint8_t
	 */
	[[nodiscard]]
	uint8_t linearToSRGBByte (float linear) noexcept;
}
