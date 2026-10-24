/*
 * src/Testing/test_MathAstronomy.cpp
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

#include <gtest/gtest.h>

/* STL inclusions. */
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <numbers>
#include <string>

/* Local inclusions. */
#include "Math/Astronomy.hpp"

using namespace EmEn::Base::Math::Astronomy;

namespace
{
	constexpr double AngleTolerance{1e-9};

	/**
	 * @brief The noon elevation for a declination and a latitude, in degrees (the star on the meridian).
	 */
	double
	noonElevationDegrees (double declination, double latitude) noexcept
	{
		return degrees(toHorizontal({.rightAscension = 0.0, .declination = declination}, 0.0, Observer{latitude}).elevation());
	}
}

TEST(MathAstronomy, EquinoxNoonElevationIsTheColatitude)
{
	/* At the epoch the star is at the vernal equinox (declination 0) and on the meridian. North of the equator the
	 * noon star stands to the south, and the other way round. */
	for ( const double latitude : {0.0, 45.0, -30.0, 60.0} )
	{
		const auto direction = starDirection(earth(), Observer{radians(latitude)}, 0.0);

		EXPECT_NEAR(degrees(direction.elevation()), 90.0 - std::abs(latitude), AngleTolerance) << "latitude " << latitude;
		EXPECT_NEAR(direction.east, 0.0, AngleTolerance) << "latitude " << latitude;
		EXPECT_EQ(std::signbit(direction.north), latitude > 0.0) << "latitude " << latitude;
	}
}

TEST(MathAstronomy, SolsticeNoonAddsTheTilt)
{
	const auto tilt = earth().axialTilt;
	/* The June solstice: the ecliptic longitude is 90°, the declination is the tilt. */
	const auto solstice = toEquatorial({.longitude = std::numbers::pi / 2.0, .latitude = 0.0}, tilt);

	EXPECT_NEAR(solstice.declination, tilt, AngleTolerance);
	EXPECT_NEAR(noonElevationDegrees(solstice.declination, radians(45.0)), 90.0 - 45.0 + degrees(tilt), AngleTolerance);
}

TEST(MathAstronomy, PolarDayAndPolarNight)
{
	const auto tilt = earth().axialTilt;
	const auto latitude = radians(80.0);

	/* Summer: the star stays above the horizon at midnight (hour angle π). */
	const auto summerMidnight = toHorizontal({.rightAscension = 0.0, .declination = tilt}, std::numbers::pi, Observer{latitude});

	EXPECT_NEAR(degrees(summerMidnight.elevation()), 80.0 + degrees(tilt) - 90.0, AngleTolerance);
	EXPECT_GT(summerMidnight.elevation(), 0.0);

	/* Winter: it stays below at noon. */
	EXPECT_LT(noonElevationDegrees(-tilt, latitude), 0.0);
}

TEST(MathAstronomy, SunriseDueEastAtTheEquinox)
{
	/* A planet whose star does not move against the stars keeps the equinox: the star rises due east, a quarter of a
	 * day before noon, whatever the latitude. */
	const Planet planet{.siderealDay = 1000.0, .orbitalPeriod = 0.0, .axialTilt = radians(23.0), .eccentricity = 0.0, .longitudeOfPerihelion = 0.0, .semiMajorAxis = 1.0};

	for ( const double latitude : {0.0, 45.0, -50.0} )
	{
		const auto sunrise = starDirection(planet, Observer{radians(latitude)}, -250.0);

		EXPECT_NEAR(sunrise.east, 1.0, AngleTolerance) << "latitude " << latitude;
		EXPECT_NEAR(sunrise.up, 0.0, AngleTolerance) << "latitude " << latitude;
	}
}

TEST(MathAstronomy, SolarDayIsLongerThanTheSiderealOne)
{
	const auto planet = earth();
	const auto solar = solarDay(planet);

	EXPECT_NEAR(solar, SecondsPerDay, 1.0);
	/* 3 min 56 s. */
	EXPECT_NEAR(solar - planet.siderealDay, 235.9, 0.5);

	/* Without an orbit the two days are one. */
	EXPECT_DOUBLE_EQ(solarDay({.siderealDay = 600.0, .orbitalPeriod = 0.0}), 600.0);
}

TEST(MathAstronomy, KeplerEquationIsSolved)
{
	for ( const double eccentricity : {0.0, 0.0167, 0.2, 0.5, 0.9, 0.99} )
	{
		for ( int step = 0; step < 64; ++step )
		{
			const auto meanAnomaly = TwoPi * static_cast< double >(step) / 64.0;
			const auto E = KeplerOrbit{eccentricity}.eccentricAnomaly(meanAnomaly);

			EXPECT_NEAR(wrapAngle(E - (eccentricity * std::sin(E))), wrapAngle(meanAnomaly), 1e-10) << "e " << eccentricity << ", M " << meanAnomaly;
		}
	}
}

TEST(MathAstronomy, TrueAndMeanAnomaliesRoundTrip)
{
	for ( const double eccentricity : {0.0, 0.0167, 0.6} )
	{
		for ( const double anomaly : {0.3, 1.7, 3.0, 4.5} )
		{
			const KeplerOrbit orbit{eccentricity};
			const auto back = orbit.trueAnomaly(orbit.eccentricAnomaly(orbit.meanAnomaly(anomaly)));

			EXPECT_NEAR(wrapAngle(back), wrapAngle(anomaly), 1e-10) << "e " << eccentricity;
		}
	}
}

TEST(MathAstronomy, TheYearComesBackToTheEquinox)
{
	const auto planet = earth();

	EXPECT_NEAR(starEclipticLongitude(planet, 0.0), 0.0, AngleTolerance);
	/* One orbital period later: the equinox again (wrapped, either side of 0). */
	const auto afterAYear = starEclipticLongitude(planet, planet.orbitalPeriod);

	EXPECT_NEAR(std::min(afterAYear, TwoPi - afterAYear), 0.0, 1e-9);

	/* The longitude only ever advances over the year. */
	double previous = 0.0;
	double travelled = 0.0;

	for ( int step = 1; step <= 365; ++step )
	{
		const auto current = starEclipticLongitude(planet, planet.orbitalPeriod * static_cast< double >(step) / 365.0);
		const auto advance = wrapAngle(current - previous);

		EXPECT_LT(advance, 0.1) << "day " << step;
		travelled += advance;
		previous = current;
	}

	EXPECT_NEAR(travelled, TwoPi, 1e-6);
}

TEST(MathAstronomy, AnEccentricOrbitIsNotUniform)
{
	/* Earth passes its perihelion in early January, so the star moves slowest in July: the March equinox to the
	 * September one (0° to 180°) takes ~186.4 days, the way back ~178.8. And the June solstice comes ~92.8 days after
	 * the March equinox. */
	const auto planet = earth();
	double june = 0.0;
	double september = 0.0;

	for ( int minute = 0; minute < 366 * 24 * 60; ++minute )
	{
		const auto time = static_cast< double >(minute) * 60.0;
		const auto longitude = starEclipticLongitude(planet, time);

		if ( june == 0.0 && longitude >= std::numbers::pi / 2.0 )
		{
			june = time;
		}

		if ( september == 0.0 && longitude >= std::numbers::pi && longitude < 1.5 * std::numbers::pi )
		{
			september = time;
		}
	}

	ASSERT_GT(june, 0.0);
	ASSERT_GT(september, june);

	EXPECT_NEAR(september / SecondsPerDay, 186.4, 0.3);
	EXPECT_NEAR(june / SecondsPerDay, 92.8, 0.3);
}

TEST(MathAstronomy, TheStarDistanceFollowsTheEllipse)
{
	/* Earth stands at a (1 − e) at its perihelion, early January (~289 days after the March equinox), at a (1 + e) at
	 * its aphelion, and at every instant on the conic r = a (1 − e²) / (1 + e cos ν) of the same true anomaly. */
	const auto planet = earth();
	const auto a = planet.semiMajorAxis;
	const auto e = planet.eccentricity;
	double nearest = 1.0e9;
	double farthest = 0.0;
	double perihelion = 0.0;

	for ( int hour = 0; hour < 366 * 24; ++hour )
	{
		const auto time = static_cast< double >(hour) * 3600.0;
		const auto distance = starDistance(planet, time);
		const auto trueAnomaly = starEclipticLongitude(planet, time) - planet.longitudeOfPerihelion;

		ASSERT_NEAR(distance, a * (1.0 - (e * e)) / (1.0 + (e * std::cos(trueAnomaly))), 1e-12) << "hour " << hour;

		if ( distance < nearest )
		{
			nearest = distance;
			perihelion = time;
		}

		farthest = std::max(farthest, distance);
	}

	EXPECT_NEAR(nearest, a * (1.0 - e), 1e-7);
	EXPECT_NEAR(farthest, a * (1.0 + e), 1e-7);
	EXPECT_NEAR(perihelion / SecondsPerDay, 289.0, 1.5);
}

TEST(MathAstronomy, AStillPlanetKeepsItsDistance)
{
	const Planet planet{.siderealDay = 1000.0, .orbitalPeriod = 0.0, .axialTilt = 0.0, .eccentricity = 0.5, .longitudeOfPerihelion = 0.0, .semiMajorAxis = 1.5};

	for ( const double time : {0.0, 1.0e3, 1.0e8} )
	{
		EXPECT_EQ(starDistance(planet, time), 1.5);
	}
}

TEST(MathAstronomy, MoonPhasesFollowTheSynodicMonth)
{
	const auto planet = earth();
	const auto orbit = moon();
	const Observer observer{radians(45.0)};
	const auto month = synodicPeriod(planet, orbit);

	EXPECT_NEAR(month / SecondsPerDay, 29.53, 0.01);

	/* Full at the epoch (the preset), new half a month later, quarters in between. The inclination keeps the new
	 * moon off the star by up to 5°: (1 - cos 5°) / 2 = 0.002. */
	const auto fraction = [&] (double time) {
		return illuminatedFraction(starDirection(planet, observer, time), moonDirection(planet, orbit, observer, time));
	};

	EXPECT_NEAR(fraction(0.0), 1.0, 1e-9);
	EXPECT_NEAR(fraction(0.5 * month), 0.0, 0.003);
	EXPECT_NEAR(fraction(0.25 * month), 0.5, 0.05);
	EXPECT_NEAR(fraction(month), 1.0, 0.003);
}

TEST(MathAstronomy, TheCelestialPoleStandsAtTheLatitude)
{
	for ( const double latitude : {0.0, 45.0, 90.0, -33.0} )
	{
		EXPECT_NEAR(degrees(celestialPole(Observer{radians(latitude)}).elevation()), latitude, AngleTolerance);
	}
}

TEST(MathAstronomy, DirectionsAreUnitVectors)
{
	const auto planet = earth();

	for ( int step = 0; step < 50; ++step )
	{
		const auto time = 1.0e6 * static_cast< double >(step);
		const Observer observer{radians(37.0)};
		const auto star = starDirection(planet, observer, time);
		const auto satellite = moonDirection(planet, moon(), observer, time);

		EXPECT_NEAR((star.east * star.east) + (star.north * star.north) + (star.up * star.up), 1.0, 1e-12);
		EXPECT_NEAR((satellite.east * satellite.east) + (satellite.north * satellite.north) + (satellite.up * satellite.up), 1.0, 1e-12);
	}
}

namespace
{
	/**
	 * @brief Checks one noon elevation of the former SunCourse against a still planet, over a whole revolution.
	 */
	void
	expectSimpleCourse (double noonElevation) noexcept
	{
		constexpr double Day{240.0};
		const Planet planet{.siderealDay = Day, .orbitalPeriod = 0.0, .axialTilt = 0.0, .eccentricity = 0.0, .longitudeOfPerihelion = 0.0, .semiMajorAxis = 1.0};
		const Observer observer{radians(90.0 - noonElevation)};

		for ( int step = 0; step < 40; ++step )
		{
			const auto phase = static_cast< double >(step) / 40.0;
			const auto angle = TwoPi * phase;
			/* Phase 0.25 is noon (t = 0). */
			const auto direction = starDirection(planet, observer, (phase - 0.25) * Day);

			EXPECT_NEAR(direction.east, std::cos(angle), 1e-9) << "phase " << phase;
			EXPECT_NEAR(direction.north, -std::cos(radians(noonElevation)) * std::sin(angle), 1e-9) << "phase " << phase;
			EXPECT_NEAR(direction.up, std::sin(radians(noonElevation)) * std::sin(angle), 1e-9) << "phase " << phase;
		}
	}
}

TEST(MathAstronomy, AStillPlanetReproducesTheSimpleSunCourse)
{
	/* The engine's former SunCourse: a great circle through the sunrise point (east) and the noon point (noon
	 * elevation e toward the south), at a constant speed, day and night of equal length, phase 0 = sunrise. A planet
	 * without tilt nor orbit, observed at the latitude 90° - e, is exactly that course. */
	for ( const double noonElevation : {60.0, 30.0, 85.0} )
	{
		expectSimpleCourse(noonElevation);
	}
}
