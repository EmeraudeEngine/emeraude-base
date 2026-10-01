/*
 * src/Math/Space3D/Contacts/ContactManifold.hpp
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
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>

/* Local inclusions for usages. */
#include "Math/Vector.hpp"
#include "StaticVector.hpp"

namespace EmEn::Base::Math::Space3D
{
	namespace ContactsDetail
	{
		/**
		 * @brief The base-3 digit of a box region along one axis, used in feature ids: 0 inside the slab [-extent,
		 * extent], 1 under it, 2 above it. Three digits name a face, an edge or a corner of the box.
		 * @param local The coordinate along the box axis, relative to its centre.
		 * @param extent The half extent along that axis.
		 * @return uint32_t
		 */
		template< typename precision_t >
		[[nodiscard]]
		constexpr
		uint32_t
		regionDigit (precision_t local, precision_t extent) noexcept
		{
			if ( local < -extent )
			{
				return 1U;
			}

			if ( local > extent )
			{
				return 2U;
			}

			return 0U;
		}
	}

	/**
	 * @brief One point of a contact manifold.
	 * @tparam precision_t The precision type. Default float.
	 */
	template< typename precision_t = float >
	requires (std::is_floating_point_v< precision_t >)
	class ContactPoint final
	{
		public:

			/**
			 * @brief Constructs an empty contact point.
			 */
			constexpr ContactPoint () noexcept = default;

			/**
			 * @brief Constructs a contact point.
			 * @param position A reference to the world position, halfway between the two surfaces.
			 * @param depth The penetration depth along the manifold normal (positive = penetrating).
			 * @param featureId An identifier of the pair of features that made the point, stable while the same
			 * features stay in contact, so a solver can carry its accumulated impulse from one step to the next.
			 */
			constexpr
			ContactPoint (const Vector< 3, precision_t > & position, precision_t depth, uint32_t featureId) noexcept
				: m_position{position},
				m_depth{depth},
				m_featureId{featureId}
			{

			}

			/**
			 * @brief Returns the world position, halfway between the two surfaces.
			 * @return const Vector< 3, precision_t > &
			 */
			[[nodiscard]]
			constexpr
			const Vector< 3, precision_t > &
			position () const noexcept
			{
				return m_position;
			}

			/**
			 * @brief Returns the penetration depth along the manifold normal (positive = penetrating).
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			depth () const noexcept
			{
				return m_depth;
			}

			/**
			 * @brief Returns the identifier of the pair of features that made this point.
			 * @return uint32_t
			 */
			[[nodiscard]]
			constexpr
			uint32_t
			featureId () const noexcept
			{
				return m_featureId;
			}

		private:

			Vector< 3, precision_t > m_position;
			precision_t m_depth{0};
			uint32_t m_featureId{0};
	};

	/**
	 * @brief The contact between two shapes A and B: one normal and up to four points.
	 * @note ⚠️ CONVENTION: the normal points FROM A TO B (Box2D, Bullet, Jolt, and the engine's constraint solver).
	 * It is the OPPOSITE of the minimum translation vector of the `isColliding()` overlap tests, which pushes A out
	 * of B. Moving A by `-normal * depth` (or B by `+normal * depth`) separates the point.
	 * @note Four points is enough for a stable resting face (D. Gregorius, "Robust Contact Creation for Physics
	 * Simulations", GDC 2015): a pair generator reduces any larger clipped polygon to the four that span it best.
	 * @tparam precision_t The precision type. Default float.
	 */
	template< typename precision_t = float >
	requires (std::is_floating_point_v< precision_t >)
	class ContactManifold final
	{
		public:

			/** @brief The largest number of points of a manifold. */
			static constexpr size_t MaxPoints{4};

			/**
			 * @brief Constructs an empty manifold.
			 */
			constexpr ContactManifold () noexcept = default;

			/**
			 * @brief Returns the normal, from A to B (unit length when the manifold holds points).
			 * @return const Vector< 3, precision_t > &
			 */
			[[nodiscard]]
			constexpr
			const Vector< 3, precision_t > &
			normal () const noexcept
			{
				return m_normal;
			}

			/**
			 * @brief Sets the normal.
			 * @param normal A reference to a unit vector, from A to B.
			 * @return void
			 */
			constexpr
			void
			setNormal (const Vector< 3, precision_t > & normal) noexcept
			{
				m_normal = normal;
			}

			/**
			 * @brief Adds a point. A full manifold refuses it.
			 * @param point A reference to a contact point.
			 * @return bool False when the manifold already holds MaxPoints points.
			 */
			bool
			addPoint (const ContactPoint< precision_t > & point) noexcept
			{
				if ( m_points.full() )
				{
					return false;
				}

				m_points.push_back(point);

				return true;
			}

			/**
			 * @brief Returns the points.
			 * @return const StaticVector< ContactPoint< precision_t >, MaxPoints > &
			 */
			[[nodiscard]]
			constexpr
			const StaticVector< ContactPoint< precision_t >, MaxPoints > &
			points () const noexcept
			{
				return m_points;
			}

			/**
			 * @brief Returns whether the manifold holds no point.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			empty () const noexcept
			{
				return m_points.empty();
			}

			/**
			 * @brief Returns the deepest penetration of the points (0 for an empty manifold).
			 * @return precision_t
			 */
			[[nodiscard]]
			precision_t
			maximumDepth () const noexcept
			{
				precision_t deepest = 0;

				for ( const auto & point : m_points )
				{
					deepest = point.depth() > deepest ? point.depth() : deepest;
				}

				return deepest;
			}

			/**
			 * @brief Swaps the roles of A and B: the normal is negated, the points stay.
			 * @note The feature ids keep their value: they identify the same features whatever the order.
			 * @return void
			 */
			constexpr
			void
			flip () noexcept
			{
				m_normal = -m_normal;
			}

			/**
			 * @brief Empties the manifold.
			 * @return void
			 */
			void
			clear () noexcept
			{
				m_normal.reset();
				m_points.clear();
			}

			/**
			 * @brief STL streams printable object.
			 * @param out A reference to the stream output.
			 * @param obj A reference to the object to print.
			 * @return std::ostream &
			 */
			friend
			std::ostream &
			operator<< (std::ostream & out, const ContactManifold & obj) noexcept
			{
				out << "Contact manifold (normal A to B " << obj.m_normal << ", " << obj.m_points.size() << " points) :\n";

				for ( const auto & point : obj.m_points )
				{
					out << " - " << point.position() << " depth " << point.depth() << " feature 0x" << std::hex << point.featureId() << std::dec << '\n';
				}

				return out;
			}

			/**
			 * @brief Stringifies the object.
			 * @param obj A reference to the object to print.
			 * @return std::string
			 */
			friend
			std::string
			to_string (const ContactManifold & obj) noexcept
			{
				std::stringstream output;

				output << obj;

				return output.str();
			}

		private:

			Vector< 3, precision_t > m_normal;
			StaticVector< ContactPoint< precision_t >, MaxPoints > m_points;
	};
}
