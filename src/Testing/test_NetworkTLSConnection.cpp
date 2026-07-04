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
#include <cstring>
#include <string>
#include <thread>

/* Local inclusions. */
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
			TLSTestServer (const ServerCredentials & credentials, Behavior behavior) noexcept
				: m_behavior(behavior)
			{
				asio::error_code error;

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
	const auto credentials = generateServerCredentials("DNS:localhost,IP:127.0.0.1");
	ASSERT_TRUE(credentials.valid);

	TLSTestServer server{credentials, TLSTestServer::Behavior::Echo};
	ASSERT_TRUE(server.isListening());

	/* Empty trust store: the self-signed server certificate must be rejected. */
	asio::ssl::context clientContext{asio::ssl::context::tls_client};

	Network::TLSConnection connection{clientContext};

	EXPECT_FALSE(connection.connect("localhost", server.port()));
	EXPECT_FALSE(connection.isConnected());
}

TEST(NetworkTLSConnection, hostnameVerificationRejectsWrongHost)
{
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
}

TEST(NetworkTLSConnection, connectionRefusedFails)
{
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
}

TEST(NetworkTLSConnection, readTimesOutOnMuteServer)
{
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
}

TEST(NetworkTLSConnection, operationsFailWhenNotConnected)
{
	asio::ssl::context clientContext{asio::ssl::context::tls_client};

	Network::TLSConnection connection{clientContext};

	EXPECT_FALSE(connection.isConnected());
	EXPECT_FALSE(connection.write("x", 1));

	std::array< char, 16 > buffer{};

	EXPECT_FALSE(connection.read(buffer.data(), buffer.size()).has_value());
}