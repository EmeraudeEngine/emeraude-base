/*
 * src/Math/SolarSystem.hpp
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
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>

/* Local inclusions. */
#include "Math/Astronomy.hpp"
#include "Math/Vector.hpp"

namespace EmEn::Base::Math::SolarSystem
{
	/**
	 * @file
	 * @brief OUR solar system at a real instant: the calendar, the sidereal time, the precession, the planets' and the
	 * Moon's positions for an observer on Earth (base item of the engine's real-date celestial clock, owner decisions
	 * 2026-10-26). Math::Astronomy is the planet-generic two-body model; this is the real sky's ephemeris.
	 * @note EVERYTHING in DOUBLE, angles in radians unless a name says degrees. Instants are Julian days: UT (the Earth's
	 * rotation, the civil clock) for the sidereal time, TT (the ephemerides' uniform time) = UT + ΔT for the positions.
	 * @note FRAMES: "J2000" is the mean equator and equinox of J2000.0 (the star catalogues'); "of date" is the mean
	 * equator and equinox of the instant (what the sidereal time turns). Nutation (~17″) and the annual aberration (~20″)
	 * are not applied: the positions are GEOMETRIC, mean of date.
	 * @note Sources, all public: the planets from E. M. Standish, "Keplerian Elements for Approximate Positions of the
	 * Major Planets", JPL/Caltech, Table 1 (1800–2050, J2000 ecliptic; https://ssd.jpl.nasa.gov/planets/approx_pos.html);
	 * the Moon from P. Schlyter, "How to compute planetary positions" (https://stjarnhimlen.se/comp/ppcomp.html: mean
	 * elements and the main perturbations, ~1-2′); the sidereal time IAU 1982 and the obliquity IAU 1980 (Meeus,
	 * *Astronomical Algorithms*, 2nd ed., 12.4 and 22.2); the precession IAU 1976 (Lieske et al., A&A 58, 1977); ΔT from
	 * F. Espenak & J. Meeus's polynomials (NASA, "Polynomial Expressions for Delta T", 2006).
	 */

	/** @brief The Julian day of J2000.0 (2000-01-01 12:00 TT). */
	constexpr double JulianDayJ2000{2451545.0};

	/** @brief The Julian day of the Unix epoch (1970-01-01 00:00 UTC). */
	constexpr double JulianDayUnixEpoch{2440587.5};

	/** @brief Days in a Julian century. */
	constexpr double DaysPerJulianCentury{36525.0};

	/** @brief The light time for one astronomical unit, in days (499.004784 s). */
	constexpr double LightDaysPerAU{499.004784 / Astronomy::SecondsPerDay};

	/** @brief The astronomical unit, in kilometres (IAU 2012). */
	constexpr double KilometresPerAU{149597870.7};

	/** @brief Earth's equatorial radius, in kilometres (WGS 84). */
	constexpr double EarthRadiusKilometres{6378.137};

	/** @brief Earth's mass over the Moon's: the barycentre sits 1 / (1 + this) of the way to the Moon. */
	constexpr double EarthMoonMassRatio{81.30056};

	/**
	 * @brief The bodies of the planetary ephemeris (Standish's Table 1 rows).
	 */
	enum class Body : uint8_t
	{
		Mercury,
		Venus,
		/** @brief The Earth-Moon barycentre: Earth's orbit (planet() refuses it as a target). */
		EarthMoonBarycentre,
		Mars,
		Jupiter,
		Saturn,
		Uranus,
		Neptune,
		Pluto
	};

	/** @brief The number of bodies in the ephemeris. */
	constexpr size_t BodyCount{static_cast< size_t >(Body::Pluto) + 1};

	/**
	 * @brief A set of osculating elements (Standish's units).
	 */
	struct OrbitalElements
	{
		/** @brief Semi-major axis, in AU. */
		double semiMajorAxis{0.0};
		/** @brief Eccentricity. */
		double eccentricity{0.0};
		/** @brief Inclination on the J2000 ecliptic, in degrees. */
		double inclination{0.0};
		/** @brief Mean longitude, in degrees. */
		double meanLongitude{0.0};
		/** @brief Longitude of the perihelion, in degrees. */
		double perihelionLongitude{0.0};
		/** @brief Longitude of the ascending node, in degrees. */
		double nodeLongitude{0.0};
	};

	/**
	 * @brief A body's elements at J2000 and their rates per Julian century.
	 */
	struct KeplerianElements
	{
		OrbitalElements atJ2000{};
		OrbitalElements perCentury{};
	};

	/** @brief Standish's Table 1 (valid 1800–2050), in the order of Body. */
	constexpr std::array< KeplerianElements, BodyCount > ApproximateElements{{
		/* Mercury. */
		{
			.atJ2000 = {.semiMajorAxis = 0.38709927, .eccentricity = 0.20563593, .inclination = 7.00497902, .meanLongitude = 252.25032350, .perihelionLongitude = 77.45779628, .nodeLongitude = 48.33076593},
			.perCentury = {.semiMajorAxis = 0.00000037, .eccentricity = 0.00001906, .inclination = -0.00594749, .meanLongitude = 149472.67411175, .perihelionLongitude = 0.16047689, .nodeLongitude = -0.12534081}
		},
		/* Venus. */
		{
			.atJ2000 = {.semiMajorAxis = 0.72333566, .eccentricity = 0.00677672, .inclination = 3.39467605, .meanLongitude = 181.97909950, .perihelionLongitude = 131.60246718, .nodeLongitude = 76.67984255},
			.perCentury = {.semiMajorAxis = 0.00000390, .eccentricity = -0.00004107, .inclination = -0.00078890, .meanLongitude = 58517.81538729, .perihelionLongitude = 0.00268329, .nodeLongitude = -0.27769418}
		},
		/* The Earth-Moon barycentre. */
		{
			.atJ2000 = {.semiMajorAxis = 1.00000261, .eccentricity = 0.01671123, .inclination = -0.00001531, .meanLongitude = 100.46457166, .perihelionLongitude = 102.93768193, .nodeLongitude = 0.0},
			.perCentury = {.semiMajorAxis = 0.00000562, .eccentricity = -0.00004392, .inclination = -0.01294668, .meanLongitude = 35999.37244981, .perihelionLongitude = 0.32327364, .nodeLongitude = 0.0}
		},
		/* Mars. */
		{
			.atJ2000 = {.semiMajorAxis = 1.52371034, .eccentricity = 0.09339410, .inclination = 1.84969142, .meanLongitude = -4.55343205, .perihelionLongitude = -23.94362959, .nodeLongitude = 49.55953891},
			.perCentury = {.semiMajorAxis = 0.00001847, .eccentricity = 0.00007882, .inclination = -0.00813131, .meanLongitude = 19140.30268499, .perihelionLongitude = 0.44441088, .nodeLongitude = -0.29257343}
		},
		/* Jupiter. */
		{
			.atJ2000 = {.semiMajorAxis = 5.20288700, .eccentricity = 0.04838624, .inclination = 1.30439695, .meanLongitude = 34.39644051, .perihelionLongitude = 14.72847983, .nodeLongitude = 100.47390909},
			.perCentury = {.semiMajorAxis = -0.00011607, .eccentricity = -0.00013253, .inclination = -0.00183714, .meanLongitude = 3034.74612775, .perihelionLongitude = 0.21252668, .nodeLongitude = 0.20469106}
		},
		/* Saturn. */
		{
			.atJ2000 = {.semiMajorAxis = 9.53667594, .eccentricity = 0.05386179, .inclination = 2.48599187, .meanLongitude = 49.95424423, .perihelionLongitude = 92.59887831, .nodeLongitude = 113.66242448},
			.perCentury = {.semiMajorAxis = -0.00125060, .eccentricity = -0.00050991, .inclination = 0.00193609, .meanLongitude = 1222.49362201, .perihelionLongitude = -0.41897216, .nodeLongitude = -0.28867794}
		},
		/* Uranus. */
		{
			.atJ2000 = {.semiMajorAxis = 19.18916464, .eccentricity = 0.04725744, .inclination = 0.77263783, .meanLongitude = 313.23810451, .perihelionLongitude = 170.95427630, .nodeLongitude = 74.01692503},
			.perCentury = {.semiMajorAxis = -0.00196176, .eccentricity = -0.00004397, .inclination = -0.00242939, .meanLongitude = 428.48202785, .perihelionLongitude = 0.40805281, .nodeLongitude = 0.04240589}
		},
		/* Neptune. */
		{
			.atJ2000 = {.semiMajorAxis = 30.06992276, .eccentricity = 0.00859048, .inclination = 1.77004347, .meanLongitude = -55.12002969, .perihelionLongitude = 44.96476227, .nodeLongitude = 131.78422574},
			.perCentury = {.semiMajorAxis = 0.00026291, .eccentricity = 0.00005105, .inclination = 0.00035372, .meanLongitude = 218.45945325, .perihelionLongitude = -0.32241464, .nodeLongitude = -0.00508664}
		},
		/* Pluto. */
		{
			.atJ2000 = {.semiMajorAxis = 39.48211675, .eccentricity = 0.24882730, .inclination = 17.14001206, .meanLongitude = 238.92903833, .perihelionLongitude = 224.06891629, .nodeLongitude = 110.30393684},
			.perCentury = {.semiMajorAxis = -0.00031596, .eccentricity = 0.00005170, .inclination = 0.00004818, .meanLongitude = 145.20780515, .perihelionLongitude = -0.04062942, .nodeLongitude = -0.01183482}
		}
	}};

	/**
	 * @brief A rotation as its three rows: v' = (row0·v, row1·v, row2·v).
	 */
	struct Rotation
	{
		std::array< Vector< 3, double >, 3 > rows{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};

		/**
		 * @brief Rotates a vector.
		 * @param vector The vector.
		 * @return Vector< 3, double >
		 */
		[[nodiscard]]
		Vector< 3, double >
		operator() (const Vector< 3, double > & vector) const noexcept
		{
			return {Vector< 3, double >::dotProduct(rows[0], vector), Vector< 3, double >::dotProduct(rows[1], vector), Vector< 3, double >::dotProduct(rows[2], vector)};
		}
	};

	/**
	 * @brief A body seen from the centre of the Earth.
	 */
	struct GeocentricPosition
	{
		/** @brief Unit vector toward it, mean equator and equinox OF DATE (x toward the equinox, z toward the pole). */
		Vector< 3, double > direction{1.0, 0.0, 0.0};
		/** @brief Its distance from the Earth's centre, in AU. */
		double distance{1.0};
		/** @brief Its distance from the Sun, in AU (0 for the Sun). */
		double heliocentricDistance{0.0};
		/** @brief The phase angle Sun-body-Earth, in radians (0 for the Sun). */
		double phaseAngle{0.0};
		/** @brief The light time from the body, in days (a planet; 0 otherwise): its look is the one it had then. */
		double lightTime{0.0};
	};

	/**
	 * @brief A Gregorian calendar instant.
	 */
	struct CalendarDate
	{
		int year{2000};
		int month{1};
		int day{1};
		int hour{12};
		int minute{0};
		/** @brief In [0, 60). */
		double second{0.0};
	};

	/**
	 * @brief Returns the Julian day of a Gregorian calendar instant (Meeus 7.1).
	 * @param date The instant: the year 1583 to 9999 (the Gregorian calendar), the month 1 to 12, the day 1 to the
	 * month's length, the hour 0 to 23, the minute 0 to 59, the second in [0, 61) (a leap second accepted).
	 * @return std::optional< double > Nothing for a field out of its range.
	 */
	[[nodiscard]]
	inline
	std::optional< double >
	julianDay (const CalendarDate & date) noexcept
	{
		constexpr std::array< int, 12 > MonthLengths{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

		const auto [year, month, day, hour, minute, second] = date;
		/* A NaN second fails both comparisons: refused. */
		const bool secondInRange = second >= 0.0 && second < 61.0;

		if ( year < 1583 || year > 9999 || month < 1 || month > 12 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || !secondInRange )
		{
			return std::nullopt;
		}

		const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
		/* month is in [1, 12]: the index is in range. */
		const auto length = *std::next(MonthLengths.cbegin(), month - 1) + (month == 2 && leap ? 1 : 0);

		if ( day < 1 || day > length )
		{
			return std::nullopt;
		}

		auto y = static_cast< double >(year);
		auto m = static_cast< double >(month);

		if ( month <= 2 )
		{
			y -= 1.0;
			m += 12.0;
		}

		const auto centuries = std::floor(y / 100.0);
		const auto gregorian = 2.0 - centuries + std::floor(centuries / 4.0);
		const auto fraction = (static_cast< double >(hour) + (static_cast< double >(minute) / 60.0) + (second / 3600.0)) / 24.0;

		return std::floor(365.25 * (y + 4716.0)) + std::floor(30.6001 * (m + 1.0)) + static_cast< double >(day) + fraction + gregorian - 1524.5;
	}

	/**
	 * @brief Returns the Gregorian calendar instant of a Julian day (Meeus ch. 7, the inverse).
	 * @param julianDay The Julian day (1583-01-01 to 9999-12-31: the Gregorian calendar, as julianDay() accepts).
	 * @return std::optional< CalendarDate > Nothing for a non-finite day or one out of that range.
	 */
	[[nodiscard]]
	inline
	std::optional< CalendarDate >
	calendarDate (double julianDay) noexcept
	{
		if ( !std::isfinite(julianDay) || julianDay < 2299238.5 || julianDay >= 5373484.5 )
		{
			return std::nullopt;
		}

		const auto shifted = julianDay + 0.5;
		const auto Z = std::floor(shifted);
		const auto fraction = shifted - Z;
		const auto alpha = std::floor((Z - 1867216.25) / 36524.25);
		const auto A = Z + 1.0 + alpha - std::floor(alpha / 4.0);
		const auto B = A + 1524.0;
		const auto C = std::floor((B - 122.1) / 365.25);
		const auto D = std::floor(365.25 * C);
		const auto E = std::floor((B - D) / 30.6001);
		const auto month = E < 14.0 ? E - 1.0 : E - 13.0;

		CalendarDate date{
			.year = static_cast< int >(month > 2.0 ? C - 4716.0 : C - 4715.0),
			.month = static_cast< int >(month),
			.day = static_cast< int >(B - D - std::floor(30.6001 * E)),
			.hour = 0,
			.minute = 0,
			.second = 0.0
		};

		/* The time of day from whole seconds of the day and their remainder: no 60.0 second from a rounding. */
		const auto daySeconds = std::clamp(fraction * Astronomy::SecondsPerDay, 0.0, Astronomy::SecondsPerDay - 1e-6);
		const auto wholeSeconds = static_cast< int >(std::floor(daySeconds));

		date.hour = wholeSeconds / 3600;
		date.minute = (wholeSeconds % 3600) / 60;
		date.second = static_cast< double >(wholeSeconds % 60) + (daySeconds - std::floor(daySeconds));

		return date;
	}

	/**
	 * @brief Returns the Julian day of a Unix time (seconds since 1970-01-01 00:00 UTC, the system clock's).
	 * @param unixSeconds The Unix time.
	 * @return double
	 */
	[[nodiscard]]
	constexpr
	double
	julianDayFromUnixTime (double unixSeconds) noexcept
	{
		return JulianDayUnixEpoch + (unixSeconds / Astronomy::SecondsPerDay);
	}

	/**
	 * @brief Returns Julian centuries since J2000.0.
	 * @param julianDay The Julian day.
	 * @return double
	 */
	[[nodiscard]]
	constexpr
	double
	julianCenturies (double julianDay) noexcept
	{
		return (julianDay - JulianDayJ2000) / DaysPerJulianCentury;
	}

	/**
	 * @brief Returns ΔT = TT − UT, in seconds (Espenak & Meeus's polynomials).
	 * @note 1961–2150 follow the measured and predicted values (2026: ~75 s by the 2005–2050 polynomial, ~69 s observed:
	 * 3″ on the Moon). Outside, the long-term parabola −20 + 32 u² (u in centuries from 1820): seconds to minutes off.
	 * @param julianDay The Julian day (UT).
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	deltaT (double julianDay) noexcept
	{
		const auto year = 2000.0 + ((julianDay - JulianDayJ2000) / 365.25);

		if ( year >= 1961.0 && year < 1986.0 )
		{
			const auto t = year - 1975.0;

			return 45.45 + (1.067 * t) - (t * t / 260.0) - (t * t * t / 718.0);
		}

		if ( year >= 1986.0 && year < 2005.0 )
		{
			const auto t = year - 2000.0;

			return 63.86 + (0.3345 * t) - (0.060374 * t * t) + (0.0017275 * t * t * t) + (0.000651814 * t * t * t * t) + (0.00002373599 * t * t * t * t * t);
		}

		if ( year >= 2005.0 && year < 2050.0 )
		{
			const auto t = year - 2000.0;

			return 62.92 + (0.32217 * t) + (0.005589 * t * t);
		}

		const auto u = (year - 1820.0) / 100.0;

		if ( year >= 2050.0 && year < 2150.0 )
		{
			return -20.0 + (32.0 * u * u) - (0.5628 * (2150.0 - year));
		}

		return -20.0 + (32.0 * u * u);
	}

	/**
	 * @brief Returns the Greenwich mean sidereal angle (IAU 1982, Meeus 12.4): the right ascension on Greenwich's
	 * meridian, mean equinox of date. Add the east longitude for a local one.
	 * @param julianDayUT The Julian day, UT.
	 * @return double In [0, 2π).
	 */
	[[nodiscard]]
	inline
	double
	greenwichMeanSiderealAngle (double julianDayUT) noexcept
	{
		const auto days = julianDayUT - JulianDayJ2000;
		const auto T = days / DaysPerJulianCentury;
		/* The whole turns first: 360.98564736629 × days is ~1e7 degrees after 80 years, the fraction is what matters. */
		const auto turns = days * (360.98564736629 / 360.0);
		const auto fraction = turns - std::floor(turns);
		const auto degrees = 280.46061837 + (360.0 * fraction) + (0.000387933 * T * T) - (T * T * T / 38710000.0);

		return Astronomy::wrapAngle(Astronomy::radians(degrees));
	}

	/**
	 * @brief Returns the mean obliquity of the ecliptic (IAU 1980, Meeus 22.2).
	 * @param centuriesTT Julian centuries since J2000.0, TT.
	 * @return double In radians.
	 */
	[[nodiscard]]
	inline
	double
	meanObliquity (double centuriesTT) noexcept
	{
		const auto T = centuriesTT;
		const auto arcseconds = 84381.448 - (46.8150 * T) - (0.00059 * T * T) + (0.001813 * T * T * T);

		return Astronomy::radians(arcseconds / 3600.0);
	}

	/**
	 * @brief Returns the precession from the mean equator and equinox of J2000.0 to those of date (IAU 1976: ζ, z, θ).
	 * @param centuriesTT Julian centuries since J2000.0, TT.
	 * @return Rotation v_date = P(v_J2000).
	 */
	[[nodiscard]]
	inline
	Rotation
	precessionFromJ2000 (double centuriesTT) noexcept
	{
		const auto t = centuriesTT;
		const auto zeta = Astronomy::radians(((2306.2181 * t) + (0.30188 * t * t) + (0.017998 * t * t * t)) / 3600.0);
		const auto z = Astronomy::radians(((2306.2181 * t) + (1.09468 * t * t) + (0.018203 * t * t * t)) / 3600.0);
		const auto theta = Astronomy::radians(((2004.3109 * t) - (0.42665 * t * t) - (0.041833 * t * t * t)) / 3600.0);
		const auto cz = std::cos(zeta);
		const auto sz = std::sin(zeta);
		const auto cZ = std::cos(z);
		const auto sZ = std::sin(z);
		const auto cT = std::cos(theta);
		const auto sT = std::sin(theta);

		return {{{
			{(cz * cT * cZ) - (sz * sZ), (-sz * cT * cZ) - (cz * sZ), -sT * cZ},
			{(cz * cT * sZ) + (sz * cZ), (-sz * cT * sZ) + (cz * cZ), -sT * sZ},
			{cz * sT, -sz * sT, cT}
		}}};
	}

	/**
	 * @brief Turns an ecliptic vector into an equatorial one (a rotation about x by the obliquity).
	 * @param ecliptic The ecliptic vector.
	 * @param obliquity The obliquity, in radians.
	 * @return Vector< 3, double >
	 */
	[[nodiscard]]
	inline
	Vector< 3, double >
	eclipticToEquatorial (const Vector< 3, double > & ecliptic, double obliquity) noexcept
	{
		const auto c = std::cos(obliquity);
		const auto s = std::sin(obliquity);

		return {ecliptic.x(), (c * ecliptic.y()) - (s * ecliptic.z()), (s * ecliptic.y()) + (c * ecliptic.z())};
	}

	/**
	 * @brief Returns a body's heliocentric position, J2000 ecliptic (Standish's procedure: Kepler's equation in the
	 * orbit's plane, then the three rotations).
	 * @param body The body.
	 * @param centuriesTT Julian centuries since J2000.0, TT.
	 * @return Vector< 3, double > In AU.
	 */
	[[nodiscard]]
	inline
	Vector< 3, double >
	heliocentricEclipticJ2000 (Body body, double centuriesTT) noexcept
	{
		/* Body has BodyCount enumerators: the index is in range. */
		const auto & elements = *std::next(ApproximateElements.cbegin(), static_cast< std::ptrdiff_t >(body));
		const auto at = [centuriesTT] (double atJ2000, double perCentury) {
			return atJ2000 + (perCentury * centuriesTT);
		};

		const auto a = at(elements.atJ2000.semiMajorAxis, elements.perCentury.semiMajorAxis);
		const auto e = at(elements.atJ2000.eccentricity, elements.perCentury.eccentricity);
		const auto I = Astronomy::radians(at(elements.atJ2000.inclination, elements.perCentury.inclination));
		const auto L = Astronomy::radians(at(elements.atJ2000.meanLongitude, elements.perCentury.meanLongitude));
		const auto perihelion = Astronomy::radians(at(elements.atJ2000.perihelionLongitude, elements.perCentury.perihelionLongitude));
		const auto node = Astronomy::radians(at(elements.atJ2000.nodeLongitude, elements.perCentury.nodeLongitude));
		const auto argument = perihelion - node;
		const Astronomy::KeplerOrbit orbit{.eccentricity = e};
		const auto E = orbit.eccentricAnomaly(L - perihelion);
		const auto xOrbit = a * (std::cos(E) - e);
		const auto yOrbit = a * std::sqrt(1.0 - (e * e)) * std::sin(E);
		const auto cw = std::cos(argument);
		const auto sw = std::sin(argument);
		const auto cO = std::cos(node);
		const auto sO = std::sin(node);
		const auto cI = std::cos(I);
		const auto sI = std::sin(I);

		return {
			(((cw * cO) - (sw * sO * cI)) * xOrbit) + (((-sw * cO) - (cw * sO * cI)) * yOrbit),
			(((cw * sO) + (sw * cO * cI)) * xOrbit) + (((-sw * sO) + (cw * cO * cI)) * yOrbit),
			((sw * sI) * xOrbit) + ((cw * sI) * yOrbit)
		};
	}

	/**
	 * @brief Returns the Moon's geocentric position, ecliptic and equinox OF DATE (Schlyter: the mean orbit and its main
	 * perturbations — evection, variation, the yearly and parallactic equations; ~1-2′ and ~0.5 % in distance).
	 * @param julianDayTT The Julian day, TT.
	 * @return Vector< 3, double > In Earth radii.
	 */
	[[nodiscard]]
	inline
	Vector< 3, double >
	moonGeocentricEclipticOfDate (double julianDayTT) noexcept
	{
		/* Schlyter's day number: days since 2000 Jan 0.0. */
		const auto d = julianDayTT - 2451543.5;
		const auto N = Astronomy::radians(125.1228 - (0.0529538083 * d));
		const auto i = Astronomy::radians(5.1454);
		const auto w = Astronomy::radians(318.0634 + (0.1643573223 * d));
		constexpr auto a = 60.2666;
		constexpr auto e = 0.054900;
		const auto M = Astronomy::radians(115.3654 + (13.0649929509 * d));
		const auto Ms = Astronomy::radians(356.0470 + (0.9856002585 * d));
		const auto ws = Astronomy::radians(282.9404 + (4.70935e-5 * d));

		const Astronomy::KeplerOrbit orbit{.eccentricity = e};
		const auto E = orbit.eccentricAnomaly(M);
		const auto xv = a * (std::cos(E) - e);
		const auto yv = a * std::sqrt(1.0 - (e * e)) * std::sin(E);
		const auto v = std::atan2(yv, xv);
		auto r = std::sqrt((xv * xv) + (yv * yv));
		const auto vw = v + w;
		const auto xh = r * ((std::cos(N) * std::cos(vw)) - (std::sin(N) * std::sin(vw) * std::cos(i)));
		const auto yh = r * ((std::sin(N) * std::cos(vw)) + (std::cos(N) * std::sin(vw) * std::cos(i)));
		const auto zh = r * std::sin(vw) * std::sin(i);
		auto longitude = std::atan2(yh, xh);
		auto latitude = std::atan2(zh, std::sqrt((xh * xh) + (yh * yh)));

		/* The mean longitudes, elongation and argument of latitude the perturbations are written in. */
		const auto Ls = Ms + ws;
		const auto Lm = M + w + N;
		const auto D = Lm - Ls;
		const auto F = Lm - N;
		const auto term = [] (double degrees, double argument) {
			return Astronomy::radians(degrees) * std::sin(argument);
		};

		longitude += term(-1.274, M - (2.0 * D)) + term(0.658, 2.0 * D) + term(-0.186, Ms) + term(-0.059, (2.0 * M) - (2.0 * D))
			+ term(-0.057, M - (2.0 * D) + Ms) + term(0.053, M + (2.0 * D)) + term(0.046, (2.0 * D) - Ms) + term(0.041, M - Ms)
			+ term(-0.035, D) + term(-0.031, M + Ms) + term(-0.015, (2.0 * F) - (2.0 * D)) + term(0.011, M - (4.0 * D));
		latitude += term(-0.173, F - (2.0 * D)) + term(-0.055, M - F - (2.0 * D)) + term(-0.046, M + F - (2.0 * D))
			+ term(0.033, F + (2.0 * D)) + term(0.017, (2.0 * M) + F);
		r += (-0.58 * std::cos(M - (2.0 * D))) - (0.46 * std::cos(2.0 * D));

		return {r * std::cos(latitude) * std::cos(longitude), r * std::cos(latitude) * std::sin(longitude), r * std::sin(latitude)};
	}

	/**
	 * @brief Returns the Earth's heliocentric position, J2000 ecliptic: the barycentre minus its offset toward the Moon.
	 * @note The Moon's of-date vector stands for a J2000 one there: 0.4° on a 4 670 km offset is 30 km.
	 * @param julianDayTT The Julian day, TT.
	 * @return Vector< 3, double > In AU.
	 */
	[[nodiscard]]
	inline
	Vector< 3, double >
	earthHeliocentricEclipticJ2000 (double julianDayTT) noexcept
	{
		const auto barycentre = heliocentricEclipticJ2000(Body::EarthMoonBarycentre, julianCenturies(julianDayTT));
		const auto moonAU = moonGeocentricEclipticOfDate(julianDayTT) * (EarthRadiusKilometres / KilometresPerAU);

		return barycentre - (moonAU / (1.0 + EarthMoonMassRatio));
	}

	/**
	 * @brief Returns the Sun seen from the Earth's centre.
	 * @param julianDayTT The Julian day, TT.
	 * @return GeocentricPosition Mean equator and equinox of date; the distance in AU.
	 */
	[[nodiscard]]
	inline
	GeocentricPosition
	sun (double julianDayTT) noexcept
	{
		const auto T = julianCenturies(julianDayTT);
		const auto toSun = earthHeliocentricEclipticJ2000(julianDayTT) * -1.0;
		const auto equatorial = precessionFromJ2000(T)(eclipticToEquatorial(toSun, meanObliquity(0.0)));

		return {.direction = equatorial.normalized(), .distance = toSun.length(), .heliocentricDistance = 0.0, .phaseAngle = 0.0};
	}

	/**
	 * @brief Returns a planet seen from the Earth's centre, at the position its light left it (the light time, twice
	 * iterated).
	 * @param body The planet (Body::EarthMoonBarycentre answers the Sun's opposite point at 1 AU: not a target).
	 * @param julianDayTT The Julian day, TT.
	 * @return GeocentricPosition Mean equator and equinox of date; distances in AU.
	 */
	[[nodiscard]]
	inline
	GeocentricPosition
	planet (Body body, double julianDayTT) noexcept
	{
		const auto earth = earthHeliocentricEclipticJ2000(julianDayTT);
		auto heliocentric = heliocentricEclipticJ2000(body, julianCenturies(julianDayTT));
		auto geocentric = heliocentric - earth;

		for ( int iteration = 0; iteration < 2; ++iteration )
		{
			const auto lightTime = geocentric.length() * LightDaysPerAU;

			heliocentric = heliocentricEclipticJ2000(body, julianCenturies(julianDayTT - lightTime));
			geocentric = heliocentric - earth;
		}

		const auto distance = geocentric.length();
		const auto heliocentricDistance = heliocentric.length();
		const auto sunDistance = earth.length();
		const auto cosine = ((heliocentricDistance * heliocentricDistance) + (distance * distance) - (sunDistance * sunDistance)) / std::max(2.0 * heliocentricDistance * distance, std::numeric_limits< double >::min());
		const auto equatorial = precessionFromJ2000(julianCenturies(julianDayTT))(eclipticToEquatorial(geocentric, meanObliquity(0.0)));

		return {.direction = equatorial.normalized(), .distance = distance, .heliocentricDistance = heliocentricDistance, .phaseAngle = std::acos(std::clamp(cosine, -1.0, 1.0)), .lightTime = distance * LightDaysPerAU};
	}

	/**
	 * @brief Returns the Moon seen from the Earth's centre.
	 * @param julianDayTT The Julian day, TT.
	 * @return GeocentricPosition Mean equator and equinox of date; the distance in AU (× KilometresPerAU /
	 * EarthRadiusKilometres for Earth radii); the phase angle from the Sun's direction.
	 */
	[[nodiscard]]
	inline
	GeocentricPosition
	moon (double julianDayTT) noexcept
	{
		const auto T = julianCenturies(julianDayTT);
		const auto earthRadii = moonGeocentricEclipticOfDate(julianDayTT);
		const auto distance = earthRadii.length() * (EarthRadiusKilometres / KilometresPerAU);
		const auto direction = eclipticToEquatorial(earthRadii, meanObliquity(T)).normalized();
		const auto towardSun = sun(julianDayTT);
		/* The Sun-Moon-Earth angle: the Sun is ~400 times farther than the Moon. */
		const auto toSunFromMoon = (towardSun.direction * towardSun.distance) - (direction * distance);
		const auto cosine = Vector< 3, double >::dotProduct(toSunFromMoon.normalized(), direction * -1.0);

		return {.direction = direction, .distance = distance, .heliocentricDistance = toSunFromMoon.length(), .phaseAngle = std::acos(std::clamp(cosine, -1.0, 1.0))};
	}

	/**
	 * @brief Returns a body's geocentric position as the observer's East-North-Up direction, seen from the observer's
	 * place on the surface (the parallax of a near body: up to ~1° for the Moon, 24″ for Mars at its closest).
	 * @param body The body: its unit direction, mean equator and equinox of date, and its geocentric distance in AU
	 * (infinity: no parallax — a star, an axis).
	 * @param localSiderealAngle The local sidereal angle (Greenwich's + the east longitude), in radians.
	 * @param observer A reference to the observer.
	 * @return Astronomy::Horizontal A unit vector.
	 */
	[[nodiscard]]
	inline
	Astronomy::Horizontal
	topocentric (const GeocentricPosition & body, double localSiderealAngle, const Astronomy::Observer & observer) noexcept
	{
		const auto & equatorialOfDate = body.direction;
		const auto distanceAU = body.distance;
		const Astronomy::Equatorial position{
			.rightAscension = std::atan2(equatorialOfDate.y(), equatorialOfDate.x()),
			.declination = std::asin(std::clamp(equatorialOfDate.z(), -1.0, 1.0))
		};
		const auto geocentric = Astronomy::toHorizontal(position, localSiderealAngle, observer);

		if ( !std::isfinite(distanceAU) || distanceAU <= 0.0 )
		{
			return geocentric;
		}

		/* The observer stands one Earth radius up from the centre (a spherical Earth: the flattening moves the Moon by
		 * under 10″). */
		const auto distance = distanceAU * (KilometresPerAU / EarthRadiusKilometres);
		const Vector< 3, double > seen{geocentric.east * distance, geocentric.north * distance, (geocentric.up * distance) - 1.0};
		const auto unit = seen.normalized();

		return {.east = unit.x(), .north = unit.y(), .up = unit.z()};
	}
	/**
	 * @brief What a planet looks like from afar: its size and its colour.
	 */
	struct PlanetAppearance
	{
		/** @brief Its equatorial radius, in km (IAU WGCCRE 2015). */
		double radius{0.0};
		/** @brief Its B−V colour index (Mallama & Hilton 2018; Pluto: Allen's). */
		double colourIndex{0.0};
	};

	/** @brief The planets' appearance, in the order of Body (the barycentre's row is empty). */
	constexpr std::array< PlanetAppearance, BodyCount > PlanetAppearances{{
		{.radius = 2440.53, .colourIndex = 0.93},
		{.radius = 6051.8, .colourIndex = 0.82},
		{.radius = 0.0, .colourIndex = 0.0},
		{.radius = 3396.19, .colourIndex = 1.36},
		{.radius = 71492.0, .colourIndex = 0.83},
		{.radius = 60268.0, .colourIndex = 1.04},
		{.radius = 25559.0, .colourIndex = 0.56},
		{.radius = 24764.0, .colourIndex = 0.41},
		{.radius = 1188.3, .colourIndex = 0.80}
	}};

	/**
	 * @brief Returns a planet's apparent angular diameter, in radians.
	 * @param body The planet.
	 * @param distanceAU Its distance, in AU, positive.
	 * @return double 0 for the barycentre or a non-positive distance.
	 */
	[[nodiscard]]
	inline
	double
	angularDiameter (Body body, double distanceAU) noexcept
	{
		/* Body has BodyCount enumerators: the index is in range. */
		const auto radius = std::next(PlanetAppearances.cbegin(), static_cast< std::ptrdiff_t >(body))->radius;
		const auto distance = distanceAU * KilometresPerAU;

		if ( !(distance > radius) || radius <= 0.0 )
		{
			return 0.0;
		}

		return 2.0 * std::asin(radius / distance);
	}

	/**
	 * @brief Returns the sine of Saturn's ring opening seen from the Earth, |sin B| (0 edge-on, ~0.45 wide open).
	 * @param saturn A reference to Saturn's geocentric position (planet(Body::Saturn, ...)).
	 * @param centuriesTT Julian centuries since J2000.0, TT (the pole is precessed like the position).
	 * @return double
	 */
	[[nodiscard]]
	inline
	double
	saturnRingOpening (const GeocentricPosition & saturn, double centuriesTT) noexcept
	{
		/* Saturn's north pole, J2000: right ascension 40.589°, declination 83.537° (IAU WGCCRE 2015). */
		const auto ra = Astronomy::radians(40.589);
		const auto dec = Astronomy::radians(83.537);
		const auto pole = precessionFromJ2000(centuriesTT)(Vector< 3, double >{std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec)});

		return std::min(std::abs(Vector< 3, double >::dotProduct(pole, saturn.direction)), 1.0);
	}

	/**
	 * @brief Returns a planet's apparent visual magnitude (A. Mallama & J. L. Hilton, "Computing apparent planetary
	 * magnitudes for The Astronomical Almanac", Astronomy and Computing 25, 2018): 5 log10(r Δ) + V(1, α).
	 * @note Mars's rotational and orbital corrections, Uranus's sub-latitude term and Neptune's secular one are left out
	 * (~0.1 mag). Pluto is not in the paper: V(1, 0) = −1.0, no phase term. Saturn is the globe AND the rings
	 * (saturnRingOpening()).
	 * @param body The planet.
	 * @param position A reference to its geocentric position (planet()).
	 * @param ringOpening |sin B| for Saturn (saturnRingOpening()), ignored otherwise.
	 * @return double NaN for the barycentre.
	 */
	[[nodiscard]]
	inline
	double
	apparentMagnitude (Body body, const GeocentricPosition & position, double ringOpening = 0.0) noexcept
	{
		const auto distances = 5.0 * std::log10(std::max(position.heliocentricDistance * position.distance, std::numeric_limits< double >::min()));
		const auto a = Astronomy::degrees(position.phaseAngle);

		switch ( body )
		{
			case Body::Mercury :
				return distances - 0.613 + (6.3280e-02 * a) - (1.6336e-03 * a * a) + (3.3644e-05 * a * a * a) - (3.4265e-07 * a * a * a * a) + (1.6893e-09 * a * a * a * a * a) - (3.0334e-12 * a * a * a * a * a * a);

			case Body::Venus :
				if ( a <= 163.7 )
				{
					return distances - 4.384 - (1.044e-03 * a) + (3.687e-04 * a * a) - (2.814e-06 * a * a * a) + (8.938e-09 * a * a * a * a);
				}

				return distances + 236.05828 - (2.81914 * a) + (8.39034e-03 * a * a);

			case Body::EarthMoonBarycentre :
				return std::numeric_limits< double >::quiet_NaN();

			case Body::Mars :
				if ( a <= 50.0 )
				{
					return distances - 1.601 + (0.02267 * a) - (0.0001302 * a * a);
				}

				return distances - 0.367 - (0.02573 * a) + (0.0003445 * a * a);

			case Body::Jupiter :
			{
				if ( a <= 12.0 )
				{
					return distances - 9.395 - (3.7e-04 * a) + (6.16e-04 * a * a);
				}

				const auto x = a / 180.0;

				return distances - 9.428 - (2.5 * std::log10(std::max(1.0 - (1.507 * x) - (0.363 * x * x) - (0.062 * x * x * x) + (2.809 * x * x * x * x) - (1.876 * x * x * x * x * x), 1e-6)));
			}

			case Body::Saturn :
			{
				const auto sinB = std::clamp(ringOpening, 0.0, 1.0);

				return distances - 8.914 - (1.825 * sinB) + (0.026 * a) - (0.378 * sinB * std::exp(-2.25 * a));
			}

			case Body::Uranus :
				return distances - 7.110 + (6.587e-03 * a) + (1.045e-04 * a * a);

			case Body::Neptune :
				return distances - 7.00 + (a > 1.9 ? (7.944e-03 * a) + (9.617e-05 * a * a) : 0.0);

			case Body::Pluto :
				return distances - 1.0;
		}

		return std::numeric_limits< double >::quiet_NaN();
	}

	/**
	 * @brief A body's orientation: its north pole and its prime meridian, unit vectors of the J2000 equator.
	 */
	struct BodyOrientation
	{
		/** @brief The north pole (the IAU's: the pole on the north side of the invariable plane). */
		Vector< 3, double > pole{0.0, 0.0, 1.0};
		/** @brief The direction from the centre toward longitude 0 on the equator. The direction toward east longitude
		 * 90° is pole × primeMeridian: an EAST planetocentric longitude of a unit direction n from the centre is
		 * atan2(n · (pole × primeMeridian), n · primeMeridian). */
		Vector< 3, double > primeMeridian{1.0, 0.0, 0.0};
	};

	/**
	 * @brief Returns a planet's orientation (the IAU WGCCRE recommended models: B. A. Archinal et al., "Report of the IAU
	 * Working Group on Cartographic Coordinates and Rotational Elements: 2015", Celest. Mech. Dyn. Astr. 130:22, 2018 —
	 * Mercury, Jupiter's System III, Saturn, Uranus, Neptune, Pluto; Mars from the 2009 report, Archinal et al., Celest.
	 * Mech. Dyn. Astr. 109:101, 2011, the one that reproduces JPL Horizons: the periodic terms below 0.01° left out).
	 * @note Checked against JPL Horizons's sub-Earth points (test MathSolarSystem.MarsFacesTheEarthLikeHorizons).
	 * @note Pass the instant the light LEFT the body (julianDayTT − GeocentricPosition::lightTime): Mars turns 0.24° per
	 * minute, Jupiter 0.6°.
	 * @param body The planet (the barycentre answers the J2000 frame itself).
	 * @param julianDayTT The Julian day, TT.
	 * @return BodyOrientation J2000 equatorial vectors.
	 */
	[[nodiscard]]
	inline
	BodyOrientation
	orientation (Body body, double julianDayTT) noexcept
	{
		const auto d = julianDayTT - JulianDayJ2000;
		const auto T = d / DaysPerJulianCentury;
		/* Right ascension and declination of the pole, prime meridian angle W, in degrees. */
		double alpha = 0.0;
		double delta = 90.0;
		double W = 0.0;

		switch ( body )
		{
			case Body::Mercury :
				alpha = 281.0103 - (0.0328 * T);
				delta = 61.4155 - (0.0049 * T);
				W = 329.5988 + (6.1385108 * d);
				break;

			case Body::Venus :
				alpha = 272.76;
				delta = 67.16;
				W = 160.20 - (1.4813688 * d);
				break;

			case Body::EarthMoonBarycentre :
				return {};

			case Body::Mars :
				alpha = 317.68143 - (0.1061 * T);
				delta = 52.88650 - (0.0609 * T);
				W = 176.630 + (350.89198226 * d);
				break;

			case Body::Jupiter :
				alpha = 268.056595 - (0.006499 * T);
				delta = 64.495303 + (0.002413 * T);
				W = 284.95 + (870.5360000 * d);
				break;

			case Body::Saturn :
				alpha = 40.589 - (0.036 * T);
				delta = 83.537 - (0.004 * T);
				W = 38.90 + (810.7939024 * d);
				break;

			case Body::Uranus :
				alpha = 257.311;
				delta = -15.175;
				W = 203.81 - (501.1600928 * d);
				break;

			case Body::Neptune :
			{
				const auto N = Astronomy::radians(357.85 + (52.316 * T));

				alpha = 299.36 + (0.70 * std::sin(N));
				delta = 43.46 - (0.51 * std::cos(N));
				W = 249.978 + (541.1397757 * d) - (0.48 * std::sin(N));
				break;
			}

			case Body::Pluto :
				alpha = 132.993;
				delta = -6.163;
				W = 302.695 + (56.3625225 * d);
				break;
		}

		const auto a = Astronomy::radians(alpha);
		const auto dec = Astronomy::radians(delta);
		/* W wraps first: 350.89° per day reaches 1e7 degrees, the fraction is what turns. */
		const auto w = Astronomy::radians(W - (360.0 * std::floor(W / 360.0)));
		const Vector< 3, double > pole{std::cos(dec) * std::cos(a), std::cos(dec) * std::sin(a), std::sin(dec)};
		/* The ascending node of the body's equator on the J2000 equator, at right ascension α + 90°. */
		const Vector< 3, double > node{-std::sin(a), std::cos(a), 0.0};
		const auto east = Vector< 3, double >::crossProduct(pole, node);

		return {.pole = pole, .primeMeridian = (node * std::cos(w)) + (east * std::sin(w))};
	}
}
