/*
 * src/Math/Space3D/OrientedBox.hpp
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
#include <array>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>
#include <type_traits>

/* Local inclusions for usages. */
#include "Math/CartesianFrame.hpp"
#include "Math/Vector.hpp"
#include "AACuboid.hpp"

namespace EmEn::Base::Math::Space3D
{
	/**
	 * @brief An oriented box in 3D space: a centre, three orthonormal axes and the half extent along each.
	 * @note This is the form the separating-axis test and the contact clipping work on (C. Ericson, "Real-Time
	 * Collision Detection", 2005, § 4.4). `Math::OrientedCuboid` (8 corners, 6 normals) stays for its own uses.
	 * @note The axes are expected orthonormal and the half extents non-negative: isValid() checks it.
	 * @tparam precision_t The precision type. Default float.
	 */
	template< typename precision_t = float >
	requires (std::is_floating_point_v< precision_t >)
	class OrientedBox final
	{
		public:

			/**
			 * @brief Constructs an empty box at the origin, aligned on the world axes.
			 */
			constexpr OrientedBox () noexcept = default;

			/**
			 * @brief Constructs an oriented box.
			 * @param center A reference to the centre, in world space.
			 * @param axes A reference to the three orthonormal axes (local X, Y, Z), in world space.
			 * @param halfExtents A reference to the half extent along each axis.
			 */
			constexpr
			OrientedBox (const Vector< 3, precision_t > & center, const std::array< Vector< 3, precision_t >, 3 > & axes, const Vector< 3, precision_t > & halfExtents) noexcept
				: m_center{center},
				m_axes{axes},
				m_halfExtents{halfExtents}
			{

			}

			/**
			 * @brief Builds the world oriented box of a local axis-aligned box placed by a frame.
			 * @note The frame's scaling multiplies the half extents; its axes become the box axes.
			 * @param localBox A reference to the box in the frame's local space.
			 * @param frame A reference to the frame (position, orientation, scaling).
			 * @return OrientedBox
			 */
			[[nodiscard]]
			static
			OrientedBox
			fromCuboid (const AACuboid< precision_t > & localBox, const CartesianFrame< precision_t > & frame) noexcept
			{
				const auto & scaling = frame.scalingFactor();
				const auto localCenter = localBox.centroid();
				const auto right = frame.rightVector();
				const auto & upward = frame.upwardVector();
				const auto & backward = frame.backwardVector();

				const Vector< 3, precision_t > center = frame.position() +
					(right * (localCenter[X] * scaling[X])) +
					(upward * (localCenter[Y] * scaling[Y])) +
					(backward * (localCenter[Z] * scaling[Z]));

				const Vector< 3, precision_t > halfExtents{
					std::abs(localBox.width() * scaling[X]) * static_cast< precision_t >(0.5),
					std::abs(localBox.height() * scaling[Y]) * static_cast< precision_t >(0.5),
					std::abs(localBox.depth() * scaling[Z]) * static_cast< precision_t >(0.5)
				};

				return OrientedBox{center, {right, upward, backward}, halfExtents};
			}

			/**
			 * @brief Returns whether the box is usable: finite values, non-negative half extents, unit axes.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isValid () const noexcept
			{
				for ( size_t index = 0; index < 3; ++index )
				{
					if ( !std::isfinite(m_center[index]) || !std::isfinite(m_halfExtents[index]) || m_halfExtents[index] < 0 )
					{
						return false;
					}
				}

				return std::ranges::all_of(m_axes, [] (const Vector< 3, precision_t > & axis) {
					/* Declared here, not captured: MSVC rejects an uncaptured constexpr local in a lambda (C3493). */
					constexpr auto OrientedBoxUnitTolerance = static_cast< precision_t >(1.0e-3);

					if ( !std::isfinite(axis[X]) || !std::isfinite(axis[Y]) || !std::isfinite(axis[Z]) )
					{
						return false;
					}

					return std::abs(axis.length() - static_cast< precision_t >(1)) <= OrientedBoxUnitTolerance;
				});
			}

			/**
			 * @brief Returns the centre.
			 * @return const Vector< 3, precision_t > &
			 */
			[[nodiscard]]
			constexpr
			const Vector< 3, precision_t > &
			center () const noexcept
			{
				return m_center;
			}

			/**
			 * @brief Sets the centre.
			 * @param center A reference to a position.
			 */
			constexpr
			void
			setCenter (const Vector< 3, precision_t > & center) noexcept
			{
				m_center = center;
			}

			/**
			 * @brief Returns one axis.
			 * @pre index < 3.
			 * @param index The axis index: 0 = local X, 1 = local Y, 2 = local Z.
			 * @return const Vector< 3, precision_t > &
			 */
			[[nodiscard]]
			constexpr
			const Vector< 3, precision_t > &
			axis (size_t index) const noexcept
			{
				return m_axes[index];
			}

			/**
			 * @brief Returns the three axes.
			 * @return const std::array< Vector< 3, precision_t >, 3 > &
			 */
			[[nodiscard]]
			constexpr
			const std::array< Vector< 3, precision_t >, 3 > &
			axes () const noexcept
			{
				return m_axes;
			}

			/**
			 * @brief Returns the half extent along one axis.
			 * @pre index < 3.
			 * @param index The axis index.
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			halfExtent (size_t index) const noexcept
			{
				return m_halfExtents[index];
			}

			/**
			 * @brief Returns the half extents.
			 * @return const Vector< 3, precision_t > &
			 */
			[[nodiscard]]
			constexpr
			const Vector< 3, precision_t > &
			halfExtents () const noexcept
			{
				return m_halfExtents;
			}

			/**
			 * @brief Returns the radius of the box projected on a direction: Σ e_i |a_i · d|.
			 * @param direction A reference to a direction (unit for a distance).
			 * @return precision_t
			 */
			[[nodiscard]]
			precision_t
			projectedRadius (const Vector< 3, precision_t > & direction) const noexcept
			{
				return
					(m_halfExtents[X] * std::abs(Vector< 3, precision_t >::dotProduct(m_axes[X], direction))) +
					(m_halfExtents[Y] * std::abs(Vector< 3, precision_t >::dotProduct(m_axes[Y], direction))) +
					(m_halfExtents[Z] * std::abs(Vector< 3, precision_t >::dotProduct(m_axes[Z], direction)));
			}

			/**
			 * @brief Returns a corner.
			 * @pre index < 8.
			 * @param index Bit 0 selects +X (1) or -X (0), bit 1 +Y / -Y, bit 2 +Z / -Z.
			 * @return Vector< 3, precision_t >
			 */
			[[nodiscard]]
			Vector< 3, precision_t >
			corner (size_t index) const noexcept
			{
				const auto signX = (index & 1U) != 0 ? static_cast< precision_t >(1) : static_cast< precision_t >(-1);
				const auto signY = (index & 2U) != 0 ? static_cast< precision_t >(1) : static_cast< precision_t >(-1);
				const auto signZ = (index & 4U) != 0 ? static_cast< precision_t >(1) : static_cast< precision_t >(-1);

				return m_center +
					(m_axes[X] * (signX * m_halfExtents[X])) +
					(m_axes[Y] * (signY * m_halfExtents[Y])) +
					(m_axes[Z] * (signZ * m_halfExtents[Z]));
			}

			/**
			 * @brief STL streams printable object.
			 * @param out A reference to the stream output.
			 * @param obj A reference to the object to print.
			 * @return std::ostream &
			 */
			friend
			std::ostream &
			operator<< (std::ostream & out, const OrientedBox & obj) noexcept
			{
				return out <<
					"Oriented box data :\n"
					"Center : " << obj.m_center << "\n"
					"Axis X : " << obj.m_axes[X] << "\n"
					"Axis Y : " << obj.m_axes[Y] << "\n"
					"Axis Z : " << obj.m_axes[Z] << "\n"
					"Half extents : " << obj.m_halfExtents << '\n';
			}

			/**
			 * @brief Stringifies the object.
			 * @param obj A reference to the object to print.
			 * @return std::string
			 */
			friend
			std::string
			to_string (const OrientedBox & obj) noexcept
			{
				std::stringstream output;

				output << obj;

				return output.str();
			}

		private:

			Vector< 3, precision_t > m_center;
			std::array< Vector< 3, precision_t >, 3 > m_axes{
				Vector< 3, precision_t >{1, 0, 0},
				Vector< 3, precision_t >{0, 1, 0},
				Vector< 3, precision_t >{0, 0, 1}
			};
			Vector< 3, precision_t > m_halfExtents;
	};
}
