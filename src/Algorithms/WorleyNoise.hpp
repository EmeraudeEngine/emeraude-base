/*
 * src/Algorithms/WorleyNoise.hpp
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
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace EmEn::Base::Algorithms
{
	/**
	 * @brief Tileable 3D cellular noise: the distance from a point to the nearest of a set of
	 * feature points scattered one per cell of the integer lattice (the "F1" basis).
	 * @note ⚠️ PERIODIC BY CONSTRUCTION, and that is what it exists for. The feature point of a cell
	 * is hashed from the cell coordinates taken MODULO the period, so `generate(x + period, y, z)`
	 * equals `generate(x, y, z)` exactly: a 3D texture baked over one period tiles with no seam,
	 * which a cloud detail texture sampled far outside [0, 1] needs. An octave evaluated at
	 * `2^i · p` keeps that property, since `2^i · period` is a multiple of the period.
	 * @note The value is the Euclidean distance in CELL units, clamped to [0, 1]. With one point per
	 * cell the nearest one always lies in the 3×3×3 neighbourhood of the cell holding the sample,
	 * which is the whole search.
	 * @note Deterministic across platforms and compilers: the feature points come from an integer
	 * hash, never from a standard-library random engine whose sequence is implementation-defined.
	 *
	 * References: S. Worley, *A Cellular Texture Basis Function*, SIGGRAPH 1996 (the basis);
	 * C. Wellons, *Prospecting for Hash Functions*, 2018, https://nullprogram.com/blog/2018/07/31/
	 * (the "lowbias32" integer hash, released into the public domain).
	 * @tparam number_t The type of number. Default float.
	 */
	template< typename number_t = float >
	requires (std::is_floating_point_v< number_t >)
	class WorleyNoise final
	{
		public:

			/**
			 * @brief Constructs a cellular noise generator.
			 * @param seed The seed of the feature point layout.
			 * @param period The period of the lattice, in cells. Clamped to 1 at least.
			 */
			WorleyNoise (uint32_t seed, uint32_t period) noexcept
				: m_seed{hash(seed)},
				m_period{std::max(period, 1U)}
			{

			}

			/**
			 * @brief Returns the period of the lattice, in cells.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			period () const noexcept
			{
				return m_period;
			}

			/**
			 * @brief Returns the distance to the nearest feature point, in cell units, clamped to [0, 1].
			 * @param x A value for the X axis, in cells.
			 * @param y A value for the Y axis, in cells.
			 * @param z A value for the Z axis, in cells.
			 * @return number_t
			 */
			[[nodiscard]]
			number_t
			generate (number_t x, number_t y, number_t z) const noexcept
			{
				constexpr auto One{static_cast< number_t >(1.0)};

				const auto floorX = std::floor(x);
				const auto floorY = std::floor(y);
				const auto floorZ = std::floor(z);

				/* ⚠️ Through a SIGNED integer: converting a negative floating-point value straight to an
				 * unsigned one is undefined behaviour, and every coordinate west of the origin is one. */
				const auto cellX = static_cast< int32_t >(floorX);
				const auto cellY = static_cast< int32_t >(floorY);
				const auto cellZ = static_cast< int32_t >(floorZ);

				const auto fractionX = x - floorX;
				const auto fractionY = y - floorY;
				const auto fractionZ = z - floorZ;

				auto nearestSquared = std::numeric_limits< number_t >::max();

				for ( int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ )
				{
					for ( int32_t offsetY = -1; offsetY <= 1; ++offsetY )
					{
						for ( int32_t offsetX = -1; offsetX <= 1; ++offsetX )
						{
							const auto cellHash = this->cellHash(cellX + offsetX, cellY + offsetY, cellZ + offsetZ);

							/* The feature point of that cell, relative to the sample's own cell. */
							const auto deltaX = static_cast< number_t >(offsetX) + unitFraction(hash(cellHash ^ 0x68E31DA4U)) - fractionX;
							const auto deltaY = static_cast< number_t >(offsetY) + unitFraction(hash(cellHash ^ 0xB5297A4DU)) - fractionY;
							const auto deltaZ = static_cast< number_t >(offsetZ) + unitFraction(hash(cellHash ^ 0x1B56C4E9U)) - fractionZ;

							nearestSquared = std::min(nearestSquared, deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
						}
					}
				}

				return std::min(std::sqrt(nearestSquared), One);
			}

			/**
			 * @brief Returns a billowy fractal sum of inverted cellular noise, in [0, 1].
			 * @note `1 - F1` peaks on the feature points, so the sum reads as rounded bulges — the
			 * "billows" of a cumulus (Schneider & Vos, *The Real-time Volumetric Cloudscapes of Horizon
			 * Zero Dawn*, SIGGRAPH 2015). Each octave doubles the frequency and halves the weight; the
			 * result keeps the period of the lattice, see the class note.
			 * @param x A value for the X axis, in cells of the FIRST octave.
			 * @param y A value for the Y axis, in cells of the first octave.
			 * @param z A value for the Z axis, in cells of the first octave.
			 * @param octaves The number of octaves. Clamped to 1 at least.
			 * @return number_t
			 */
			[[nodiscard]]
			number_t
			generateBillows (number_t x, number_t y, number_t z, uint32_t octaves) const noexcept
			{
				constexpr auto One{static_cast< number_t >(1.0)};
				constexpr auto Half{static_cast< number_t >(0.5)};
				constexpr auto Two{static_cast< number_t >(2.0)};

				auto sum = static_cast< number_t >(0.0);
				auto weightSum = static_cast< number_t >(0.0);
				auto weight = One;
				auto frequency = One;

				for ( uint32_t octave = 0; octave < std::max(octaves, 1U); ++octave )
				{
					sum += weight * (One - this->generate(x * frequency, y * frequency, z * frequency));
					weightSum += weight;
					weight *= Half;
					frequency *= Two;
				}

				return sum / weightSum;
			}

		private:

			/**
			 * @brief The "lowbias32" integer hash (C. Wellons, public domain).
			 * @param value The value to hash.
			 * @return uint32_t
			 */
			[[nodiscard]]
			static
			constexpr
			uint32_t
			hash (uint32_t value) noexcept
			{
				value ^= value >> 16U;
				value *= 0x7FEB352DU;
				value ^= value >> 15U;
				value *= 0x846CA68BU;
				value ^= value >> 16U;

				return value;
			}

			/**
			 * @brief Maps the 24 high bits of a hash to [0, 1).
			 * @param value The hash.
			 * @return number_t
			 */
			[[nodiscard]]
			static
			constexpr
			number_t
			unitFraction (uint32_t value) noexcept
			{
				constexpr auto Scale{static_cast< number_t >(1.0 / 16777216.0)};

				return static_cast< number_t >(value >> 8U) * Scale;
			}

			/**
			 * @brief Returns the hash of a lattice cell, its coordinates wrapped onto the period.
			 * @param cellX The cell X coordinate, any sign.
			 * @param cellY The cell Y coordinate, any sign.
			 * @param cellZ The cell Z coordinate, any sign.
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			cellHash (int32_t cellX, int32_t cellY, int32_t cellZ) const noexcept
			{
				const auto period = static_cast< int32_t >(m_period);

				/* A modulo that stays positive for negative coordinates. */
				const auto wrappedX = static_cast< uint32_t >(((cellX % period) + period) % period);
				const auto wrappedY = static_cast< uint32_t >(((cellY % period) + period) % period);
				const auto wrappedZ = static_cast< uint32_t >(((cellZ % period) + period) % period);

				return hash(wrappedX ^ hash(wrappedY ^ hash(wrappedZ ^ m_seed)));
			}

			uint32_t m_seed;
			uint32_t m_period;
	};
}