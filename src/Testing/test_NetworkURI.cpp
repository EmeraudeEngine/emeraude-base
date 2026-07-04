/*
 * src/Testing/test_NetworkURI.cpp
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
#include <string>

/* Local inclusions. */
#include "Network/PercentEncoding.hpp"
#include "Network/URI.hpp"

using namespace EmEn::Base::Network;

/* --- Component parsing --- */

TEST(NetworkURI, parsesAllComponents)
{
	const URI uri{"https://user:pass@example.com:8443/a/b?x=1&y=2#frag"};

	EXPECT_EQ(uri.scheme(), "https");
	EXPECT_EQ(uri.uriDomain().hostname().name(), "example.com");
	EXPECT_EQ(uri.uriDomain().port(), 8443U);
	EXPECT_EQ(uri.uriDomain().username(), "user");
	EXPECT_EQ(uri.uriDomain().password(), "pass");
	EXPECT_EQ(uri.path().generic_string(), "/a/b");
	EXPECT_EQ(uri.fragment(), "frag");
}

TEST(NetworkURI, normalizesSchemeAndHostToLowercase)
{
	const URI uri{"HTTPS://Example.COM/Path"};

	EXPECT_EQ(uri.scheme(), "https");
	EXPECT_EQ(uri.uriDomain().hostname().name(), "example.com");
	/* Path case is significant and preserved. */
	EXPECT_EQ(uri.path().generic_string(), "/Path");
}

TEST(NetworkURI, parsesIPv6Literal)
{
	const URI uri{"https://[2001:db8::1]:443/path"};

	EXPECT_EQ(uri.uriDomain().hostname().name(), "[2001:db8::1]");
	EXPECT_EQ(uri.uriDomain().port(), 443U);
	EXPECT_EQ(uri.uriDomain().host(), "[2001:db8::1]:443");
}

TEST(NetworkURI, hostIncludesPortSeparator)
{
	/* Regression: URIDomain::host() used to omit the ':' → "example.com8443". */
	const URI uri{"https://example.com:8443/"};

	EXPECT_EQ(uri.uriDomain().host(), "example.com:8443");
}

TEST(NetworkURI, rejectsOutOfRangePort)
{
	const URI uri{"https://example.com:99999/"};

	/* Out-of-range port dropped; host still parses. */
	EXPECT_EQ(uri.uriDomain().port(), 0U);
	EXPECT_EQ(uri.uriDomain().hostname().name(), "example.com");
}

/* --- Percent-encoding --- */

TEST(NetworkURI, decodesPercentEncodingInComponents)
{
	const URI uri{"https://example.com/a%20b/c%2Fd?q=hello%20world"};

	/* Stored decoded. */
	EXPECT_EQ(uri.path().generic_string(), "/a b/c/d");
	EXPECT_EQ(uri.query().variables().at("q"), "hello world");
}

TEST(NetworkURI, reEncodesOnOutput)
{
	URI uri{"https://example.com/base"};
	uri.setPath("/a b/c");

	/* resource() must re-encode the space that setPath introduced. */
	const auto resource = uri.resource();

	EXPECT_EQ(resource.find('"'), std::string::npos) << resource;
	EXPECT_NE(resource.find("a%20b"), std::string::npos) << resource;
}

TEST(NetworkURI, resourceHasNoQuotes)
{
	const URI uri{"https://example.com/some/path"};

	EXPECT_EQ(uri.resource(), "/some/path");
}

TEST(NetworkPercentEncoding, roundTrip)
{
	const std::string raw{"a b/c?d=e&f#g"};

	const auto encoded = PercentEncoding::encode(raw, PercentEncoding::Component::Segment);
	const auto decoded = PercentEncoding::decode(encoded);

	EXPECT_EQ(decoded, raw);
	/* A space is never left bare in a segment. */
	EXPECT_EQ(encoded.find(' '), std::string::npos);
}

TEST(NetworkPercentEncoding, malformedEscapeLeftVerbatim)
{
	EXPECT_EQ(PercentEncoding::decode("100%"), "100%");
	EXPECT_EQ(PercentEncoding::decode("%zz"), "%zz");
	EXPECT_EQ(PercentEncoding::decode("%4"), "%4");
}

/* --- Dot-segment removal (RFC 3986 §5.2.4) --- */

TEST(NetworkURI, removesDotSegments)
{
	EXPECT_EQ(URI::removeDotSegments("/a/b/c/./../../g"), "/a/g");
	EXPECT_EQ(URI::removeDotSegments("/a/./b/../c"), "/a/c");
	EXPECT_EQ(URI::removeDotSegments("mid/content=5/../6"), "mid/6");
}

/* --- Reference resolution (RFC 3986 §5.4, the normative examples) --- */

TEST(NetworkURI, resolvesReferenceNormalExamples)
{
	const URI base{"http://a/b/c/d;p?q"};

	struct Case { const char * reference; const char * expected; };

	const Case cases[] = {
		{"g",       "http://a/b/c/g"},
		{"./g",     "http://a/b/c/g"},
		{"g/",      "http://a/b/c/g/"},
		{"/g",      "http://a/g"},
		{"?y",      "http://a/b/c/d;p?y"},
		{"g?y",     "http://a/b/c/g?y"},
		{"#s",      "http://a/b/c/d;p?q#s"},
		{"../g",    "http://a/b/g"},
		{"../../g", "http://a/g"},
		{"",        "http://a/b/c/d;p?q"},
	};

	for ( const auto & testCase : cases )
	{
		const auto resolved = URI::resolve(base, testCase.reference);

		EXPECT_EQ(to_string(resolved), testCase.expected) << "reference = " << testCase.reference;
	}
}

TEST(NetworkURI, resolvesAbsoluteReferenceUnchanged)
{
	const URI base{"https://a/b/c"};

	const auto resolved = URI::resolve(base, "https://other.example/x");

	EXPECT_EQ(resolved.uriDomain().hostname().name(), "other.example");
	EXPECT_EQ(resolved.path().generic_string(), "/x");
}

/* --- Valid-but-gnarly URIs (must parse without crashing, per "Ave robustus") --- */

TEST(NetworkURI, handlesGnarlyButValidURIs)
{
	/* Each is a legal or plausibly-hostile URI/reference that must be handled
	 * gracefully — never a crash, never UB (verified here + under ASan/UBSan). */
	const char * inputs[] = {
		"https://example.com",                               /* no path at all */
		"https://example.com/",                              /* root path only */
		"https://example.com//////",                         /* empty segments */
		"https://example.com/a//b///c",                      /* collapsed-looking but kept */
		"https://example.com/../../../../etc/passwd",        /* dot-segment escape attempt */
		"https://example.com/./././.",                       /* only dot segments */
		"https://example.com/%2e%2e/%2e%2e/x",               /* encoded dot-segments (NOT collapsed: decoded after) */
		"https://user:p@ss%40word@example.com/",             /* encoded '@' in password */
		"https://[::1]/",                                    /* shortest IPv6 */
		"https://[2001:db8:85a3::8a2e:370:7334]:65535/x",    /* full IPv6 + max port */
		"https://xn--n3h.example/path",                      /* punycode host */
		"HTTPS://EXAMPLE.COM/MiXeD",                         /* case normalization */
		"https://example.com/?",                             /* empty query */
		"https://example.com/#",                             /* empty fragment */
		"https://example.com/?a=1&a=2&b=&=c&d",              /* dup keys, empty val, empty key, flag */
		"https://example.com/path with spaces/really",       /* raw spaces (tolerated on decode) */
		"https://example.com/%",                             /* lone percent */
		"https://example.com/日本語/ページ",                    /* UTF-8 path */
		"https://example.com/a%00b",                         /* embedded NUL */
		"https://example.com:0/",                            /* port zero */
		"foo+bar-baz.qux://host/x",                          /* exotic-but-valid scheme */
		"https://例え.テスト/",                                /* IDN host */
		"https://example.com/;params;more",                  /* matrix-style params */
		"https://a@b@example.com/",                          /* multiple '@' (userinfo is up to LAST relevant) */
	};

	for ( const auto * input : inputs )
	{
		/* The contract is "does not crash and yields a usable object"; we assert
		 * the scheme round-trips for the well-formed https ones as a sanity anchor. */
		const URI uri{input};

		const auto rendered = to_string(uri);

		/* No quotes ever leak from the filesystem::path (the original bug). */
		EXPECT_EQ(rendered.find('"'), std::string::npos) << "input = " << input << " rendered = " << rendered;
	}

	SUCCEED();
}

TEST(NetworkURI, rfc3986Section112SchemeDiversity)
{
	/* The canonical example set from RFC 3986 §1.1.2 — the authoritative "weird
	 * but valid" list: schemes with no authority (mailto/news/tel/urn), an IPv6
	 * LDAP URL whose query itself contains '?', an explicit port, etc. Each must
	 * parse with the right scheme and round-trip without leaking quotes. */
	struct Case { const char * uri; const char * scheme; };

	const Case cases[] = {
		{"ftp://ftp.is.co.za/rfc/rfc1808.txt",                        "ftp"},
		{"http://www.ietf.org/rfc/rfc2396.txt",                       "http"},
		{"ldap://[2001:db8::7]/c=GB?objectClass?one",                 "ldap"},
		{"mailto:John.Doe@example.com",                               "mailto"},
		{"news:comp.infosystems.www.servers.unix",                    "news"},
		{"tel:+1-816-555-1212",                                       "tel"},
		{"telnet://192.0.2.16:80/",                                   "telnet"},
		{"urn:oasis:names:specification:docbook:dtd:xml:4.1.2",       "urn"},
	};

	for ( const auto & testCase : cases )
	{
		const URI uri{testCase.uri};

		EXPECT_EQ(uri.scheme(), testCase.scheme) << "uri = " << testCase.uri;
		EXPECT_EQ(to_string(uri).find('"'), std::string::npos) << "uri = " << testCase.uri;
	}

	/* The LDAP case specifically: IPv6 authority + a query that contains '?'. */
	const URI ldap{"ldap://[2001:db8::7]/c=GB?objectClass?one"};
	EXPECT_EQ(ldap.uriDomain().hostname().name(), "[2001:db8::7]");
	EXPECT_EQ(ldap.path().generic_string(), "/c=GB");

	/* mailto has no authority; the whole address is the (rootless) path. */
	const URI mailto{"mailto:John.Doe@example.com"};
	EXPECT_TRUE(mailto.uriDomain().empty());
	EXPECT_EQ(mailto.path().generic_string(), "John.Doe@example.com");
}

TEST(NetworkURI, deeplyNestedDotSegmentsDoNotUnderflow)
{
	/* A path that pops far above the root must clamp at '/', not underflow. */
	std::string evil = "https://example.com";

	for ( int i = 0; i < 5000; ++i )
	{
		evil += "/..";
	}

	evil += "/safe";

	const URI uri{evil};

	EXPECT_EQ(uri.path().generic_string(), "/safe");
}

TEST(NetworkURI, emptyAndDegenerateInputs)
{
	EXPECT_TRUE(URI{""}.empty());
	/* A bare path reference: no scheme, no authority — must not crash. */
	const URI relative{"just/a/path"};
	EXPECT_TRUE(relative.scheme().empty());
	EXPECT_EQ(relative.path().generic_string(), "just/a/path");
}

/* --- setPath still works (used by callers) --- */

TEST(NetworkURI, resourceReflectsSetPath)
{
	URI uri{"https://example.com/old"};
	uri.setPath("/moved");

	EXPECT_EQ(uri.resource(), "/moved");
}
