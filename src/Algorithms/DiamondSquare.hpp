/*
 * src/Algorithms/DiamondSquare.hpp
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
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

/* Local inclusions. */
#include "Math/Base.hpp"
#include "Randomizer.hpp"

namespace EmEn::Base::Algorithms
{
	/**
	 * @brief Class performing the diamond square algorithm for terrain generation.
	 * @tparam number_t The type of number. Default float.
	 */
	template< typename number_t = float > requires (std::is_floating_point_v< number_t >)
	class DiamondSquare final
	{
		public:

			/**
			 * @brief Constructs a diamond square processor.
			 * @param useSameValueForCorner Use the same value for corner.
			 */
			explicit
			DiamondSquare (bool useSameValueForCorner) noexcept
				: m_useSameValueForCorner(useSameValueForCorner)
			{

			}

			/**
			 * @brief Constructs a diamond square processor with a seed.
			 * @param seed A seed value.
			 * @param useSameValueForCorner Use the same value for corner.
			 */
			DiamondSquare (int32_t seed, bool useSameValueForCorner) noexcept
				: m_randomizer(seed),
				m_useSameValueForCorner(useSameValueForCorner)
			{

			}

			/**
			 * @brief Returns the generated data.
			 * @return const std::vector< type_t > &
			 */
			[[nodiscard]]
			const std::vector< number_t > &
			data () const noexcept
			{
				return m_data;
			}

			/**
			 * @brief Returns a specific point from the generated data.
			 * @param coordX The coordinate in X.
			 * @param coordY The coordinate in Y.
			 * @return type_t
			 */
			[[nodiscard]]
			number_t
			value (size_t coordX, size_t coordY) const noexcept
			{
				return m_data[this->index(coordX, coordY)];
			}

			/**
			 * @brief Generates the noise data.
			 * @note The displacement of the first subdivision level is `roughness × size / 2`, and every
			 * finer level multiplies it by `2^-hurst`. `hurst = 1` is the classical Brownian relief and
			 * reproduces the historical formula (`roughness × halfSize` at every level) exactly; a higher
			 * value damps the fine levels faster, so the finest subdivisions stop depositing white noise
			 * on the grid — on a terrain that noise shades as a regular lattice aligned on the mesh.
			 * The output is normalised afterwards, so `hurst` shapes the relief and never its range.
			 * @param size The size of pattern which must be 2^n + 1.
			 * @param roughness A value from 0 to 1 scaling every level's displacement against the corner values.
			 * @param hurst The per-level decay exponent, 0 or more. Default 1 (Brownian).
			 * @param normalize If true, normalizes output values to [-1, 1] range. Default true.
			 * @return bool
			 */
			bool
			generate (size_t size, number_t roughness, number_t hurst = 1, bool normalize = true) noexcept
			{
				/* Size must be at least 3. */
				if ( size < 3 )
				{
					std::cerr << "The size must be at least 3! (size:" << size << ", roughness:" << roughness << ")" "\n";

					return false;
				}

				/* Size must be of the form 2^n + 1, meaning (size - 1) must be a power of two. */
				if ( !Math::isPowerOfTwo(size - 1) )
				{
					std::cerr << "The size minus one must be a power of two! (size:" << size << ", roughness:" << roughness << ")" "\n";

					return false;
				}

				roughness = Math::clampToUnit(roughness);
				hurst = std::max(hurst, static_cast< number_t >(0));

				m_size = size;
				m_data.resize(m_size * m_size);

				this->cornerStep();

				auto currentSize = m_size;

				/* The first level displaces by roughness × (size / 2), then each finer level by 2^-H less.
				 * With H = 1 this is exactly roughness × halfSize at every level, the historical amplitude. */
				auto amplitude = roughness * static_cast< number_t >(m_size / 2);
				const auto decay = std::pow(static_cast< number_t >(2), -hurst);

				while ( currentSize > 1 )
				{
					auto halfSize = currentSize / 2;

					/* Diamond step. The centers of each tile. */
					this->diamondStep(currentSize, halfSize, amplitude);

					/* Square step. The midpoints of the sides. */
					this->squareStep(currentSize, halfSize, amplitude);

					currentSize = halfSize;
					amplitude *= decay;
				}

				/* Normalize values to [-1, 1] range if requested. */
				if ( normalize )
				{
					this->normalizeData();
				}

				return true;
			}

		private:

			/**
			 * @brief The index of a point from coordinates.
			 * @param coordX The coordinate in X.
			 * @param coordY The coordinate in Y.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			index (size_t coordX, size_t coordY) const noexcept
			{
				return (coordY * m_size) + coordX;
			}

			/**
			 * @brief Performs the corner step.
			 * @return void
			 */
			void
			cornerStep () noexcept
			{
				const auto size = static_cast< number_t >(m_size);

				if ( m_useSameValueForCorner )
				{
					const auto randValue = m_randomizer.value(-size, size);

					m_data[0] = randValue;
					m_data[m_size - 1] = randValue;
					m_data[m_size * (m_size - 1)] = randValue;
					m_data[(m_size * m_size) - 1] = randValue;
				}
				else
				{
					m_data[0] = m_randomizer.value(-size, size);
					m_data[m_size - 1] = m_randomizer.value(-size, size);
					m_data[m_size * (m_size - 1)] = m_randomizer.value(-size, size);
					m_data[(m_size * m_size) - 1] = m_randomizer.value(-size, size);
				}
			}

			/**
			 * @brief Performs the diamond step.
			 * @param size
			 * @param halfSize
			 * @param amplitude The displacement amplitude of this level.
			 * @return void
			 */
			void
			diamondStep (size_t size, size_t halfSize, number_t amplitude) noexcept
			{
				for ( size_t coordX = halfSize; coordX < m_size; coordX += size )
				{
					for ( size_t coordY = halfSize; coordY < m_size; coordY += size )
					{
						const auto posX = coordX - halfSize;
						const auto negX = coordX + halfSize;
						const auto negY = coordY - halfSize;
						const auto posY = coordY + halfSize;

						auto average = m_data[this->index(negX, negY)];
						average += m_data[this->index(negX, posY)];
						average += m_data[this->index(posX, posY)];
						average += m_data[this->index(posX, negY)];
						average *= 0.25;

						m_data[this->index(coordX, coordY)] = average + m_randomizer.value(-amplitude, amplitude);
					}
				}
			}

			/**
			 * @brief Performs the square step.
			 * @param size
			 * @param halfSize
			 * @param amplitude The displacement amplitude of this level.
			 * @return void
			 */
			void
			squareStep (size_t size, size_t halfSize, number_t amplitude) noexcept
			{
				size_t offset = 0;

				for ( size_t coordX = 0; coordX < m_size; coordX += halfSize )
				{
					if ( offset == 0 )
					{
						offset = halfSize;
					}
					else
					{
						offset = 0;
					}

					for ( size_t coordY = offset; coordY < m_size; coordY += size )
					{
						number_t sum = 0;
						size_t count = 0;

						if ( coordX >= halfSize )
						{
							sum += m_data[index(coordX - halfSize, coordY)];
							count++;
						}

						if ( coordX + halfSize < m_size )
						{
							sum += m_data[index(coordX + halfSize, coordY)];
							count++;
						}

						if ( coordY >= halfSize )
						{
							sum += m_data[index(coordX, coordY - halfSize)];
							count++;
						}

						if ( coordY + halfSize < m_size )
						{
							sum += m_data[index(coordX, coordY + halfSize)];
							count++;
						}

						m_data[this->index(coordX, coordY)] = (sum / count) + m_randomizer.value(-amplitude, amplitude);
					}
				}
			}

			/**
			 * @brief Normalizes all data values to the [-1, 1] range.
			 *
			 * This ensures the factor parameter in applyDiamondSquare represents
			 * the actual maximum height displacement in world units.
			 *
			 * @return void
			 */
			void
			normalizeData () noexcept
			{
				if ( m_data.empty() )
				{
					return;
				}

				/* Find min and max values. */
				auto minVal = m_data[0];
				auto maxVal = m_data[0];

				for ( const auto & value : m_data )
				{
					if ( value < minVal )
					{
						minVal = value;
					}

					if ( value > maxVal )
					{
						maxVal = value;
					}
				}

				/* Avoid division by zero if all values are the same. */
				const auto range = maxVal - minVal;

				if ( range < static_cast< number_t >(0.0001) )
				{
					/* All values are essentially the same, set them to 0. */
					std::fill(m_data.begin(), m_data.end(), static_cast< number_t >(0));

					return;
				}

				/* Normalize to [-1, 1] range. */
				const auto scale = static_cast< number_t >(2) / range;

				for ( auto & value : m_data )
				{
					value = ((value - minVal) * scale) - static_cast< number_t >(1);
				}
			}

			size_t m_size{0};
			std::vector< number_t > m_data;
			Randomizer< number_t > m_randomizer;
			bool m_useSameValueForCorner{false};
	};
}
