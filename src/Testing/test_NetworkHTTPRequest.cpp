/*
 * src/Testing/test_NetworkHTTPRequest.cpp
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

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <string>

/* Local inclusions. */
#include "IO/IO.hpp"
#include "Network/HTTPRequest.hpp"
#include "Network/HTTPResponse.hpp"
#include "Network/URI.hpp"

using namespace EmEn::Base::Network;

namespace
{
	/** @brief The request line (the text before the first CRLF) of a GET on http://example.com + path. */
	std::string
	requestLine (const std::string & path)
	{
		URI resource{"http://example.com"};
		resource.setPath(EmEn::Base::IO::u8path(path));

		const auto text = HTTPRequest{HTTPRequest::Method::GET, resource}.toString();

		return text.substr(0, text.find("\r\n"));
	}
}

/* 2026-10-12: the request line carries ONE leading slash and the path as is. It wrote " /" before a path that already
 * starts with one ("GET //a/b"), through std's operator<<, which quotes a path ("GET /\"/a/b\""). */
TEST(NetworkHTTPRequest, requestLineAbsolutePath)
{
	EXPECT_EQ(requestLine("/a/b"), "GET /a/b HTTP/1.1");
}

TEST(NetworkHTTPRequest, requestLineRelativePath)
{
	EXPECT_EQ(requestLine("a/b"), "GET /a/b HTTP/1.1");
}

TEST(NetworkHTTPRequest, requestLineEmptyPath)
{
	EXPECT_EQ(requestLine(""), "GET / HTTP/1.1");
}

/* The coverage pass (2026-10-17): the method table, the request parse path, HTTP/0.9 output, the version table, and the
 * status line of a response (the trust boundary: HTTPResponseParser feeds it the server's bytes). */
TEST(NetworkHTTPRequest, everyMethodRoundTrips)
{
	for ( const auto method : {HTTPRequest::Method::GET, HTTPRequest::Method::HEAD, HTTPRequest::Method::POST, HTTPRequest::Method::OPTIONS,
		HTTPRequest::Method::CONNECT, HTTPRequest::Method::TRACE, HTTPRequest::Method::PUT, HTTPRequest::Method::PATCH, HTTPRequest::Method::DELETE} )
	{
		const auto * const name = HTTPRequest::method(method);

		ASSERT_NE(name, nullptr);
		EXPECT_EQ(HTTPRequest::parseMethod(name), method) << name;
	}

	EXPECT_EQ(HTTPRequest::method(HTTPRequest::Method::NONE), nullptr);
	EXPECT_EQ(HTTPRequest::parseMethod("get"), HTTPRequest::Method::NONE);
	EXPECT_EQ(HTTPRequest::parseMethod(""), HTTPRequest::Method::NONE);
}

TEST(NetworkHTTPRequest, parsedRequestKeepsItsLineAndHeaders)
{
	const HTTPRequest request{"POST /upload HTTP/1.0\r\nHost: example.com\r\nX-Empty:\r\nContent-Type :  text/plain  \r\n"};

	ASSERT_TRUE(request.isValid());
	EXPECT_EQ(request.method(), HTTPRequest::Method::POST);
	EXPECT_EQ(EmEn::Base::IO::toGenericU8String(request.resource().path()), "/upload");
	EXPECT_EQ(request.version(), HTTPHeaders::Version::HTTP10);
	EXPECT_EQ(request.value("host"), "example.com");
	EXPECT_EQ(request.value("X-Empty"), "");
	EXPECT_EQ(request.value("Content-Type"), "text/plain");
	EXPECT_EQ(request.value("Missing"), "");
}

TEST(NetworkHTTPRequest, malformedRequestsAreRefused)
{
	/* No target, an unknown method, nothing at all. */
	EXPECT_FALSE(HTTPRequest{"GET\r\n"}.isValid());
	EXPECT_FALSE(HTTPRequest{"FETCH /a HTTP/1.1\r\n"}.isValid());
	EXPECT_FALSE(HTTPRequest{""}.isValid());

	/* A header line without a colon, or with an empty name: the parse reports it. */
	HTTPRequest request;
	EXPECT_FALSE(request.parse("GET /a HTTP/1.1\r\nno colon here\r\n"));
	EXPECT_FALSE(request.parse("GET /a HTTP/1.1\r\n: no name\r\n"));
}

TEST(NetworkHTTPRequest, http09RequestHasNoVersionAndHeadersFollow)
{
	URI resource{"http://example.com"};
	resource.setPath(EmEn::Base::IO::u8path("/x"));

	const HTTPRequest request{HTTPRequest::Method::HEAD, resource, HTTPHeaders::Version::HTTP09};

	EXPECT_EQ(request.toString(), "HEAD /x\r\nHost: example.com\r\n\r\n");
}

TEST(NetworkHTTPRequest, versionTableRoundTrips)
{
	for ( const auto version : {HTTPHeaders::Version::HTTP09, HTTPHeaders::Version::HTTP10, HTTPHeaders::Version::HTTP11, HTTPHeaders::Version::HTTP20, HTTPHeaders::Version::HTTP30} )
	{
		EXPECT_EQ(HTTPHeaders::parseVersion(HTTPHeaders::version(version)), version);
	}
}

TEST(NetworkHTTPResponse, statusLines)
{
	{
		const HTTPResponse response{"HTTP/1.1 404\r\n"};

		ASSERT_TRUE(response.isValid());
		EXPECT_EQ(response.codeResponse(), 404);
		EXPECT_EQ(response.textResponse(), "");
	}

	{
		const HTTPResponse response{"HTTP/1.0 200 OK fine\r\nConnection: keep-alive\r\n"};

		ASSERT_TRUE(response.isValid());
		EXPECT_EQ(response.textResponse(), "OK fine");
		EXPECT_TRUE(response.keepConnectionAlive());
		EXPECT_EQ(response.toString(), "HTTP/1.0 200 OK fine\r\nConnection: keep-alive\r\n\r\n");
	}

	/* Out of the 100-599 range, not a number, no code at all. */
	EXPECT_FALSE(HTTPResponse{"HTTP/1.1 99 Low\r\n"}.isValid());
	EXPECT_FALSE(HTTPResponse{"HTTP/1.1 600 High\r\n"}.isValid());
	EXPECT_FALSE(HTTPResponse{"HTTP/1.1 abc\r\n"}.isValid());
	EXPECT_FALSE(HTTPResponse{"HTTP/1.1\r\n"}.isValid());
}

TEST(NetworkHTTPResponse, statusLineIsStrict)
{
	/* RFC 9112 § 4: status-line = HTTP-version SP 3DIGIT SP [ reason-phrase ]; HTTP-version = "HTTP/" DIGIT "." DIGIT. */
	EXPECT_FALSE(HTTPResponse{"HTTP/1.1 200abc OK\r\n"}.isValid());
	EXPECT_FALSE(HTTPResponse{"HTTP/1.1 +200 OK\r\n"}.isValid());
	EXPECT_FALSE(HTTPResponse{"HTTP/1.1 099 Low\r\n"}.isValid());
	EXPECT_FALSE(HTTPResponse{"HTTP/1.1 2000 OK\r\n"}.isValid());
	EXPECT_FALSE(HTTPResponse{"FOO 200 OK\r\n"}.isValid());
	EXPECT_FALSE(HTTPResponse{"SSH-2.0 200 OK\r\n"}.isValid());
	EXPECT_FALSE(HTTPResponse{"HTTP/2.0 200 OK\r\n"}.isValid());
	EXPECT_FALSE(HTTPResponse{"HTTP/1.10 200 OK\r\n"}.isValid());
	EXPECT_FALSE(HTTPResponse{"http/1.1 200 OK\r\n"}.isValid());

	/* A higher HTTP/1 minor version is read as 1.1 (RFC 9110 § 2.5). */
	const HTTPResponse response{"HTTP/1.2 200 OK\r\n"};
	ASSERT_TRUE(response.isValid());
	EXPECT_EQ(response.version(), HTTPHeaders::Version::HTTP11);
}

TEST(NetworkHTTPRequest, theHostFieldFollowsTheResourceAuthority)
{
	/* RFC 9110 § 7.2: Host is the URI's authority without the userinfo. It held the host name alone (no port), and a
	 * second resource kept the FIRST Host (the header map keeps the first value of a name) — 2026-10-18. */
	HTTPRequest request{HTTPRequest::Method::GET, URI{"http://alpha.example.net:8080/a"}};
	EXPECT_EQ(request.value("Host"), "alpha.example.net:8080");

	request.setResource(URI{"https://beta.example.net/b"});
	EXPECT_EQ(request.value("Host"), "beta.example.net");

	const HTTPRequest withUserinfo{HTTPRequest::Method::GET, URI{"https://user:secret@gamma.example.net/c"}};
	EXPECT_EQ(withUserinfo.value("Host"), "gamma.example.net");

	/* A resource set without the header leaves the previous one alone. */
	request.setResource(URI{"https://delta.example.net/d"}, true);
	EXPECT_EQ(request.value("Host"), "beta.example.net");

	/* set() replaces, add() keeps the first value. */
	request.add("Accept", "text/plain");
	request.add("Accept", "text/html");
	EXPECT_EQ(request.value("Accept"), "text/plain");
	request.set("accept", "application/json");
	EXPECT_EQ(request.value("Accept"), "application/json");
}

