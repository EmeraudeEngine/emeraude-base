/*
 * src/Math/Space3D/Collisions/CapsuleCuboid.hpp
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

/* Local inclusions for usages. */
#include "Math/Space3D/AACuboid.hpp"
#include "Math/Space3D/Capsule.hpp"
#include "Math/Space3D/OrientedBox.hpp"
#include "Math/Space3D/Contacts/CapsuleBox.hpp"

namespace EmEn::Base::Math::Space3D
{
	/**
	 * @brief Helper to clamp a point to the nearest point on/in an AABB.
	 * @tparam precision_t The data precision.
	 * @param point The point to clamp.
	 * @param cuboid The AABB.
	 * @return Point< precision_t > The clamped point.
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	Point< precision_t >
	clampPointToCuboid (const Point< precision_t > & point, const AACuboid< precision_t > & cuboid) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		const auto & min = cuboid.minimum();
		const auto & max = cuboid.maximum();

		return Point< precision_t >{
			std::max(min[X], std::min(point[X], max[X])),
			std::max(min[Y], std::min(point[Y], max[Y])),
			std::max(min[Z], std::min(point[Z], max[Z]))
		};
	}

	/**
	 * @brief Returns an axis-aligned cuboid as an oriented box (world axes), for the contact routines of Contacts/.
	 * @tparam precision_t The data precision.
	 * @param cuboid The AABB.
	 * @return OrientedBox< precision_t >
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	OrientedBox< precision_t >
	toOrientedBox (const AACuboid< precision_t > & cuboid) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		constexpr auto Half = static_cast< precision_t >(0.5);

		return OrientedBox< precision_t >{
			(cuboid.maximum() + cuboid.minimum()) * Half,
			{Vector< 3, precision_t >::positiveX(), Vector< 3, precision_t >::positiveY(), Vector< 3, precision_t >::positiveZ()},
			(cuboid.maximum() - cuboid.minimum()) * Half
		};
	}

	/**
	 * @brief Finds the closest point on a segment to an AABB, and the closest point on the AABB to that segment point.
	 * @note This uses an iterative refinement approach for accuracy.
	 * @tparam precision_t The data precision.
	 * @param capsule The capsule.
	 * @param cuboid The AABB.
	 * @param closestOnAxis Output: closest point on capsule axis.
	 * @param closestOnCuboid Output: closest point on/in cuboid.
	 */
	template< typename precision_t = float >
	void
	closestPointsCapsuleCuboid (const Capsule< precision_t > & capsule, const AACuboid< precision_t > & cuboid, Point< precision_t > & closestOnAxis, Point< precision_t > & closestOnCuboid) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		/* NOTE: EXACT (Contacts' CapsuleBoxDetail::closestOfSegmentAndBox(), a convex piecewise quadratic minimised
		 * piece by piece). It used four alternating projections from the axis centre (2026-10-07). */
		constexpr auto Half = static_cast< precision_t >(0.5);
		constexpr auto Degenerate = static_cast< precision_t >(1.0e-6);

		const auto segmentCenter = (capsule.startPoint() + capsule.endPoint()) * Half;
		auto direction = capsule.endPoint() - capsule.startPoint();
		const auto length = direction.length();
		precision_t halfLength = 0;

		if ( length > Degenerate )
		{
			direction *= static_cast< precision_t >(1) / length;
			halfLength = length * Half;
		}
		else
		{
			direction = Vector< 3, precision_t >::positiveX();
		}

		const auto closest = CapsuleBoxDetail::closestOfSegmentAndBox(segmentCenter, direction, halfLength, toOrientedBox(cuboid));

		closestOnAxis = segmentCenter + (direction * closest.parameter);
		closestOnCuboid = clampPointToCuboid(closestOnAxis, cuboid);
	}

	/**
	 * @brief Checks if a capsule is colliding with an axis-aligned cuboid.
	 * @tparam precision_t The data precision. Default float.
	 * @param capsule A reference to a capsule.
	 * @param cuboid A reference to a cuboid.
	 * @return bool
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	isColliding (const Capsule< precision_t > & capsule, const AACuboid< precision_t > & cuboid) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !capsule.isValid() || !cuboid.isValid() )
		{
			return false;
		}

		/* NOTE: The contact manifold's exact test (Contacts/CapsuleBox.hpp), so both agree on every pair. */
		ContactManifold< precision_t > manifold;

		return computeContactManifold(capsule, toOrientedBox(cuboid), manifold);
	}

	/**
	 * @brief Checks if a capsule is colliding with an axis-aligned cuboid and gives the MTV.
	 * @note The MTV pushes the capsule out of the cuboid (consistent with convention: MTV pushes first arg out of second).
	 * @note From the contact manifold: −normal × the deepest point's depth, for the WHOLE segment. The deep case used to
	 * push only the axis point nearest to the centre out, leaving a tilted capsule's other end inside (2026-10-07).
	 * @tparam precision_t The data precision. Default float.
	 * @param capsule A reference to a capsule.
	 * @param cuboid A reference to a cuboid.
	 * @param minimumTranslationVector A writable reference to a vector.
	 * @return bool
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	isColliding (const Capsule< precision_t > & capsule, const AACuboid< precision_t > & cuboid, Vector< 3, precision_t > & minimumTranslationVector) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !capsule.isValid() || !cuboid.isValid() )
		{
			minimumTranslationVector.reset();

			return false;
		}

		ContactManifold< precision_t > manifold;

		if ( !computeContactManifold(capsule, toOrientedBox(cuboid), manifold) )
		{
			minimumTranslationVector.reset();

			return false;
		}

		/* NOTE: The manifold normal goes from the capsule (A) to the box (B): moving A by −normal × depth frees it. */
		minimumTranslationVector = manifold.normal() * -manifold.maximumDepth();

		return true;
	}

	/** @copydoc EmEn::Base::Math::Space3D::isColliding(const Capsule< precision_t > &, const AACuboid< precision_t > &) noexcept */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	isColliding (const AACuboid< precision_t > & cuboid, const Capsule< precision_t > & capsule) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		return isColliding(capsule, cuboid);
	}

	/**
	 * @brief Checks if a cuboid is colliding with a capsule and gives the minimum translation vector (MTV).
	 * @note The MTV pushes the cuboid out of the capsule (consistent with convention: MTV pushes first arg out of second).
	 * @tparam precision_t The data precision. Default float.
	 * @param cuboid A reference to a cuboid.
	 * @param capsule A reference to a capsule.
	 * @param minimumTranslationVector A writable reference to a vector.
	 * @return bool
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	isColliding (const AACuboid< precision_t > & cuboid, const Capsule< precision_t > & capsule, Vector< 3, precision_t > & minimumTranslationVector) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		/* NOTE: isColliding(capsule, cuboid, mtv) computes MTV to push capsule out of cuboid.
		 * We need the opposite: push cuboid out of capsule, so we negate the MTV. */
		if ( isColliding(capsule, cuboid, minimumTranslationVector) )
		{
			minimumTranslationVector = -minimumTranslationVector;

			return true;
		}

		return false;
	}
}
