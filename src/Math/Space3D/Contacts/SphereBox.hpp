/*
 * src/Math/Space3D/Contacts/SphereBox.hpp
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
#include <limits>
#include <type_traits>

/* Local inclusions for usages. */
#include "Math/Space3D/OrientedBox.hpp"
#include "Math/Space3D/Sphere.hpp"
#include "Math/Vector.hpp"
#include "ContactManifold.hpp"

/*
 * Sphere ↔ box contact generation: the closest point of the box to the sphere centre (clamping in the box frame),
 * or, with the centre inside the box, the face of least penetration. One point.
 * Reference: C. Ericson, "Real-Time Collision Detection" (2005), § 5.1.4 (closest point on an OBB). No third-party code.
 */

namespace EmEn::Base::Math::Space3D
{
	/**
	 * @brief Generates the contact manifold of a sphere (A) and an oriented box (B): one point.
	 * @note The normal points FROM the sphere TO the box. The point lies halfway between the two surfaces.
	 * @note Feature ids: the box feature closest to the centre, each axis in base 3 (0 inside the slab, 1 under it,
	 * 2 above it) — a face, an edge or a corner; 0x100 | face (axis × 2 + side) when the centre is inside the box.
	 * @param sphere A reference to the sphere (A). @pre sphere.isValid().
	 * @param box A reference to the box (B). @pre box.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const Sphere< precision_t > & sphere, const OrientedBox< precision_t > & box, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		using Vec3 = Vector< 3, precision_t >;

		/* Under this distance the centre is ON the box surface: the outside normal is undefined, the inside one is used. */
		constexpr auto SphereBoxSurface = static_cast< precision_t >(1.0e-6);

		manifold.clear();

		const Vec3 & center = sphere.position();
		const precision_t radius = sphere.radius();
		const Vec3 offset = center - box.center();

		/* The centre in the box frame, then clamped to the box. */
		Vec3 closest = box.center();
		uint32_t region = 0;
		bool inside = true;

		for ( size_t index = 3; index-- > 0; )
		{
			const precision_t local = Vec3::dotProduct(offset, box.axis(index));
			const precision_t extent = box.halfExtent(index);
			const precision_t clamped = std::clamp(local, -extent, extent);

			region = (region * 3U) + ContactsDetail::regionDigit(local, extent);
			inside = inside && local >= -extent && local <= extent;
			closest += box.axis(index) * clamped;
		}

		const Vec3 towardsCenter = center - closest;
		const precision_t distanceSquared = towardsCenter.lengthSquared();

		if ( !inside && distanceSquared > SphereBoxSurface * SphereBoxSurface )
		{
			if ( distanceSquared > radius * radius )
			{
				return false;
			}

			const precision_t distance = std::sqrt(distanceSquared);
			/* From the sphere towards the box. */
			const Vec3 normal = towardsCenter * (static_cast< precision_t >(-1) / distance);
			const Vec3 sphereSurface = center + (normal * radius);

			manifold.setNormal(normal);
			manifold.addPoint({(sphereSurface + closest) * static_cast< precision_t >(0.5), radius - distance, region});

			return true;
		}

		/* The centre is inside (or on) the box: leave through the face of least penetration. */
		size_t faceAxis = 0;
		precision_t faceDistance = std::numeric_limits< precision_t >::max();
		precision_t faceSide = 1;

		for ( size_t index = 0; index < 3; ++index )
		{
			const precision_t local = Vec3::dotProduct(offset, box.axis(index));
			const precision_t distanceToFace = box.halfExtent(index) - std::abs(local);

			if ( distanceToFace < faceDistance )
			{
				faceDistance = distanceToFace;
				faceAxis = index;
				faceSide = local >= 0 ? static_cast< precision_t >(1) : static_cast< precision_t >(-1);
			}
		}

		const Vec3 faceNormal = box.axis(faceAxis) * faceSide;
		const Vec3 facePoint = center + (faceNormal * faceDistance);
		const Vec3 sphereDeepest = center - (faceNormal * radius);
		const auto faceId = static_cast< uint32_t >((faceAxis * 2U) + (faceSide > 0 ? 0U : 1U));

		/* The sphere leaves along +faceNormal, so the normal from the sphere to the box is its opposite. */
		manifold.setNormal(-faceNormal);
		manifold.addPoint({(facePoint + sphereDeepest) * static_cast< precision_t >(0.5), radius + faceDistance, 0x100U | faceId});

		return true;
	}

	/**
	 * @brief Generates the contact manifold of an oriented box (A) and a sphere (B): one point.
	 * @note The normal points FROM the box TO the sphere; otherwise identical to the sphere ↔ box overload.
	 * @param box A reference to the box (A). @pre box.isValid().
	 * @param sphere A reference to the sphere (B). @pre sphere.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const OrientedBox< precision_t > & box, const Sphere< precision_t > & sphere, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !computeContactManifold(sphere, box, manifold) )
		{
			return false;
		}

		manifold.flip();

		return true;
	}
}
