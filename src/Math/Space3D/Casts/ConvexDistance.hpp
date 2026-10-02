/*
 * src/Math/Space3D/Casts/ConvexDistance.hpp
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
#include <initializer_list>
#include <limits>
#include <type_traits>

/* Local inclusions for usages. */
#include "Math/Space3D/OrientedBox.hpp"
#include "Math/Space3D/Triangle.hpp"
#include "Math/Vector.hpp"
#include "StaticVector.hpp"

/*
 * The distance and the closest points of two convex polytopes (a point, a segment, a triangle, a box) by the
 * Gilbert-Johnson-Keerthi algorithm: the closest point of the Minkowski difference A − B to the origin, found on a
 * simplex of at most 4 support points (E. G. Gilbert, D. W. Johnson, S. S. Keerthi, "A fast procedure for computing the
 * distance between complex objects in three-dimensional space", 1988; C. Ericson, "Real-Time Collision Detection",
 * 2005, § 9.5 and § 5.1.5-5.1.6 for the simplex's closest points). Rounded shapes (a sphere, a capsule) are a core
 * polytope (a point, a segment) plus a radius: the caller subtracts the radii. What a box's linear cast needs (physics
 * overhaul P5, decision 14). No third-party code.
 */

namespace EmEn::Base::Math::Space3D
{
	/**
	 * @brief A convex polytope given by its vertices (at most 8): a point, a segment, a triangle or a box.
	 * @tparam precision_t The floating point type. Default float.
	 */
	template< typename precision_t = float >
	requires (std::is_floating_point_v< precision_t >)
	class ConvexPolytope final
	{
		public:

			using Vec3 = Vector< 3, precision_t >;

			/** @brief The most vertices (a box). */
			static constexpr size_t MaxVertices{8};

			/** @brief Constructs an empty polytope. */
			constexpr ConvexPolytope () noexcept = default;

			/**
			 * @brief Constructs a point.
			 * @param point A reference to the point.
			 */
			explicit
			ConvexPolytope (const Vec3 & point) noexcept
			{
				m_vertices.push_back(point);
			}

			/**
			 * @brief Constructs a segment.
			 * @param start A reference to its start.
			 * @param end A reference to its end.
			 */
			ConvexPolytope (const Vec3 & start, const Vec3 & end) noexcept
			{
				m_vertices.push_back(start);
				m_vertices.push_back(end);
			}

			/**
			 * @brief Constructs a triangle.
			 * @param triangle A reference to the triangle.
			 */
			explicit
			ConvexPolytope (const Triangle< precision_t > & triangle) noexcept
			{
				m_vertices.push_back(triangle.pointA());
				m_vertices.push_back(triangle.pointB());
				m_vertices.push_back(triangle.pointC());
			}

			/**
			 * @brief Constructs a box (its 8 corners).
			 * @param box A reference to the oriented box.
			 */
			explicit
			ConvexPolytope (const OrientedBox< precision_t > & box) noexcept
			{
				for ( size_t corner = 0; corner < MaxVertices; ++corner )
				{
					m_vertices.push_back(box.corner(corner));
				}
			}

			/** @brief Returns whether it has no vertex. */
			[[nodiscard]]
			bool
			empty () const noexcept
			{
				return m_vertices.empty();
			}

			/** @brief Returns its vertices. */
			[[nodiscard]]
			const StaticVector< Vec3, MaxVertices > &
			vertices () const noexcept
			{
				return m_vertices;
			}

			/**
			 * @brief Returns the vertex farthest along a direction (the first one on a tie: deterministic).
			 * @pre The polytope is not empty.
			 * @param direction A reference to the direction (any length).
			 * @return const Vec3 &
			 */
			[[nodiscard]]
			const Vec3 &
			support (const Vec3 & direction) const noexcept
			{
				size_t best = 0;
				auto bestDot = Vec3::dotProduct(m_vertices[0], direction);

				for ( size_t index = 1; index < m_vertices.size(); ++index )
				{
					const auto dot = Vec3::dotProduct(m_vertices[index], direction);

					if ( dot > bestDot )
					{
						bestDot = dot;
						best = index;
					}
				}

				return m_vertices[best];
			}

			/**
			 * @brief Returns this polytope moved by an offset.
			 * @param offset A reference to the offset.
			 * @return ConvexPolytope
			 */
			[[nodiscard]]
			ConvexPolytope
			translated (const Vec3 & offset) const noexcept
			{
				ConvexPolytope moved;

				for ( const auto & vertex : m_vertices )
				{
					moved.m_vertices.push_back(vertex + offset);
				}

				return moved;
			}

		private:

			StaticVector< Vec3, MaxVertices > m_vertices;
	};

	/**
	 * @brief The closest points of two convex polytopes.
	 * @tparam precision_t The floating point type.
	 */
	template< typename precision_t >
	requires (std::is_floating_point_v< precision_t >)
	struct ClosestPoints final
	{
		/** The point of A closest to B. */
		Vector< 3, precision_t > onA;
		/** The point of B closest to A. */
		Vector< 3, precision_t > onB;
		/** Their distance (0 when they overlap). */
		precision_t distance{0};
		/** Whether they overlap (the points are then meaningless). */
		bool overlapping{false};
	};

	namespace ConvexDistanceDetail
	{
		/** @brief A vertex of the simplex: the support points of A and B, and w = a − b. */
		template< typename precision_t >
		struct SimplexVertex final
		{
			Vector< 3, precision_t > a;
			Vector< 3, precision_t > b;
			Vector< 3, precision_t > w;
		};

		template< typename precision_t >
		using Simplex = StaticVector< SimplexVertex< precision_t >, 4 >;

		template< typename precision_t >
		using Weights = StaticVector< precision_t, 4 >;

		/** @brief Replaces the simplex by the given vertices of it, with their weights. */
		template< typename precision_t >
		void
		keep (Simplex< precision_t > & simplex, Weights< precision_t > & weights, std::initializer_list< size_t > indices, std::initializer_list< precision_t > values) noexcept
		{
			Simplex< precision_t > kept;

			for ( const auto index : indices )
			{
				kept.push_back(simplex[index]);
			}

			simplex = kept;
			weights.clear();

			for ( const auto value : values )
			{
				weights.push_back(value);
			}
		}

		/** @brief The closest point of a segment simplex to the origin (Ericson § 5.1.2). */
		template< typename precision_t >
		void
		solveSegment (Simplex< precision_t > & simplex, Weights< precision_t > & weights) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			const Vec3 & a = simplex[0].w;
			const Vec3 ab = simplex[1].w - a;
			const auto lengthSquared = ab.lengthSquared();

			if ( !(lengthSquared > std::numeric_limits< precision_t >::min()) )
			{
				keep(simplex, weights, {0}, {static_cast< precision_t >(1)});

				return;
			}

			const auto t = -Vec3::dotProduct(a, ab) / lengthSquared;

			if ( t <= 0 )
			{
				keep(simplex, weights, {0}, {static_cast< precision_t >(1)});
			}
			else if ( t >= 1 )
			{
				keep(simplex, weights, {1}, {static_cast< precision_t >(1)});
			}
			else
			{
				weights.clear();
				weights.push_back(1 - t);
				weights.push_back(t);
			}
		}

		/** @brief The closest point of a triangle simplex to the origin, by its Voronoi regions (Ericson § 5.1.5). */
		template< typename precision_t >
		void
		solveTriangle (Simplex< precision_t > & simplex, Weights< precision_t > & weights) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			const Vec3 a = simplex[0].w;
			const Vec3 b = simplex[1].w;
			const Vec3 c = simplex[2].w;
			const Vec3 ab = b - a;
			const Vec3 ac = c - a;

			const auto d1 = -Vec3::dotProduct(ab, a);
			const auto d2 = -Vec3::dotProduct(ac, a);

			if ( d1 <= 0 && d2 <= 0 )
			{
				keep(simplex, weights, {0}, {static_cast< precision_t >(1)});

				return;
			}

			const auto d3 = -Vec3::dotProduct(ab, b);
			const auto d4 = -Vec3::dotProduct(ac, b);

			if ( d3 >= 0 && d4 <= d3 )
			{
				keep(simplex, weights, {1}, {static_cast< precision_t >(1)});

				return;
			}

			const auto vc = (d1 * d4) - (d3 * d2);

			if ( vc <= 0 && d1 >= 0 && d3 <= 0 && (d1 - d3) > 0 )
			{
				const auto v = d1 / (d1 - d3);

				keep(simplex, weights, {0, 1}, {1 - v, v});

				return;
			}

			const auto d5 = -Vec3::dotProduct(ab, c);
			const auto d6 = -Vec3::dotProduct(ac, c);

			if ( d6 >= 0 && d5 <= d6 )
			{
				keep(simplex, weights, {2}, {static_cast< precision_t >(1)});

				return;
			}

			const auto vb = (d5 * d2) - (d1 * d6);

			if ( vb <= 0 && d2 >= 0 && d6 <= 0 && (d2 - d6) > 0 )
			{
				const auto w = d2 / (d2 - d6);

				keep(simplex, weights, {0, 2}, {1 - w, w});

				return;
			}

			const auto va = (d3 * d6) - (d5 * d4);

			if ( va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0 && ((d4 - d3) + (d5 - d6)) > 0 )
			{
				const auto w = (d4 - d3) / ((d4 - d3) + (d5 - d6));

				keep(simplex, weights, {1, 2}, {1 - w, w});

				return;
			}

			const auto sum = va + vb + vc;

			/* A degenerate (flat) triangle: the closest of its edges. */
			if ( !(sum > std::numeric_limits< precision_t >::min()) )
			{
				keep(simplex, weights, {0, 1}, {static_cast< precision_t >(0.5), static_cast< precision_t >(0.5)});
				solveSegment(simplex, weights);

				return;
			}

			const auto inverse = static_cast< precision_t >(1) / sum;
			const auto v = vb * inverse;
			const auto w = vc * inverse;

			weights.clear();
			weights.push_back(1 - v - w);
			weights.push_back(v);
			weights.push_back(w);
		}

		/** @brief The point of the simplex its weights describe. */
		template< typename precision_t >
		[[nodiscard]]
		Vector< 3, precision_t >
		pointOf (const Simplex< precision_t > & simplex, const Weights< precision_t > & weights) noexcept
		{
			Vector< 3, precision_t > point;

			for ( size_t index = 0; index < simplex.size(); ++index )
			{
				point += simplex[index].w * weights[index];
			}

			return point;
		}

		/**
		 * @brief The closest point of a tetrahedron simplex to the origin: the best of the faces the origin is outside
		 * of (Ericson § 5.1.6). Answers false when the origin is inside (the polytopes overlap).
		 */
		template< typename precision_t >
		[[nodiscard]]
		bool
		solveTetrahedron (Simplex< precision_t > & simplex, Weights< precision_t > & weights) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			/* Each face with its opposite vertex. */
			constexpr std::array< std::array< size_t, 4 >, 4 > Faces{{{0, 1, 2, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {1, 3, 2, 0}}};
			/* A tetrahedron whose height over a face is under this fraction of its size is FLAT: its sign tests are
			 * noise (a support point nearly in the plane of the triangle answered "inside" 0.1 m from the origin). */
			constexpr auto FlatFraction = static_cast< precision_t >(1.0e-4);

			precision_t size = 0;

			for ( size_t first = 0; first < 4; ++first )
			{
				for ( size_t second = first + 1; second < 4; ++second )
				{
					size = std::max(size, (simplex[first].w - simplex[second].w).length());
				}
			}

			bool outsideAny = false;
			auto bestDistance = std::numeric_limits< precision_t >::max();
			Simplex< precision_t > bestSimplex;
			Weights< precision_t > bestWeights;

			for ( const auto & [i, j, k, opposite] : Faces )
			{
				const Vec3 & a = simplex[i].w;
				const Vec3 normal = Vec3::crossProduct(simplex[j].w - a, simplex[k].w - a);
				const auto originSide = -Vec3::dotProduct(a, normal);
				const auto oppositeSide = Vec3::dotProduct(simplex[opposite].w - a, normal);

				/* The origin beyond this face (or a flat tetrahedron: every face is a candidate). The height of the opposite
				 * vertex over the face is oppositeSide / |normal|. */
				const bool flat = !(std::abs(oppositeSide) > FlatFraction * size * normal.length());

				if ( originSide * oppositeSide < 0 || flat )
				{
					outsideAny = true;

					Simplex< precision_t > face;

					face.push_back(simplex[i]);
					face.push_back(simplex[j]);
					face.push_back(simplex[k]);

					Weights< precision_t > faceWeights;

					solveTriangle(face, faceWeights);

					const auto distance = pointOf(face, faceWeights).lengthSquared();

					if ( distance < bestDistance )
					{
						bestDistance = distance;
						bestSimplex = face;
						bestWeights = faceWeights;
					}
				}
			}

			if ( !outsideAny )
			{
				return false;
			}

			simplex = bestSimplex;
			weights = bestWeights;

			return true;
		}
	}

	/**
	 * @brief Computes the distance and the closest points of two convex polytopes (GJK).
	 * @note Deterministic: the supports break ties on the first vertex, the iterations are bounded (64).
	 * @param polytopeA A reference to the first polytope (not empty).
	 * @param polytopeB A reference to the second polytope (not empty).
	 * @return ClosestPoints< precision_t > `overlapping` (distance 0) when they intersect or when one is empty.
	 */
	template< typename precision_t >
	[[nodiscard]]
	ClosestPoints< precision_t >
	closestPoints (const ConvexPolytope< precision_t > & polytopeA, const ConvexPolytope< precision_t > & polytopeB) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		using Vec3 = Vector< 3, precision_t >;
		using namespace ConvexDistanceDetail;

		/* The relative convergence of the distance (a hundred ulps: 1e-6 was out of a float's reach, and GJK then added a
		 * nearly coplanar point and left the minimum), and the absolute floor under which the origin is reached. */
		constexpr auto RelativeTolerance = std::numeric_limits< precision_t >::epsilon() * static_cast< precision_t >(100);
		constexpr auto ContactTolerance = static_cast< precision_t >(1.0e-10);
		constexpr size_t MaxIterations{64};

		ClosestPoints< precision_t > result;

		if ( polytopeA.empty() || polytopeB.empty() )
		{
			result.overlapping = true;

			return result;
		}

		Simplex< precision_t > simplex;
		Weights< precision_t > weights;

		simplex.push_back({polytopeA.vertices()[0], polytopeB.vertices()[0], polytopeA.vertices()[0] - polytopeB.vertices()[0]});
		weights.push_back(static_cast< precision_t >(1));

		for ( size_t iteration = 0; iteration < MaxIterations; ++iteration )
		{
			const Vec3 closest = pointOf(simplex, weights);
			const auto distanceSquared = closest.lengthSquared();

			if ( distanceSquared <= ContactTolerance )
			{
				result.overlapping = true;

				return result;
			}

			const Vec3 & a = polytopeA.support(-closest);
			const Vec3 & b = polytopeB.support(closest);
			const Vec3 w = a - b;

			/* No support point brings the simplex closer to the origin: converged. */
			if ( distanceSquared - Vec3::dotProduct(closest, w) <= RelativeTolerance * distanceSquared )
			{
				break;
			}

			/* A repeated support point: no progress is possible (numerical stall). */
			bool repeated = false;

			for ( const auto & vertex : simplex )
			{
				repeated = repeated || (vertex.w - w).lengthSquared() <= ContactTolerance;
			}

			if ( repeated || simplex.full() )
			{
				break;
			}

			/* The simplex before this step: kept when the step does not bring it closer (Ericson § 9.5). */
			const auto previousSimplex = simplex;
			const auto previousWeights = weights;

			simplex.push_back({a, b, w});
			weights.push_back(0);

			switch ( simplex.size() )
			{
				case 2 :
					solveSegment(simplex, weights);
					break;

				case 3 :
					solveTriangle(simplex, weights);
					break;

				default :
					if ( !solveTetrahedron(simplex, weights) )
					{
						result.overlapping = true;

						return result;
					}
					break;
			}

			if ( pointOf(simplex, weights).lengthSquared() >= distanceSquared )
			{
				simplex = previousSimplex;
				weights = previousWeights;

				break;
			}
		}

		for ( size_t index = 0; index < simplex.size(); ++index )
		{
			result.onA += simplex[index].a * weights[index];
			result.onB += simplex[index].b * weights[index];
		}

		result.distance = (result.onA - result.onB).length();

		return result;
	}
}
