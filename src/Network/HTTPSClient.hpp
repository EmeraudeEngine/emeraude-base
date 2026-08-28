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
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
	 * @brief Progress hook of HTTPSClient::download(): decoded body bytes so far, and the total when known.
	 * @note Called on the thread that runs download(), after each transport read that carried body
	 * bytes of the final 2xx hop (redirect hops report nothing). The total is the Content-Length of
	 * that hop; it is std::nullopt for a chunked or read-until-close body. The last call carries the
	 * final byte count. Must be cheap and must not block: it sits in the read loop.
	 */
	using DownloadProgress = std::function< void (uint64_t bytesReceived, std::optional< uint64_t > bytesTotal) >;

	/**
	 * @brief Why a download ended the way it did.
	 * @note A bool cannot tell a 404 from an expired certificate, and the consumer has to show
	 * the user something actionable. Coarse on purpose: one value per thing a caller would do
	 * differently.
	 */
	enum class DownloadOutcome : uint8_t
	{
		Success,        /* 2xx, body complete, file written. */
		BadScheme,      /* Not https, or a port that was declared and invalid. */
		Unreachable,    /* DNS, connect, proxy tunnel: nothing was ever spoken to. */
		TLSFailure,     /* Handshake or certificate verification refused the peer. */
		Timeout,        /* A per-operation or total deadline expired. */
		Protocol,       /* Malformed, truncated, too large, or a redirect that could not be followed. */
		HTTPStatus,     /* The exchange completed, the status was not 2xx (see downloadStatusCode). */
		LocalIO,        /* The destination file could not be opened, written or flushed. */
		BadRequest      /* The CALLER's request was refused before a byte was sent (bad header, bad body). */
	};

	/**
	 * @brief Returns the textual name of a download outcome.
	 * @param outcome The outcome.
	 * @return const char *
	 */
	[[nodiscard]]
	constexpr
	const char *
	to_cstring (DownloadOutcome outcome) noexcept
	{
		switch ( outcome )
		{
			case DownloadOutcome::Success : return "Success";
			case DownloadOutcome::BadScheme : return "BadScheme";
			case DownloadOutcome::Unreachable : return "Unreachable";
			case DownloadOutcome::TLSFailure : return "TLSFailure";
			case DownloadOutcome::Timeout : return "Timeout";
			case DownloadOutcome::Protocol : return "Protocol";
			case DownloadOutcome::HTTPStatus : return "HTTPStatus";
			case DownloadOutcome::LocalIO : return "LocalIO";
			case DownloadOutcome::BadRequest : return "BadRequest";
		}

		return "Unknown";
	}

	/**
	 * @brief What a download reports back beyond success: why it failed, the HTTP status it got,
	 * and the media type the server declared (a cache keyed by URL has no filename to trust).
	 */
	struct DownloadReport final
	{
		DownloadOutcome outcome{DownloadOutcome::Success};
		std::string contentType;
		uint16_t statusCode{0};
	};

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

		/**
		 * @brief Parser hardening limits (header/chunk caps, body cap).
		 * @note The body cap defaults to maxInMemoryBodySize below: an in-memory body is held
		 * whole, so it must never be unbounded.
		 */
		HTTPResponseParserLimits parserLimits{};

		/**
		 * @brief Ceiling for a body held in memory — get(), and any redirect or error body.
		 * @note Applied to parserLimits.maxBodySize when it was left at its (unbounded) default,
		 * so a hostile or misbehaving server cannot make the process grow without limit.
		 */
		uint64_t maxInMemoryBodySize{64ULL * 1024 * 1024};

		/**
		 * @brief Ceiling for a body streamed to a file by download().
		 * @note Much larger than the in-memory one: the body never sits in RAM. Still bounded, so
		 * an endless response cannot fill the disk silently.
		 */
		uint64_t maxDownloadSize{4ULL * 1024 * 1024 * 1024};

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
	 * @brief What a caller adds to a request beyond its method and URI: headers, and a body.
	 * @note Exists for API traffic; get()/head()/download() leave it empty. The client owns the
	 * framing headers (Host, Content-Length, Connection, Transfer-Encoding, Accept-Encoding) and
	 * REFUSES a caller that supplies one rather than silently ignoring it: a duplicate framing
	 * header is a request-smuggling primitive. User-Agent is the exception — it overrides the one
	 * from HTTPSClientOptions, which is what an API expecting a named client needs.
	 */
	struct HTTPRequestOptions final
	{
		/**
		 * @brief Extra request headers, sent in this order.
		 * @note A vector, not a map: a field may legitimately repeat (Accept, Cookie), and a
		 * request-signing scheme can be order-sensitive. Every name and value is validated
		 * before anything reaches the socket — see HTTPSClient::isRequestHeaderAcceptable().
		 */
		std::vector< std::pair< std::string, std::string > > headers;

		/** @brief The request body, sent verbatim. Empty for a body-less method. */
		std::string body;

		/**
		 * @brief Media type of the body, sent as Content-Type when 'headers' carries none.
		 * @note Ignored when the body is empty.
		 */
		std::string contentType;
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
				DownloadOutcome outcome{DownloadOutcome::Success};

				return this->run(HTTPRequest::Method::GET, uri, BodySink::Memory, {}, {}, outcome);
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
				DownloadOutcome outcome{DownloadOutcome::Success};

				return this->run(HTTPRequest::Method::HEAD, uri, BodySink::Discard, {}, {}, outcome);
			}

			/**
			 * @brief Performs an arbitrary request — method, extra headers, body — and returns the response + body.
			 * @note The API-traffic entry point; get() and head() are façades over it. The response
			 * body is held in memory and capped by HTTPSClientOptions::maxInMemoryBodySize.
			 * @warning Redirect handling differs from a plain GET in two ways, both of which exist
			 * to avoid leaking a credential or replaying a write:
			 *  - a 301/302/303 that rewrites the method to GET also DROPS the body;
			 *  - a redirect that changes scheme, host or port DROPS every caller header, because an
			 *    Authorization forwarded to the redirect target is a credential leak.
			 * @param method The HTTP method.
			 * @param uri The target URI (https scheme).
			 * @param options The extra headers and body [std::move]. Default none.
			 * @param report Where to write the outcome, HTTP status and media type. Default none.
			 * @return std::optional< HTTPResult > std::nullopt on a refused request, or on a transport/parse/redirect error.
			 */
			[[nodiscard]]
			std::optional< HTTPResult > request (HTTPRequest::Method method, const URI & uri, HTTPRequestOptions options = {}, DownloadReport * report = nullptr) const noexcept;

			/**
			 * @brief Returns whether a caller-supplied request header may be sent.
			 * @note Refuses an empty or non-token name (RFC 9110 §5.1 tchar), a value carrying CR,
			 * LF or NUL — the request is built by concatenation, so a CRLF inside a value injects
			 * arbitrary headers, which is the request-splitting primitive — a value carrying any
			 * other C0 control but HTAB, and every framing header the client owns (Host,
			 * Content-Length, Connection, Transfer-Encoding, Accept-Encoding).
			 * @param name The header field name.
			 * @param value The header field value.
			 * @return bool
			 */
			[[nodiscard]]
			static bool isRequestHeaderAcceptable (const std::string & name, const std::string & value) noexcept;

			/**
			 * @brief Downloads a resource to a file, streaming the body (never held whole in memory).
			 * @param uri The target URI (https scheme).
			 * @param filepath The destination file path.
			 * @param progress An optional progress hook, see DownloadProgress. Default none.
			 * @return bool True on a 2xx response fully written to the file.
			 */
			[[nodiscard]]
			bool download (const URI & uri, const std::filesystem::path & filepath, const DownloadProgress & progress = {}, DownloadReport * report = nullptr) const noexcept;

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
			 * @param options The caller headers and body [std::move]. Mutated across hops: a method
			 * rewrite drops the body, a cross-origin redirect drops the headers.
			 * @param outcome Where the coarse reason is written [out].
			 * @param progress The progress hook of a File sink, nullptr otherwise.
			 * @return std::optional< HTTPResult > The final response (body empty when streamed to file).
			 */
			[[nodiscard]]
			std::optional< HTTPResult > run (HTTPRequest::Method method, const URI & uri, BodySink sink, const std::filesystem::path & filepath, HTTPRequestOptions options, DownloadOutcome & outcome, const DownloadProgress * progress = nullptr) const noexcept;

			/**
			 * @brief Performs a single request/response exchange (one connection, no redirect).
			 * @param method The HTTP method.
			 * @param uri The target URI for this hop (https).
			 * @param sink Where the body goes.
			 * @param filepath The destination file when sink is File.
			 * @param options The caller headers and body for this hop.
			 * @param deadline The absolute total-budget deadline.
			 * @param outcome Where the coarse reason is written [out].
			 * @param progress The progress hook of a File sink, nullptr otherwise. Reported only on a 2xx.
			 * @return std::optional< HTTPResult >
			 */
			[[nodiscard]]
			std::optional< HTTPResult > performHop (HTTPRequest::Method method, const URI & uri, BodySink sink, const std::filesystem::path & filepath, const HTTPRequestOptions & options, std::chrono::steady_clock::time_point deadline, DownloadOutcome & outcome, const DownloadProgress * progress = nullptr) const noexcept;

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

			/* ⚠️ The coarse reason used to be a `mutable` member written by these const methods.
			 * Net::Manager runs several download() calls CONCURRENTLY on one shared client, so that
			 * member was a genuine data race, and a failing transfer could report the reason of
			 * another one. It is now threaded through run()/performHop() as an out-parameter, which
			 * makes every call self-contained. Never put it back on the object. */
			asio::ssl::context & m_tlsContext;
			HTTPSClientOptions m_options;
	};
}
