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
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <fstream>
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
		constexpr size_t TransportReadBufferSize{16384};

		/* Fits the fixed header skeleton (~90 B) plus a typical host, target
		 * and user-agent in a single allocation. A long query string may still
		 * grow it once, which is acceptable. */
		constexpr size_t RequestReserveBytes{256};

		/**
		 * @brief Extracts the HTTPS host and port from a URI, validating the scheme.
		 * @param uri The URI.
		 * @param host The host [out].
		 * @param port The port [out].
		 * @return bool False when the scheme is not https or the host is empty.
		 */
		bool
		extractHTTPSTarget (const URI & uri, std::string & host, uint16_t & port) noexcept
		{
			if ( String::toLower(uri.scheme()) != "https" )
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
				port = HTTPSDefaultPort;
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
		 * @brief Returns whether two URIs share an origin (host and effective port).
		 * @note The scheme is not compared: every hop this client speaks is https by construction
		 * — a downgrade Location is refused and an http one is upgraded.
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

			if ( !extractHTTPSTarget(lhs, leftHost, leftPort) || !extractHTTPSTarget(rhs, rightHost, rightPort) )
			{
				return false;
			}

			return leftPort == rightPort && headerNameEquals(leftHost, rightHost);
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
		if ( report != nullptr )
		{
			*report = {};
		}

		/* The transport records its own coarse reason; anything it did not classify is a protocol
		 * or local-I/O problem, which run() distinguishes. The variable is a LOCAL: several
		 * download() calls run concurrently on one shared client (Net::Manager does exactly that),
		 * and the member this used to be was a data race between them. */
		DownloadOutcome outcome{DownloadOutcome::Protocol};

		const auto result = this->run(HTTPRequest::Method::GET, uri, BodySink::File, filepath, {}, outcome, progress ? &progress : nullptr);

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

	std::optional< HTTPResult >
	HTTPSClient::performHop (HTTPRequest::Method method, const URI & uri, BodySink sink, const std::filesystem::path & filepath, const HTTPRequestOptions & options, std::chrono::steady_clock::time_point deadline, DownloadOutcome & outcome, const DownloadProgress * progress) const noexcept
	{
		std::string host;
		uint16_t port = 0;

		outcome = DownloadOutcome::BadScheme;

		if ( !extractHTTPSTarget(uri, host, port) )
		{
			Logging::error(Tag, "performHop(), only https URIs with a host are supported (got '" + uri.scheme() + "').");

			return std::nullopt;
		}

		/* Build the request (origin-form target, explicit close — no keep-alive
		 * reuse yet, identity encoding so no client-side decompression needed). */
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
		request += "Connection: close\r\n";

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

		TLSConnection connection{m_tlsContext, m_options.transportTimeouts};

		std::string proxyHost;
		uint16_t proxyPort = 0;

		const auto connected = this->resolveProxy(host, proxyHost, proxyPort)
			? connection.connectViaProxy(proxyHost, proxyPort, host, port)
			: connection.connect(host, port);

		if ( !connected )
		{
			/* Tell the two apart instead of calling both Unreachable. DownloadOutcome::TLSFailure
			 * documents itself as "handshake or certificate verification refused the peer", and
			 * until 2026-08-28 nothing in this file ever produced it - an expired certificate came
			 * back as Unreachable, which invites the retry that must never happen and hides the
			 * one thing the caller has to show the user. */
			outcome = connection.handshakeRefused() ? DownloadOutcome::TLSFailure : DownloadOutcome::Unreachable;

			return std::nullopt;
		}

		/* Past the handshake: anything from here is protocol or local I/O. */
		outcome = DownloadOutcome::Protocol;

		if ( !connection.write(request.data(), request.size()) )
		{
			return std::nullopt;
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

		std::array< char, TransportReadBufferSize > buffer{};

		/* Progress total: the Content-Length of a 2xx hop, when the body is framed by it (a
		 * Transfer-Encoding header takes precedence and leaves the total unknown). Resolved
		 * once, when the headers are complete. */
		std::optional< uint64_t > progressTotal;
		bool progressTotalResolved = false;

		auto result = HTTPResponseParser::Result::NeedMoreData;

		while ( result == HTTPResponseParser::Result::NeedMoreData )
		{
			if ( std::chrono::steady_clock::now() >= deadline )
			{
				outcome = DownloadOutcome::Timeout;

				Logging::error(Tag, "performHop(), the total time budget expired.");

				discardPartialFile();

				return std::nullopt;
			}

			const auto bytesRead = connection.read(buffer.data(), buffer.size());

			if ( !bytesRead.has_value() )
			{
				discardPartialFile();

				return std::nullopt;
			}

			if ( bytesRead.value() == 0 )
			{
				/* Peer closed: let the parser decide (until-close = done, else truncated). */
				result = parser.finish();

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
									progressTotal = std::stoull(contentLength);
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

		HTTPResult httpResult;
		httpResult.response = parser.response();

		if ( sink == BodySink::Memory )
		{
			httpResult.body = std::move(parser.body());
		}

		return httpResult;
	}
}
