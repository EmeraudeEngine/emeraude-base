/*
 * src/Math/Astronomy.hpp
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
#include <numbers>
#include <string>

namespace EmEn::Base::Math::Astronomy
{
	/**
	 * @file
	 * @brief A planet-generic sky: where a star and the moons stand for an observer, from a Keplerian orbit, an axial
	 * tilt, a rotation and a latitude.
	 * @note EVERYTHING here is in DOUBLE precision, angles in radians, times in seconds. A date of a few years in seconds
	 * (1e8) leaves a float with ~8 s of resolution, which moves the sun by 2 arc-minutes per step: the caller keeps its
	 * time in double and converts the resulting DIRECTIONS to float.
	 * @note The EPOCH (t = 0) is the planet's vernal equinox — the star's ecliptic longitude is 0 — at the observer's
	 * local noon: the star is on the meridian. Earth's numbers make t = 0 the March equinox at noon.
	 * @note The LOCAL FRAME of an observer is East-North-Up, right-handed, unit vectors. The engine maps it onto its
	 * world axes (Y up); nothing here knows about the engine's frame.
	 * @note References: J. Meeus, *Astronomical Algorithms*, 2nd ed., 1998 (ch. 13 the coordinate transformations, ch.
	 * 30 Kepler's equation, ch. 48 the illuminated fraction); the orbital elements of Earth and the Moon from NASA's
	 * planetary fact sheets; the semi-major axes from E. M. Standish, "Keplerian Elements for Approximate Positions of the
	 * Major Planets", JPL (https://ssd.jpl.nasa.gov/planets/approx_pos.html, J2000). The orbits are the GENERIC two-body ones (owner decision 2026-10-24): no perturbation, the
	 * Moon's nodes and perigee do not precess — the Sun stays within ~1° and the Moon within a few degrees of Meeus.
	 */

	/** @brief 2π. */
	constexpr double TwoPi{2.0 * std::numbers::pi};

	/** @brief Seconds in a day of 86 400 s (the unit the presets are written in). */
	constexpr double SecondsPerDay{86400.0};

	/**
	 * @brief Converts degrees to radians.
	 * @param degrees The angle in degrees.
	 * @return double
	 */
	[[nodiscard]]
	constexpr
	double
	radians (double degrees) noexcept
	{
		return degrees * (std::numbers::pi / 180.0);
	}

	/**
	 * @brief Converts radians to degrees.
	 * @param angle The angle in radians.
	 * @return double
	 */
	[[nodiscard]]
	constexpr
	double
	degrees (double angle) noexcept
	{
		return angle * (180.0 / std::numbers::pi);
	}

	/**
	 * @brief The planet the observer stands on: its rotation and its orbit around its star.
	 */
	struct Planet
	{
		/** @brief Rotation period against the stars, in seconds (Earth: 86 164.0905 s). */
		double siderealDay{86164.0905};
		/** @brief Orbital period against the stars, in seconds (Earth: 365.256363 days). 0 = the star does not move
		 * against the stars — the solar day equals the sidereal day (the "simple course" of a scene). */
		double orbitalPeriod{365.256363 * SecondsPerDay};
		/** @brief Obliquity: the angle between the equator and the orbit, in radians (Earth: 23.4393°). */
		double axialTilt{radians(23.4393)};
		/** @brief Eccentricity of the orbit, in [0, 1) (Earth: 0.0167086). */
		double eccentricity{0.0167086};
		/** @brief The star's apparent ecliptic longitude when the planet passes its perihelion, from the vernal equinox,
		 * in radians. Earth: 282.9373° (early January) — ⚠️ the tables give 102.9373°, the PLANET's longitude seen from
		 * the star: the star seen from the planet stands opposite, 180° away. */
		double longitudeOfPerihelion{radians(282.9373)};
		/** @brief Semi-major axis of the orbit, the mean distance to the star, in astronomical units (Earth: 1.00000261 AU,
		 * J2000). The star's illuminance at the planet goes as the inverse square of starDistance(). */
		double semiMajorAxis{1.00000261};
		/** @brief The planet's name, its identity for a consumer that pairs it with other data (a physical sky checks
		 * that its atmosphere belongs to the same planet). Empty = unnamed: nothing is checked. Default "Earth". */
		std::string name{"Earth"};
	};

	/**
	 * @brief A moon's orbit around the planet: circular, inclined on the ecliptic, its nodes fixed.
	 */
	struct MoonOrbit
	{
		/** @brief Orbital period against the stars, in seconds (the Moon: 27.321661 days). */
		double period{27.321661 * SecondsPerDay};
		/** @brief Inclination on the ecliptic, in radians (the Moon: 5.145°). */
		double inclination{radians(5.145)};
		/** @brief Ecliptic longitude of the ascending node, in radians. */
		double ascendingNode{0.0};
		/** @brief Ecliptic longitude at the epoch, in radians: π puts a full moon at the epoch, 0 a new moon. */
		double longitudeAtEpoch{std::numbers::pi};
	};

	/**
	 * @brief Ecliptic coordinates (geocentric for a moon, of the star as seen from the planet).
	 */
	struct Ecliptic
	{
		double longitude{0.0};
		double latitude{0.0};
	};

	/**
	 * @brief Equatorial coordinates.
	 */
	struct Equatorial
	{
		double rightAscension{0.0};
		double declination{0.0};
	};

	/**
	 * @brief A unit direction in the observer's East-North-Up frame.
	 */
	struct Horizontal
	{
		double east{0.0};
		double north{0.0};
		double up{0.0};

		/**
		 * @brief Returns the elevation above the horizon, in radians (negative below it).
		 * @return double
		 */
		[[nodiscard]]
		double
		elevation () const noexcept
		{
			return std::asin(std::clamp(up, -1.0, 1.0));
		}
	};

	/**
	 * @brief Returns the Earth preset.
	 * @return Planet
	 */
	[[nodiscard]]
	inline
	Planet
	earth () noexcept
	{
		return {};
	}

	/**
	 * @brief Returns the Moon preset: Earth's Moon, full at the epoch.
	 * @return MoonOrbit
	 */
	[[nodiscard]]
	inline
	MoonOrbit
	moon () noexcept
	{
		return {};
	}

	/**
	 * @brief Wraps an angle into [0, 2π).
	 * @param angle The angle in radians.
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	wrapAngle (double angle) noexcept
	{
		const auto wrapped = std::fmod(angle, TwoPi);

		return wrapped < 0.0 ? wrapped + TwoPi : wrapped;
	}

	/**
	 * @brief Returns the length of the SOLAR day (the star back on the meridian), in seconds.
	 * @note A prograde rotation and orbit: 1 / solar = 1 / sidereal − 1 / year. Earth: 86 400.0 s, i.e. the sidereal
	 * day is 3 min 56 s shorter.
	 * @param planet A reference to the planet.
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	solarDay (const Planet & planet) noexcept
	{
		if ( planet.orbitalPeriod <= 0.0 )
		{
			return planet.siderealDay;
		}

		return 1.0 / ((1.0 / planet.siderealDay) - (1.0 / planet.orbitalPeriod));
	}

	/**
	 * @brief Kepler's equation for one eccentricity: the anomalies of a point on an elliptic orbit.
	 */
	struct KeplerOrbit
	{
		/** @brief The eccentricity, in [0, 1). */
		double eccentricity{0.0};

		/**
		 * @brief Solves Kepler's equation M = E − e·sin E for the eccentric anomaly E.
		 * @note Newton's iteration from E = M (or π for a very eccentric orbit, Meeus ch. 30), to 1e-14.
		 * @param meanAnomaly M, in radians.
		 * @return double The eccentric anomaly, in radians.
		 */
		[[nodiscard]]
		double
		eccentricAnomaly (double meanAnomaly) const noexcept
		{
			const auto e = std::clamp(eccentricity, 0.0, 0.999999);
			const auto M = wrapAngle(meanAnomaly);
			auto E = e > 0.8 ? std::numbers::pi : M;

			for ( int iteration = 0; iteration < 50; ++iteration )
			{
				const auto delta = (E - (e * std::sin(E)) - M) / (1.0 - (e * std::cos(E)));

				E -= delta;

				if ( std::abs(delta) < 1e-14 )
				{
					break;
				}
			}

			return E;
		}

		/**
		 * @brief Returns the true anomaly from the eccentric one.
		 * @param anomaly E, in radians.
		 * @return double ν, in radians.
		 */
		[[nodiscard]]
		double
		trueAnomaly (double anomaly) const noexcept
		{
			const auto halfE = 0.5 * anomaly;

			return 2.0 * std::atan2(std::sqrt(1.0 + eccentricity) * std::sin(halfE), std::sqrt(1.0 - eccentricity) * std::cos(halfE));
		}

		/**
		 * @brief Returns the mean anomaly matching a true anomaly (the inverse of eccentricAnomaly() + trueAnomaly()).
		 * @param anomaly ν, in radians.
		 * @return double M, in radians.
		 */
		[[nodiscard]]
		double
		meanAnomaly (double anomaly) const noexcept
		{
			const auto halfNu = 0.5 * anomaly;
			const auto E = 2.0 * std::atan2(std::sqrt(1.0 - eccentricity) * std::sin(halfNu), std::sqrt(1.0 + eccentricity) * std::cos(halfNu));

			return E - (eccentricity * std::sin(E));
		}
	};

	/**
	 * @brief Where the observer stands on the planet.
	 */
	struct Observer
	{
		/** @brief Latitude, in radians, positive north. */
		double latitude{0.0};
	};

	/**
	 * @brief Returns the eccentric anomaly of the planet on its orbit at a time.
	 * @note At the epoch λ = 0, i.e. the true anomaly is −ϖ: its mean anomaly is the origin of the mean motion.
	 * @pre planet.orbitalPeriod > 0.
	 * @param planet A reference to the planet.
	 * @param time Seconds since the epoch.
	 * @return double E, in radians.
	 */
	[[nodiscard]]
	inline
	double
	orbitalEccentricAnomaly (const Planet & planet, double time) noexcept
	{
		const KeplerOrbit orbit{.eccentricity = planet.eccentricity};
		const auto meanAnomalyAtEpoch = orbit.meanAnomaly(-planet.longitudeOfPerihelion);

		return orbit.eccentricAnomaly(meanAnomalyAtEpoch + (TwoPi * time / planet.orbitalPeriod));
	}

	/**
	 * @brief Returns the star's ecliptic longitude as seen from the planet at a time.
	 * @note 0 at the epoch by definition (the vernal equinox); it advances non-uniformly on an eccentric orbit (Kepler).
	 * A planet without orbital motion keeps it at 0.
	 * @param planet A reference to the planet.
	 * @param time Seconds since the epoch.
	 * @return double λ, in radians, in [0, 2π).
	 */
	[[nodiscard]]
	inline
	double
	starEclipticLongitude (const Planet & planet, double time) noexcept
	{
		if ( planet.orbitalPeriod <= 0.0 )
		{
			return 0.0;
		}

		const KeplerOrbit orbit{.eccentricity = planet.eccentricity};
		const auto anomaly = orbit.trueAnomaly(orbitalEccentricAnomaly(planet, time));

		return wrapAngle(anomaly + planet.longitudeOfPerihelion);
	}

	/**
	 * @brief Returns the planet's distance to its star at a time, in the unit of Planet::semiMajorAxis (AU).
	 * @note r = a (1 − e cos E), E the eccentric anomaly of the mean motion starEclipticLongitude() follows. Earth:
	 * 0.9833 AU at its perihelion (early January), 1.0167 AU at its aphelion. A planet without orbital motion stays at
	 * its semi-major axis.
	 * @param planet A reference to the planet.
	 * @param time Seconds since the epoch.
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	starDistance (const Planet & planet, double time) noexcept
	{
		if ( planet.orbitalPeriod <= 0.0 )
		{
			return planet.semiMajorAxis;
		}

		const auto eccentricity = std::clamp(planet.eccentricity, 0.0, 0.999999);

		return planet.semiMajorAxis * (1.0 - (eccentricity * std::cos(orbitalEccentricAnomaly(planet, time))));
	}

	/**
	 * @brief Returns a moon's ecliptic coordinates at a time (a circular orbit on an inclined plane).
	 * @param orbit A reference to the moon's orbit.
	 * @param time Seconds since the epoch.
	 * @return Ecliptic
	 */
	[[nodiscard]]
	inline
	Ecliptic
	moonEcliptic (const MoonOrbit & orbit, double time) noexcept
	{
		const auto meanLongitude = orbit.period > 0.0 ? orbit.longitudeAtEpoch + (TwoPi * time / orbit.period) : orbit.longitudeAtEpoch;
		/* The argument of latitude: the angle travelled from the ascending node, in the orbit's plane. */
		const auto argument = meanLongitude - orbit.ascendingNode;

		return {
			.longitude = wrapAngle(orbit.ascendingNode + std::atan2(std::cos(orbit.inclination) * std::sin(argument), std::cos(argument))),
			.latitude = std::asin(std::sin(orbit.inclination) * std::sin(argument))
		};
	}

	/**
	 * @brief Converts ecliptic coordinates to equatorial ones (Meeus 13.3, 13.4).
	 * @param ecliptic The ecliptic coordinates.
	 * @param axialTilt The obliquity, in radians.
	 * @return Equatorial
	 */
	[[nodiscard]]
	inline
	Equatorial
	toEquatorial (const Ecliptic & ecliptic, double axialTilt) noexcept
	{
		const auto sinLongitude = std::sin(ecliptic.longitude);
		const auto cosTilt = std::cos(axialTilt);
		const auto sinTilt = std::sin(axialTilt);

		return {
			.rightAscension = wrapAngle(std::atan2((sinLongitude * cosTilt) - (std::tan(ecliptic.latitude) * sinTilt), std::cos(ecliptic.longitude))),
			.declination = std::asin(std::clamp((std::sin(ecliptic.latitude) * cosTilt) + (std::cos(ecliptic.latitude) * sinTilt * sinLongitude), -1.0, 1.0))
		};
	}

	/**
	 * @brief Returns the local sidereal angle — the right ascension on the observer's meridian — at a time.
	 * @note 0 at the epoch: the star (right ascension 0 at the vernal equinox) is on the meridian, i.e. it is noon.
	 * @param planet A reference to the planet.
	 * @param time Seconds since the epoch.
	 * @return double θ, in radians, in [0, 2π).
	 */
	[[nodiscard]]
	inline
	double
	siderealAngle (const Planet & planet, double time) noexcept
	{
		return wrapAngle(TwoPi * time / planet.siderealDay);
	}

	/**
	 * @brief Returns the direction of a body in the observer's East-North-Up frame (Meeus 13.5, 13.6).
	 * @param position The body's equatorial coordinates.
	 * @param localSiderealAngle The local sidereal angle (siderealAngle()), in radians.
	 * @param observer A reference to the observer.
	 * @return Horizontal
	 */
	[[nodiscard]]
	inline
	Horizontal
	toHorizontal (const Equatorial & position, double localSiderealAngle, const Observer & observer) noexcept
	{
		const auto latitude = observer.latitude;
		/* The hour angle: 0 on the meridian, growing westward. */
		const auto hourAngle = localSiderealAngle - position.rightAscension;
		const auto cosDeclination = std::cos(position.declination);
		const auto sinDeclination = std::sin(position.declination);
		const auto cosHour = std::cos(hourAngle);

		return {
			.east = -cosDeclination * std::sin(hourAngle),
			.north = (std::cos(latitude) * sinDeclination) - (std::sin(latitude) * cosDeclination * cosHour),
			.up = (std::sin(latitude) * sinDeclination) + (std::cos(latitude) * cosDeclination * cosHour)
		};
	}

	/**
	 * @brief Returns the direction of the celestial pole of the planet's northern hemisphere, in the observer's frame.
	 * @note The axis the stars turn around (counter-clockwise, seen toward it, at the sidereal rate).
	 * @param observer A reference to the observer.
	 * @return Horizontal
	 */
	[[nodiscard]]
	inline
	Horizontal
	celestialPole (const Observer & observer) noexcept
	{
		return {.east = 0.0, .north = std::cos(observer.latitude), .up = std::sin(observer.latitude)};
	}

	/**
	 * @brief Returns the star's direction for an observer at a time.
	 * @param planet A reference to the planet.
	 * @param observer A reference to the observer.
	 * @param time Seconds since the epoch.
	 * @return Horizontal
	 */
	[[nodiscard]]
	inline
	Horizontal
	starDirection (const Planet & planet, const Observer & observer, double time) noexcept
	{
		const auto position = toEquatorial({.longitude = starEclipticLongitude(planet, time), .latitude = 0.0}, planet.axialTilt);

		return toHorizontal(position, siderealAngle(planet, time), observer);
	}

	/**
	 * @brief Returns a moon's direction for an observer at a time.
	 * @note The moon's parallax (an observer on the surface, not at the planet's centre) is neglected: ~1° for the Moon.
	 * @param planet A reference to the planet.
	 * @param orbit A reference to the moon's orbit.
	 * @param observer A reference to the observer.
	 * @param time Seconds since the epoch.
	 * @return Horizontal
	 */
	[[nodiscard]]
	inline
	Horizontal
	moonDirection (const Planet & planet, const MoonOrbit & orbit, const Observer & observer, double time) noexcept
	{
		const auto position = toEquatorial(moonEcliptic(orbit, time), planet.axialTilt);

		return toHorizontal(position, siderealAngle(planet, time), observer);
	}

	/**
	 * @brief Returns the illuminated fraction of a moon's disc, in [0, 1] (Meeus 48.1, the star at infinity).
	 * @note From the elongation ψ, the angle between the star and the moon seen from the observer: k = (1 − cos ψ) / 2.
	 * 0 = new, 0.5 = quarter, 1 = full.
	 * @param towardStar The direction toward the star (any frame, unit).
	 * @param towardMoon The direction toward the moon (the same frame, unit).
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	illuminatedFraction (const Horizontal & towardStar, const Horizontal & towardMoon) noexcept
	{
		const auto cosElongation = std::clamp((towardStar.east * towardMoon.east) + (towardStar.north * towardMoon.north) + (towardStar.up * towardMoon.up), -1.0, 1.0);

		return 0.5 * (1.0 - cosElongation);
	}

	/**
	 * @brief Returns a moon's synodic period — new moon to new moon — in seconds (Earth's Moon: 29.53 days).
	 * @param planet A reference to the planet.
	 * @param orbit A reference to the moon's orbit.
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	synodicPeriod (const Planet & planet, const MoonOrbit & orbit) noexcept
	{
		if ( planet.orbitalPeriod <= 0.0 )
		{
			return orbit.period;
		}

		return 1.0 / ((1.0 / orbit.period) - (1.0 / planet.orbitalPeriod));
	}

	/**
	 * @brief Returns the pole of a moon's orbit for an observer at a time: the axis its face is oriented by.
	 * @note A tidally locked moon keeps its spin axis close to its orbit's normal (the Moon's: within ~7°, Cassini's
	 * laws); the orbit's nodes do not precess here (see the file's note). The moon lies in the plane this pole is the
	 * normal of.
	 * @param planet A reference to the planet.
	 * @param orbit A reference to the moon's orbit.
	 * @param observer A reference to the observer.
	 * @param time Seconds since the epoch.
	 * @return Horizontal
	 */
	[[nodiscard]]
	inline
	Horizontal
	moonOrbitPole (const Planet & planet, const MoonOrbit & orbit, const Observer & observer, double time) noexcept
	{
		/* The orbit's normal in ecliptic coordinates: the ecliptic pole tilted by the inclination about the line of nodes,
		 * (sin i sin Ω, −sin i cos Ω, cos i), i.e. the latitude 90° − i at the longitude Ω − 90°. */
		const auto position = toEquatorial({.longitude = orbit.ascendingNode - (0.5 * std::numbers::pi), .latitude = (0.5 * std::numbers::pi) - orbit.inclination}, planet.axialTilt);

		return toHorizontal(position, siderealAngle(planet, time), observer);
	}

	/* ---- The photometry of a moon's surface: the Hapke–Lommel-Seeliger reflectance of H. W. Jensen, F. Durand, J.
	 * Dorsey, M. M. Stark, P. Shirley, S. Premože, "A Physically-Based Night Sky Model", SIGGRAPH 2001, § 4.2 (eq. 2 to
	 * 4) and § 4.3 (eq. 1 and 5), after B. Hapke's lunar reflectance. A regolith reflects light back toward its source
	 * (the opposition surge: a full moon is much brighter than two quarters) and shows no limb darkening when full. ---- */

	/** @brief The lunar surface's density parameter g of the retrodirective function (Jensen et al. use 0.6; 0.4 for the
	 * rays to 0.8 for the craters). */
	constexpr double LunarSurfaceDensity{0.6};

	/** @brief The lunar surface's forward-scattering amount t (Jensen et al.: 0.1, fitted to Rougier's measurements). */
	constexpr double LunarForwardScattering{0.1};

	/**
	 * @brief The illuminance the full PLANET pours on its moon, relative to the star's: its geometric albedo × (its
	 * radius / the moon's distance)². Earth's: 0.434 (NASA's planetary fact sheet, visual geometric albedo) × (6 371 km /
	 * 384 400 km)² ≈ 1.19e-4 — the earthshine, ~16 lx under a 134 klx Sun (Jensen et al.'s eq. 1: 0.095 W/m², the same
	 * order).
	 */
	constexpr double EarthshineRatio{0.434 * (6371.0 / 384400.0) * (6371.0 / 384400.0)};

	/**
	 * @brief Returns the retrodirective function B(φ, g) of the lunar reflectance (Jensen et al., eq. 3).
	 * @note 2 at zero phase (the opposition surge), 1 from a right angle on.
	 * @param phaseAngle The phase angle φ, the angle star–surface–observer, in radians.
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	lunarRetrodirective (double phaseAngle) noexcept
	{
		const auto phase = std::abs(phaseAngle);

		if ( phase >= 0.5 * std::numbers::pi )
		{
			return 1.0;
		}

		const auto tanPhase = std::tan(phase);

		/* The limit at zero phase: tan φ / 2g × (1 − e^(−g / tan φ)) × (3 − e^(−g / tan φ)) → 0. */
		if ( !(tanPhase > 1e-12) )
		{
			return 2.0;
		}

		const auto decay = std::exp(-LunarSurfaceDensity / tanPhase);

		return 2.0 - ((tanPhase / (2.0 * LunarSurfaceDensity)) * (1.0 - decay) * (3.0 - decay));
	}

	/**
	 * @brief Returns the scattering law S(φ) of the lunar surface's particles (Jensen et al., eq. 4).
	 * @param phaseAngle The phase angle φ, in radians.
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	lunarScattering (double phaseAngle) noexcept
	{
		const auto phase = std::min(std::abs(phaseAngle), std::numbers::pi);
		const auto cosPhase = std::cos(phase);
		const auto forward = 1.0 - (0.5 * cosPhase);

		return ((std::sin(phase) + ((std::numbers::pi - phase) * cosPhase)) / std::numbers::pi) + (LunarForwardScattering * forward * forward);
	}

	/**
	 * @brief Returns the Lommel-Seeliger kernel μ0 / (μ0 + μ): the only part of the lunar reflectance that changes over a
	 * disc lit by a distant star and seen from a distant observer (lunarReflectance() multiplies it by factors of the
	 * phase angle alone).
	 * @note 1/2 at zero phase everywhere on the disc: no limb darkening when full. 0 where the element faces away from
	 * the light or from the observer.
	 * @param cosIncidence cos θi (μ0), between the surface normal and the light.
	 * @param cosEmission cos θr (μ), between the surface normal and the observer.
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	lommelSeeligerKernel (double cosIncidence, double cosEmission) noexcept
	{
		if ( !(cosIncidence > 0.0) || !(cosEmission > 0.0) )
		{
			return 0.0;
		}

		return cosIncidence / (cosIncidence + cosEmission);
	}

	/**
	 * @brief Returns the lunar REFLECTANCE: the radiance a surface element sends toward the observer per unit albedo and
	 * per unit illuminance normal to the light (Jensen et al., eq. 2: f = 2 / 3π × B × S / (1 + cos θr / cos θi)).
	 * @note The radiance is `albedo × lunarReflectance() × E⊥`: the cos θi of the incidence is INSIDE it (the
	 * Lommel-Seeliger kernel, lommelSeeligerKernel()), so a full moon is uniform from its centre to its limb. 0 where
	 * the element faces away from the light or from the observer.
	 * @param cosIncidence cos θi, between the surface normal and the light.
	 * @param cosEmission cos θr, between the surface normal and the observer.
	 * @param phaseAngle The phase angle φ, in radians.
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	lunarReflectance (double cosIncidence, double cosEmission, double phaseAngle) noexcept
	{
		return (2.0 / (3.0 * std::numbers::pi)) * lunarRetrodirective(phaseAngle) * lunarScattering(phaseAngle) * lommelSeeligerKernel(cosIncidence, cosEmission);
	}

	/**
	 * @brief Returns the Lommel-Seeliger phase law of a sphere: its disc-integrated brightness at a phase angle, relative
	 * to full (Jensen et al., eq. 1 and 5: 1 − sin(φ/2) tan(φ/2) ln(cot(φ/4))).
	 * @note The exact integral of μ0 / (μ0 + μ) over the visible disc, divided by its value at zero phase (π / 2). 1 at
	 * zero phase, 0 at π.
	 * @param phaseAngle The phase angle φ, in radians.
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	lommelSeeligerPhaseLaw (double phaseAngle) noexcept
	{
		const auto phase = std::abs(phaseAngle);

		if ( !(phase > 1e-9) )
		{
			return 1.0;
		}

		if ( phase >= std::numbers::pi - 1e-9 )
		{
			return 0.0;
		}

		const auto half = 0.5 * phase;

		return std::clamp(1.0 - (std::sin(half) * std::tan(half) * std::log(1.0 / std::tan(0.25 * phase))), 0.0, 1.0);
	}

	/**
	 * @brief Returns a moon's PHASE LAW: its disc-integrated brightness relative to full, from the lunar reflectance.
	 * @note lunarReflectance() integrated over the visible disc: B(φ) and S(φ) do not depend on the surface element, so the
	 * integral is [B(φ) S(φ) / B(0) S(0)] × lommelSeeligerPhaseLaw(φ) — exact, no quadrature. The quarter (φ = 90°)
	 * gives 0.077; the measured Moon 0.083 (V −10.0 against −12.7 at full). A Lambert sphere gives 0.32: the
	 * opposition surge is the difference.
	 * @param phaseAngle The phase angle φ, in radians.
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	lunarPhaseLaw (double phaseAngle) noexcept
	{
		return (lunarRetrodirective(phaseAngle) * lunarScattering(phaseAngle) * lommelSeeligerPhaseLaw(phaseAngle)) / (lunarRetrodirective(0.0) * lunarScattering(0.0));
	}

	/**
	 * @brief Returns a moon's phase angle from the elongation: φ = π − ψ, the star at infinity.
	 * @param towardStar The direction toward the star (any frame, unit).
	 * @param towardMoon The direction toward the moon (the same frame, unit).
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	phaseAngle (const Horizontal & towardStar, const Horizontal & towardMoon) noexcept
	{
		const auto cosElongation = std::clamp((towardStar.east * towardMoon.east) + (towardStar.north * towardMoon.north) + (towardStar.up * towardMoon.up), -1.0, 1.0);

		return std::numbers::pi - std::acos(cosElongation);
	}
}
