/*
 * src/Testing/test_NetworkHTTPSClient.cpp
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
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <optional>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

/* Local inclusions. */
#include "Network/HTTPSClient.hpp"
#include "TLSTestHelpers.hpp"

using namespace EmEn::Base;
using EmEn::Base::Testing::HTTPSTestServer;
using EmEn::Base::Testing::generateServerCredentials;

namespace
{
	/** @brief Portable setenv (the MSVC CRT has no POSIX setenv/unsetenv). */
	void
	setTestEnv (const char * name, const char * value) noexcept
	{
#if defined(_WIN32)
		_putenv_s(name, value);
#else
		setenv(name, value, 1);
#endif
	}

	/** @brief Portable unsetenv. */
	void
	unsetTestEnv (const char * name) noexcept
	{
#if defined(_WIN32)
		_putenv_s(name, ""); /* An empty value removes the variable from the CRT environment. */
#else
		unsetenv(name);
#endif
	}

	/** @brief Builds a client TLS context trusting the given (PEM) certificate. */
	asio::ssl::context
	makeTrustingClientContext (const std::string & certificatePEM) noexcept
	{
		asio::ssl::context context{asio::ssl::context::tls_client};

		asio::error_code error;

		context.add_certificate_authority(asio::buffer(certificatePEM), error);

		return context;
	}

	/** @brief Builds a client TLS context trusting NOTHING the test server can present.
	 * @note Used to make the handshake fail on chain verification while the peer IS reachable -
	 * the only way to tell DownloadOutcome::TLSFailure from Unreachable hermetically. */
	asio::ssl::context
	makeUntrustingClientContext () noexcept
	{
		asio::ssl::context context{asio::ssl::context::tls_client};

		/* No certificate authority added on purpose: the server's self-signed certificate has
		 * nothing to chain to. Verification stays ON - that is the point of the test. */
		asio::error_code error;
		context.set_verify_mode(asio::ssl::verify_peer, error);

		return context;
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

TEST(NetworkHTTPSClient, getReturnsBody)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return plainResponse("Hello from the engine!");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto result = client.get(serverURI(server, "/asset.txt"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->response.codeResponse(), 200);
	EXPECT_EQ(result->body, "Hello from the engine!");
}

TEST(NetworkHTTPSClient, getDecodesChunkedBody)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return std::string{
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Connection: close\r\n"
			"\r\n"
			"6\r\nHello \r\n"
			"6\r\nchunks\r\n"
			"0\r\n\r\n"
		};
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto result = client.get(serverURI(server, "/chunked"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->body, "Hello chunks");
}

TEST(NetworkHTTPSClient, headReturnsHeadersWithoutBody)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & request) {
		/* A HEAD answer announces a length but carries no body. */
		EXPECT_EQ(request.rfind("HEAD ", 0), 0U);

		return std::string{
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: 1234\r\n"
			"Connection: close\r\n"
			"\r\n"
		};
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto result = client.head(serverURI(server, "/asset.bin"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->response.codeResponse(), 200);
	EXPECT_EQ(result->response.value(Network::HTTPResponse::ContentLength), "1234");
	EXPECT_TRUE(result->body.empty());
}

TEST(NetworkHTTPSClient, followsRedirectChain)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	/* /old -> (relative) /moved -> (absolute) /final -> 200. */
	HTTPSTestServer * serverPointer = nullptr;

	HTTPSTestServer server{credentials, [&serverPointer] (const std::string & request) {
		const auto target = requestTarget(request);

		if ( target == "/old" )
		{
			return redirectResponse(302, "/moved");
		}

		if ( target == "/moved" )
		{
			return redirectResponse(301, "https://localhost:" + std::to_string(serverPointer->port()) + "/final");
		}

		return plainResponse("final content");
	}};
	serverPointer = &server;
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto result = client.get(serverURI(server, "/old"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->response.codeResponse(), 200);
	EXPECT_EQ(result->body, "final content");
	EXPECT_EQ(server.requestCount(), 3U);
}

TEST(NetworkHTTPSClient, redirectLoopIsCapped)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return redirectResponse(302, "/loop");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClientOptions options;
	options.maxRedirects = 3;

	Network::HTTPSClient client{tlsContext, options};

	EXPECT_FALSE(client.get(serverURI(server, "/loop")).has_value());

	/* Initial request + 3 followed redirects. */
	EXPECT_EQ(server.requestCount(), 4U);
}

TEST(NetworkHTTPSClient, refusesDowngradeRedirect)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return redirectResponse(302, "http://localhost/insecure");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	EXPECT_FALSE(client.get(serverURI(server, "/secure")).has_value());
	EXPECT_EQ(server.requestCount(), 1U);
}

TEST(NetworkHTTPSClient, downloadStreamsToFile)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	/* A body large enough to cross several transport reads. */
	std::string payload;

	while ( payload.size() < 300000 )
	{
		payload += "0123456789abcdef";
	}

	HTTPSTestServer server{credentials, [&payload] (const std::string & /*request*/) {
		return plainResponse(payload);
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto filepath = std::filesystem::temp_directory_path() / "emeraude-base-download-test.bin";

	ASSERT_TRUE(client.download(serverURI(server, "/big.bin"), filepath));

	std::stringstream content;

	{
		/* Scope the stream so its handle is released before remove(): on Windows a file
		 * with an open handle cannot be deleted (unlike POSIX unlink-while-open). */
		std::ifstream file{filepath, std::ios::binary};
		content << file.rdbuf();
	}

	EXPECT_EQ(content.str(), payload);

	std::filesystem::remove(filepath);
}

TEST(NetworkHTTPSClient, downloadReportsProgressWithContentLength)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::string payload;

	while ( payload.size() < 300000 )
	{
		payload += "0123456789abcdef";
	}

	HTTPSTestServer server{credentials, [&payload] (const std::string & /*request*/) {
		return plainResponse(payload);
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto filepath = std::filesystem::temp_directory_path() / "emeraude-base-download-progress-test.bin";

	std::vector< uint64_t > received;
	std::vector< std::optional< uint64_t > > totals;

	ASSERT_TRUE(client.download(serverURI(server, "/big.bin"), filepath, [&received, &totals] (uint64_t bytesReceived, std::optional< uint64_t > bytesTotal) {
		received.push_back(bytesReceived);
		totals.push_back(bytesTotal);
	}));

	/* Several transport reads for 300 KB: the hook fires more than once, monotonically. */
	ASSERT_GE(received.size(), 2U);
	EXPECT_TRUE(std::is_sorted(received.cbegin(), received.cend()));
	EXPECT_EQ(received.back(), payload.size());

	/* Content-Length framing: the total is known from the first call and never changes. */
	for ( const auto & total : totals )
	{
		ASSERT_TRUE(total.has_value());
		EXPECT_EQ(*total, payload.size());
	}

	std::filesystem::remove(filepath);
}

TEST(NetworkHTTPSClient, downloadReportsProgressWithoutTotalWhenChunked)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return std::string{
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Connection: close\r\n"
			"\r\n"
			"6\r\nHello \r\n"
			"6\r\nchunks\r\n"
			"0\r\n\r\n"
		};
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto filepath = std::filesystem::temp_directory_path() / "emeraude-base-download-progress-chunked-test.bin";

	uint64_t lastReceived = 0;
	bool anyTotalKnown = false;
	size_t calls = 0;

	ASSERT_TRUE(client.download(serverURI(server, "/chunked"), filepath, [&] (uint64_t bytesReceived, std::optional< uint64_t > bytesTotal) {
		lastReceived = bytesReceived;
		anyTotalKnown = anyTotalKnown || bytesTotal.has_value();
		calls++;
	}));

	EXPECT_GE(calls, 1U);
	EXPECT_EQ(lastReceived, std::string{"Hello chunks"}.size());
	EXPECT_FALSE(anyTotalKnown);

	std::filesystem::remove(filepath);
}

TEST(NetworkHTTPSClient, downloadWithoutHookStillWorks)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return plainResponse("no hook");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto filepath = std::filesystem::temp_directory_path() / "emeraude-base-download-nohook-test.bin";

	ASSERT_TRUE(client.download(serverURI(server, "/plain"), filepath));

	std::filesystem::remove(filepath);
}

TEST(NetworkHTTPSClient, downloadFailsOnErrorStatus)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return std::string{
			"HTTP/1.1 404 Not Found\r\n"
			"Content-Length: 9\r\n"
			"Connection: close\r\n"
			"\r\n"
			"not found"
		};
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto filepath = std::filesystem::temp_directory_path() / "emeraude-base-download-404.bin";

	EXPECT_FALSE(client.download(serverURI(server, "/missing.bin"), filepath));
	EXPECT_FALSE(std::filesystem::exists(filepath));
}

TEST(NetworkHTTPSClient, refusesHostWithInjectedCRLF)
{
	/* The host reaches a CONNECT request line, a Host: header and SNI. Percent-decoding used to
	 * let CR/LF through, i.e. request smuggling. URIDomain must refuse the host outright. */
	const Network::URI uri{"https://exam%0d%0aX-Smuggled:%20yes%0d%0aple.com/asset.bin"};

	EXPECT_TRUE(uri.uriDomain().hostname().name().empty());

	asio::ssl::context tlsContext{asio::ssl::context::tls_client};

	Network::HTTPSClient client{tlsContext};

	EXPECT_FALSE(client.get(uri).has_value());
}

TEST(NetworkHTTPSClient, refusesHostWithControlCharacters)
{
	for ( const auto * raw : {"https://ex%00ample.com/", "https://ex%20ample.com/", "https://ex%09ample.com/"} )
	{
		const Network::URI uri{raw};

		EXPECT_TRUE(uri.uriDomain().hostname().name().empty()) << raw;
	}
}

TEST(NetworkHTTPSClient, refusesOutOfRangePortInsteadOfFallingBackTo443)
{
	/* "https://host:99999/" used to connect to 443 — a typo or a hostile input silently reaching
	 * a different service. A declared-but-invalid port must refuse the URI. */
	asio::ssl::context tlsContext{asio::ssl::context::tls_client};

	Network::HTTPSClient client{tlsContext};

	EXPECT_FALSE(client.get(Network::URI{"https://localhost:99999/x"}).has_value());
	EXPECT_FALSE(client.get(Network::URI{"https://localhost:abc/x"}).has_value());
}

TEST(NetworkHTTPSClient, ipv6LiteralIsResolvedWithoutItsBrackets)
{
	/* The authority keeps the brackets ("[::1]"), getaddrinfo() must not see them. Port 1 is
	 * closed, so a stripped literal fails at CONNECT — never at resolution. */
	asio::ssl::context tlsContext{asio::ssl::context::tls_client};

	Network::HTTPSClientOptions options;
	options.transportTimeouts.connectTimeout = std::chrono::milliseconds{2000};

	Network::HTTPSClient client{tlsContext, options};

	EXPECT_FALSE(client.get(Network::URI{"https://[::1]:1/x"}).has_value());
}

TEST(NetworkHTTPSClient, truncatedUntilCloseBodyIsRejected)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	/* No Content-Length, no Transfer-Encoding: the body ends when the connection ends. The server
	 * then drops the connection WITHOUT close_notify — a truncated body must not be "complete". */
	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return std::string{
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: application/octet-stream\r\n"
			"Connection: close\r\n"
			"\r\n"
			"half-an-asset"
		};
	}};
	ASSERT_TRUE(server.isListening());

	server.setAbortWithoutCloseNotify(true);

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto filepath = std::filesystem::temp_directory_path() / "emeraude-base-truncated-untilclose.bin";

	EXPECT_FALSE(client.download(serverURI(server, "/asset.bin"), filepath));
	EXPECT_FALSE(std::filesystem::exists(filepath));
}

TEST(NetworkHTTPSClient, inMemoryBodyIsCapped)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::string payload(200000, 'x');

	HTTPSTestServer server{credentials, [&payload] (const std::string & /*request*/) {
		return plainResponse(payload);
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClientOptions options;
	options.maxInMemoryBodySize = 64 * 1024;

	Network::HTTPSClient client{tlsContext, options};

	/* Over the ceiling: refused rather than buffered whole. */
	EXPECT_FALSE(client.get(serverURI(server, "/big.txt")).has_value());
}

TEST(NetworkHTTPSClient, downloadIsCappedByMaxDownloadSize)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	std::string payload(200000, 'y');

	HTTPSTestServer server{credentials, [&payload] (const std::string & /*request*/) {
		return plainResponse(payload);
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClientOptions options;
	options.maxDownloadSize = 64 * 1024;

	Network::HTTPSClient client{tlsContext, options};

	const auto filepath = std::filesystem::temp_directory_path() / "emeraude-base-download-capped.bin";

	EXPECT_FALSE(client.download(serverURI(server, "/big.bin"), filepath));
	EXPECT_FALSE(std::filesystem::exists(filepath));
}

TEST(NetworkHTTPSClient, headStaysHeadAcrossA303)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	/* RFC 9110 §15.4.4 exempts HEAD from the 303 method rewrite. */
	HTTPSTestServer server{credentials, [] (const std::string & request) -> std::string {
		if ( requestTarget(request) == "/start" )
		{
			return redirectResponse(303, "/final");
		}

		if ( request.rfind("HEAD ", 0) != 0 )
		{
			return plainResponse("the 303 turned HEAD into GET");
		}

		return std::string{
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 12\r\n"
			"Connection: close\r\n"
			"\r\n"
		};
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto result = client.head(serverURI(server, "/start"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->response.codeResponse(), 200);
	EXPECT_TRUE(result->body.empty());
}

TEST(NetworkHTTPSClient, emptyHeaderValueDoesNotRejectTheResponse)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	/* RFC 9110 §5.5 allows an empty field value; it used to make the whole response unparseable. */
	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return std::string{
			"HTTP/1.1 200 OK\r\n"
			"X-Cache:\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 2\r\n"
			"Connection: close\r\n"
			"\r\n"
			"ok"
		};
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto result = client.get(serverURI(server, "/empty-header"));

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->body, "ok");
	EXPECT_TRUE(result->response.value("X-Cache").empty());
}

TEST(NetworkHTTPSClient, downloadOfAnEmptyBodyStillReportsProgressOnce)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return plainResponse({});
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto filepath = std::filesystem::temp_directory_path() / "emeraude-base-download-empty.bin";

	size_t calls = 0;
	uint64_t received = 42;

	ASSERT_TRUE(client.download(serverURI(server, "/empty.bin"), filepath, [&calls, &received] (uint64_t bytesReceived, std::optional< uint64_t > /*bytesTotal*/) {
		calls++;
		received = bytesReceived;
	}));

	EXPECT_EQ(calls, 1U);
	EXPECT_EQ(received, 0U);

	std::filesystem::remove(filepath);
}

TEST(NetworkHTTPSClient, downloadReportsWhyItFailed)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return std::string{
			"HTTP/1.1 404 Not Found\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 9\r\n"
			"Connection: close\r\n"
			"\r\n"
			"not found"
		};
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	const auto filepath = std::filesystem::temp_directory_path() / "emeraude-base-report-test.bin";

	/* A 404 must be distinguishable from a certificate failure and from a timeout. */
	Network::DownloadReport report;

	EXPECT_FALSE(client.download(serverURI(server, "/missing.bin"), filepath, {}, &report));
	EXPECT_EQ(report.outcome, Network::DownloadOutcome::HTTPStatus);
	EXPECT_EQ(report.statusCode, 404);
	EXPECT_EQ(report.contentType, "text/plain");

	/* Not https at all. */
	Network::DownloadReport schemeReport;
	EXPECT_FALSE(client.download(Network::URI{"http://localhost/x"}, filepath, {}, &schemeReport));
	EXPECT_EQ(schemeReport.outcome, Network::DownloadOutcome::BadScheme);

	/* Nothing listening. */
	Network::HTTPSClientOptions quick;
	quick.transportTimeouts.connectTimeout = std::chrono::milliseconds{1500};

	Network::HTTPSClient quickClient{tlsContext, quick};

	Network::DownloadReport unreachableReport;
	EXPECT_FALSE(quickClient.download(Network::URI{"https://127.0.0.1:1/x"}, filepath, {}, &unreachableReport));
	EXPECT_EQ(unreachableReport.outcome, Network::DownloadOutcome::Unreachable);

	/* A success reports Success and the media type. */
	HTTPSTestServer okServer{credentials, [] (const std::string & /*request*/) {
		return plainResponse("payload");
	}};
	ASSERT_TRUE(okServer.isListening());

	Network::DownloadReport okReport;
	ASSERT_TRUE(client.download(serverURI(okServer, "/ok.bin"), filepath, {}, &okReport));
	EXPECT_EQ(okReport.outcome, Network::DownloadOutcome::Success);
	EXPECT_EQ(okReport.statusCode, 200);
	EXPECT_EQ(okReport.contentType, "text/plain");

	std::filesystem::remove(filepath);
}

TEST(NetworkHTTPSClient, downloadReportsTLSFailureWhenTheCertificateIsRefused)
{
	/* Regression test for a value that existed, was documented, and was NEVER produced: until
	 * 2026-08-28 a refused certificate came back as DownloadOutcome::Unreachable, because
	 * TLSConnection::connect() returns one bool for "never reached the peer" and "peer refused the
	 * handshake". A caller retries Unreachable and must never retry a refused certificate, so the
	 * two must not collapse. The sibling assertion in downloadReportsWhyItFailed covers the other
	 * side (nothing listening on port 1 -> still Unreachable). */
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return plainResponse("never reached");
	}};
	ASSERT_TRUE(server.isListening());

	/* The server is up and reachable; only the trust chain is missing. */
	auto tlsContext = makeUntrustingClientContext();

	Network::HTTPSClient client{tlsContext};

	const auto filepath = std::filesystem::temp_directory_path() / "emeraude-tlsfailure-test.bin";

	std::filesystem::remove(filepath);

	Network::DownloadReport report;

	EXPECT_FALSE(client.download(serverURI(server, "/x.bin"), filepath, {}, &report));
	EXPECT_EQ(report.outcome, Network::DownloadOutcome::TLSFailure);
	EXPECT_FALSE(std::filesystem::exists(filepath));

	std::filesystem::remove(filepath);
}

TEST(NetworkHTTPSClient, refusesPlainHTTPScheme)
{
	asio::ssl::context tlsContext{asio::ssl::context::tls_client};

	Network::HTTPSClient client{tlsContext};

	EXPECT_FALSE(client.get(Network::URI{"http://localhost/clear.txt"}).has_value());
}

TEST(NetworkHTTPSClient, getThroughProxyTunnel)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	/* Proxy-mode server: accepts the plaintext CONNECT, then serves as the target. */
	HTTPSTestServer proxy{credentials, [] (const std::string & request) {
		EXPECT_EQ(request.rfind("GET ", 0), 0U);

		return plainResponse("through the tunnel");
	}, true};
	ASSERT_TRUE(proxy.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClientOptions options;
	options.proxy = "127.0.0.1:" + std::to_string(proxy.port());
	options.useEnvironmentProxy = false;

	Network::HTTPSClient client{tlsContext, options};

	/* Target authority is 'localhost:8443' (never actually dialed — the proxy
	 * tunnels), so hostname verification uses the target cert (SAN localhost). */
	const auto result = client.get(Network::URI{"https://localhost:8443/tunneled"});

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->response.codeResponse(), 200);
	EXPECT_EQ(result->body, "through the tunnel");
	EXPECT_EQ(proxy.tunnelCount(), 1U);
}

TEST(NetworkHTTPSClient, environmentNoProxyBypassesProxy)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	/* Direct server (NOT proxy mode): reaching it proves the proxy was bypassed. */
	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		return plainResponse("bypassed");
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	/* https_proxy points at a dead port; no_proxy lists localhost → the client must
	 * ignore the proxy and connect directly to the (direct-mode) server. */
	setTestEnv("https_proxy", "127.0.0.1:1");
	setTestEnv("no_proxy", "localhost");

	Network::HTTPSClientOptions options;
	options.useEnvironmentProxy = true;

	Network::HTTPSClient client{tlsContext, options};

	const auto result = client.get(serverURI(server, "/direct"));

	unsetTestEnv("https_proxy");
	unsetTestEnv("no_proxy");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->body, "bypassed");
	EXPECT_EQ(server.tunnelCount(), 0U);
}

TEST(NetworkHTTPSClient, environmentProxyIsUsed)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer proxy{credentials, [] (const std::string & /*request*/) {
		return plainResponse("via env proxy");
	}, true};
	ASSERT_TRUE(proxy.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	const auto proxyEnv = "http://127.0.0.1:" + std::to_string(proxy.port());
	setTestEnv("https_proxy", proxyEnv.c_str());
	unsetTestEnv("no_proxy");

	Network::HTTPSClientOptions options;
	options.useEnvironmentProxy = true;

	Network::HTTPSClient client{tlsContext, options};

	const auto result = client.get(Network::URI{"https://localhost:8443/via-env"});

	unsetTestEnv("https_proxy");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->body, "via env proxy");
	EXPECT_EQ(proxy.tunnelCount(), 1U);
}

TEST(NetworkHTTPSClient, truncatedBodyFails)
{
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	HTTPSTestServer server{credentials, [] (const std::string & /*request*/) {
		/* Announces 100 bytes, delivers 9, then closes. */
		return std::string{
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: 100\r\n"
			"Connection: close\r\n"
			"\r\n"
			"truncated"
		};
	}};
	ASSERT_TRUE(server.isListening());

	auto tlsContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::HTTPSClient client{tlsContext};

	EXPECT_FALSE(client.get(serverURI(server, "/broken.bin")).has_value());
}