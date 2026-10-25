/*
 * src/Testing/test_MathSolarSystem.cpp
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
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>

/* Local inclusions. */
#include "Math/Astronomy.hpp"
#include "Math/SolarSystem.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base::Math;
using namespace EmEn::Base::Math::SolarSystem;

namespace
{
	/**
	 * @brief Returns the angle between two unit vectors, in degrees.
	 * @param a The first vector.
	 * @param b The second vector.
	 * @return double
	 */
	double
	separationDegrees (const Vector< 3, double > & a, const Vector< 3, double > & b)
	{
		return Astronomy::degrees(std::acos(std::clamp(Vector< 3, double >::dotProduct(a, b), -1.0, 1.0)));
	}

	/**
	 * @brief Returns the unit vector of a right ascension and a declination, in degrees.
	 * @param rightAscension The right ascension.
	 * @param declination The declination.
	 * @return Vector< 3, double >
	 */
	Vector< 3, double >
	unit (double rightAscension, double declination)
	{
		const auto ra = Astronomy::radians(rightAscension);
		const auto dec = Astronomy::radians(declination);

		return {std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec)};
	}

	/**
	 * @brief Returns whether two calendar instants are the same, to the millisecond.
	 * @param lhs The first instant.
	 * @param rhs The second instant.
	 * @return bool
	 */
	bool
	sameInstant (const CalendarDate & lhs, const CalendarDate & rhs)
	{
		return lhs.year == rhs.year && lhs.month == rhs.month && lhs.day == rhs.day && lhs.hour == rhs.hour && lhs.minute == rhs.minute && std::abs(lhs.second - rhs.second) < 1e-3;
	}

	/**
	 * @brief Returns the Julian day TT of a UT calendar instant (Espenak & Meeus's ΔT).
	 * @param date The instant, UT.
	 * @return double
	 */
	double
	terrestrialTime (const CalendarDate & date)
	{
		const auto ut = julianDay(date).value_or(JulianDayJ2000);

		return ut + (deltaT(ut) / Astronomy::SecondsPerDay);
	}
}

TEST(MathSolarSystem, JulianDayFollowsMeeus)
{
	EXPECT_DOUBLE_EQ(julianDay({.year = 2000, .month = 1, .day = 1, .hour = 12, .minute = 0, .second = 0.0}).value_or(0.0), JulianDayJ2000);
	/* Meeus 7.a: 1957 October 4.81 (Sputnik 1). */
	EXPECT_NEAR(julianDay({.year = 1957, .month = 10, .day = 4, .hour = 19, .minute = 26, .second = 24.0}).value_or(0.0), 2436116.31, 1e-6);
	/* Meeus 7.b and the list after it. */
	EXPECT_DOUBLE_EQ(julianDay({.year = 1988, .month = 1, .day = 27, .hour = 0, .minute = 0, .second = 0.0}).value_or(0.0), 2447187.5);
	EXPECT_DOUBLE_EQ(julianDay({.year = 1600, .month = 12, .day = 31, .hour = 0, .minute = 0, .second = 0.0}).value_or(0.0), 2305812.5);
	EXPECT_DOUBLE_EQ(julianDayFromUnixTime(946728000.0), JulianDayJ2000);
}

TEST(MathSolarSystem, JulianDayRefusesAnImpossibleDate)
{
	EXPECT_TRUE(julianDay({.year = 2000, .month = 2, .day = 29, .hour = 0, .minute = 0, .second = 0.0}).has_value());
	EXPECT_FALSE(julianDay({.year = 2100, .month = 2, .day = 29, .hour = 0, .minute = 0, .second = 0.0}).has_value());
	EXPECT_FALSE(julianDay({.year = 2001, .month = 2, .day = 29, .hour = 0, .minute = 0, .second = 0.0}).has_value());
	EXPECT_FALSE(julianDay({.year = 2001, .month = 13, .day = 1, .hour = 0, .minute = 0, .second = 0.0}).has_value());
	EXPECT_FALSE(julianDay({.year = 2001, .month = 4, .day = 31, .hour = 0, .minute = 0, .second = 0.0}).has_value());
	EXPECT_FALSE(julianDay({.year = 1582, .month = 10, .day = 20, .hour = 0, .minute = 0, .second = 0.0}).has_value());
	EXPECT_FALSE(julianDay({.year = 2001, .month = 1, .day = 1, .hour = 24, .minute = 0, .second = 0.0}).has_value());
	EXPECT_FALSE(julianDay({.year = 2001, .month = 1, .day = 1, .hour = 0, .minute = 0, .second = std::nan("")}).has_value());
}

TEST(MathSolarSystem, CalendarDateFollowsMeeus)
{
	/* Meeus 7.c: JD 2436116.31 → 1957 October 4.81. */
	EXPECT_TRUE(sameInstant(calendarDate(2436116.31).value_or(CalendarDate{}), {.year = 1957, .month = 10, .day = 4, .hour = 19, .minute = 26, .second = 24.0}));
	EXPECT_FALSE(calendarDate(std::nan("")).has_value());
	EXPECT_FALSE(calendarDate(0.0).has_value());
}

TEST(MathSolarSystem, CalendarDateRoundTrips)
{
	constexpr std::array< CalendarDate, 4 > dates{{
		{.year = 2000, .month = 2, .day = 29, .hour = 23, .minute = 59, .second = 59.5},
		{.year = 2026, .month = 12, .day = 31, .hour = 23, .minute = 59, .second = 59.5},
		{.year = 1600, .month = 1, .day = 1, .hour = 0, .minute = 0, .second = 0.25},
		{.year = 2026, .month = 10, .day = 26, .hour = 12, .minute = 30, .second = 15.0}
	}};

	for ( const auto & date : dates )
	{
		EXPECT_TRUE(sameInstant(calendarDate(julianDay(date).value_or(0.0)).value_or(CalendarDate{}), date)) << date.year;
	}
}

TEST(MathSolarSystem, SiderealTimeFollowsMeeus)
{
	/* Meeus 12.a: 1987 April 10, 0h UT → 13h10m46.3668s; 12.b: 19h21m00s UT → 8h34m57.0896s. */
	EXPECT_NEAR(Astronomy::degrees(greenwichMeanSiderealAngle(julianDay({.year = 1987, .month = 4, .day = 10, .hour = 0, .minute = 0, .second = 0.0}).value_or(0.0))), 197.693195, 1e-5);
	EXPECT_NEAR(Astronomy::degrees(greenwichMeanSiderealAngle(julianDay({.year = 1987, .month = 4, .day = 10, .hour = 19, .minute = 21, .second = 0.0}).value_or(0.0))), 128.7378734, 1e-5);
}

TEST(MathSolarSystem, ObliquityFollowsMeeus)
{
	/* Meeus 22.a: 1987 April 10, 0h TD → 23°26′27.407″. */
	EXPECT_NEAR(Astronomy::degrees(meanObliquity(julianCenturies(2446895.5))), 23.0 + (26.0 / 60.0) + (27.407 / 3600.0), 1e-6);
}

TEST(MathSolarSystem, DeltaTIsContinuousAndPlausible)
{
	for ( const auto year : {1986, 2005, 2050, 2150} )
	{
		const auto at = julianDay({.year = year, .month = 1, .day = 1, .hour = 0}).value_or(0.0);

		EXPECT_NEAR(deltaT(at - 1.0), deltaT(at + 1.0), 1.5) << year;
	}

	const auto now = deltaT(julianDay({.year = 2026, .month = 1, .day = 1, .hour = 0, .minute = 0, .second = 0.0}).value_or(0.0));

	EXPECT_GT(now, 65.0);
	EXPECT_LT(now, 80.0);
}

TEST(MathSolarSystem, PrecessionFollowsMeeus)
{
	/* Meeus 21.b: θ Persei, J2000 (with its proper motion over 28.867 years) to 2028 November 13.19 TD. */
	EXPECT_NEAR(separationDegrees(precessionFromJ2000(0.0)(unit(41.0, 49.0)), unit(41.0, 49.0)), 0.0, 1e-12);

	const auto t = julianCenturies(2462088.69);
	const auto years = t * 100.0;
	/* α0 2h44m11.986s, δ0 +49°13′42.48″, μα +0.03425 s/yr, μδ −0.0895″/yr; expected α 2h46m11.331s, δ +49°20′54.54″. */
	const auto start = unit(41.0499417 + (0.03425 * 15.0 * years / 3600.0), 49.2284667 - (0.0895 * years / 3600.0));
	const auto expected = unit(41.5472125, 49.3484833);

	EXPECT_LT(separationDegrees(precessionFromJ2000(t)(start), expected) * 3600.0, 1.0);
}

TEST(MathSolarSystem, VenusFollowsMeeus)
{
	/* Meeus 33.a: 1992 December 20, 0h TD, apparent α 21h04m41.454s, δ −18°53′16.84″ (the nutation and the aberration,
	 * ~35″, are not applied here; Standish's elements are good to ~20″). */
	const auto venus = planet(Body::Venus, 2448976.5);

	EXPECT_LT(separationDegrees(venus.direction, unit(316.172725, -18.888011)), 0.02);
	EXPECT_NEAR(venus.distance, 0.910845, 2e-4);
}

TEST(MathSolarSystem, MarsClosestApproachOf2003)
{
	/* 2003 August 27, 9:51 UT: 0.372719 AU, the closest in ~60 000 years. */
	const auto at = terrestrialTime({.year = 2003, .month = 8, .day = 27, .hour = 9, .minute = 51, .second = 0.0});
	const auto closest = planet(Body::Mars, at).distance;

	EXPECT_NEAR(closest, 0.372719, 3e-4);
	EXPECT_GT(planet(Body::Mars, at - 2.0).distance, closest);
	EXPECT_GT(planet(Body::Mars, at + 2.0).distance, closest);
}

TEST(MathSolarSystem, TransitOfVenus2004)
{
	/* 2004 June 8, 8:20 UT: Venus crossed the Sun's disc, its centre 627″ from the Sun's at the transit's middle. */
	const auto at = terrestrialTime({.year = 2004, .month = 6, .day = 8, .hour = 8, .minute = 20, .second = 0.0});
	const auto separation = separationDegrees(planet(Body::Venus, at).direction, sun(at).direction);

	EXPECT_NEAR(separation * 3600.0, 627.0, 60.0);
}

TEST(MathSolarSystem, MoonFollowsMeeus)
{
	/* Meeus 47.a: 1992 April 12, 0h TD: geometric λ 133.162655°, β −3.229126°, 368 409.7 km. */
	const auto ecliptic = moonGeocentricEclipticOfDate(2448724.5);
	const auto longitude = Astronomy::degrees(std::atan2(ecliptic.y(), ecliptic.x()));
	const auto latitude = Astronomy::degrees(std::asin(ecliptic.z() / ecliptic.length()));

	EXPECT_NEAR(longitude, 133.162655, 0.05);
	EXPECT_NEAR(latitude, -3.229126, 0.03);
	EXPECT_NEAR(ecliptic.length() * EarthRadiusKilometres, 368409.7, 368409.7 * 0.006);
}

TEST(MathSolarSystem, TotalEclipseOf2017)
{
	/* 2017 August 21, greatest eclipse at 18:25:29 UT, 36°58′N 87°40′W: the Moon covers the Sun's centre. */
	const auto ut = julianDay({.year = 2017, .month = 8, .day = 21, .hour = 18, .minute = 25, .second = 29.0}).value_or(0.0);
	const auto tt = ut + (deltaT(ut) / Astronomy::SecondsPerDay);
	const Astronomy::Observer observer{.latitude = Astronomy::radians(36.0 + (58.0 / 60.0))};
	const auto localSidereal = greenwichMeanSiderealAngle(ut) + Astronomy::radians(-(87.0 + (40.0 / 60.0)));
	const auto towardSun = sun(tt);
	const auto towardMoon = moon(tt);
	const auto sunSeen = topocentric(towardSun, localSidereal, observer);
	const auto moonSeen = topocentric(towardMoon, localSidereal, observer);
	const Vector< 3, double > s{sunSeen.east, sunSeen.north, sunSeen.up};
	const Vector< 3, double > m{moonSeen.east, moonSeen.north, moonSeen.up};

	EXPECT_LT(separationDegrees(s, m), 0.08);
	/* Without the parallax the Moon is off by its horizontal parallax (0.95°) × cos(elevation): the topocentric correction
	 * is what aligns them. */
	const auto moonGeocentric = topocentric({.direction = towardMoon.direction, .distance = std::numeric_limits< double >::infinity()}, localSidereal, observer);
	const Vector< 3, double > g{moonGeocentric.east, moonGeocentric.north, moonGeocentric.up};

	EXPECT_NEAR(separationDegrees(s, g), 0.95 * std::cos(Astronomy::radians(64.0)), 0.05);
	/* High in the sky: the eclipse was seen at 64° of elevation. */
	EXPECT_NEAR(Astronomy::degrees(sunSeen.elevation()), 64.0, 1.0);
}

TEST(MathSolarSystem, DirectionsAreUnitVectors)
{
	const auto at = terrestrialTime({.year = 2026, .month = 10, .day = 26, .hour = 0, .minute = 0, .second = 0.0});

	for ( size_t index = 0; index < BodyCount; ++index )
	{
		const auto body = static_cast< Body >(index);

		if ( body == Body::EarthMoonBarycentre )
		{
			continue;
		}

		EXPECT_NEAR(planet(body, at).direction.length(), 1.0, 1e-12) << index;
	}

	EXPECT_NEAR(sun(at).direction.length(), 1.0, 1e-12);
	EXPECT_NEAR(moon(at).direction.length(), 1.0, 1e-12);
	EXPECT_NEAR(sun(at).distance, 0.993, 0.01);
}

TEST(MathSolarSystem, PlanetMagnitudesFollowTheAlmanac)
{
	/* Mars at its 2003 opposition: V −2.88, 25.1″. */
	const auto marsAt = terrestrialTime({.year = 2003, .month = 8, .day = 28, .hour = 18, .minute = 0, .second = 0.0});
	const auto mars = planet(Body::Mars, marsAt);

	EXPECT_NEAR(apparentMagnitude(Body::Mars, mars), -2.88, 0.15);
	EXPECT_NEAR(Astronomy::degrees(angularDiameter(Body::Mars, mars.distance)) * 3600.0, 25.1, 0.3);

	/* Venus at its greatest brilliancy, 2020 April 28: V −4.7. */
	const auto venusAt = terrestrialTime({.year = 2020, .month = 4, .day = 28, .hour = 0, .minute = 0, .second = 0.0});

	EXPECT_NEAR(apparentMagnitude(Body::Venus, planet(Body::Venus, venusAt)), -4.7, 0.15);

	/* Jupiter at its 2022 September 26 opposition, the closest since 1963: V −2.94, 49.9″. */
	const auto jupiterAt = terrestrialTime({.year = 2022, .month = 9, .day = 26, .hour = 20, .minute = 0, .second = 0.0});
	const auto jupiter = planet(Body::Jupiter, jupiterAt);

	EXPECT_NEAR(apparentMagnitude(Body::Jupiter, jupiter), -2.94, 0.15);
	EXPECT_NEAR(Astronomy::degrees(angularDiameter(Body::Jupiter, jupiter.distance)) * 3600.0, 49.9, 0.4);
}

TEST(MathSolarSystem, SaturnsRingsBrightenIt)
{
	/* Wide open at the 2003 December 31 opposition (B ≈ −26°), edge-on at the 2025 September 21 one (B ≈ 0°). */
	const auto openAt = terrestrialTime({.year = 2003, .month = 12, .day = 31, .hour = 12, .minute = 0, .second = 0.0});
	const auto edgeAt = terrestrialTime({.year = 2025, .month = 9, .day = 21, .hour = 12, .minute = 0, .second = 0.0});
	const auto open = planet(Body::Saturn, openAt);
	const auto edge = planet(Body::Saturn, edgeAt);
	const auto openSinB = saturnRingOpening(open, julianCenturies(openAt));
	const auto edgeSinB = saturnRingOpening(edge, julianCenturies(edgeAt));

	EXPECT_NEAR(Astronomy::degrees(std::asin(openSinB)), 26.0, 1.5);
	EXPECT_LT(Astronomy::degrees(std::asin(edgeSinB)), 2.0);
	EXPECT_LT(apparentMagnitude(Body::Saturn, open, openSinB), apparentMagnitude(Body::Saturn, edge, edgeSinB) - 0.8);
	EXPECT_TRUE(std::isnan(apparentMagnitude(Body::EarthMoonBarycentre, open)));
}
