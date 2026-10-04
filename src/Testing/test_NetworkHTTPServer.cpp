/*
 * src/Testing/test_NetworkHTTPServer.cpp
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
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Network/HTTPServer.hpp"
#include "Network/HTTPSClient.hpp"
#include "Network/TLSConnection.hpp"

using namespace EmEn::Base::Network;

namespace
{
	/** @brief A parsed raw response. */
	struct RawResponse
	{
		int status{0};
		std::string head;
		std::string body;
	};

	/**
	 * @brief Sends raw bytes to 127.0.0.1:port and reads until the server closes.
	 * @param port The port.
	 * @param request The raw request bytes.
	 * @return std::string Everything the server sent.
	 */
	std::string
	rawExchange (uint16_t port, std::string_view request)
	{
		asio::io_context ioContext;
		asio::ip::tcp::socket socket{ioContext};
		asio::error_code ec;

		socket.connect(asio::ip::tcp::endpoint{asio::ip::make_address("127.0.0.1"), port}, ec);

		if ( ec )
		{
			return {};
		}

		static_cast< void >(asio::write(socket, asio::buffer(request.data(), request.size()), ec));

		std::string received;
		std::array< char, 65536 > buffer{};

		while ( true )
		{
			const auto bytes = socket.read_some(asio::buffer(buffer), ec);

			received.append(buffer.data(), bytes);

			if ( ec )
			{
				break;
			}
		}

		return received;
	}

	/**
	 * @brief Splits the first response of a raw stream.
	 * @param raw The raw bytes.
	 * @return RawResponse
	 */
	RawResponse
	firstResponse (const std::string & raw)
	{
		RawResponse response;

		if ( raw.size() < 12 || !raw.starts_with("HTTP/1.1 ") )
		{
			return response;
		}

		for ( size_t index = 9; index < 12; ++index )
		{
			if ( raw[index] < '0' || raw[index] > '9' )
			{
				return response;
			}

			response.status = (response.status * 10) + (raw[index] - '0');
		}

		const auto headEnd = raw.find("\r\n\r\n");

		if ( headEnd == std::string::npos )
		{
			return response;
		}

		response.head = raw.substr(0, headEnd + 2);
		response.body = raw.substr(headEnd + 4);

		return response;
	}

	/**
	 * @brief Returns a GET request for 127.0.0.1:port.
	 * @param port The port.
	 * @param target The target.
	 * @param extraHeaders Complete header lines.
	 * @return std::string
	 */
	std::string
	get (uint16_t port, std::string_view target, std::string_view extraHeaders = {})
	{
		return "GET " + std::string{target} + " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) + "\r\n" + std::string{extraHeaders} + "Connection: close\r\n\r\n";
	}

	/** @brief Options for a loopback test server: an ephemeral port, short timeouts. */
	HTTPServerOptions
	testOptions ()
	{
		HTTPServerOptions options;
		options.address = "127.0.0.1";
		options.port = 0;
		options.name = "Test server";
		options.requestTimeoutSeconds = 2;
		options.idleTimeoutSeconds = 2;

		return options;
	}

	/** @brief A handler answering "hello" with the request's path in a header. */
	void
	helloHandler (const std::shared_ptr< HTTPServerConnection > & connection)
	{
		connection->respond(200, "text/plain", "hello", "X-Path: " + connection->request().path() + "\r\n");
	}

	/**
	 * @brief Writes a file of 'size' bytes, byte i = i * 7 mod 251.
	 * @param size The size.
	 * @return std::filesystem::path
	 */
	std::filesystem::path
	patternFile (size_t size)
	{
		std::error_code ec;
		auto filepath = std::filesystem::temp_directory_path(ec) / ("emeraude-httpserver-test-" + std::to_string(size) + ".bin");
		std::string content(size, '\0');

		for ( size_t index = 0; index < size; ++index )
		{
			content[index] = static_cast< char >((index * 7) % 251);
		}

		std::ofstream file{filepath, std::ios::binary | std::ios::trunc};
		file.write(content.data(), static_cast< std::streamsize >(content.size()));

		return filepath;
	}
}

TEST(NetworkHTTPServer, ByteRangeParsing)
{
	using Range = std::optional< std::pair< uint64_t, uint64_t > >;

	bool unsatisfiable = false;

	EXPECT_EQ(parseByteRange("bytes=10-19", 100, unsatisfiable), (Range{{10, 10}}));
	EXPECT_EQ(parseByteRange("bytes=90-", 100, unsatisfiable), (Range{{90, 10}}));
	/* The last position is clamped to the size. */
	EXPECT_EQ(parseByteRange("bytes=90-500", 100, unsatisfiable), (Range{{90, 10}}));
	/* A suffix: the last N bytes, clamped. */
	EXPECT_EQ(parseByteRange("bytes=-30", 100, unsatisfiable), (Range{{70, 30}}));
	EXPECT_EQ(parseByteRange("bytes=-500", 100, unsatisfiable), (Range{{0, 100}}));
	EXPECT_FALSE(unsatisfiable);

	/* Unsatisfiable: past the end, an empty suffix. */
	EXPECT_FALSE(parseByteRange("bytes=100-", 100, unsatisfiable).has_value());
	EXPECT_TRUE(unsatisfiable);
	EXPECT_FALSE(parseByteRange("bytes=-0", 100, unsatisfiable).has_value());
	EXPECT_TRUE(unsatisfiable);

	/* Ignored (the whole file is served): malformed, another unit, several ranges, a sign, an overflow. */
	for ( const auto * const value : {"bytes=20-10", "items=0-1", "bytes=0-1,5-6", "bytes=+1-2", "bytes=a-b", "bytes=", "bytes=99999999999999999999-"} )
	{
		EXPECT_FALSE(parseByteRange(value, 100, unsatisfiable).has_value()) << value;
		EXPECT_FALSE(unsatisfiable) << value;
	}
}

TEST(NetworkHTTPServer, AnswersOnLoopback)
{
	HTTPServer server{testOptions()};

	ASSERT_TRUE(server.start(helloHandler));
	ASSERT_NE(server.port(), 0);
	EXPECT_EQ(server.baseURL(), "http://127.0.0.1:" + std::to_string(server.port()));

	const auto response = firstResponse(rawExchange(server.port(), get(server.port(), "/a/b?x=1")));

	EXPECT_EQ(response.status, 200);
	EXPECT_EQ(response.body, "hello");
	EXPECT_NE(response.head.find("X-Path: /a/b\r\n"), std::string::npos);
	EXPECT_NE(response.head.find("Content-Length: 5\r\n"), std::string::npos);

	server.stop();
	EXPECT_FALSE(server.isRunning());
	EXPECT_EQ(server.port(), 0);
}

TEST(NetworkHTTPServer, RefusesForeignHostAndOrigin)
{
	HTTPServer server{testOptions()};

	ASSERT_TRUE(server.start(helloHandler));

	const auto port = server.port();
	const auto foreignHost = "GET / HTTP/1.1\r\nHost: attacker.example:" + std::to_string(port) + "\r\nConnection: close\r\n\r\n";

	EXPECT_EQ(firstResponse(rawExchange(port, foreignHost)).status, 403);
	EXPECT_EQ(firstResponse(rawExchange(port, get(port, "/", "Origin: http://attacker.example\r\n"))).status, 403);
	EXPECT_EQ(firstResponse(rawExchange(port, get(port, "/", "Origin: http://127.0.0.1:" + std::to_string(port) + "\r\n"))).status, 200);
}

TEST(NetworkHTTPServer, BearerToken)
{
	auto options = testOptions();
	options.bearerToken = "s3cret";

	HTTPServer server{options};

	ASSERT_TRUE(server.start(helloHandler));

	const auto port = server.port();
	const auto refused = firstResponse(rawExchange(port, get(port, "/")));

	EXPECT_EQ(refused.status, 401);
	EXPECT_NE(refused.head.find("WWW-Authenticate: Bearer\r\n"), std::string::npos);
	EXPECT_EQ(firstResponse(rawExchange(port, get(port, "/", "Authorization: Bearer s3creT\r\n"))).status, 401);
	EXPECT_EQ(firstResponse(rawExchange(port, get(port, "/", "Authorization: Bearer s3cret!\r\n"))).status, 401);
	EXPECT_EQ(firstResponse(rawExchange(port, get(port, "/", "Authorization: Bearer s3cret\r\n"))).status, 200);
}

TEST(NetworkHTTPServer, NonLoopbackNeedsAToken)
{
	auto options = testOptions();
	options.address = "0.0.0.0";

	HTTPServer refused{options};
	EXPECT_FALSE(refused.start(helloHandler));
	EXPECT_FALSE(refused.isRunning());

	options.address = "not-an-address";

	HTTPServer invalid{options};
	EXPECT_FALSE(invalid.start(helloHandler));
}

TEST(NetworkHTTPServer, RefusesSmugglingAndBadFraming)
{
	auto options = testOptions();
	options.maxBodyBytes = 64;

	HTTPServer server{options};

	ASSERT_TRUE(server.start(helloHandler));

	const auto port = server.port();
	const auto host = "Host: 127.0.0.1:" + std::to_string(port) + "\r\n";

	/* Two Content-Length, two Host, two Range: refused, never chosen between. */
	EXPECT_EQ(firstResponse(rawExchange(port, "POST / HTTP/1.1\r\n" + host + "Content-Length: 1\r\nContent-Length: 2\r\n\r\nab")).status, 400);
	EXPECT_EQ(firstResponse(rawExchange(port, "GET / HTTP/1.1\r\n" + host + host + "\r\n")).status, 400);
	EXPECT_EQ(firstResponse(rawExchange(port, "GET / HTTP/1.1\r\n" + host + "Range: bytes=0-1\r\nRange: bytes=2-3\r\n\r\n")).status, 400);
	/* No chunked bodies. */
	EXPECT_EQ(firstResponse(rawExchange(port, "POST / HTTP/1.1\r\n" + host + "Transfer-Encoding: chunked\r\n\r\n0\r\n\r\n")).status, 501);
	/* A POST says its length; a length is digits only and bounded. */
	EXPECT_EQ(firstResponse(rawExchange(port, "POST / HTTP/1.1\r\n" + host + "\r\n")).status, 411);
	EXPECT_EQ(firstResponse(rawExchange(port, "POST / HTTP/1.1\r\n" + host + "Content-Length: -1\r\n\r\n")).status, 400);
	EXPECT_EQ(firstResponse(rawExchange(port, "POST / HTTP/1.1\r\n" + host + "Content-Length: 65\r\n\r\n")).status, 413);
	/* A malformed request line, an unknown version. */
	EXPECT_EQ(firstResponse(rawExchange(port, "GET\r\n" + host + "\r\n")).status, 400);
	EXPECT_EQ(firstResponse(rawExchange(port, "GET / HTTP/2.0\r\n" + host + "\r\n")).status, 400);
}

TEST(NetworkHTTPServer, RefusesOversizedHead)
{
	auto options = testOptions();
	options.maxHeaderBytes = 256;
	options.maxBodyBytes = 16;

	HTTPServer server{options};

	ASSERT_TRUE(server.start(helloHandler));

	const auto port = server.port();
	const auto request = "GET / HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) + "\r\nX-Padding: " + std::string(1024, 'a') + "\r\n\r\n";

	EXPECT_EQ(firstResponse(rawExchange(port, request)).status, 431);
}

TEST(NetworkHTTPServer, KeepAliveServesPipelinedRequests)
{
	HTTPServer server{testOptions()};

	ASSERT_TRUE(server.start(helloHandler));

	const auto port = server.port();
	const auto host = "Host: 127.0.0.1:" + std::to_string(port) + "\r\n";
	const auto raw = rawExchange(port, "GET /one HTTP/1.1\r\n" + host + "\r\nGET /two HTTP/1.1\r\n" + host + "Connection: close\r\n\r\n");

	EXPECT_NE(raw.find("X-Path: /one\r\n"), std::string::npos);
	EXPECT_NE(raw.find("X-Path: /two\r\n"), std::string::npos);
	EXPECT_NE(raw.find("Connection: keep-alive\r\n"), std::string::npos);
}

TEST(NetworkHTTPServer, ServesFilesWithRanges)
{
	/* Larger than one chunk (256 KiB), not a multiple of it. */
	constexpr size_t Size{(600 * 1024) + 13};
	const auto filepath = patternFile(Size);

	std::string expected;
	{
		std::ifstream file{filepath, std::ios::binary};
		expected.assign(std::istreambuf_iterator< char >{file}, std::istreambuf_iterator< char >{});
	}
	ASSERT_EQ(expected.size(), Size);

	HTTPServer server{testOptions()};

	ASSERT_TRUE(server.start([&filepath] (const std::shared_ptr< HTTPServerConnection > & connection) {
		if ( connection->request().path() == "/missing" )
		{
			connection->respondFile(filepath.string() + ".absent", "application/octet-stream");

			return;
		}

		connection->respondFile(filepath, "application/octet-stream");
	}));

	const auto port = server.port();

	const auto whole = firstResponse(rawExchange(port, get(port, "/file")));
	EXPECT_EQ(whole.status, 200);
	EXPECT_NE(whole.head.find("Accept-Ranges: bytes\r\n"), std::string::npos);
	EXPECT_NE(whole.head.find("Content-Length: " + std::to_string(Size) + "\r\n"), std::string::npos);
	EXPECT_TRUE(whole.body == expected);

	const auto middle = firstResponse(rawExchange(port, get(port, "/file", "Range: bytes=262140-262150\r\n")));
	EXPECT_EQ(middle.status, 206);
	EXPECT_NE(middle.head.find("Content-Range: bytes 262140-262150/" + std::to_string(Size) + "\r\n"), std::string::npos);
	EXPECT_EQ(middle.body, expected.substr(262140, 11));

	const auto tail = firstResponse(rawExchange(port, get(port, "/file", "Range: bytes=-7\r\n")));
	EXPECT_EQ(tail.status, 206);
	EXPECT_EQ(tail.body, expected.substr(Size - 7));

	const auto resumed = firstResponse(rawExchange(port, get(port, "/file", "Range: bytes=1000-\r\n")));
	EXPECT_EQ(resumed.status, 206);
	EXPECT_TRUE(resumed.body == expected.substr(1000));

	const auto beyond = firstResponse(rawExchange(port, get(port, "/file", "Range: bytes=" + std::to_string(Size) + "-\r\n")));
	EXPECT_EQ(beyond.status, 416);
	EXPECT_NE(beyond.head.find("Content-Range: bytes */" + std::to_string(Size) + "\r\n"), std::string::npos);

	/* An If-Range needs a validator this server does not emit: the whole file. */
	EXPECT_EQ(firstResponse(rawExchange(port, get(port, "/file", "Range: bytes=0-1\r\nIf-Range: \"x\"\r\n"))).status, 200);

	const auto head = firstResponse(rawExchange(port, "HEAD /file HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) + "\r\nConnection: close\r\n\r\n"));
	EXPECT_EQ(head.status, 200);
	EXPECT_NE(head.head.find("Content-Length: " + std::to_string(Size) + "\r\n"), std::string::npos);
	EXPECT_TRUE(head.body.empty());

	EXPECT_EQ(firstResponse(rawExchange(port, get(port, "/missing"))).status, 404);

	server.stop();

	std::error_code ec;
	std::filesystem::remove(filepath, ec);
}

TEST(NetworkHTTPServer, AnswersFromAnotherThreadThroughPost)
{
	HTTPServer server{testOptions()};
	std::weak_ptr< HTTPServerConnection > pending;
	std::atomic< bool > received{false};

	ASSERT_TRUE(server.start([&pending, &received] (const std::shared_ptr< HTTPServerConnection > & connection) {
		pending = connection;
		received = true;
	}));

	const auto port = server.port();

	std::thread worker{[&server, &pending, &received] () {
		while ( !received )
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{5});
		}

		server.post([&pending] () {
			if ( const auto connection = pending.lock() )
			{
				connection->respond(200, "application/json", "{}");
			}
		});
	}};

	const auto response = firstResponse(rawExchange(port, get(port, "/")));

	worker.join();

	EXPECT_EQ(response.status, 200);
	EXPECT_EQ(response.body, "{}");
}

TEST(NetworkHTTPServer, StreamsGetTheirLastWordsAtShutdown)
{
	HTTPServer server{testOptions()};
	std::atomic< uint64_t > closedId{0};
	std::atomic< bool > streaming{false};

	ASSERT_TRUE(server.start(
		[&streaming] (const std::shared_ptr< HTTPServerConnection > & connection) {
			connection->startStream("text/event-stream", "Cache-Control: no-cache\r\n", ":\n\n", 15);
			connection->sendStream("data: first\n\n");
			streaming = true;
		},
		[] (HTTPServerConnection & connection) {
			connection.writeNowAndClose("data: last\n\n");
		},
		[&closedId] (uint64_t id) {
			closedId = id;
		}
	));

	const auto port = server.port();

	std::thread stopper{[&server, &streaming] () {
		while ( !streaming )
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{5});
		}

		std::this_thread::sleep_for(std::chrono::milliseconds{100});
		server.stop();
	}};

	const auto raw = rawExchange(port, get(port, "/events"));

	stopper.join();

	EXPECT_EQ(firstResponse(raw).status, 200);
	EXPECT_NE(raw.find("Content-Type: text/event-stream\r\n"), std::string::npos);
	EXPECT_EQ(raw.find("Content-Length"), std::string::npos);
	EXPECT_NE(raw.find("data: first\n\n"), std::string::npos);
	EXPECT_NE(raw.find("data: last\n\n"), std::string::npos);
	EXPECT_NE(closedId.load(), 0U);
}

TEST(NetworkHTTPServer, BoundsConnections)
{
	auto options = testOptions();
	options.maxConnections = 1;

	HTTPServer server{options};

	ASSERT_TRUE(server.start(helloHandler));

	const auto port = server.port();

	/* A first client holds the only slot, idle. */
	asio::io_context ioContext;
	asio::ip::tcp::socket holder{ioContext};
	asio::error_code ec;
	holder.connect(asio::ip::tcp::endpoint{asio::ip::make_address("127.0.0.1"), port}, ec);
	ASSERT_FALSE(ec);

	/* NOTE: the accept of the holder runs on the network thread; leave it the time to register. */
	std::this_thread::sleep_for(std::chrono::milliseconds{100});

	EXPECT_EQ(firstResponse(rawExchange(port, get(port, "/"))).status, 503);

	holder.close(ec);
}

TEST(NetworkHTTPServer, PrivateNetworkAddresses)
{
	const auto isPrivate = [] (const char * text) {
		return isPrivateNetworkAddress(asio::ip::make_address(text));
	};

	for ( const auto * const address : {"127.0.0.1", "127.255.255.254", "10.0.0.1", "10.255.255.255", "172.16.0.0", "172.31.255.255", "192.168.1.10", "169.254.0.1", "::1", "fe80::1", "febf::1", "fc00::1", "fdff::1", "::ffff:192.168.1.10"} )
	{
		EXPECT_TRUE(isPrivate(address)) << address;
	}

	/* The edges just outside each range, and public ones. */
	for ( const auto * const address : {"9.255.255.255", "11.0.0.0", "172.15.255.255", "172.32.0.0", "192.167.255.255", "192.169.0.0", "169.253.255.255", "169.255.0.0", "8.8.8.8", "100.64.0.1", "0.0.0.0", "fec0::1", "fe7f::1", "fbff::1", "fe00::1", "2001:db8::1", "::", "::ffff:8.8.8.8"} )
	{
		EXPECT_FALSE(isPrivate(address)) << address;
	}
}

TEST(NetworkHTTPServer, CleartextDownloadFromAPrivatePeer)
{
	constexpr size_t Size{(300 * 1024) + 5};
	const auto source = patternFile(Size);
	std::error_code ec;
	const auto destination = std::filesystem::temp_directory_path(ec) / "emeraude-httpserver-test-download.bin";

	auto options = testOptions();
	options.bearerToken = "peer-token";

	HTTPServer server{options};

	ASSERT_TRUE(server.start([&source] (const std::shared_ptr< HTTPServerConnection > & connection) {
		connection->respondFile(source, "application/octet-stream");
	}));

	const URI uri{server.baseURL() + "/files/pattern.bin"};
	asio::ssl::context tlsContext{asio::ssl::context::tls_client};

	HTTPRequestOptions authorized;
	authorized.headers.emplace_back("Authorization", "Bearer peer-token");

	/* Refused unless the client opts in: https only by default. */
	{
		const HTTPSClient client{tlsContext};
		DownloadReport report;

		EXPECT_FALSE(client.download(uri, destination, authorized, {}, &report));
		EXPECT_EQ(report.outcome, DownloadOutcome::BadScheme);
	}

	HTTPSClientOptions clientOptions;
	clientOptions.allowPrivateCleartext = true;
	clientOptions.useEnvironmentProxy = false;

	const HTTPSClient client{tlsContext, clientOptions};

	/* Without the token: the server's 401. */
	{
		DownloadReport report;

		EXPECT_FALSE(client.download(uri, destination, {}, &report));
		EXPECT_EQ(report.outcome, DownloadOutcome::HTTPStatus);
		EXPECT_EQ(report.statusCode, 401);
	}

	/* With it: the whole file, byte for byte (the Host field carries the port, which the loopback server checks). */
	{
		DownloadReport report;

		ASSERT_TRUE(client.download(uri, destination, authorized, {}, &report));
		EXPECT_EQ(std::filesystem::file_size(destination, ec), Size);

		std::ifstream expectedFile{source, std::ios::binary};
		std::ifstream actualFile{destination, std::ios::binary};
		const std::string expected{std::istreambuf_iterator< char >{expectedFile}, std::istreambuf_iterator< char >{}};
		const std::string actual{std::istreambuf_iterator< char >{actualFile}, std::istreambuf_iterator< char >{}};

		EXPECT_TRUE(expected == actual);
	}

	/* A public address is refused before anything is sent, even opted in. */
	{
		DownloadReport report;

		EXPECT_FALSE(client.download(URI{"http://8.8.8.8:9/never"}, destination, authorized, {}, &report));
		EXPECT_EQ(report.outcome, DownloadOutcome::Unreachable);
	}

	server.stop();

	std::filesystem::remove(source, ec);
	std::filesystem::remove(destination, ec);
}
