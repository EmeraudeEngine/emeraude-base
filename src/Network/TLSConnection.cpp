/*
 * src/Network/TLSConnection.cpp
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

#include "TLSConnection.hpp"

/* Project configuration. */
#include "emeraude_base_config.hpp"

/* STL inclusions. */
#include <array>
#include <cstddef>
#include <string>

/* Third-party inclusions. LibreSSL, through its OpenSSL-compatible API. */
#include <openssl/ssl.h>

/* Local inclusions. */
#include "Logging/Logging.hpp"

namespace EmEn::Base::Network
{
	namespace
	{
		constexpr auto Tag{"Network::TLSConnection"};
		constexpr size_t MaxProxyResponseSize{8192};
	}

	TLSConnection::TLSConnection (asio::ssl::context & tlsContext, const TLSConnectionOptions & options) noexcept
		: m_stream(m_ioContext, tlsContext),
		m_options(options)
	{

	}

	TLSConnection::~TLSConnection ()
	{
		this->disconnect();
	}

	void
	TLSConnection::runWithTimeout (std::chrono::milliseconds timeout) noexcept
	{
		m_ioContext.restart();
		m_ioContext.run_for(timeout);

		if ( !m_ioContext.stopped() )
		{
			/* Timeout: closing the socket completes the pending operation
			 * with asio::error::operation_aborted, then the context drains. */
			asio::error_code closeError;

			m_stream.lowest_layer().close(closeError);

			m_ioContext.run();
		}
	}

	bool
	TLSConnection::establishTcp (const std::string & host, uint16_t port) noexcept
	{
		/* Name resolution (async, under the connect timeout). */
		asio::ip::tcp::resolver resolver{m_ioContext};

		asio::error_code resolveError{asio::error::would_block};
		asio::ip::tcp::resolver::results_type endpoints;

		resolver.async_resolve(host, std::to_string(port), [&resolveError, &endpoints] (const asio::error_code & error, asio::ip::tcp::resolver::results_type results) {
			resolveError = error;
			endpoints = std::move(results);
		});

		this->runWithTimeout(m_options.connectTimeout);

		if ( resolveError || endpoints.empty() )
		{
			Logging::error(Tag, "establishTcp(), unable to resolve '" + host + "' : " + resolveError.message());

			return false;
		}

		/* TCP connection (async, under the connect timeout). */
		asio::error_code connectError{asio::error::would_block};

		asio::async_connect(m_stream.lowest_layer(), endpoints, [&connectError] (const asio::error_code & error, const asio::ip::tcp::endpoint & /*endpoint*/) {
			connectError = error;
		});

		this->runWithTimeout(m_options.connectTimeout);

		if ( connectError )
		{
			Logging::error(Tag, "establishTcp(), unable to reach '" + host + ':' + std::to_string(port) + "' : " + connectError.message());

			return false;
		}

		return true;
	}

	bool
	TLSConnection::tunnelThroughProxy (const std::string & targetHost, uint16_t targetPort) noexcept
	{
		/* Plaintext CONNECT over the raw socket (before any TLS). */
		const auto authority = targetHost + ':' + std::to_string(targetPort);
		const auto request = "CONNECT " + authority + " HTTP/1.1\r\nHost: " + authority + "\r\n\r\n";

		asio::error_code writeError{asio::error::would_block};

		asio::async_write(m_stream.next_layer(), asio::buffer(request), [&writeError] (const asio::error_code & error, size_t /*transferred*/) {
			writeError = error;
		});

		this->runWithTimeout(m_options.writeTimeout);

		if ( writeError )
		{
			Logging::error(Tag, "tunnelThroughProxy(), unable to send the CONNECT request : " + writeError.message());

			return false;
		}

		/* Read the proxy response headers up to the blank line (bounded). */
		std::string response;

		while ( response.find("\r\n\r\n") == std::string::npos )
		{
			if ( response.size() >= MaxProxyResponseSize )
			{
				Logging::error(Tag, "tunnelThroughProxy(), the proxy response exceeds the size limit.");

				return false;
			}

			std::array< char, 512 > buffer{};

			asio::error_code readError{asio::error::would_block};
			size_t bytesRead = 0;

			m_stream.next_layer().async_read_some(asio::buffer(buffer), [&readError, &bytesRead] (const asio::error_code & error, size_t transferred) {
				readError = error;
				bytesRead = transferred;
			});

			this->runWithTimeout(m_options.readTimeout);

			if ( readError )
			{
				Logging::error(Tag, "tunnelThroughProxy(), unable to read the proxy response : " + readError.message());

				return false;
			}

			response.append(buffer.data(), bytesRead);
		}

		/* Status line: "HTTP/1.x NNN ...". A 2xx means the tunnel is open. */
		const auto firstSpace = response.find(' ');

		if ( firstSpace == std::string::npos || firstSpace + 4 > response.size() )
		{
			Logging::error(Tag, "tunnelThroughProxy(), malformed proxy response status line.");

			return false;
		}

		const auto statusText = response.substr(firstSpace + 1, 3);

		if ( statusText.empty() || statusText.front() != '2' )
		{
			Logging::error(Tag, "tunnelThroughProxy(), the proxy refused the tunnel (status " + statusText + ").");

			return false;
		}

		return true;
	}

	bool
	TLSConnection::performHandshake (const std::string & targetHost) noexcept
	{
		/* TLS setup: SNI, chain verification against the context trust store,
		 * hostname verification (X509_check_host). Always enforced. */
		if ( SSL_set_tlsext_host_name(m_stream.native_handle(), targetHost.c_str()) != 1 )
		{
			Logging::error(Tag, "performHandshake(), unable to set the SNI hostname '" + targetHost + "' !");

			return false;
		}

		asio::error_code setupError;

		m_stream.set_verify_mode(asio::ssl::verify_peer, setupError);

		if ( !setupError )
		{
			m_stream.set_verify_callback(asio::ssl::host_name_verification{targetHost}, setupError);
		}

		if ( setupError )
		{
			Logging::error(Tag, "performHandshake(), unable to configure the peer verification : " + setupError.message());

			return false;
		}

		/* TLS handshake (async, under the handshake timeout). */
		asio::error_code handshakeError{asio::error::would_block};

		m_stream.async_handshake(asio::ssl::stream_base::client, [&handshakeError] (const asio::error_code & error) {
			handshakeError = error;
		});

		this->runWithTimeout(m_options.handshakeTimeout);

		if ( handshakeError )
		{
			Logging::error(Tag, "performHandshake(), the TLS handshake with '" + targetHost + "' failed : " + handshakeError.message());

			return false;
		}

		return true;
	}

	bool
	TLSConnection::connect (const std::string & hostname, uint16_t port) noexcept
	{
		if ( m_connected )
		{
			Logging::error(Tag, "connect(), the connection is already established (single-use object) !");

			return false;
		}

		if ( !this->establishTcp(hostname, port) || !this->performHandshake(hostname) )
		{
			return false;
		}

		m_connected = true;

		return true;
	}

	bool
	TLSConnection::connectViaProxy (const std::string & proxyHost, uint16_t proxyPort, const std::string & targetHost, uint16_t targetPort) noexcept
	{
		if ( m_connected )
		{
			Logging::error(Tag, "connectViaProxy(), the connection is already established (single-use object) !");

			return false;
		}

		/* Reach the proxy, open the plaintext CONNECT tunnel to the target, then
		 * run the end-to-end TLS handshake with the target through the tunnel. */
		if ( !this->establishTcp(proxyHost, proxyPort) || !this->tunnelThroughProxy(targetHost, targetPort) || !this->performHandshake(targetHost) )
		{
			return false;
		}

		m_connected = true;

		return true;
	}

	bool
	TLSConnection::write (const char * data, size_t size) noexcept
	{
		if ( !m_connected )
		{
			Logging::error(Tag, "write(), the connection is not established !");

			return false;
		}

		asio::error_code writeError{asio::error::would_block};
		size_t bytesWritten = 0;

		asio::async_write(m_stream, asio::buffer(data, size), [&writeError, &bytesWritten] (const asio::error_code & error, size_t transferred) {
			writeError = error;
			bytesWritten = transferred;
		});

		this->runWithTimeout(m_options.writeTimeout);

		if ( writeError || bytesWritten != size )
		{
			Logging::error(Tag, "write(), unable to send " + std::to_string(size) + " bytes : " + writeError.message());

			m_connected = false;

			return false;
		}

		return true;
	}

	std::optional< size_t >
	TLSConnection::read (char * buffer, size_t capacity) noexcept
	{
		if ( !m_connected )
		{
			Logging::error(Tag, "read(), the connection is not established !");

			return std::nullopt;
		}

		asio::error_code readError{asio::error::would_block};
		size_t bytesRead = 0;

		m_stream.async_read_some(asio::buffer(buffer, capacity), [&readError, &bytesRead] (const asio::error_code & error, size_t transferred) {
			readError = error;
			bytesRead = transferred;
		});

		this->runWithTimeout(m_options.readTimeout);

		if ( !readError )
		{
			return bytesRead;
		}

		/* Clean end of stream: TLS close_notify or TCP EOF. */
		if ( readError == asio::ssl::error::stream_truncated || readError == asio::error::eof )
		{
			m_connected = false;

			return 0;
		}

		Logging::error(Tag, "read(), unable to read from the peer : " + readError.message());

		m_connected = false;

		return std::nullopt;
	}

	void
	TLSConnection::disconnect () noexcept
	{
		if ( m_connected )
		{
			/* Best-effort TLS close_notify, bounded by the write timeout. */
			asio::error_code shutdownError{asio::error::would_block};

			m_stream.async_shutdown([&shutdownError] (const asio::error_code & error) {
				shutdownError = error;
			});

			this->runWithTimeout(m_options.writeTimeout);

			m_connected = false;
		}

		if ( m_stream.lowest_layer().is_open() )
		{
			asio::error_code closeError;

			m_stream.lowest_layer().close(closeError);
		}
	}
}