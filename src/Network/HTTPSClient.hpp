/*
 * src/Network/HTTPSClient.hpp
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
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

/* Third-party inclusions.
 * NOTE: the no-exceptions hook MUST be included before any asio header. */
#include "Network/asio_throw_exception.hpp"
#include "asio/ssl.hpp"

/* Local inclusions. */
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "HTTPResponseParser.hpp"
#include "TLSConnection.hpp"
#include "URI.hpp"

namespace EmEn::Base::Network
{
	/**
	 * @brief The result of a completed HTTP exchange: parsed response + decoded body.
	 * @note HTTPResponse itself is header-only data; the body is carried alongside.
	 */
	struct HTTPResult final
	{
		HTTPResponse response;
		std::string body;
	};

	/**
	 * @brief Behavior options for the HTTPS client (sync facade, owner-ruled 2026-07-04).
	 */
	struct HTTPSClientOptions final
	{
		/** @brief Per-operation transport timeouts (connect / handshake / read / write). */
		TLSConnectionOptions transportTimeouts{};

		/** @brief Total budget for a whole request including all redirect hops. */
		std::chrono::milliseconds totalTimeout{120000};

		/** @brief Parser hardening limits (header/chunk caps, body cap). */
		HTTPResponseParserLimits parserLimits{};

		/** @brief Value sent as the User-Agent header. */
		std::string userAgent{"EmeraudeBase/1.0"};

		/**
		 * @brief Explicit proxy authority ("host:port" or "http://host:port"); empty = none.
		 * @note Takes precedence over the environment. The tunnel to a https target
		 * is always a plaintext HTTP CONNECT to this proxy (see TLSConnection).
		 */
		std::string proxy;

		/** @brief Maximum number of redirects followed before giving up. */
		uint8_t maxRedirects{5};

		/**
		 * @brief When 'proxy' is empty, consult the https_proxy / no_proxy environment.
		 * @note Honors https_proxy/HTTPS_PROXY and the no_proxy/NO_PROXY bypass list
		 * (comma-separated host or domain suffixes, '*' meaning bypass everything).
		 */
		bool useEnvironmentProxy{true};
	};

	/**
	 * @brief A blocking, redirect-following HTTPS/1.1 client over LibreSSL.
	 * @note Assembles the trust store (via the shared TLS context), the TLS transport
	 * (TLSConnection) and the response codec (HTTPResponseParser) into the synchronous
	 * facade decided 2026-07-04 (see docs/plans/network-tls/README.md). The public API is
	 * protocol-agnostic (h2-ready): HTTP/1.1, chunked and keep-alive are internal.
	 * @note HTTPS only: a http:// target is refused (plaintext HTTP is a separate concern; the
	 * legacy Network::download() that once covered it was removed 2026-08-27). Redirects: https→https always,
	 * http→https upgrade honored on a Location, https→http downgrade refused. Proxy
	 * support is the next increment (needs a two-phase TLSConnection connect).
	 * @note One connection per hop (no keep-alive reuse yet — a later optimization).
	 */
	class HTTPSClient final
	{
		public:

			/**
			 * @brief Constructs an HTTPS client bound to a shared, trust-configured TLS context.
			 * @note The caller configures the context trust store once (see TrustStore) and
			 * reuses it across clients; the context outlives the client.
			 * @param tlsContext A reference to the TLS context.
			 * @param options The client options. Default: 2-minute total budget, 5 redirects.
			 */
			explicit HTTPSClient (asio::ssl::context & tlsContext, HTTPSClientOptions options = {}) noexcept;

			/**
			 * @brief Performs a GET request, following redirects, and returns the response + body.
			 * @param uri The target URI (https scheme).
			 * @return std::optional< HTTPResult > std::nullopt on transport/parse/redirect error.
			 */
			[[nodiscard]]
			std::optional< HTTPResult >
			get (const URI & uri) const noexcept
			{
				return this->run(HTTPRequest::Method::GET, uri, BodySink::Memory, {});
			}

			/**
			 * @brief Performs a HEAD request, following redirects, and returns the response (without body).
			 * @param uri The target URI (https scheme).
			 * @return std::optional< HTTPResult >
			 */
			[[nodiscard]]
			std::optional< HTTPResult >
			head (const URI & uri) const noexcept
			{
				return this->run(HTTPRequest::Method::HEAD, uri, BodySink::Discard, {});
			}

			/**
			 * @brief Downloads a resource to a file, streaming the body (never held whole in memory).
			 * @param uri The target URI (https scheme).
			 * @param filepath The destination file path.
			 * @return bool True on a 2xx response fully written to the file.
			 */
			[[nodiscard]]
			bool download (const URI & uri, const std::filesystem::path & filepath) const noexcept;

		private:

			/** @brief Where a single hop writes the decoded body. */
			enum class BodySink : uint8_t
			{
				Memory,
				File,
				Discard
			};

			/**
			 * @brief Runs the redirect loop for a given method.
			 * @param method The HTTP method.
			 * @param uri The initial target URI.
			 * @param sink Where the final response body goes.
			 * @param filepath The destination file when sink is File.
			 * @return std::optional< HTTPResult > The final response (body empty when streamed to file).
			 */
			[[nodiscard]]
			std::optional< HTTPResult > run (HTTPRequest::Method method, const URI & uri, BodySink sink, const std::filesystem::path & filepath) const noexcept;

			/**
			 * @brief Performs a single request/response exchange (one connection, no redirect).
			 * @param method The HTTP method.
			 * @param uri The target URI for this hop (https).
			 * @param sink Where the body goes.
			 * @param filepath The destination file when sink is File.
			 * @param deadline The absolute total-budget deadline.
			 * @return std::optional< HTTPResult >
			 */
			[[nodiscard]]
			std::optional< HTTPResult > performHop (HTTPRequest::Method method, const URI & uri, BodySink sink, const std::filesystem::path & filepath, std::chrono::steady_clock::time_point deadline) const noexcept;

			/**
			 * @brief Resolves the proxy to use for a target host (explicit option or environment).
			 * @param targetHost The target hostname (checked against the no_proxy bypass list).
			 * @param proxyHost The resolved proxy host [out].
			 * @param proxyPort The resolved proxy port [out].
			 * @return bool True when a proxy applies (out params valid); false = connect directly.
			 */
			[[nodiscard]]
			bool resolveProxy (const std::string & targetHost, std::string & proxyHost, uint16_t & proxyPort) const noexcept;

			/**
			 * @brief Resolves a redirect target against the current URI (absolute or absolute-path).
			 * @note Full RFC 3986 §5 relative resolution, delegated to URI::resolve(): absolute
			 * URLs, absolute-path and relative-path references, dot-segment removal included.
			 * @param current The URI that produced the redirect.
			 * @param location The raw Location header value.
			 * @param resolved The resolved target [out].
			 * @return bool False when the Location is unusable or a downgrade.
			 */
			[[nodiscard]]
			static bool resolveRedirect (const URI & current, const std::string & location, URI & resolved) noexcept;

			asio::ssl::context & m_tlsContext;
			HTTPSClientOptions m_options;
	};
}
