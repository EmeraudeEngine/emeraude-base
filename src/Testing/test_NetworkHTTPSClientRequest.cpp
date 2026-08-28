/*
 * src/Testing/test_NetworkHTTPSClientRequest.cpp
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
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

/* Local inclusions. */
#include "Network/HTTPSClient.hpp"
#include "TLSTestHelpers.hpp"

using namespace EmEn::Base;
using EmEn::Base::Testing::HTTPSTestServer;
using EmEn::Base::Testing::declaredContentLength;
using EmEn::Base::Testing::generateServerCredentials;

namespace
{
	/** @brief Builds a client TLS context trusting the given (PEM) certificate. */
	asio::ssl::context
	makeTrustingClientContext (const std::string & certificatePEM) noexcept
	{
		asio::ssl::context context{asio::ssl::context::tls_client};

		asio::error_code error;

		context.add_certificate_authority(asio::buffer(certificatePEM), error);

		return context;
	}

	/** @brief Extracts the method of a raw HTTP request (test helper). */
	std::string
	requestMethod (const std::string & rawRequest) noexcept
	{
		const auto firstSpace = rawRequest.find(' ');

		if ( firstSpace == std::string::npos )
		{
			return {};
		}

		return rawRequest.substr(0, firstSpace);
	}

	/** @brief Extracts the request-target of a raw HTTP request (test helper). */
	std::string
	requestTarget (const std::string & rawRequest) noexcept
	{
		const auto firstSpace = rawRequest.find(' ');
		const auto secondSpace = rawRequest.find(' ', firstSpace + 1);

		if ( firstSpace == std::string::npos || secondSpace == std::string::npos )
		{
			return {};
		}

		return rawRequest.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	}

	/** @brief Extracts the body that followed the header terminator (test helper). */
	std::string
	requestBody (const std::string & rawRequest) noexcept
	{
		const auto headerEnd = rawRequest.find("\r\n\r\n");

		if ( headerEnd == std::string::npos )
		{
			return {};
		}

		return rawRequest.substr(headerEnd + 4);
	}

	/** @brief Lowercases a copy of a string (test helper). */
	std::string
	lowercased (std::string text) noexcept
	{
		std::ranges::transform(text, text.begin(), [] (char character) {
			return static_cast< char >(std::tolower(static_cast< unsigned char >(character)));
		});

		return text;
	}

	/**
	 * @brief Returns every value a raw request carried for a header name (test helper).
	 * @note A vector, not a single value: several tests exist precisely to prove a header was
	 * sent ONCE, and a helper returning the first match could not tell that apart.
	 */
	std::vector< std::string >
	requestHeaderValues (const std::string & rawRequest, const std::string & name) noexcept
	{
		std::vector< std::string > values;

		const auto headerEnd = rawRequest.find("\r\n\r\n");

		if ( headerEnd == std::string::npos )
		{
			return values;
		}

		const auto needle = "\r\n" + lowercased(name) + ":";
		const auto headers = lowercased(rawRequest.substr(0, headerEnd));

		for ( auto position = headers.find(needle); position != std::string::npos; position = headers.find(needle, position + 1) )
		{
			const auto valueStart = position + needle.size();
			const auto lineEnd = headers.find("\r\n", valueStart);

			auto value = headers.substr(valueStart, lineEnd == std::string::npos ? std::string::npos : lineEnd - valueStart);

			/* The comparison is on the lowercased copy, so the value comes back lowercased too;
			 * every assertion below compares against a lowercase literal. */
			while ( !value.empty() && (value.front() == ' ' || value.front() == '\t') )
			{
				value.erase(value.begin());
			}

			values.push_back(value);
		}

		return values;
	}

	/** @brief Builds a plain 200 response with a fixed body (test helper). */
	std::string
	plainResponse (const std::string & body) noexcept
	{
		std::stringstream response;
		response << "HTTP/1.1 200 OK\r\n"
			<< "Content-Type: text/plain\r\n"
			<< "Content-Length: " << body.size() << "\r\n"
			<< "Connection: close\r\n"
			<< "\r\n"
			<< body;

		return response.str();
	}

	/** @brief Builds a response with an arbitrary status and body (test helper). */
	std::string
	statusResponse (int statusCode, const std::string & body) noexcept
	{
		std::stringstream response;
		response << "HTTP/1.1 " << statusCode << " Status\r\n"
			<< "Content-Type: application/json\r\n"
			<< "Content-Length: " << body.size() << "\r\n"
			<< "Connection: close\r\n"
			<< "\r\n"
			<< body;

		return response.str();
	}

	/** @brief Builds a redirect response (test helper). */
	std::string
	redirectResponse (int statusCode, const std::string & location) noexcept
	{
		std::stringstream response;
		response << "HTTP/1.1 " << statusCode << " Redirect\r\n"
			<< "Location: " << location << "\r\n"
			<< "Content-Length: 0\r\n"
			<< "Connection: close\r\n"
			<< "\r\n";

		return response.str();
	}

	/** @brief Builds the https URI of a test-server path (test helper). */
	Network::URI
	serverURI (const HTTPSTestServer & server, const std::string & path) noexcept
	{
		return Network::URI{"https://localhost:" + std::to_string(server.port()) + path};
	}
}

/* ---- What the caller puts on the wire ---- */

TEST(NetworkHTTPSClientRequest, postSendsItsBodyAndFramesIt)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::string seen;

	HTTPSTestServer server{credentials, [&seen] (const std::string & request) {
		seen = request;

		return plainResponse("stored");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	Network::HTTPRequestOptions options;
	options.body = R"({"name":"paladin","level":7})";
	options.contentType = "application/json";

	const auto result = client.request(Network::HTTPRequest::Method::POST, serverURI(server, "/actors"), options);

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->response.codeResponse(), 200);
	EXPECT_EQ(result->body, "stored");

	EXPECT_EQ(requestMethod(seen), "POST");
	EXPECT_EQ(requestTarget(seen), "/actors");
	EXPECT_EQ(requestBody(seen), R"({"name":"paladin","level":7})");
	EXPECT_EQ(declaredContentLength(seen), options.body.size());

	const auto contentTypes = requestHeaderValues(seen, "Content-Type");
	ASSERT_EQ(contentTypes.size(), 1U);
	EXPECT_EQ(contentTypes[0], "application/json");
}

TEST(NetworkHTTPSClientRequest, postWithAnEmptyBodyStillFramesContentLengthZero)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::string seen;

	HTTPSTestServer server{credentials, [&seen] (const std::string & request) {
		seen = request;

		return plainResponse("ok");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto result = client.request(Network::HTTPRequest::Method::POST, serverURI(server, "/ping"));

	ASSERT_TRUE(result.has_value());

	/* ⚠️ Without it a server reads the POST and waits for a body that never comes. */
	const auto lengths = requestHeaderValues(seen, "Content-Length");
	ASSERT_EQ(lengths.size(), 1U);
	EXPECT_EQ(lengths[0], "0");
}

TEST(NetworkHTTPSClientRequest, getWithoutABodyFramesNothing)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::string seen;

	HTTPSTestServer server{credentials, [&seen] (const std::string & request) {
		seen = request;

		return plainResponse("ok");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto result = client.request(Network::HTTPRequest::Method::GET, serverURI(server, "/thing"));

	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(requestHeaderValues(seen, "Content-Length").empty());
}

TEST(NetworkHTTPSClientRequest, callerHeadersReachTheServer)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::string seen;

	HTTPSTestServer server{credentials, [&seen] (const std::string & request) {
		seen = request;

		return plainResponse("ok");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	Network::HTTPRequestOptions options;
	options.headers.emplace_back("Authorization", "Bearer abc.def.ghi");
	options.headers.emplace_back("X-Api-Key", "k-1234");
	options.headers.emplace_back("Accept", "application/json");

	const auto result = client.request(Network::HTTPRequest::Method::GET, serverURI(server, "/me"), options);

	ASSERT_TRUE(result.has_value());

	const auto authorizations = requestHeaderValues(seen, "Authorization");
	ASSERT_EQ(authorizations.size(), 1U);
	EXPECT_EQ(authorizations[0], lowercased("Bearer abc.def.ghi"));

	const auto keys = requestHeaderValues(seen, "X-Api-Key");
	ASSERT_EQ(keys.size(), 1U);
	EXPECT_EQ(keys[0], "k-1234");

	EXPECT_EQ(requestHeaderValues(seen, "Accept").size(), 1U);
}

TEST(NetworkHTTPSClientRequest, aCallerUserAgentReplacesTheDefaultRatherThanDuplicatingIt)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::string seen;

	HTTPSTestServer server{credentials, [&seen] (const std::string & request) {
		seen = request;

		return plainResponse("ok");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClientOptions clientOptions;
	clientOptions.userAgent = "DefaultAgent/9.9";

	Network::HTTPSClient client{tlsContext, clientOptions};

	Network::HTTPRequestOptions options;
	options.headers.emplace_back("User-Agent", "ProjetAlpha/1.0");

	const auto result = client.request(Network::HTTPRequest::Method::GET, serverURI(server, "/ua"), options);

	ASSERT_TRUE(result.has_value());

	/* Two User-Agent lines is what a naive "append the caller's" implementation produces. */
	const auto agents = requestHeaderValues(seen, "User-Agent");
	ASSERT_EQ(agents.size(), 1U);
	EXPECT_EQ(agents[0], lowercased("ProjetAlpha/1.0"));
}

TEST(NetworkHTTPSClientRequest, everyMethodReachesTheServerUnderItsOwnName)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::string seen;

	HTTPSTestServer server{credentials, [&seen] (const std::string & request) {
		seen = request;

		return plainResponse("ok");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	/* ⚠️ Method::DELETE is deliberately absent: winnt.h defines DELETE as a macro, this TU pulls
	 * in gtest (which reaches windows.h on MSVC), and the enumerator would not survive the
	 * preprocessor there. The method string is built by the same switch as the three below, so
	 * nothing about it is left untested. */
	const std::vector< std::pair< Network::HTTPRequest::Method, std::string > > cases{
		{Network::HTTPRequest::Method::PUT, "PUT"},
		{Network::HTTPRequest::Method::PATCH, "PATCH"},
		{Network::HTTPRequest::Method::OPTIONS, "OPTIONS"}
	};

	for ( const auto & [method, name] : cases )
	{
		const auto result = client.request(method, serverURI(server, "/resource"));

		ASSERT_TRUE(result.has_value()) << "method " << name;
		EXPECT_EQ(requestMethod(seen), name);
	}
}

/* ---- What the caller is NOT allowed to put on the wire ---- */

TEST(NetworkHTTPSClientRequest, refusesAHeaderValueCarryingCRLF)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return plainResponse("ok");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	Network::HTTPRequestOptions options;
	options.headers.emplace_back("X-Trace", "abc\r\nX-Injected: pwned");

	Network::DownloadReport report;

	const auto result = client.request(Network::HTTPRequest::Method::GET, serverURI(server, "/inject"), options, &report);

	EXPECT_FALSE(result.has_value());
	EXPECT_EQ(report.outcome, Network::DownloadOutcome::BadRequest);

	/* ⚠️ Refused BEFORE the socket is opened: a request rejected only at concatenation time would
	 * already have resolved and contacted the target. */
	EXPECT_EQ(server.requestCount(), 0U);
}

TEST(NetworkHTTPSClientRequest, refusesAHeaderValueCarryingABareControlCharacter)
{
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("X-Trace", std::string{"abc\0def", 7}));
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("X-Trace", "abc\ndef"));
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("X-Trace", "abc\rdef"));

	/* Split literal: "\x7Fd" would be read as one over-long hex escape. */
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("X-Trace", "abc\x7F" "def"));

	/* HTAB is the one control a field value may carry (RFC 9110 §5.5). */
	EXPECT_TRUE(Network::HTTPSClient::isRequestHeaderAcceptable("X-Trace", "abc\tdef"));
	EXPECT_TRUE(Network::HTTPSClient::isRequestHeaderAcceptable("X-Trace", ""));
}

TEST(NetworkHTTPSClientRequest, refusesANameThatIsNotAToken)
{
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("", "value"));
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("X Trace", "value"));
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("X-Trace:", "value"));
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("X-Trace\r\nY", "value"));

	EXPECT_TRUE(Network::HTTPSClient::isRequestHeaderAcceptable("X-Trace", "value"));
	EXPECT_TRUE(Network::HTTPSClient::isRequestHeaderAcceptable("If-None-Match", "value"));
}

TEST(NetworkHTTPSClientRequest, refusesTheFramingHeadersTheClientOwns)
{
	/* A second copy of any of these is a request-smuggling primitive. */
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("Host", "evil.example"));
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("content-length", "0"));
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("Connection", "keep-alive"));
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("Transfer-Encoding", "chunked"));
	EXPECT_FALSE(Network::HTTPSClient::isRequestHeaderAcceptable("Accept-Encoding", "gzip"));

	/* User-Agent is deliberately NOT reserved: overriding it is legitimate. */
	EXPECT_TRUE(Network::HTTPSClient::isRequestHeaderAcceptable("User-Agent", "ProjetAlpha/1.0"));
}

TEST(NetworkHTTPSClientRequest, refusesAContentTypeCarryingCRLF)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return plainResponse("ok");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	/* ⚠️ The media type takes the same concatenation path as a header, so it needs the same check
	 * — it does not travel through options.headers and would otherwise be unvalidated. */
	Network::HTTPRequestOptions options;
	options.body = "{}";
	options.contentType = "application/json\r\nX-Injected: pwned";

	Network::DownloadReport report;

	const auto result = client.request(Network::HTTPRequest::Method::POST, serverURI(server, "/inject"), options, &report);

	EXPECT_FALSE(result.has_value());
	EXPECT_EQ(report.outcome, Network::DownloadOutcome::BadRequest);
	EXPECT_EQ(server.requestCount(), 0U);
}

/* ---- Redirect hygiene ---- */

TEST(NetworkHTTPSClientRequest, callerHeadersSurviveASameOriginRedirect)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::vector< std::string > authorizationsPerHop;

	HTTPSTestServer server{credentials, [&authorizationsPerHop] (const std::string & request) {
		const auto values = requestHeaderValues(request, "Authorization");
		authorizationsPerHop.push_back(values.empty() ? std::string{"<none>"} : values[0]);

		if ( requestTarget(request) == "/old" )
		{
			return redirectResponse(302, "/new");
		}

		return plainResponse("arrived");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	Network::HTTPRequestOptions options;
	options.headers.emplace_back("Authorization", "Bearer secret");

	const auto result = client.request(Network::HTTPRequest::Method::GET, serverURI(server, "/old"), options);

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->body, "arrived");

	ASSERT_EQ(authorizationsPerHop.size(), 2U);
	EXPECT_EQ(authorizationsPerHop[0], lowercased("Bearer secret"));
	EXPECT_EQ(authorizationsPerHop[1], lowercased("Bearer secret"));
}

TEST(NetworkHTTPSClientRequest, callerHeadersAreDroppedOnACrossOriginRedirect)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::string secondHop;

	/* The second origin: same host, DIFFERENT port — enough to make it another origin. */
	HTTPSTestServer target{credentials, [&secondHop] (const std::string & request) {
		secondHop = request;

		return plainResponse("elsewhere");
	}};
	ASSERT_TRUE(target.isListening());

	const auto targetPort = target.port();

	HTTPSTestServer entry{credentials, [targetPort] (const std::string & /*request*/) {
		return redirectResponse(302, "https://localhost:" + std::to_string(targetPort) + "/taken");
	}};
	ASSERT_TRUE(entry.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	Network::HTTPRequestOptions options;
	options.headers.emplace_back("Authorization", "Bearer secret");

	const auto result = client.request(Network::HTTPRequest::Method::GET, serverURI(entry, "/start"), options);

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->body, "elsewhere");

	/* ⚠️ The credential must NOT follow the Location: whoever controls that host would receive it. */
	EXPECT_TRUE(requestHeaderValues(secondHop, "Authorization").empty());
}

TEST(NetworkHTTPSClientRequest, aBodyIsDroppedWhenA302RewritesThePostToAGet)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::vector< std::string > methodsPerHop;
	std::string secondHop;

	HTTPSTestServer server{credentials, [&methodsPerHop, &secondHop] (const std::string & request) {
		methodsPerHop.push_back(requestMethod(request));

		if ( requestTarget(request) == "/submit" )
		{
			return redirectResponse(302, "/done");
		}

		secondHop = request;

		return plainResponse("acknowledged");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	Network::HTTPRequestOptions options;
	options.body = R"({"charge":4200})";
	options.contentType = "application/json";

	const auto result = client.request(Network::HTTPRequest::Method::POST, serverURI(server, "/submit"), options);

	ASSERT_TRUE(result.has_value());

	ASSERT_EQ(methodsPerHop.size(), 2U);
	EXPECT_EQ(methodsPerHop[0], "POST");
	EXPECT_EQ(methodsPerHop[1], "GET");

	/* ⚠️ Keeping the body would replay a write the target never asked for, and the GET would
	 * carry a payload the server reads as its own. */
	EXPECT_TRUE(requestBody(secondHop).empty());
	EXPECT_TRUE(requestHeaderValues(secondHop, "Content-Length").empty());
	EXPECT_TRUE(requestHeaderValues(secondHop, "Content-Type").empty());
}

TEST(NetworkHTTPSClientRequest, aBodyAndItsMethodSurviveA307)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::vector< std::string > methodsPerHop;
	std::string secondHop;

	HTTPSTestServer server{credentials, [&methodsPerHop, &secondHop] (const std::string & request) {
		methodsPerHop.push_back(requestMethod(request));

		if ( requestTarget(request) == "/submit" )
		{
			return redirectResponse(307, "/done");
		}

		secondHop = request;

		return plainResponse("acknowledged");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	Network::HTTPRequestOptions options;
	options.body = R"({"charge":4200})";
	options.contentType = "application/json";

	const auto result = client.request(Network::HTTPRequest::Method::POST, serverURI(server, "/submit"), options);

	ASSERT_TRUE(result.has_value());

	ASSERT_EQ(methodsPerHop.size(), 2U);
	EXPECT_EQ(methodsPerHop[0], "POST");
	EXPECT_EQ(methodsPerHop[1], "POST");
	EXPECT_EQ(requestBody(secondHop), R"({"charge":4200})");
}

/* ---- What an API answers ---- */

TEST(NetworkHTTPSClientRequest, aNonSuccessStatusStillReturnsItsBody)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return statusResponse(422, R"({"error":"level must be positive"})");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	Network::DownloadReport report;

	const auto result = client.request(Network::HTTPRequest::Method::POST, serverURI(server, "/actors"), {}, &report);

	/* ⚠️ Unlike download(), a non-2xx is NOT a failure here: an API says what went wrong in the
	 * body, and swallowing it would leave the caller with a bare status code. */
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->response.codeResponse(), 422);
	EXPECT_EQ(result->body, R"({"error":"level must be positive"})");
	EXPECT_EQ(report.outcome, Network::DownloadOutcome::HTTPStatus);
	EXPECT_EQ(report.statusCode, 422);
	EXPECT_EQ(report.contentType, "application/json");
}

TEST(NetworkHTTPSClientRequest, aSuccessfulRequestReportsSuccess)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return statusResponse(201, R"({"id":42})");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	Network::DownloadReport report;

	const auto result = client.request(Network::HTTPRequest::Method::POST, serverURI(server, "/actors"), {}, &report);

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->response.codeResponse(), 201);
	EXPECT_EQ(report.outcome, Network::DownloadOutcome::Success);
	EXPECT_EQ(report.statusCode, 201);
}

TEST(NetworkHTTPSClientRequest, refusesAPlainHTTPTarget)
{
	auto tlsContext = makeTrustingClientContext({});

	Network::HTTPSClient client{tlsContext};

	Network::DownloadReport report;

	const auto result = client.request(Network::HTTPRequest::Method::POST, Network::URI{"http://localhost/whatever"}, {}, &report);

	EXPECT_FALSE(result.has_value());
	EXPECT_EQ(report.outcome, Network::DownloadOutcome::BadScheme);
}
