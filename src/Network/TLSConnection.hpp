/*
 * src/Network/TLSConnection.hpp
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

#pragma once

/* STL inclusions. */
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

/* Third-party inclusions.
 * NOTE: the no-exceptions hook MUST be included before any asio header. */
#include "Network/asio_throw_exception.hpp"
#include "asio.hpp"
#include "asio/ssl.hpp"

namespace EmEn::Base::Network
{
	/**
	 * @brief Timeout configuration for a TLS connection.
	 * @note Transport-level timeouts only (connect / TLS handshake / single read or
	 * write). The HTTP client adds its own response/total budgets on top.
	 */
	struct TLSConnectionOptions final
	{
		std::chrono::milliseconds connectTimeout{30000};
		std::chrono::milliseconds handshakeTimeout{30000};
		std::chrono::milliseconds readTimeout{30000};
		std::chrono::milliseconds writeTimeout{30000};
	};

	/**
	 * @brief A blocking, single-use TLS client connection over LibreSSL (asio::ssl).
	 * @note This is the transport layer of the HTTPS client (sync facade decided
	 * 2026-07-04, see docs/plans/network-tls/README.md). Every operation blocks the
	 * calling thread but is implemented with asio async operations driven by a private
	 * io_context (run_for) — which is what makes the timeouts possible under the
	 * `-fno-exceptions` / ASIO_NO_EXCEPTIONS regime.
	 * @note Certificate-chain verification (against the trust store configured on the
	 * shared TLS context, see TrustStore) and hostname verification (X509_check_host)
	 * are always enforced — there is deliberately no "insecure" switch.
	 * @note Single-use: one connect() per instance; create a new instance to reconnect.
	 */
	class TLSConnection final
	{
		public:

			/**
			 * @brief Constructs a TLS connection bound to a shared TLS context.
			 * @param tlsContext A reference to the TLS context (trust store, see TrustStore).
			 * @param options The transport timeouts. Default: 30 seconds each.
			 */
			explicit TLSConnection (asio::ssl::context & tlsContext, const TLSConnectionOptions & options = {}) noexcept;

			/**
			 * @brief Destructs the connection, closing the socket if still open.
			 */
			~TLSConnection ();

			/* Single-use object owning OS resources: not copyable, not movable
			 * (the internal asio stream holds a reference to the io_context member). */
			TLSConnection (const TLSConnection & copy) noexcept = delete;
			TLSConnection (TLSConnection && move) noexcept = delete;
			TLSConnection & operator= (const TLSConnection & copy) noexcept = delete;
			TLSConnection & operator= (TLSConnection && move) noexcept = delete;

			/**
			 * @brief Resolves, connects, then performs the TLS handshake (SNI + chain + hostname verification).
			 * @param hostname The server hostname. Used for resolution, SNI and certificate hostname verification.
			 * @param port The TCP port.
			 * @return bool True when the connection is established and the peer is verified.
			 */
			[[nodiscard]]
			bool connect (const std::string & hostname, uint16_t port) noexcept;

			/**
			 * @brief Connects to the target through an HTTP proxy via a CONNECT tunnel, then handshakes.
			 * @note The CONNECT request and its response travel in PLAINTEXT to the proxy;
			 * the TLS handshake then runs end-to-end with the target (SNI + chain + hostname
			 * verification against the target, exactly as a direct connect), so the proxy
			 * never sees the decrypted stream.
			 * @param proxyHost The proxy hostname.
			 * @param proxyPort The proxy TCP port.
			 * @param targetHost The final target hostname (SNI + certificate verification).
			 * @param targetPort The final target TCP port.
			 * @return bool True when the tunnel is established and the target peer is verified.
			 */
			[[nodiscard]]
			bool connectViaProxy (const std::string & proxyHost, uint16_t proxyPort, const std::string & targetHost, uint16_t targetPort) noexcept;

			/**
			 * @brief Writes a whole buffer to the peer.
			 * @param data A pointer to the bytes to send.
			 * @param size The number of bytes to send.
			 * @return bool True when every byte was written.
			 */
			[[nodiscard]]
			bool write (const char * data, size_t size) noexcept;

			/**
			 * @brief Reads at most 'capacity' bytes from the peer (blocks until some bytes arrive).
			 * @param buffer A pointer to the destination buffer.
			 * @param capacity The destination buffer capacity.
			 * @return std::optional< size_t > The number of bytes read; 0 on clean peer
			 * close (TLS close_notify or EOF); std::nullopt on error or timeout.
			 */
			[[nodiscard]]
			std::optional< size_t > read (char * buffer, size_t capacity) noexcept;

			/**
			 * @brief Shuts the TLS session down (close_notify) and closes the socket.
			 * @note Best-effort: the socket is closed even when the TLS shutdown fails.
			 * @return void
			 */
			void disconnect () noexcept;

			/**
			 * @brief Returns whether the connection is established and usable.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isConnected () const noexcept
			{
				return m_connected;
			}

			/**
			 * @brief Returns whether a failed connect() got as far as the TLS handshake.
			 * @note Only meaningful after connect() or connectViaProxy() returned false: it tells
			 * apart "nothing was ever spoken to" (DNS, TCP, proxy tunnel) from "the peer was
			 * reached and the handshake or the certificate verification refused it". A caller
			 * retries the former and must never retry the latter, so collapsing the two into one
			 * bool loses the only distinction that changes what the caller does.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			handshakeRefused () const noexcept
			{
				return m_handshakeRefused;
			}

		private:

			/**
			 * @brief Runs the private io_context until the pending operation completes or a timeout expires.
			 * @note On timeout the socket is closed, which completes the pending
			 * operation with asio::error::operation_aborted.
			 * @param timeout The time budget for the pending operation.
			 * @return void
			 */
			void runWithTimeout (std::chrono::milliseconds timeout) noexcept;

			/**
			 * @brief Resolves and establishes the raw TCP connection (async, under the connect timeout).
			 * @param host The host to reach (target for a direct connect, proxy for a tunneled one).
			 * @param port The TCP port.
			 * @return bool
			 */
			[[nodiscard]]
			bool establishTcp (const std::string & host, uint16_t port) noexcept;

			/**
			 * @brief Performs the plaintext HTTP CONNECT exchange with the proxy over the raw socket.
			 * @param targetHost The tunnel target hostname.
			 * @param targetPort The tunnel target port.
			 * @return bool True on a 2xx CONNECT response.
			 */
			[[nodiscard]]
			bool tunnelThroughProxy (const std::string & targetHost, uint16_t targetPort) noexcept;

			/**
			 * @brief Configures SNI + peer/hostname verification and runs the TLS handshake.
			 * @param targetHost The hostname for SNI and certificate verification.
			 * @return bool
			 */
			[[nodiscard]]
			bool performHandshake (const std::string & targetHost) noexcept;

			asio::io_context m_ioContext;
			asio::ssl::stream< asio::ip::tcp::socket > m_stream;
			TLSConnectionOptions m_options;
			bool m_connected{false};
			bool m_handshakeRefused{false};
	};
}