/*
 * src/Network/HTTPSClient.cpp
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

#include "HTTPSClient.hpp"

/* STL inclusions. */
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <string_view>
#include <utility>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "String.hpp"
#include "URI.hpp"

namespace EmEn::Base::Network
{
	namespace
	{
		constexpr auto Tag{"Network::HTTPSClient"};
		constexpr uint16_t HTTPSDefaultPort{443};
		constexpr uint16_t HTTPDefaultPort{80};
		constexpr size_t TransportReadBufferSize{16384};

		/* Fits the fixed header skeleton (~90 B) plus a typical host, target
		 * and user-agent in a single allocation. A long query string may still
		 * grow it once, which is acceptable. */
		constexpr size_t RequestReserveBytes{256};

		/**
		 * @brief Extracts the host and port from a URI, validating the scheme.
		 * @param uri The URI.
		 * @param allowCleartext Whether an http:// URI is accepted (HTTPSClientOptions::allowPrivateCleartext).
		 * @param host The host [out].
		 * @param port The port [out].
		 * @param cleartext Set to true for an http:// URI [out].
		 * @return bool False when the scheme is not accepted or the host is empty.
		 */
		bool
		extractTarget (const URI & uri, bool allowCleartext, std::string & host, uint16_t & port, bool & cleartext) noexcept
		{
			const auto scheme = String::toLower(uri.scheme());

			if ( scheme == "https" )
			{
				cleartext = false;
			}
			else if ( scheme == "http" && allowCleartext )
			{
				cleartext = true;
			}
			else
			{
				return false;
			}

			host = uri.uriDomain().hostname().name();

			if ( host.empty() )
			{
				return false;
			}

			/* A port that was written but rejected by the parser (out of range, non-numeric) must
			 * NOT fall back to 443: "https://host:99999/" would silently connect elsewhere. */
			if ( uri.uriDomain().hasInvalidPort() )
			{
				return false;
			}

			const auto declaredPort = uri.uriDomain().port();

			if ( declaredPort == 0 )
			{
				port = cleartext ? HTTPDefaultPort : HTTPSDefaultPort;
			}
			else if ( declaredPort > 65535 )
			{
				return false;
			}
			else
			{
				port = static_cast< uint16_t >(declaredPort);
			}

			return true;
		}

		/**
		 * @brief Builds the request-target (origin form) from a URI.
		 * @param uri The URI.
		 * @return std::string Always begins with '/'.
		 */
		std::string
		buildRequestTarget (const URI & uri) noexcept
		{
			auto target = uri.resource();

			if ( target.empty() || target.front() != '/' )
			{
				target.insert(target.begin(), '/');
			}

			return target;
		}

		/**
		 * @brief Reads an environment variable, trying the lower- then upper-case name.
		 * @param lowerName The lower-case variable name.
		 * @param upperName The upper-case variable name.
		 * @return std::string Empty when neither is set.
		 */
		std::string
		environmentValue (const char * lowerName, const char * upperName) noexcept
		{
			if ( const auto * value = std::getenv(lowerName); value != nullptr )
			{
				return value;
			}

			if ( const auto * value = std::getenv(upperName); value != nullptr )
			{
				return value;
			}

			return {};
		}

		/**
		 * @brief Returns whether a target host matches a no_proxy bypass list.
		 * @param host The target host (lower-cased).
		 * @param noProxyList The comma-separated no_proxy value.
		 * @return bool
		 */
		bool
		matchesNoProxy (const std::string & host, const std::string & noProxyList) noexcept
		{
			for ( auto entry : String::explode(noProxyList, ',') )
			{
				entry = String::toLower(String::trim(entry));

				if ( entry.empty() )
				{
					continue;
				}

				/* '*' bypasses everything. */
				if ( entry == "*" )
				{
					return true;
				}

				/* A leading '.' is a domain suffix; otherwise exact or suffix match. */
				if ( entry.front() == '.' )
				{
					if ( host.size() >= entry.size() && host.compare(host.size() - entry.size(), entry.size(), entry) == 0 )
					{
						return true;
					}
				}
				else if ( host == entry || (host.size() > entry.size() && host.compare(host.size() - entry.size() - 1, entry.size() + 1, '.' + entry) == 0) )
				{
					return true;
				}
			}

			return false;
		}

		/**
		 * @brief Compares two header field names, case-insensitively (RFC 9110 §5.1).
		 * @note Takes views and allocates nothing: it sits on the validation path of every
		 * caller-supplied header.
		 * @param lhs The first name.
		 * @param rhs The second name.
		 * @return bool
		 */
		[[nodiscard]]
		bool
		headerNameEquals (std::string_view lhs, std::string_view rhs) noexcept
		{
			if ( lhs.size() != rhs.size() )
			{
				return false;
			}

			for ( size_t index = 0; index < lhs.size(); ++index )
			{
				if ( std::tolower(static_cast< unsigned char >(lhs[index])) != std::tolower(static_cast< unsigned char >(rhs[index])) )
				{
					return false;
				}
			}

			return true;
		}

		/**
		 * @brief Returns whether a character is a RFC 9110 §5.6.2 token character.
		 * @param character The character.
		 * @return bool
		 */
		[[nodiscard]]
		bool
		isTokenChar (char character) noexcept
		{
			if ( std::isalnum(static_cast< unsigned char >(character)) != 0 )
			{
				return true;
			}

			constexpr std::string_view Specials{"!#$%&'*+-.^_`|~"};

			return Specials.find(character) != std::string_view::npos;
		}

		/**
		 * @brief Returns whether a header name is one the client writes itself.
		 * @note A second copy of a framing header is a request-smuggling primitive, so a caller
		 * supplying one is refused rather than silently overridden. User-Agent is deliberately
		 * absent: overriding it is legitimate and harmless.
		 * @param name The header field name.
		 * @return bool
		 */
		[[nodiscard]]
		bool
		isReservedRequestHeader (std::string_view name) noexcept
		{
			constexpr std::array< std::string_view, 5 > Reserved{
				std::string_view{"Host"},
				std::string_view{"Content-Length"},
				std::string_view{"Connection"},
				std::string_view{"Transfer-Encoding"},
				std::string_view{"Accept-Encoding"}
			};

			return std::ranges::any_of(Reserved, [name] (std::string_view reserved) {
				return headerNameEquals(name, reserved);
			});
		}

		/**
		 * @brief Returns whether a method always frames a body, even an empty one.
		 * @note A server reading a POST without Content-Length waits for a body that never comes,
		 * so these three get the header whatever the body size.
		 * @param method The HTTP method.
		 * @return bool
		 */
		[[nodiscard]]
		bool
		methodFramesABody (HTTPRequest::Method method) noexcept
		{
			switch ( method )
			{
				case HTTPRequest::Method::POST :
				case HTTPRequest::Method::PUT :
				case HTTPRequest::Method::PATCH :
					return true;

				default :
					return false;
			}
		}

		/**
		 * @brief Returns whether two URIs share an origin (scheme, host and effective port).
		 * @note The scheme counts since the private cleartext path exists (2026-10-04): an http:// and an
		 * https:// URI on one host are two origins (RFC 6454).
		 * @param lhs The first URI.
		 * @param rhs The second URI.
		 * @return bool False when either target cannot be extracted.
		 */
		[[nodiscard]]
		bool
		sameOrigin (const URI & lhs, const URI & rhs) noexcept
		{
			std::string leftHost;
			std::string rightHost;
			uint16_t leftPort = 0;
			uint16_t rightPort = 0;
			bool leftCleartext = false;
			bool rightCleartext = false;

			if ( !extractTarget(lhs, true, leftHost, leftPort, leftCleartext) || !extractTarget(rhs, true, rightHost, rightPort, rightCleartext) )
			{
				return false;
			}

			return leftCleartext == rightCleartext && leftPort == rightPort && headerNameEquals(leftHost, rightHost);
		}
	}

	HTTPSClient::HTTPSClient (asio::ssl::context & tlsContext, HTTPSClientOptions options) noexcept
		: m_tlsContext{tlsContext},
		m_options{std::move(options)}
	{

	}

	bool
	HTTPSClient::download (const URI & uri, const std::filesystem::path & filepath, const DownloadProgress & progress, DownloadReport * report) const noexcept
	{
		return this->download(uri, filepath, HTTPRequestOptions{}, progress, report);
	}

	bool
	HTTPSClient::download (const URI & uri, const std::filesystem::path & filepath, HTTPRequestOptions options, const DownloadProgress & progress, DownloadReport * report) const noexcept
	{
		if ( report != nullptr )
		{
			*report = {};
		}

		if ( !options.body.empty() )
		{
			Logging::error(Tag, "download(), a download is a GET: it carries no body.");

			if ( report != nullptr )
			{
				report->outcome = DownloadOutcome::BadRequest;
			}

			return false;
		}

		/* The transport records its own coarse reason; anything it did not classify is a protocol
		 * or local-I/O problem, which run() distinguishes. The variable is a LOCAL: several
		 * download() calls run concurrently on one shared client (Net::Manager does exactly that),
		 * and the member this used to be was a data race between them. */
		DownloadOutcome outcome{DownloadOutcome::Protocol};

		const auto result = this->run(HTTPRequest::Method::GET, uri, BodySink::File, filepath, std::move(options), outcome, progress ? &progress : nullptr);

		if ( !result.has_value() )
		{
			/* performHop() already discarded its own partial file; make sure a file left by a
			 * previous attempt is not mistaken for this one's result. */
			std::error_code removeError;
			std::filesystem::remove(filepath, removeError);

			if ( report != nullptr )
			{
				report->outcome = outcome;
			}

			return false;
		}

		const auto statusCode = result->response.codeResponse();

		if ( report != nullptr )
		{
			report->statusCode = static_cast< uint16_t >(statusCode);
			report->contentType = result->response.value(HTTPResponse::ContentType);
		}

		if ( statusCode < 200 || statusCode > 299 )
		{
			Logging::error(Tag, "download(), the server answered with status " + std::to_string(statusCode) + ".");

			std::error_code removeError;
			std::filesystem::remove(filepath, removeError);

			if ( report != nullptr )
			{
				report->outcome = DownloadOutcome::HTTPStatus;
			}

			return false;
		}

		return true;
	}

	bool
	HTTPSClient::isRequestHeaderAcceptable (const std::string & name, const std::string & value) noexcept
	{
		if ( name.empty() || !std::ranges::all_of(name, isTokenChar) )
		{
			return false;
		}

		if ( isReservedRequestHeader(name) )
		{
			return false;
		}

		/* ⚠️ The request is built by concatenation, so a CR or LF inside a value ends the header
		 * line early and injects everything after it — header injection, and with a body, request
		 * splitting. Every other C0 control (and DEL) is refused too; only HTAB is legal in a
		 * field value (RFC 9110 §5.5). */
		return std::ranges::none_of(value, [] (char character) {
			const auto byte = static_cast< unsigned char >(character);

			if ( byte == '\t' )
			{
				return false;
			}

			return byte < 0x20 || byte == 0x7F;
		});
	}

	std::optional< HTTPResult >
	HTTPSClient::request (HTTPRequest::Method method, const URI & uri, HTTPRequestOptions options, DownloadReport * report) const noexcept
	{
		if ( report != nullptr )
		{
			*report = {};
		}

		DownloadOutcome outcome{DownloadOutcome::Protocol};

		auto result = this->run(method, uri, BodySink::Memory, {}, std::move(options), outcome);

		if ( !result.has_value() )
		{
			if ( report != nullptr )
			{
				report->outcome = outcome;
			}

			return std::nullopt;
		}

		const auto statusCode = result->response.codeResponse();

		if ( report != nullptr )
		{
			report->statusCode = static_cast< uint16_t >(statusCode);
			report->contentType = result->response.value(HTTPResponse::ContentType);

			/* ⚠️ Unlike download(), a non-2xx is NOT a failure here and the response is still
			 * returned: an API answers 404 or 422 with a body the caller has to read to know what
			 * went wrong. The outcome merely labels it so the caller can branch without
			 * re-deriving the class from the status code. */
			report->outcome = statusCode >= 200 && statusCode <= 299 ? DownloadOutcome::Success : DownloadOutcome::HTTPStatus;
		}

		return result;
	}

	bool
	HTTPSClient::resolveRedirect (const URI & current, const std::string & location, URI & resolved) noexcept
	{
		const auto trimmedLocation = String::trim(location);

		if ( trimmedLocation.empty() )
		{
			return false;
		}

		/* Full RFC 3986 §5 reference resolution: handles absolute URLs,
		 * absolute-path ("/x"), and relative ("../x") Locations uniformly. */
		resolved = URI::resolve(current, trimmedLocation);

		if ( resolved.uriDomain().hostname().name().empty() )
		{
			Logging::error(Tag, "resolveRedirect(), the Location '" + trimmedLocation + "' resolves to no host.");

			return false;
		}

		/* Refuse a https -> http downgrade (owner-ruled trust policy). */
		if ( String::toLower(current.scheme()) == "https" && String::toLower(resolved.scheme()) != "https" )
		{
			Logging::error(Tag, "resolveRedirect(), refused https -> http downgrade to '" + trimmedLocation + "'.");

			return false;
		}

		return true;
	}

	bool
	HTTPSClient::resolveProxy (const std::string & targetHost, std::string & proxyHost, uint16_t & proxyPort) const noexcept
	{
		std::string proxyAuthority = m_options.proxy;

		/* No explicit proxy: consult the environment when allowed. */
		if ( proxyAuthority.empty() )
		{
			if ( !m_options.useEnvironmentProxy )
			{
				return false;
			}

			if ( const auto noProxy = environmentValue("no_proxy", "NO_PROXY"); !noProxy.empty() && matchesNoProxy(String::toLower(targetHost), noProxy) )
			{
				return false;
			}

			proxyAuthority = environmentValue("https_proxy", "HTTPS_PROXY");

			if ( proxyAuthority.empty() )
			{
				return false;
			}
		}

		/* Parse "host:port" or "scheme://host:port" via the URI parser. A bare
		 * "host:port" has no scheme, so prefix "//" to force authority parsing. */
		const auto normalized = proxyAuthority.find("://") != std::string::npos ? proxyAuthority : "//" + proxyAuthority;

		const URI proxyURI{normalized};

		proxyHost = proxyURI.uriDomain().hostname().name();

		if ( proxyHost.empty() )
		{
			Logging::error(Tag, "resolveProxy(), unparseable proxy '" + proxyAuthority + "'.");

			return false;
		}

		const auto declaredPort = proxyURI.uriDomain().port();

		/* Default to the conventional proxy port when none is given. */
		proxyPort = declaredPort == 0 ? uint16_t{8080} : static_cast< uint16_t >(declaredPort);

		return true;
	}

	std::optional< HTTPResult >
	HTTPSClient::run (HTTPRequest::Method method, const URI & uri, BodySink sink, const std::filesystem::path & filepath, HTTPRequestOptions options, DownloadOutcome & outcome, const DownloadProgress * progress) const noexcept
	{
		/* ⚠️ Validated ONCE, before the first connection is opened. Refusing a header only when
		 * performHop() concatenates it would already have resolved and contacted the target, and
		 * the caller cannot tell that apart from a transport failure. */
		for ( const auto & [name, value] : options.headers )
		{
			if ( !HTTPSClient::isRequestHeaderAcceptable(name, value) )
			{
				outcome = DownloadOutcome::BadRequest;

				Logging::error(Tag, "run(), the request header '" + name + "' is refused: bad field name, control character in the value, or a framing header the client owns.");

				return std::nullopt;
			}
		}

		/* The media type takes the same path into the request line, so it needs the same check. */
		if ( !options.contentType.empty() && !HTTPSClient::isRequestHeaderAcceptable(HTTPRequest::ContentType, options.contentType) )
		{
			outcome = DownloadOutcome::BadRequest;

			Logging::error(Tag, "run(), the request content type is refused: it carries a control character.");

			return std::nullopt;
		}

		const auto deadline = std::chrono::steady_clock::now() + m_options.totalTimeout;

		/* A method rewritten to GET must not keep the body it was going to POST: the target would
		 * read it as the GET's own body, and a write would be replayed where none was intended. */
		const auto dropBody = [&options] () {
			options.body.clear();
			options.contentType.clear();
		};

		URI currentURI{uri};

		for ( uint8_t redirect = 0; redirect <= m_options.maxRedirects; ++redirect )
		{
			auto result = this->performHop(method, currentURI, sink, filepath, options, deadline, outcome, progress);

			if ( !result.has_value() )
			{
				return std::nullopt;
			}

			const auto statusCode = result->response.codeResponse();

			/* Not a redirect: this is the final response. */
			if ( statusCode < 300 || statusCode > 399 || statusCode == 304 )
			{
				return result;
			}

			if ( redirect == m_options.maxRedirects )
			{
				outcome = DownloadOutcome::Protocol;

				Logging::error(Tag, "run(), too many redirects (limit " + std::to_string(m_options.maxRedirects) + ").");

				return std::nullopt;
			}

			const auto location = result->response.value(HTTPResponse::Location);

			URI nextURI;

			if ( !HTTPSClient::resolveRedirect(currentURI, location, nextURI) )
			{
				outcome = DownloadOutcome::Protocol;

				return std::nullopt;
			}

			/* Method rewriting (RFC 9110 §15.4):
			 *  - 303 always becomes GET;
			 *  - 301/302 turn a POST into GET (established practice);
			 *  - 307/308 preserve the method (and would preserve the body). */
			/* RFC 9110 §15.4.4 exempts HEAD from the 303 rewrite. */
			if ( statusCode == 303 && method != HTTPRequest::Method::HEAD )
			{
				method = HTTPRequest::Method::GET;

				dropBody();
			}
			else if ( (statusCode == 301 || statusCode == 302) && method == HTTPRequest::Method::POST )
			{
				method = HTTPRequest::Method::GET;

				dropBody();
			}

			/* ⚠️ A redirect that leaves the origin DROPS every caller header. Forwarding an
			 * Authorization to whatever host a Location names hands that host the credential — the
			 * classic redirect credential leak. curl and every browser behave the same way. */
			if ( !options.headers.empty() && !sameOrigin(currentURI, nextURI) )
			{
				Logging::info(Tag, "run(), the redirect leaves the origin: the caller headers are not forwarded.");

				options.headers.clear();
			}

			currentURI = nextURI;
		}

		return std::nullopt;
	}

	std::unique_ptr< TLSConnection >
	HTTPSClient::takeIdleConnection (const ConnectionKey & key) const noexcept
	{
		while ( true )
		{
			std::unique_ptr< TLSConnection > candidate;
			std::vector< std::unique_ptr< TLSConnection > > expired;

			{
				const std::scoped_lock lock{m_idleConnectionsAccess};

				const auto keyIt = m_idleConnections.find(key);

				if ( keyIt == m_idleConnections.end() )
				{
					return nullptr;
				}

				auto & idle = keyIt->second;
				const auto now = std::chrono::steady_clock::now();

				/* The most recently used first: the likeliest to be still open. */
				while ( !idle.empty() && candidate == nullptr )
				{
					auto entry = std::move(idle.back());

					idle.pop_back();

					if ( now - entry.idleSince > IdleConnectionLifetime )
					{
						expired.push_back(std::move(entry.connection));
					}
					else
					{
						candidate = std::move(entry.connection);
					}
				}

				if ( idle.empty() )
				{
					m_idleConnections.erase(keyIt);
				}
			}

			/* Outside the lock: the expired ones close here (close_notify), the candidate is probed here. */
			expired.clear();

			if ( candidate == nullptr )
			{
				return nullptr;
			}

			if ( candidate->isOpenAndIdle() )
			{
				return candidate;
			}
		}
	}

	void
	HTTPSClient::keepIdleConnection (const ConnectionKey & key, std::unique_ptr< TLSConnection > connection) const noexcept
	{
		if ( connection == nullptr || !connection->isConnected() )
		{
			return;
		}

		/* Closed after the lock is released: a connection over the cap, and every expired one. */
		std::vector< std::unique_ptr< TLSConnection > > closing;

		{
			const std::scoped_lock lock{m_idleConnectionsAccess};

			const auto now = std::chrono::steady_clock::now();

			for ( auto keyIt = m_idleConnections.begin(); keyIt != m_idleConnections.end(); )
			{
				auto & idle = keyIt->second;

				for ( auto & entry : idle )
				{
					if ( now - entry.idleSince > IdleConnectionLifetime )
					{
						closing.push_back(std::move(entry.connection));
					}
				}

				std::erase_if(idle, [] (const IdleConnection & entry) {
					return entry.connection == nullptr;
				});

				keyIt = idle.empty() ? m_idleConnections.erase(keyIt) : std::next(keyIt);
			}

			auto & idle = m_idleConnections[key];

			if ( idle.size() < MaxIdleConnectionsPerKey )
			{
				idle.push_back({std::move(connection), now});
			}
			else
			{
				closing.push_back(std::move(connection));
			}
		}
	}

	std::optional< HTTPResult >
	HTTPSClient::performHop (HTTPRequest::Method method, const URI & uri, BodySink sink, const std::filesystem::path & filepath, const HTTPRequestOptions & options, std::chrono::steady_clock::time_point deadline, DownloadOutcome & outcome, const DownloadProgress * progress) const noexcept
	{
		std::string host;
		uint16_t port = 0;
		bool cleartext = false;

		outcome = DownloadOutcome::BadScheme;

		if ( !extractTarget(uri, m_options.allowPrivateCleartext, host, port, cleartext) )
		{
			Logging::error(Tag, "performHop(), only https URIs with a host are supported (got '" + uri.scheme() + "'; http needs allowPrivateCleartext).");

			return std::nullopt;
		}

		/* Build the request (origin-form target, identity encoding so no client-side decompression needed). HTTP/1.1
		 * connections are persistent by default: 'Connection: close' only when this client does not reuse them. */
		const auto callerHasUserAgent = std::ranges::any_of(options.headers, [] (const auto & header) {
			return headerNameEquals(header.first, HTTPRequest::UserAgent);
		});

		const auto callerHasContentType = std::ranges::any_of(options.headers, [] (const auto & header) {
			return headerNameEquals(header.first, HTTPRequest::ContentType);
		});

		std::string request;
		request.reserve(RequestReserveBytes + options.body.size());
		request += HTTPRequest::method(method);
		request += ' ';
		request += buildRequestTarget(uri);
		request += " HTTP/1.1\r\n";
		request += HTTPRequest::Host;
		request += ": ";
		request += host;

		/* RFC 9110 § 7.2: the port is part of the Host field when it is not the scheme's default. A loopback
		 * server checking its own name (the engine's MCP and sharing servers) refuses a bare "127.0.0.1". */
		if ( port != ( cleartext ? HTTPDefaultPort : HTTPSDefaultPort ) )
		{
			request += ':';
			request += std::to_string(port);
		}

		request += "\r\n";

		/* An API that keys on a named client needs its own User-Agent; the caller's wins. */
		if ( !callerHasUserAgent )
		{
			request += HTTPRequest::UserAgent;
			request += ": ";
			request += m_options.userAgent;
			request += "\r\n";
		}

		request += HTTPRequest::AcceptEncoding;
		request += ": identity\r\n";

		if ( !m_options.reuseConnections )
		{
			request += "Connection: close\r\n";
		}

		/* Caller headers. run() validated every one of them before this function ever ran, so no
		 * CR or LF can reach this concatenation. */
		for ( const auto & [name, value] : options.headers )
		{
			request += name;
			request += ": ";
			request += value;
			request += "\r\n";
		}

		if ( !options.body.empty() && !options.contentType.empty() && !callerHasContentType )
		{
			request += HTTPRequest::ContentType;
			request += ": ";
			request += options.contentType;
			request += "\r\n";
		}

		if ( !options.body.empty() || methodFramesABody(method) )
		{
			request += HTTPRequest::ContentLength;
			request += ": ";
			request += std::to_string(options.body.size());
			request += "\r\n";
		}

		request += "\r\n";
		request += options.body;

		std::string proxyHost;
		uint16_t proxyPort = 0;

		/* NOTE: the cleartext path goes straight to a private address — never through a proxy, which would
		 * carry it off the private network. */
		const bool proxied = !cleartext && this->resolveProxy(host, proxyHost, proxyPort);
		const ConnectionKey connectionKey{
			.host = host,
			.proxyHost = proxied ? proxyHost : std::string{},
			.port = port,
			.proxyPort = proxied ? proxyPort : uint16_t{0},
			.cleartext = cleartext
		};

		/* A request that cannot be replayed never rides a pooled connection: one the server closed while idle would
		 * fail it after it may have been acted upon. Idempotent methods (RFC 9110 § 9.2.2) are retried once. */
		const bool idempotent = method != HTTPRequest::Method::POST && method != HTTPRequest::Method::PATCH && method != HTTPRequest::Method::CONNECT;

		std::unique_ptr< TLSConnection > connection;
		bool reused = false;

		if ( m_options.reuseConnections && idempotent )
		{
			connection = this->takeIdleConnection(connectionKey);
			reused = connection != nullptr;
		}

		std::array< char, TransportReadBufferSize > buffer{};
		std::optional< size_t > firstRead;

		while ( true )
		{
			if ( connection == nullptr )
			{
				connection = std::make_unique< TLSConnection >(m_tlsContext, m_options.transportTimeouts);
				reused = false;

				bool connected = false;

				if ( cleartext )
				{
					connected = connection->connectCleartextPrivate(host, port);
				}
				else if ( proxied )
				{
					connected = connection->connectViaProxy(proxyHost, proxyPort, host, port);
				}
				else
				{
					connected = connection->connect(host, port);
				}

				if ( !connected )
				{
					/* Tell the two apart instead of calling both Unreachable. DownloadOutcome::TLSFailure
					 * documents itself as "handshake or certificate verification refused the peer", and
					 * until 2026-08-28 nothing in this file ever produced it - an expired certificate came
					 * back as Unreachable, which invites the retry that must never happen and hides the
					 * one thing the caller has to show the user. */
					outcome = connection->handshakeRefused() ? DownloadOutcome::TLSFailure : DownloadOutcome::Unreachable;

					return std::nullopt;
				}
			}

			/* Past the handshake: anything from here is protocol or local I/O. */
			outcome = DownloadOutcome::Protocol;

			const bool written = connection->write(request.data(), request.size());

			if ( written )
			{
				firstRead = connection->read(buffer.data(), buffer.size());
			}

			/* A pooled connection that fails before the first response byte was closed by the server while idle (the
			 * idle check and the server's close crossed): the request never reached it — retried on a new one. */
			if ( reused && (!written || !firstRead.has_value() || firstRead.value() == 0) )
			{
				connection = nullptr;

				continue;
			}

			if ( !written )
			{
				return std::nullopt;
			}

			break;
		}

		/* Body ceiling per sink: a file body may be large because it never sits in RAM; anything
		 * held in memory (get(), a redirect body, an error body) gets the in-memory ceiling. */
		auto parserLimits = m_options.parserLimits;

		if ( parserLimits.maxBodySize == std::numeric_limits< uint64_t >::max() )
		{
			parserLimits.maxBodySize = sink == BodySink::File ? m_options.maxDownloadSize : m_options.maxInMemoryBodySize;
		}

		HTTPResponseParser parser{parserLimits};

		if ( method == HTTPRequest::Method::HEAD )
		{
			parser.expectBodilessResponse();
		}

		std::ofstream fileStream;

		if ( sink == BodySink::File )
		{
			fileStream.open(filepath, std::ios::binary | std::ios::trunc);

			if ( !fileStream.is_open() )
			{
				outcome = DownloadOutcome::LocalIO;

				Logging::error(Tag, "performHop(), unable to open '" + filepath.string() + "' for writing.");

				return std::nullopt;
			}
		}

		/* Anything that leaves this function without a complete 2xx body must not leave a
		 * truncated file behind: the caller asked for a file, not for a fragment. */
		const auto discardPartialFile = [&fileStream, sink, &filepath] () noexcept {
			if ( sink != BodySink::File )
			{
				return;
			}

			fileStream.close();

			std::error_code removeError;
			std::filesystem::remove(filepath, removeError);
		};

		/* Progress total: the Content-Length of a 2xx hop, when the body is framed by it (a
		 * Transfer-Encoding header takes precedence and leaves the total unknown). Resolved
		 * once, when the headers are complete. */
		std::optional< uint64_t > progressTotal;
		bool progressTotalResolved = false;

		auto result = HTTPResponseParser::Result::NeedMoreData;
		/* Whether the body ended with the connection (read until close): such a connection is spent. */
		bool endedByClose = false;
		bool firstReadPending = true;

		while ( result == HTTPResponseParser::Result::NeedMoreData )
		{
			if ( options.cancel != nullptr && options.cancel->load() )
			{
				outcome = DownloadOutcome::Cancelled;

				Logging::info(Tag, "performHop(), cancelled by the caller.");

				discardPartialFile();

				return std::nullopt;
			}

			if ( std::chrono::steady_clock::now() >= deadline )
			{
				outcome = DownloadOutcome::Timeout;

				Logging::error(Tag, "performHop(), the total time budget expired.");

				discardPartialFile();

				return std::nullopt;
			}

			/* The first read already happened (it told a stale pooled connection apart). */
			const auto bytesRead = firstReadPending ? firstRead : connection->read(buffer.data(), buffer.size());

			firstReadPending = false;

			if ( !bytesRead.has_value() )
			{
				discardPartialFile();

				return std::nullopt;
			}

			if ( bytesRead.value() == 0 )
			{
				/* Peer closed: let the parser decide (until-close = done, else truncated). */
				result = parser.finish();
				endedByClose = true;

				break;
			}

			result = parser.feed(buffer.data(), bytesRead.value());

			/* Stream the body out and free the buffer between feeds. Only redirect-free
			 * hops go to disk; a redirect's small body stays in memory and is dropped. */
			if ( sink == BodySink::File && parser.headersComplete() )
			{
				const auto statusCode = parser.response().codeResponse();

				if ( statusCode >= 200 && statusCode <= 299 && !parser.body().empty() )
				{
					fileStream.write(parser.body().data(), static_cast< std::streamsize >(parser.body().size()));

					if ( fileStream.fail() )
					{
						outcome = DownloadOutcome::LocalIO;

						Logging::error(Tag, "performHop(), unable to write to '" + filepath.string() + "'.");

						discardPartialFile();

						return std::nullopt;
					}

					parser.body().clear();

					if ( progress != nullptr )
					{
						if ( !progressTotalResolved )
						{
							progressTotalResolved = true;

							if ( parser.response().value(HTTPResponse::TransferEncoding).empty() )
							{
								const auto contentLength = parser.response().value(HTTPResponse::ContentLength);

								if ( !contentLength.empty() && std::ranges::all_of(contentLength, [] (char character) {
									return character >= '0' && character <= '9';
								}) && contentLength.size() <= 19 )
								{
									/* NOTE: std::from_chars, not the throwing std::stoull (digits only, at most 19). */
									uint64_t total = 0;

									if ( const auto [end, error] = std::from_chars(contentLength.data(), contentLength.data() + contentLength.size(), total); error == std::errc{} )
									{
										progressTotal = total;
									}
								}
							}
						}

						(*progress)(parser.bodyBytesDecoded(), progressTotal);
					}
				}
			}

			/* A body that is not kept must not accumulate: an error body during a download, a
			 * redirect body, or a HEAD/Discard body would otherwise be buffered whole. */
			if ( sink != BodySink::Memory )
			{
				parser.body().clear();
			}
		}

		if ( result != HTTPResponseParser::Result::Complete )
		{
			discardPartialFile();

			return std::nullopt;
		}

		if ( sink == BodySink::File )
		{
			/* ⚠️ The destructor flushes and SWALLOWS the error: a full disk would be reported as a
			 * successful download. The last flush is checked here instead. */
			fileStream.flush();
			fileStream.close();

			if ( fileStream.fail() )
			{
				outcome = DownloadOutcome::LocalIO;

				Logging::error(Tag, "performHop(), unable to flush '" + filepath.string() + "' (disk full?).");

				discardPartialFile();

				return std::nullopt;
			}

			/* A zero-length 2xx body never entered the write branch: the hook still owes the
			 * consumer its terminal call. */
			if ( progress != nullptr && parser.bodyBytesDecoded() == 0 )
			{
				const auto statusCode = parser.response().codeResponse();

				if ( statusCode >= 200 && statusCode <= 299 )
				{
					(*progress)(0, progressTotal);
				}
			}
		}

		/* Keep-alive: a response framed by its length (not by the close), that the server did not close (RFC 9112 § 9.3),
		 * leaves the connection ready for the next request. */
		if ( m_options.reuseConnections && !endedByClose && parser.response().keepConnectionAlive() )
		{
			this->keepIdleConnection(connectionKey, std::move(connection));
		}

		HTTPResult httpResult;
		httpResult.response = parser.response();

		if ( sink == BodySink::Memory )
		{
			httpResult.body = std::move(parser.body());
		}

		return httpResult;
	}
}
