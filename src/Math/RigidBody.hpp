/*
 * src/Math/RigidBody.hpp
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
#include <numbers>
#include <optional>
#include <type_traits>

/* Local inclusions for usages. */
#include "Matrix.hpp"
#include "Quaternion.hpp"
#include "Vector.hpp"

/*
 * Rigid-body helpers (physics overhaul P1): the inertia tensors of the solid primitives about their centre of mass,
 * in their local frame (axes of symmetry on X, Y, Z; the round shapes along Y), the parallel-axis theorem, the
 * cross-product (skew-symmetric) matrix, and the integration of an orientation by a WORLD angular velocity.
 * Reference: any rigid-body text, e.g. D. Baraff, "An Introduction to Physically Based Modeling: Rigid Body Simulation"
 * (SIGGRAPH 1997 course notes); the capsule split as in the usual cylinder + two hemispheres derivation.
 * Inputs from a scene or a file are refused (std::nullopt) when negative or non-finite: a wrong tensor would spin a
 * body silently.
 */

namespace EmEn::Base::Math::RigidBody
{
	namespace Detail
	{
		/** @brief Whether every value is finite and not negative. */
		template< typename precision_t, typename... values_t >
		[[nodiscard]]
		constexpr
		bool
		nonNegative (values_t... values) noexcept
		{
			return ((std::isfinite(values) && values >= static_cast< precision_t >(0)) && ...);
		}

		/** @brief A diagonal 3 × 3 matrix. */
		template< typename precision_t >
		[[nodiscard]]
		Matrix< 3, precision_t >
		diagonal (precision_t x, precision_t y, precision_t z) noexcept
		{
			return Matrix< 3, precision_t >{
				x, 0, 0,
				0, y, 0,
				0, 0, z
			};
		}
	}

	/**
	 * @brief The inertia tensor of a solid box: m (h² + d²) / 12, m (w² + d²) / 12, m (w² + h²) / 12.
	 * @param mass The mass (kg), finite and >= 0.
	 * @param size A reference to the FULL size (width X, height Y, depth Z), finite and >= 0.
	 * @return std::optional< Matrix< 3, precision_t > > Nothing for an invalid input.
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	std::optional< Matrix< 3, precision_t > >
	solidBoxInertia (precision_t mass, const Vector< 3, precision_t > & size) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !Detail::nonNegative< precision_t >(mass, size[X], size[Y], size[Z]) )
		{
			return std::nullopt;
		}

		const precision_t factor = mass / static_cast< precision_t >(12);
		const precision_t xx = size[X] * size[X];
		const precision_t yy = size[Y] * size[Y];
		const precision_t zz = size[Z] * size[Z];

		return Detail::diagonal(factor * (yy + zz), factor * (xx + zz), factor * (xx + yy));
	}

	/**
	 * @brief The inertia tensor of a solid sphere: 2/5 m r² on every axis.
	 * @param mass The mass (kg), finite and >= 0.
	 * @param radius The radius, finite and >= 0.
	 * @return std::optional< Matrix< 3, precision_t > > Nothing for an invalid input.
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	std::optional< Matrix< 3, precision_t > >
	solidSphereInertia (precision_t mass, precision_t radius) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !Detail::nonNegative< precision_t >(mass, radius) )
		{
			return std::nullopt;
		}

		const precision_t moment = static_cast< precision_t >(0.4) * mass * radius * radius;

		return Detail::diagonal(moment, moment, moment);
	}

	/**
	 * @brief The inertia tensor of a solid cylinder along Y: m r² / 2 about Y, m (3 r² + h²) / 12 about X and Z.
	 * @param mass The mass (kg), finite and >= 0.
	 * @param radius The radius, finite and >= 0.
	 * @param height The height along Y, finite and >= 0.
	 * @return std::optional< Matrix< 3, precision_t > > Nothing for an invalid input.
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	std::optional< Matrix< 3, precision_t > >
	solidCylinderInertia (precision_t mass, precision_t radius, precision_t height) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !Detail::nonNegative< precision_t >(mass, radius, height) )
		{
			return std::nullopt;
		}

		const precision_t rr = radius * radius;
		const precision_t across = mass * ((static_cast< precision_t >(3) * rr) + (height * height)) / static_cast< precision_t >(12);

		return Detail::diagonal(across, mass * rr * static_cast< precision_t >(0.5), across);
	}

	/**
	 * @brief The inertia tensor of a solid capsule along Y: a cylinder of height h plus two hemispheres of radius r,
	 * the mass split by volume.
	 * @note About Y: m_c r² / 2 + m_s 2/5 r². About X and Z: m_c (h² / 12 + r² / 4) + m_s (2/5 r² + h² / 4 + 3 h r / 8),
	 * where m_s is the mass of both caps together (each cap's centre of mass lies 3 r / 8 from the cylinder's end).
	 * @param mass The mass (kg), finite and >= 0.
	 * @param radius The radius, finite and >= 0.
	 * @param cylinderHeight The height of the cylindrical part (the axis length), finite and >= 0.
	 * @return std::optional< Matrix< 3, precision_t > > Nothing for an invalid input.
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	std::optional< Matrix< 3, precision_t > >
	solidCapsuleInertia (precision_t mass, precision_t radius, precision_t cylinderHeight) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !Detail::nonNegative< precision_t >(mass, radius, cylinderHeight) )
		{
			return std::nullopt;
		}

		const precision_t rr = radius * radius;
		const precision_t hh = cylinderHeight * cylinderHeight;
		const precision_t cylinderVolume = std::numbers::pi_v< precision_t > * rr * cylinderHeight;
		const precision_t capsVolume = static_cast< precision_t >(4) / static_cast< precision_t >(3) * std::numbers::pi_v< precision_t > * rr * radius;
		const precision_t totalVolume = cylinderVolume + capsVolume;

		if ( totalVolume <= static_cast< precision_t >(0) )
		{
			/* A point mass: no moment. */
			return Detail::diagonal< precision_t >(0, 0, 0);
		}

		const precision_t cylinderMass = mass * (cylinderVolume / totalVolume);
		const precision_t capsMass = mass - cylinderMass;
		const precision_t aboutAxis = (cylinderMass * rr * static_cast< precision_t >(0.5)) + (capsMass * static_cast< precision_t >(0.4) * rr);
		const precision_t across =
			(cylinderMass * ((hh / static_cast< precision_t >(12)) + (rr * static_cast< precision_t >(0.25)))) +
			(capsMass * ((static_cast< precision_t >(0.4) * rr) + (hh * static_cast< precision_t >(0.25)) + (static_cast< precision_t >(0.375) * cylinderHeight * radius)));

		return Detail::diagonal(across, aboutAxis, across);
	}

	/**
	 * @brief Moves an inertia tensor from the centre of mass to a point (parallel-axis theorem): I + m (|d|² E − d dᵀ).
	 * @param inertia A reference to the tensor about the centre of mass.
	 * @param mass The mass (kg).
	 * @param offset A reference to the offset from the centre of mass to the new reference point.
	 * @return Matrix< 3, precision_t >
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	Matrix< 3, precision_t >
	parallelAxis (const Matrix< 3, precision_t > & inertia, precision_t mass, const Vector< 3, precision_t > & offset) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		const precision_t lengthSquared = offset.lengthSquared();
		const precision_t x = offset[X];
		const precision_t y = offset[Y];
		const precision_t z = offset[Z];

		const Matrix< 3, precision_t > shift{
			lengthSquared - (x * x), -(x * y), -(x * z),
			-(y * x), lengthSquared - (y * y), -(y * z),
			-(z * x), -(z * y), lengthSquared - (z * z)
		};

		return inertia + (shift * mass);
	}

	/**
	 * @brief The cross-product matrix of a vector: [v]× · u = v × u.
	 * @param vector A reference to the vector.
	 * @return Matrix< 3, precision_t >
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	Matrix< 3, precision_t >
	skewSymmetric (const Vector< 3, precision_t > & vector) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		return Matrix< 3, precision_t >{
			0, -vector[Z], vector[Y],
			vector[Z], 0, -vector[X],
			-vector[Y], vector[X], 0
		};
	}

	/**
	 * @brief Integrates an orientation by a WORLD angular velocity over a step: q' = exp(ω dt) ∘ q, renormalised.
	 * @note Exact for a constant ω over the step (the exponential map); the rotation is applied in WORLD space (on the
	 * left), which is what a solver's world-space ω means — applying it in the body's local space is the defect of the
	 * engine's former Node::rotateFromPhysics().
	 * @param orientation A reference to the orientation (local → world).
	 * @param worldAngularVelocity A reference to the angular velocity in world space (rad/s).
	 * @param deltaTime The step (s).
	 * @return Quaternion< precision_t >
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	Quaternion< precision_t >
	integrateOrientation (const Quaternion< precision_t > & orientation, const Vector< 3, precision_t > & worldAngularVelocity, precision_t deltaTime) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		Quaternion< precision_t > step;

		step.setFromScaledAxis(worldAngularVelocity * deltaTime);

		return (step * orientation).normalized();
	}
}
