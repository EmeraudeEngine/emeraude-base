/*
 * src/Math/PiecewiseLinear.hpp
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
#include <cstddef>
#include <type_traits>
#include <utility>

/* Local inclusions for usages. */
#include "StaticVector.hpp"

namespace EmEn::Base::Math
{
	/**
	 * @brief A function of one variable given by points, linear between them and constant beyond the first and the last
	 * (a torque curve over the RPM, a tyre's friction over its slip).
	 * @tparam precision_t The floating point type. Default float.
	 * @tparam max_points The most points it holds (a StaticVector: no allocation). Default 16.
	 */
	template< typename precision_t = float, size_t max_points = 16 >
	requires (std::is_floating_point_v< precision_t > && max_points >= 1)
	class PiecewiseLinear final
	{
		public:

			/**
			 * @brief Constructs an empty function (it answers 0).
			 */
			constexpr PiecewiseLinear () noexcept = default;

			/**
			 * @brief Adds a point, after the last one.
			 * @note Refused (false, nothing added): a non-finite value, an x not strictly above the last one, a full curve.
			 * @param x The abscissa.
			 * @param y The value there.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			addPoint (precision_t x, precision_t y) noexcept
			{
				if ( !std::isfinite(x) || !std::isfinite(y) || m_points.full() || (!m_points.empty() && !(x > m_points.back().first)) )
				{
					return false;
				}

				m_points.push_back({x, y});

				return true;
			}

			/**
			 * @brief Removes every point.
			 * @return void
			 */
			void
			clear () noexcept
			{
				m_points.clear();
			}

			/** @brief Returns whether it has no point. */
			[[nodiscard]]
			bool
			empty () const noexcept
			{
				return m_points.empty();
			}

			/** @brief Returns its points (x strictly increasing). */
			[[nodiscard]]
			const StaticVector< std::pair< precision_t, precision_t >, max_points > &
			points () const noexcept
			{
				return m_points;
			}

			/**
			 * @brief Returns the value at an abscissa: linear between two points, the first / the last value beyond them,
			 * 0 without a point, the first value for a NaN.
			 * @param x The abscissa.
			 * @return precision_t
			 */
			[[nodiscard]]
			precision_t
			value (precision_t x) const noexcept
			{
				if ( m_points.empty() )
				{
					return 0;
				}

				if ( !(x > m_points.front().first) )
				{
					return m_points.front().second;
				}

				for ( size_t index = 1; index < m_points.size(); ++index )
				{
					const auto & [x1, y1] = m_points[index];

					if ( x <= x1 )
					{
						const auto & [x0, y0] = m_points[index - 1];
						const auto t = (x - x0) / (x1 - x0);

						return y0 + ((y1 - y0) * t);
					}
				}

				return m_points.back().second;
			}

		private:

			StaticVector< std::pair< precision_t, precision_t >, max_points > m_points;
	};
}
