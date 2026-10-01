/*
 * src/Math/Space3D/Contacts/SphereTriangle.hpp
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
#include <cstdint>
#include <type_traits>

/* Local inclusions for usages. */
#include "Math/Space3D/Sphere.hpp"
#include "Math/Space3D/Triangle.hpp"
#include "Math/Vector.hpp"
#include "ContactManifold.hpp"

/*
 * Sphere ↔ triangle contact generation, and the exact closest point of a triangle shared with capsule ↔ triangle.
 * The triangle is TWO-SIDED: a shallow contact takes its normal from the closest points; a centre lying on the triangle
 * is pushed along the winding normal ((B - A) × (C - A)).
 * Reference: C. Ericson, "Real-Time Collision Detection" (2005), § 5.1.5 (closest point on a triangle by its Voronoi
 * regions). No third-party code.
 */

namespace EmEn::Base::Math::Space3D
{
	namespace TriangleDetail
	{
		/** @brief The Voronoi region of a triangle a closest point lies in (used in feature ids). */
		enum class Region : uint8_t
		{
			VertexA = 1,
			VertexB = 2,
			VertexC = 3,
			EdgeAB = 4,
			EdgeAC = 5,
			EdgeBC = 6,
			Face = 7
		};

		/**
		 * @brief The unit winding normal of a triangle, or false for a degenerate (collinear) one.
		 * @param triangle A reference to the triangle.
		 * @param normal A reference to the normal written.
		 * @return bool
		 */
		template< typename precision_t >
		[[nodiscard]]
		bool
		unitNormal (const Triangle< precision_t > & triangle, Vector< 3, precision_t > & normal) noexcept
		{
			constexpr auto TriangleDegenerate = static_cast< precision_t >(1.0e-12);

			normal = Vector< 3, precision_t >::crossProduct(triangle.pointB() - triangle.pointA(), triangle.pointC() - triangle.pointA());

			const precision_t lengthSquared = normal.lengthSquared();

			if ( lengthSquared <= TriangleDegenerate )
			{
				return false;
			}

			normal *= static_cast< precision_t >(1) / std::sqrt(lengthSquared);

			return true;
		}

		/**
		 * @brief The exact closest point of a triangle to a point (Ericson § 5.1.5).
		 * @pre The triangle is not degenerate (unitNormal() answered true).
		 * @param point A reference to the point.
		 * @param triangle A reference to the triangle.
		 * @param region A reference to the region written.
		 * @return Vector< 3, precision_t >
		 */
		template< typename precision_t >
		[[nodiscard]]
		Vector< 3, precision_t >
		closestPointOnTriangle (const Vector< 3, precision_t > & point, const Triangle< precision_t > & triangle, Region & region) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			const Vec3 & a = triangle.pointA();
			const Vec3 & b = triangle.pointB();
			const Vec3 & c = triangle.pointC();
			const Vec3 ab = b - a;
			const Vec3 ac = c - a;

			const Vec3 ap = point - a;
			const precision_t d1 = Vec3::dotProduct(ab, ap);
			const precision_t d2 = Vec3::dotProduct(ac, ap);

			if ( d1 <= 0 && d2 <= 0 )
			{
				region = Region::VertexA;

				return a;
			}

			const Vec3 bp = point - b;
			const precision_t d3 = Vec3::dotProduct(ab, bp);
			const precision_t d4 = Vec3::dotProduct(ac, bp);

			if ( d3 >= 0 && d4 <= d3 )
			{
				region = Region::VertexB;

				return b;
			}

			const precision_t vc = (d1 * d4) - (d3 * d2);

			/* d1 > 0 >= d3 here (else a vertex region answered), so d1 - d3 > 0. */
			if ( vc <= 0 && d1 >= 0 && d3 <= 0 )
			{
				region = Region::EdgeAB;

				return a + (ab * (d1 / (d1 - d3)));
			}

			const Vec3 cp = point - c;
			const precision_t d5 = Vec3::dotProduct(ab, cp);
			const precision_t d6 = Vec3::dotProduct(ac, cp);

			if ( d6 >= 0 && d5 <= d6 )
			{
				region = Region::VertexC;

				return c;
			}

			const precision_t vb = (d5 * d2) - (d1 * d6);

			if ( vb <= 0 && d2 >= 0 && d6 <= 0 )
			{
				region = Region::EdgeAC;

				return a + (ac * (d2 / (d2 - d6)));
			}

			const precision_t va = (d3 * d6) - (d5 * d4);

			if ( va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0 )
			{
				region = Region::EdgeBC;

				return b + ((c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6))));
			}

			/* Inside the face: va + vb + vc is twice the squared area, positive for a non-degenerate triangle. */
			const precision_t inverse = static_cast< precision_t >(1) / (va + vb + vc);

			region = Region::Face;

			return a + (ab * (vb * inverse)) + (ac * (vc * inverse));
		}
	}

	/**
	 * @brief Generates the contact manifold of a sphere (A) and a triangle (B): one point.
	 * @note The normal points FROM the sphere TO the triangle. A degenerate (collinear) triangle gives no contact.
	 * @note Feature ids: the region of the closest point (`TriangleDetail::Region`, 1-7); 0x100 | 7 for a centre on
	 * the triangle.
	 * @param sphere A reference to the sphere (A). @pre sphere.isValid().
	 * @param triangle A reference to the triangle (B).
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const Sphere< precision_t > & sphere, const Triangle< precision_t > & triangle, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		using Vec3 = Vector< 3, precision_t >;
		using namespace TriangleDetail;

		constexpr auto SphereTriangleSurface = static_cast< precision_t >(1.0e-6);
		constexpr auto SphereTriangleHalf = static_cast< precision_t >(0.5);

		manifold.clear();

		Vec3 faceNormal;

		if ( !unitNormal(triangle, faceNormal) )
		{
			return false;
		}

		const Vec3 & center = sphere.position();
		const precision_t radius = sphere.radius();

		Region region{Region::Face};
		const Vec3 closest = closestPointOnTriangle(center, triangle, region);
		const Vec3 towardsCenter = center - closest;
		const precision_t distanceSquared = towardsCenter.lengthSquared();

		if ( distanceSquared > radius * radius )
		{
			return false;
		}

		if ( distanceSquared > SphereTriangleSurface * SphereTriangleSurface )
		{
			const precision_t distance = std::sqrt(distanceSquared);
			const Vec3 normal = towardsCenter * (static_cast< precision_t >(-1) / distance);

			manifold.setNormal(normal);
			manifold.addPoint({((center + (normal * radius)) + closest) * SphereTriangleHalf, radius - distance, static_cast< uint32_t >(region)});

			return true;
		}

		/* The centre lies on the triangle: it leaves along the winding normal. */
		manifold.setNormal(-faceNormal);
		manifold.addPoint({(center + (center - (faceNormal * radius))) * SphereTriangleHalf, radius, 0x100U | static_cast< uint32_t >(Region::Face)});

		return true;
	}

	/**
	 * @brief Generates the contact manifold of a triangle (A) and a sphere (B): one point.
	 * @note The normal points FROM the triangle TO the sphere; otherwise identical to the sphere ↔ triangle overload.
	 * @param triangle A reference to the triangle (A).
	 * @param sphere A reference to the sphere (B). @pre sphere.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const Triangle< precision_t > & triangle, const Sphere< precision_t > & sphere, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !computeContactManifold(sphere, triangle, manifold) )
		{
			return false;
		}

		manifold.flip();

		return true;
	}
}
