/*
 * src/Algorithms/Mandelbrot.hpp
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
#include <functional>
#include <type_traits>

/* Local inclusions. */
#include "PixelFactory/Color.hpp"
#include "PixelFactory/Pixmap.hpp"
#include "PixelFactory/Types.hpp"

namespace EmEn::Base::Algorithms
{
	/**
	 * @brief The Mandelbrot algorithm class.
	 * @tparam number_t The type of number. Default float.
	 * @tparam precision_t The pixel component type of the generated pixmap. Default uint8_t.
	 */
	template< typename number_t = float, typename precision_t = uint8_t >
	requires (std::is_floating_point_v< number_t > && std::is_arithmetic_v< precision_t >)
	class Mandelbrot final
	{
		public:

			using colorize_fn = std::function< void (PixelFactory::Pixmap< precision_t > &, uint32_t, uint32_t, int32_t, int32_t) >;

			/**
			 * @brief Constructs the mandelbrot algorithm.
			 * @param width The width of the pixmap.
			 * @param height The height of the pixmap.
			 * @param channels The channel mode. Default RGB.
			 */
			Mandelbrot (uint32_t width, uint32_t height, PixelFactory::ChannelMode channels = PixelFactory::ChannelMode::RGB) noexcept
				: m_pixmap(width, height, channels)
			{

			}

			/**
			 * @brief Returns the generated pixmap.
			 * @return const PixelFactory::Pixmap< precision_t > &
			 */
			[[nodiscard]]
			const PixelFactory::Pixmap< precision_t > &
			pixmap () const noexcept
			{
				return m_pixmap;
			}

			/**
			 * @brief Sets the function that colors a pixel from its escape iteration count.
			 * @param function The function (pixmap, x, y, iterations, maximum iterations).
			 */
			void
			setColorizationMethod (const colorize_fn & function)
			{
				m_colorizationFunction = function;
			}

			/**
			 * @brief Executes the generation.
			 * @param minReal The real part at the left edge. Default -1.5.
			 * @param maxReal The real part at the right edge. Default 0.7.
			 * @param minImaginary The imaginary part at the top edge. Default -1.0.
			 * @param maxImaginary The imaginary part at the bottom edge. Default 1.0.
			 * @param maxIterations The iteration count after which a point is considered inside the set. Default 1000.
			 */
			void
			execute (number_t minReal = -1.5, number_t maxReal = 0.7, number_t minImaginary = -1.0, number_t maxImaginary = 1.0, int32_t maxIterations = 1000)
			{
				for ( uint32_t y = 0; y < m_pixmap.height(); y++ )
				{
					const auto ci = Mandelbrot::map(y, m_pixmap.height(), minImaginary, maxImaginary);

					for ( uint32_t x = 0; x < m_pixmap.width(); x++ )
					{
						const auto cr = Mandelbrot::map(x, m_pixmap.width(), minReal, maxReal);

						m_colorizationFunction(m_pixmap, x, y, Mandelbrot::findMandelbrot(cr, ci, maxIterations), maxIterations);
					}
				}
			}

		private:

			/**
			 * @brief Iterates z = z² + c from zero until |z| > 2 or the iteration limit.
			 * @param cr The real part of c.
			 * @param ci The imaginary part of c.
			 * @param maxIterations The iteration limit.
			 * @return int32_t The iteration count (maxIterations for a point inside the set).
			 */
			static
			int32_t
			findMandelbrot (number_t cr, number_t ci, int32_t maxIterations) noexcept
			{
				int32_t index = 0;

				number_t zr = 0;
				number_t zi = 0;

				while ( index < maxIterations && zr * zr + zi * zi < static_cast< number_t >(4.0) )
				{
					const auto tmp = zr * zr - zi * zi + cr;

					zi = static_cast< number_t >(2.0) * zr * zi + ci;

					zr = tmp;

					index++;
				}

				return index;
			}

			/**
			 * @brief Maps a pixel coordinate onto the complex plane range.
			 * @param offset The pixel coordinate.
			 * @param dimension The pixmap dimension on that axis.
			 * @param min The value at coordinate 0.
			 * @param max The value at coordinate dimension.
			 * @return number_t
			 */
			static
			number_t
			map (uint32_t offset, uint32_t dimension, number_t min, number_t max) noexcept
			{
				const auto range = max - min;

				return static_cast< number_t >(offset) * (range / static_cast< number_t >(dimension)) + min;
			}

			PixelFactory::Pixmap< precision_t > m_pixmap;
			colorize_fn m_colorizationFunction{[] (PixelFactory::Pixmap< precision_t > & pixmap, uint32_t x, uint32_t y, int32_t value, int32_t /*iteration*/) {
				const auto cmp = static_cast< uint8_t >(value % 256);

				pixmap.setPixel(x, y, PixelFactory::ColorFromInteger(cmp, cmp, cmp));
			}};
	};
}
