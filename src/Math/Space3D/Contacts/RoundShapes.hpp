/*
 * src/Math/Space3D/Contacts/RoundShapes.hpp
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
#include <type_traits>

/* Local inclusions for usages. */
#include "Math/Space3D/Capsule.hpp"
#include "Math/Space3D/Sphere.hpp"
#include "Math/Vector.hpp"
#include "BoxBox.hpp"
#include "ContactManifold.hpp"

/*
 * Contacts between round shapes: sphere ↔ sphere, sphere ↔ capsule, capsule ↔ capsule. A capsule is a segment with a
 * radius, so each pair reduces to the closest points of a point or a segment, then two spheres. Two parallel capsules
 * side by side get two points (the ends of their overlap), so a capsule lying on another does not rock.
 * ⚠️ Coincident centres (or crossing axes) leave the direction undefined: the normal is then +Y. Owner decision
 * (2026-10-01): a fixed, deterministic answer, not one depending on the caller (e.g. the relative velocity).
 * Reference: C. Ericson, "Real-Time Collision Detection" (2005), § 4.3, § 5.1.2, § 5.1.9. No third-party code.
 */

namespace EmEn::Base::Math::Space3D
{
	namespace RoundShapesDetail
	{
		/**
		 * @brief Adds the contact of two spheres (centre, radius) to a manifold, the normal from A to B.
		 * @param centerA A reference to A's centre.
		 * @param radiusA A's radius.
		 * @param centerB A reference to B's centre.
		 * @param radiusB B's radius.
		 * @param fallbackNormal The normal used when the centres coincide.
		 * @param featureId The point's feature id.
		 * @param manifold A reference to the manifold (its normal is set by the first point only).
		 * @return bool False when the spheres are apart.
		 */
		template< typename precision_t >
		[[nodiscard]]
		bool
		addSpherePair (const Vector< 3, precision_t > & centerA, precision_t radiusA, const Vector< 3, precision_t > & centerB, precision_t radiusB, const Vector< 3, precision_t > & fallbackNormal, uint32_t featureId, ContactManifold< precision_t > & manifold) noexcept
		{
			constexpr auto RoundCoincident = static_cast< precision_t >(1.0e-6);

			const auto offset = centerB - centerA;
			const precision_t distanceSquared = offset.lengthSquared();
			const precision_t reach = radiusA + radiusB;

			if ( distanceSquared > reach * reach )
			{
				return false;
			}

			precision_t distance = 0;
			Vector< 3, precision_t > normal = fallbackNormal;

			if ( distanceSquared > RoundCoincident * RoundCoincident )
			{
				distance = std::sqrt(distanceSquared);
				normal = offset * (static_cast< precision_t >(1) / distance);
			}

			if ( manifold.empty() )
			{
				manifold.setNormal(normal);
			}

			const auto surfaceA = centerA + (normal * radiusA);
			const auto surfaceB = centerB - (normal * radiusB);

			return manifold.addPoint({(surfaceA + surfaceB) * static_cast< precision_t >(0.5), reach - distance, featureId});
		}

		/**
		 * @brief The parameter of the closest point of a segment (centre + t·direction, |t| <= halfLength) to a point.
		 */
		template< typename precision_t >
		[[nodiscard]]
		precision_t
		closestParameterOnSegment (const Vector< 3, precision_t > & point, const Vector< 3, precision_t > & center, const Vector< 3, precision_t > & direction, precision_t halfLength) noexcept
		{
			return std::clamp(Vector< 3, precision_t >::dotProduct(point - center, direction), -halfLength, halfLength);
		}

		/**
		 * @brief Splits a capsule into its centre, unit direction and half length (a zero-length one gets +Y).
		 */
		template< typename precision_t >
		void
		decompose (const Capsule< precision_t > & capsule, Vector< 3, precision_t > & center, Vector< 3, precision_t > & direction, precision_t & halfLength) noexcept
		{
			constexpr auto RoundDegenerate = static_cast< precision_t >(1.0e-6);

			center = (capsule.startPoint() + capsule.endPoint()) * static_cast< precision_t >(0.5);
			direction = capsule.endPoint() - capsule.startPoint();

			const precision_t length = direction.length();

			if ( length > RoundDegenerate )
			{
				direction *= static_cast< precision_t >(1) / length;
				halfLength = length * static_cast< precision_t >(0.5);
			}
			else
			{
				direction = Vector< 3, precision_t >{0, 1, 0};
				halfLength = 0;
			}
		}

		/** @brief The region of a segment parameter for the feature ids: 1 the start, 2 the end, 3 inside. */
		template< typename precision_t >
		[[nodiscard]]
		constexpr
		uint32_t
		segmentRegion (precision_t parameter, precision_t halfLength) noexcept
		{
			if ( parameter <= -halfLength )
			{
				return 1U;
			}

			if ( parameter >= halfLength )
			{
				return 2U;
			}

			return 3U;
		}
	}

	/**
	 * @brief Generates the contact manifold of two spheres: one point, the normal from A to B (+Y for coincident centres).
	 * @param sphereA A reference to sphere A. @pre sphereA.isValid().
	 * @param sphereB A reference to sphere B. @pre sphereB.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const Sphere< precision_t > & sphereA, const Sphere< precision_t > & sphereB, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		manifold.clear();

		return RoundShapesDetail::addSpherePair(sphereA.position(), sphereA.radius(), sphereB.position(), sphereB.radius(), Vector< 3, precision_t >{0, 1, 0}, 0U, manifold);
	}

	/**
	 * @brief Generates the contact manifold of a sphere (A) and a capsule (B): one point, the normal from A to B.
	 * @note Feature ids: 1 the capsule's start cap, 2 its end cap, 3 its cylinder.
	 * @param sphere A reference to the sphere (A). @pre sphere.isValid().
	 * @param capsule A reference to the capsule (B). @pre capsule.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const Sphere< precision_t > & sphere, const Capsule< precision_t > & capsule, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		using namespace RoundShapesDetail;

		manifold.clear();

		Vector< 3, precision_t > center;
		Vector< 3, precision_t > direction;
		precision_t halfLength = 0;

		decompose(capsule, center, direction, halfLength);

		const precision_t parameter = closestParameterOnSegment(sphere.position(), center, direction, halfLength);

		return addSpherePair(sphere.position(), sphere.radius(), center + (direction * parameter), capsule.radius(), Vector< 3, precision_t >{0, 1, 0}, segmentRegion(parameter, halfLength), manifold);
	}

	/**
	 * @brief Generates the contact manifold of a capsule (A) and a sphere (B): the sphere ↔ capsule one, flipped.
	 * @param capsule A reference to the capsule (A). @pre capsule.isValid().
	 * @param sphere A reference to the sphere (B). @pre sphere.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const Capsule< precision_t > & capsule, const Sphere< precision_t > & sphere, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !computeContactManifold(sphere, capsule, manifold) )
		{
			return false;
		}

		manifold.flip();

		return true;
	}

	/**
	 * @brief Generates the contact manifold of two capsules: one point, or two for parallel capsules side by side (the
	 * ends of their overlap). The normal points from A to B.
	 * @note Crossing axes (distance 0) take the common perpendicular cross(axis A, axis B) oriented from A's centre to
	 * B's, or +Y when the axes are parallel and coincide.
	 * @note Feature ids: segmentRegion(A) << 4 | segmentRegion(B) for one point; 0x100 | end for the parallel pair.
	 * @param capsuleA A reference to capsule A. @pre capsuleA.isValid().
	 * @param capsuleB A reference to capsule B. @pre capsuleB.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const Capsule< precision_t > & capsuleA, const Capsule< precision_t > & capsuleB, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		using Vec3 = Vector< 3, precision_t >;
		using namespace RoundShapesDetail;

		/* Within ~2.6° two axes are parallel: their overlap is a line, so two points. */
		constexpr auto RoundParallelCosine = static_cast< precision_t >(0.999);
		constexpr auto RoundOverlap = static_cast< precision_t >(1.0e-4);
		constexpr auto RoundCross = static_cast< precision_t >(1.0e-12);

		manifold.clear();

		Vec3 centerA;
		Vec3 directionA;
		precision_t halfLengthA = 0;
		Vec3 centerB;
		Vec3 directionB;
		precision_t halfLengthB = 0;

		decompose(capsuleA, centerA, directionA, halfLengthA);
		decompose(capsuleB, centerB, directionB, halfLengthB);

		/* The fallback for crossing axes: their common perpendicular, from A towards B. */
		Vec3 fallback = Vec3::crossProduct(directionA, directionB);

		if ( fallback.lengthSquared() > RoundCross )
		{
			fallback.normalize();

			if ( Vec3::dotProduct(fallback, centerB - centerA) < 0 )
			{
				fallback = -fallback;
			}
		}
		else
		{
			fallback = Vec3{0, 1, 0};
		}

		/* Parallel capsules side by side: the two ends of the overlap of B's projection on A. */
		if ( halfLengthA > 0 && halfLengthB > 0 && std::abs(Vec3::dotProduct(directionA, directionB)) > RoundParallelCosine )
		{
			const precision_t projectedStart = Vec3::dotProduct((centerB - (directionB * halfLengthB)) - centerA, directionA);
			const precision_t projectedEnd = Vec3::dotProduct((centerB + (directionB * halfLengthB)) - centerA, directionA);
			const precision_t low = std::max(-halfLengthA, std::min(projectedStart, projectedEnd));
			const precision_t high = std::min(halfLengthA, std::max(projectedStart, projectedEnd));

			if ( high - low > RoundOverlap )
			{
				uint32_t end = 0;

				for ( const precision_t parameter : {low, high} )
				{
					const Vec3 onA = centerA + (directionA * parameter);
					const Vec3 onB = centerB + (directionB * closestParameterOnSegment(onA, centerB, directionB, halfLengthB));

					/* A point beyond the reach is skipped; the other may still touch. */
					static_cast< void >(addSpherePair(onA, capsuleA.radius(), onB, capsuleB.radius(), fallback, 0x100U | end, manifold));

					++end;
				}

				return !manifold.empty();
			}
		}

		Vec3 onA;
		Vec3 onB;

		BoxBoxDetail::closestPointsOfSegments(centerA, directionA, halfLengthA, centerB, directionB, halfLengthB, onA, onB);

		const precision_t parameterA = Vec3::dotProduct(onA - centerA, directionA);
		const precision_t parameterB = Vec3::dotProduct(onB - centerB, directionB);
		const uint32_t featureId = (segmentRegion(parameterA, halfLengthA) << 4U) | segmentRegion(parameterB, halfLengthB);

		return addSpherePair(onA, capsuleA.radius(), onB, capsuleB.radius(), fallback, featureId, manifold);
	}
}
