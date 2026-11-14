/*
 * src/Testing/test_NetworkTLSConnection.cpp
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
#include <chrono>
#include <compare>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

/* Third-party inclusions. */
#include "asio.hpp"
#include <openssl/ssl.h>
#include <openssl/tls1.h>

/* Local inclusions. */
#include "DiagnosticsCapture.hpp"
#include "Logging/Severity.hpp"
#include "Network/TLSConnection.hpp"
#include "TLSTestHelpers.hpp"

using namespace EmEn::Base;
using EmEn::Base::Testing::ServerCredentials;
using EmEn::Base::Testing::generateServerCredentials;

namespace
{
	/**
	 * @brief A one-shot hermetic TLS server on 127.0.0.1 (ephemeral port).
	 * @note Echo mode: reads one message and echoes it back, then shuts down.
	 * Mute mode: performs the handshake then never answers (read-timeout tests).
	 * @note A non-zero maximumVersion caps the protocol the server offers (TLS1_1_VERSION: an outdated server) and drops
	 * its security level to 0, the only level at which the TLS library still negotiates below TLS 1.2.
	 */
	class TLSTestServer final
	{
		public:

			enum class Behavior : uint8_t
			{
				Echo,
				Mute
			};

			explicit
			TLSTestServer (const ServerCredentials & credentials, Behavior behavior, uint16_t maximumVersion = 0) noexcept
				: m_behavior(behavior)
			{
				asio::error_code error;

				if ( maximumVersion != 0 )
				{
					SSL_CTX_set_security_level(m_serverContext.native_handle(), 0);

					if ( SSL_CTX_set_max_proto_version(m_serverContext.native_handle(), maximumVersion) != 1 )
					{
						return;
					}
				}

				m_serverContext.use_certificate(asio::buffer(credentials.certificatePEM), asio::ssl::context::pem, error);

				if ( !error )
				{
					m_serverContext.use_private_key(asio::buffer(credentials.privateKeyPEM), asio::ssl::context::pem, error);
				}

				if ( error )
				{
					return;
				}

				const auto address = asio::ip::make_address("127.0.0.1", error);

				m_acceptor.open(asio::ip::tcp::v4(), error);

				if ( !error )
				{
					m_acceptor.bind(asio::ip::tcp::endpoint{address, 0}, error);
				}

				if ( !error )
				{
					m_acceptor.listen(1, error);
				}

				if ( error )
				{
					return;
				}

				m_port = m_acceptor.local_endpoint(error).port();

				m_thread = std::thread{[this] () {
					this->serve();
				}};
			}

			~TLSTestServer ()
			{
				asio::error_code error;

				m_acceptor.close(error);

				if ( m_thread.joinable() )
				{
					m_thread.join();
				}
			}

			TLSTestServer (const TLSTestServer & copy) noexcept = delete;
			TLSTestServer (TLSTestServer && move) noexcept = delete;
			TLSTestServer & operator= (const TLSTestServer & copy) noexcept = delete;
			TLSTestServer & operator= (TLSTestServer && move) noexcept = delete;

			[[nodiscard]]
			uint16_t
			port () const noexcept
			{
				return m_port;
			}

			[[nodiscard]]
			bool
			isListening () const noexcept
			{
				return m_port != 0;
			}

		private:

			void
			serve () noexcept
			{
				asio::error_code error;

				asio::ip::tcp::socket socket{m_ioContext};

				m_acceptor.accept(socket, error);

				if ( error )
				{
					return;
				}

				asio::ssl::stream< asio::ip::tcp::socket > stream{std::move(socket), m_serverContext};

				stream.handshake(asio::ssl::stream_base::server, error);

				if ( error )
				{
					/* Expected in the untrusted/mismatch scenarios: the client aborts. */
					return;
				}

				std::array< char, 1024 > buffer{};

				const auto bytesRead = stream.read_some(asio::buffer(buffer), error);

				if ( error || m_behavior == Behavior::Mute )
				{
					/* Mute: the read only returns when the client gives up and closes. */
					return;
				}

				asio::write(stream, asio::buffer(buffer.data(), bytesRead), error);

				stream.shutdown(error);
			}

			asio::io_context m_ioContext;
			asio::ssl::context m_serverContext{asio::ssl::context::tls_server};
			asio::ip::tcp::acceptor m_acceptor{m_ioContext};
			std::thread m_thread;
			uint16_t m_port{0};
			Behavior m_behavior;
	};

	/** @brief Builds a client TLS context trusting the given (PEM) certificate. */
	asio::ssl::context
	makeTrustingClientContext (const std::string & certificatePEM) noexcept
	{
		asio::ssl::context context{asio::ssl::context::tls_client};

		asio::error_code error;

		context.add_certificate_authority(asio::buffer(certificatePEM), error);

		return context;
	}
}

TEST(NetworkTLSConnection, handshakeAndEchoWithTrustedServer)
{
	const auto credentials = generateServerCredentials("DNS:localhost,IP:127.0.0.1");
	ASSERT_TRUE(credentials.valid);

	TLSTestServer server{credentials, TLSTestServer::Behavior::Echo};
	ASSERT_TRUE(server.isListening());

	auto clientContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::TLSConnection connection{clientContext};

	ASSERT_TRUE(connection.connect("localhost", server.port()));
	EXPECT_TRUE(connection.isConnected());

	const std::string message{"Ave robustus!"};

	ASSERT_TRUE(connection.write(message.data(), message.size()));

	std::array< char, 64 > buffer{};

	const auto bytesRead = connection.read(buffer.data(), buffer.size());

	ASSERT_TRUE(bytesRead.has_value());
	ASSERT_EQ(bytesRead.value(), message.size());
	EXPECT_EQ(std::memcmp(buffer.data(), message.data(), message.size()), 0);

	connection.disconnect();

	EXPECT_FALSE(connection.isConnected());
}

TEST(NetworkTLSConnection, handshakeFailsWithUntrustedServer)
{
	EmEn::Base::Testing::DiagnosticsCapture diagnosticsCapture;

	const auto credentials = generateServerCredentials("DNS:localhost,IP:127.0.0.1");
	ASSERT_TRUE(credentials.valid);

	TLSTestServer server{credentials, TLSTestServer::Behavior::Echo};
	ASSERT_TRUE(server.isListening());

	/* Empty trust store: the self-signed server certificate must be rejected. */
	asio::ssl::context clientContext{asio::ssl::context::tls_client};

	Network::TLSConnection connection{clientContext};

	EXPECT_FALSE(connection.connect("localhost", server.port()));
	EXPECT_FALSE(connection.isConnected());

	/* What the body logs (the suite's guard fails a test on any uncaptured diagnostic, 2026-11-09). */
	const auto captured = diagnosticsCapture.snapshot();
	EXPECT_EQ(captured.count(EmEn::Base::Severity::Error, "Network::TLSConnection"), 1U);
	EXPECT_EQ(captured.countWithoutDebug(), 1U);
	EXPECT_TRUE(captured.standardError.empty()) << captured.standardError;
}

TEST(NetworkTLSConnection, aServerBelowTLS12IsRefused)
{
	EmEn::Base::Testing::DiagnosticsCapture diagnosticsCapture;

	/* RFC 8996: TLS 1.0 and 1.1 are deprecated. The floor is the connection's own, so even a caller's permissive
	 * context (security level 0, which re-enables them in the TLS library) cannot reach an outdated server. */
	const auto credentials = generateServerCredentials("DNS:localhost,IP:127.0.0.1");
	ASSERT_TRUE(credentials.valid);

	/* Control: the same permissive client reaches a server capped at TLS 1.2. */
	{
		const TLSTestServer server{credentials, TLSTestServer::Behavior::Echo, TLS1_2_VERSION};
		ASSERT_TRUE(server.isListening());

		auto clientContext = makeTrustingClientContext(credentials.certificatePEM);
		SSL_CTX_set_security_level(clientContext.native_handle(), 0);

		Network::TLSConnection connection{clientContext};
		EXPECT_TRUE(connection.connect("localhost", server.port()));
	}

	{
		const TLSTestServer server{credentials, TLSTestServer::Behavior::Echo, TLS1_1_VERSION};
		ASSERT_TRUE(server.isListening());

		auto clientContext = makeTrustingClientContext(credentials.certificatePEM);
		SSL_CTX_set_security_level(clientContext.native_handle(), 0);

		Network::TLSConnection connection{clientContext};
		EXPECT_FALSE(connection.connect("localhost", server.port()));
		EXPECT_FALSE(connection.isConnected());
	}

	/* What the body logs (the suite's guard fails a test on any uncaptured diagnostic, 2026-11-09). */
	const auto captured = diagnosticsCapture.snapshot();
	EXPECT_EQ(captured.count(EmEn::Base::Severity::Error, "Network::TLSConnection"), 1U);
	EXPECT_EQ(captured.countWithoutDebug(), 1U);
	EXPECT_TRUE(captured.standardError.empty()) << captured.standardError;
}

TEST(NetworkTLSConnection, hostnameVerificationRejectsWrongHost)
{
	EmEn::Base::Testing::DiagnosticsCapture diagnosticsCapture;

	/* The certificate only names 'localhost': reaching the very same server
	 * through '127.0.0.1' must fail the hostname verification. */
	const auto credentials = generateServerCredentials("DNS:localhost");
	ASSERT_TRUE(credentials.valid);

	TLSTestServer server{credentials, TLSTestServer::Behavior::Echo};
	ASSERT_TRUE(server.isListening());

	auto clientContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::TLSConnection connection{clientContext};

	EXPECT_FALSE(connection.connect("127.0.0.1", server.port()));
	EXPECT_FALSE(connection.isConnected());

	/* What the body logs (the suite's guard fails a test on any uncaptured diagnostic, 2026-11-09). */
	const auto captured = diagnosticsCapture.snapshot();
	EXPECT_EQ(captured.count(EmEn::Base::Severity::Error, "Network::TLSConnection"), 1U);
	EXPECT_EQ(captured.countWithoutDebug(), 1U);
	EXPECT_TRUE(captured.standardError.empty()) << captured.standardError;
}

TEST(NetworkTLSConnection, connectionRefusedFails)
{
	EmEn::Base::Testing::DiagnosticsCapture diagnosticsCapture;

	asio::ssl::context clientContext{asio::ssl::context::tls_client};

	/* Reserve an ephemeral port then release it: nothing listens on it. */
	uint16_t freePort = 0;

	{
		asio::io_context probeContext;
		asio::ip::tcp::acceptor probe{probeContext};

		asio::error_code error;
		probe.open(asio::ip::tcp::v4(), error);
		probe.bind(asio::ip::tcp::endpoint{asio::ip::make_address("127.0.0.1", error), 0}, error);

		freePort = probe.local_endpoint(error).port();

		probe.close(error);
	}

	ASSERT_NE(freePort, 0);

	Network::TLSConnection connection{clientContext};

	EXPECT_FALSE(connection.connect("127.0.0.1", freePort));
	EXPECT_FALSE(connection.isConnected());

	/* What the body logs (the suite's guard fails a test on any uncaptured diagnostic, 2026-11-09). */
	const auto captured = diagnosticsCapture.snapshot();
	EXPECT_EQ(captured.count(EmEn::Base::Severity::Error, "Network::TLSConnection"), 1U);
	EXPECT_EQ(captured.countWithoutDebug(), 1U);
	EXPECT_TRUE(captured.standardError.empty()) << captured.standardError;
}

TEST(NetworkTLSConnection, readTimesOutOnMuteServer)
{
	EmEn::Base::Testing::DiagnosticsCapture diagnosticsCapture;

	const auto credentials = generateServerCredentials("DNS:localhost,IP:127.0.0.1");
	ASSERT_TRUE(credentials.valid);

	TLSTestServer server{credentials, TLSTestServer::Behavior::Mute};
	ASSERT_TRUE(server.isListening());

	auto clientContext = makeTrustingClientContext(credentials.certificatePEM);

	Network::TLSConnectionOptions options;
	options.readTimeout = std::chrono::milliseconds{250};

	Network::TLSConnection connection{clientContext, options};

	ASSERT_TRUE(connection.connect("localhost", server.port()));

	std::array< char, 16 > buffer{};

	const auto begin = std::chrono::steady_clock::now();
	const auto bytesRead = connection.read(buffer.data(), buffer.size());
	const auto elapsed = std::chrono::steady_clock::now() - begin;

	/* The mute server never answers: the read must time out, not block forever. */
	EXPECT_FALSE(bytesRead.has_value());
	EXPECT_FALSE(connection.isConnected());
	EXPECT_GE(elapsed, std::chrono::milliseconds{200});
	EXPECT_LT(elapsed, std::chrono::seconds{10});

	/* What the body logs (the suite's guard fails a test on any uncaptured diagnostic, 2026-11-09). */
	const auto captured = diagnosticsCapture.snapshot();
	EXPECT_EQ(captured.count(EmEn::Base::Severity::Error, "Network::TLSConnection"), 1U);
	EXPECT_EQ(captured.countWithoutDebug(), 1U);
	EXPECT_TRUE(captured.standardError.empty()) << captured.standardError;
}

/* 2026-11-14: a stop requested on the connection's token ends the operation in flight at once (setStopToken()). The
 * token was only looked at by the HTTP client between two reads: an operation parked on a silent peer waited its own
 * timeout. Each helper parks one operation for up to 20 s and requests the stop from another thread 200 ms later. */
namespace
{
	/** @brief What an operation parked on a silent peer did once a stop was requested. */
	struct InterruptedOperation
	{
		std::chrono::milliseconds elapsed{0};
		size_t loggedLines{0};
		/** The server, the certificate and (for a read) the connection were set up. */
		bool setUp{false};
		bool succeeded{false};
		bool stillConnected{false};

		/** @brief Tells whether the operation ended because of the stop: not before it, and long before its timeout. */
		[[nodiscard]]
		bool
		endedByTheStop () const noexcept
		{
			return elapsed >= std::chrono::milliseconds{150} && elapsed < std::chrono::seconds{2};
		}
	};

	/** @brief Runs an operation while another thread requests a stop 200 ms after its start, and times it. */
	template< typename operation_t >
	[[nodiscard]]
	std::chrono::milliseconds
	timeUntilStopped (std::stop_source & stopSource, operation_t && operation)
	{
		std::thread stopper{[&stopSource] () {
			std::this_thread::sleep_for(std::chrono::milliseconds{200});

			stopSource.request_stop();
		}};

		const auto begin = std::chrono::steady_clock::now();

		std::forward< operation_t >(operation)();

		const auto elapsed = std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::steady_clock::now() - begin);

		stopper.join();

		return elapsed;
	}

	/** @brief A listener which never accepts: the kernel completes the TCP handshake and takes what is written, nothing
	 * ever answers. */
	class SilentListener final
	{
		public:

			SilentListener () noexcept
			{
				asio::error_code error;

				m_listener.open(asio::ip::tcp::v4(), error);

				if ( !error )
				{
					m_listener.bind(asio::ip::tcp::endpoint{asio::ip::address_v4::loopback(), 0}, error);
				}

				if ( !error )
				{
					m_listener.listen(1, error);
				}

				if ( !error )
				{
					m_port = m_listener.local_endpoint(error).port();
				}

				if ( error )
				{
					m_port = 0;
				}
			}

			/** @brief Returns the port, 0 when the listener could not be set up. */
			[[nodiscard]]
			uint16_t
			port () const noexcept
			{
				return m_port;
			}

		private:

			asio::io_context m_context;
			asio::ip::tcp::acceptor m_listener{m_context};
			uint16_t m_port{0};
	};

	/** @brief Requests the stop.
	 * @note Through a reference to a non-const source: libstdc++ declares request_stop() const, the standard does not —
	 * a const source would not compile elsewhere. */
	void
	requestStop (std::stop_source & stopSource) noexcept
	{
		stopSource.request_stop();
	}

	/** @brief A read parked on a server which performed the handshake and never answers. */
	[[nodiscard]]
	InterruptedOperation
	readFromAMuteServerThenStop ()
	{
		EmEn::Base::Testing::DiagnosticsCapture diagnosticsCapture;
		InterruptedOperation outcome;

		const auto credentials = generateServerCredentials("DNS:localhost,IP:127.0.0.1");
		const TLSTestServer server{credentials, TLSTestServer::Behavior::Mute};

		if ( !credentials.valid || !server.isListening() )
		{
			return outcome;
		}

		auto clientContext = makeTrustingClientContext(credentials.certificatePEM);

		Network::TLSConnectionOptions options;
		options.readTimeout = std::chrono::seconds{20};

		Network::TLSConnection connection{clientContext, options};

		if ( !connection.connect("localhost", server.port()) )
		{
			return outcome;
		}

		outcome.setUp = true;

		std::stop_source stopSource;
		connection.setStopToken(stopSource.get_token());

		std::array< char, 16 > buffer{};

		outcome.elapsed = timeUntilStopped(stopSource, [&] () {
			outcome.succeeded = connection.read(buffer.data(), buffer.size()).has_value();
		});
		outcome.stillConnected = connection.isConnected();
		outcome.loggedLines = diagnosticsCapture.snapshot().countWithoutDebug();

		return outcome;
	}

	/** @brief A TLS handshake parked on a listener which accepts the TCP connection and never speaks. */
	[[nodiscard]]
	InterruptedOperation
	handshakeWithASilentListenerThenStop ()
	{
		EmEn::Base::Testing::DiagnosticsCapture diagnosticsCapture;
		InterruptedOperation outcome;

		/* Nothing answers the ClientHello. */
		const SilentListener listener;
		const auto port = listener.port();

		if ( port == 0 )
		{
			return outcome;
		}

		outcome.setUp = true;

		asio::ssl::context clientContext{asio::ssl::context::tls_client};

		Network::TLSConnectionOptions options;
		options.handshakeTimeout = std::chrono::seconds{20};

		Network::TLSConnection connection{clientContext, options};

		std::stop_source stopSource;
		connection.setStopToken(stopSource.get_token());

		outcome.elapsed = timeUntilStopped(stopSource, [&] () {
			outcome.succeeded = connection.connect("127.0.0.1", port);
		});
		outcome.stillConnected = connection.isConnected();
		outcome.loggedLines = diagnosticsCapture.snapshot().countWithoutDebug();

		return outcome;
	}

	/** @brief What a stop requested BEFORE two operations did to them: a write which needs no wait, then a read which
	 * would wait for ever. */
	struct StoppedBeforehand
	{
		std::chrono::milliseconds readElapsed{0};
		size_t loggedLines{0};
		bool setUp{false};
		bool writeSucceeded{false};
		bool openAfterTheWrite{false};
		bool readSucceeded{false};
		bool connectedAfterTheRead{false};
	};

	/** @brief A cleartext connection to a silent listener, its token stopped, then a small write and a read. */
	[[nodiscard]]
	StoppedBeforehand
	writeThenReadUnderAStoppedToken ()
	{
		EmEn::Base::Testing::DiagnosticsCapture diagnosticsCapture;
		StoppedBeforehand outcome;

		const SilentListener listener;
		asio::ssl::context clientContext{asio::ssl::context::tls_client};

		Network::TLSConnectionOptions options;
		options.readTimeout = std::chrono::seconds{20};

		Network::TLSConnection connection{clientContext, options};

		if ( listener.port() == 0 || !connection.connectCleartextPrivate("127.0.0.1", listener.port()) )
		{
			return outcome;
		}

		outcome.setUp = true;

		std::stop_source stopSource;
		connection.setStopToken(stopSource.get_token());
		requestStop(stopSource);

		/* Four bytes the kernel takes at once: nothing to wait for. */
		const std::array< char, 4 > request{'p', 'i', 'n', 'g'};

		outcome.writeSucceeded = connection.write(request.data(), request.size());
		outcome.openAfterTheWrite = connection.isOpenAndIdle();

		/* Nothing will ever come: this one would wait. */
		std::array< char, 16 > buffer{};
		const auto begin = std::chrono::steady_clock::now();

		outcome.readSucceeded = connection.read(buffer.data(), buffer.size()).has_value();
		outcome.readElapsed = std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::steady_clock::now() - begin);
		outcome.connectedAfterTheRead = connection.isConnected();
		outcome.loggedLines = diagnosticsCapture.snapshot().countWithoutDebug();

		return outcome;
	}
}

TEST(NetworkTLSConnection, aStopRequestedFromAnotherThreadReturnsAParkedRead)
{
	const auto read = readFromAMuteServerThenStop();

	ASSERT_TRUE(read.setUp);
	EXPECT_FALSE(read.succeeded);
	EXPECT_FALSE(read.stillConnected);
	EXPECT_TRUE(read.endedByTheStop()) << read.elapsed.count() << " ms";
	/* A stop is not a failure of the transport: nothing is logged. */
	EXPECT_EQ(read.loggedLines, 0U);
}

TEST(NetworkTLSConnection, aStopRequestedFromAnotherThreadReturnsAParkedHandshake)
{
	const auto handshake = handshakeWithASilentListenerThenStop();

	ASSERT_TRUE(handshake.setUp);
	EXPECT_FALSE(handshake.succeeded);
	EXPECT_FALSE(handshake.stillConnected);
	EXPECT_TRUE(handshake.endedByTheStop()) << handshake.elapsed.count() << " ms";
	EXPECT_EQ(handshake.loggedLines, 0U);
}

/* 2026-11-15: a stop ends a WAIT, it never spoils an operation which had none. The first version closed the socket
 * whenever the token was stopped — also after a write the kernel had already taken: the write answered true on a
 * connection closed and still said connected (and, in the HTTP client, a complete response would have put that
 * connection back in the idle pool). */
TEST(NetworkTLSConnection, aStopNeverSpoilsAnOperationWhichDoesNotWait)
{
	const auto outcome = writeThenReadUnderAStoppedToken();

	ASSERT_TRUE(outcome.setUp);
	/* The kernel took the bytes: the write holds, and the connection is what it says it is — open. */
	EXPECT_TRUE(outcome.writeSucceeded);
	EXPECT_TRUE(outcome.openAfterTheWrite);
	/* The read would wait: it is refused at once, under a 20 s timeout, and the connection is spent. */
	EXPECT_FALSE(outcome.readSucceeded);
	EXPECT_FALSE(outcome.connectedAfterTheRead);
	EXPECT_LT(outcome.readElapsed, std::chrono::seconds{2}) << outcome.readElapsed.count() << " ms";
	/* A stop is not a failure of the transport: nothing is logged. */
	EXPECT_EQ(outcome.loggedLines, 0U);
}

TEST(NetworkTLSConnection, operationsFailWhenNotConnected)
{
	EmEn::Base::Testing::DiagnosticsCapture diagnosticsCapture;

	asio::ssl::context clientContext{asio::ssl::context::tls_client};

	Network::TLSConnection connection{clientContext};

	EXPECT_FALSE(connection.isConnected());
	EXPECT_FALSE(connection.write("x", 1));

	std::array< char, 16 > buffer{};

	EXPECT_FALSE(connection.read(buffer.data(), buffer.size()).has_value());

	/* What the body logs (the suite's guard fails a test on any uncaptured diagnostic, 2026-11-09). */
	const auto captured = diagnosticsCapture.snapshot();
	EXPECT_EQ(captured.count(EmEn::Base::Severity::Error, "Network::TLSConnection"), 2U);
	EXPECT_EQ(captured.countWithoutDebug(), 2U);
	EXPECT_TRUE(captured.standardError.empty()) << captured.standardError;
}
