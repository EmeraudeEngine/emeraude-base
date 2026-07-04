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

/* Project configuration. */
#include "emeraude_base_config.hpp"

/* STL inclusions. */
#include <array>
#include <cstdlib>
#include <fstream>

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
	}

	HTTPSClient::HTTPSClient (asio::ssl::context & tlsContext, HTTPSClientOptions options) noexcept
		: m_tlsContext(tlsContext),
		m_options(std::move(options))
	{

	}

	std::optional< HTTPResult >
	HTTPSClient::get (const URI & uri) noexcept
	{
		return this->run(HTTPRequest::Method::GET, uri, BodySink::Memory, {});
	}

	std::optional< HTTPResult >
	HTTPSClient::head (const URI & uri) noexcept
	{
		return this->run(HTTPRequest::Method::HEAD, uri, BodySink::Discard, {});
	}

	bool
	HTTPSClient::download (const URI & uri, const std::filesystem::path & filepath) noexcept
	{
		const auto result = this->run(HTTPRequest::Method::GET, uri, BodySink::File, filepath);

		if ( !result.has_value() )
		{
			return false;
		}

		const auto statusCode = result->response.codeResponse();

		if ( statusCode < 200 || statusCode > 299 )
		{
			Logging::error(Tag, "download(), the server answered with status " + std::to_string(statusCode) + ".");

			std::error_code removeError;
			std::filesystem::remove(filepath, removeError);

			return false;
		}

		return true;
	}

	bool
	HTTPSClient::resolveRedirect (const URI & current, const std::string & location, URI & resolved) const noexcept
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

		/* Refuse an https -> http downgrade (owner-ruled trust policy). */
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
	HTTPSClient::run (HTTPRequest::Method method, const URI & uri, BodySink sink, const std::filesystem::path & filepath) noexcept
	{
		const auto deadline = std::chrono::steady_clock::now() + m_options.totalTimeout;

		URI currentURI{uri};

		for ( uint8_t redirect = 0; redirect <= m_options.maxRedirects; ++redirect )
		{
			auto result = this->performHop(method, currentURI, sink, filepath, deadline);

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
				Logging::error(Tag, "run(), too many redirects (limit " + std::to_string(m_options.maxRedirects) + ").");

				return std::nullopt;
			}

			const auto location = result->response.value(HTTPResponse::Location);

			URI nextURI;

			if ( !this->resolveRedirect(currentURI, location, nextURI) )
			{
				return std::nullopt;
			}

			/* Method rewriting (RFC 9110 §15.4):
			 *  - 303 always becomes GET;
			 *  - 301/302 turn a POST into GET (established practice);
			 *  - 307/308 preserve the method (and would preserve the body). */
			if ( statusCode == 303 )
			{
				method = HTTPRequest::Method::GET;
			}
			else if ( (statusCode == 301 || statusCode == 302) && method == HTTPRequest::Method::POST )
			{
				method = HTTPRequest::Method::GET;
			}

			currentURI = nextURI;
		}

		return std::nullopt;
	}

	std::optional< HTTPResult >
	HTTPSClient::performHop (HTTPRequest::Method method, const URI & uri, BodySink sink, const std::filesystem::path & filepath, std::chrono::steady_clock::time_point deadline) noexcept
	{
		std::string host;
		uint16_t port = 0;

		if ( !extractHTTPSTarget(uri, host, port) )
		{
			Logging::error(Tag, "performHop(), only https URIs with a host are supported (got '" + uri.scheme() + "').");

			return std::nullopt;
		}

		/* Build the request (origin-form target, explicit close — no keep-alive
		 * reuse yet, identity encoding so no client-side decompression needed). */
		std::string request;
		request += HTTPRequest::method(method);
		request += ' ';
		request += buildRequestTarget(uri);
		request += " HTTP/1.1\r\n";
		request += std::string{HTTPRequest::Host} + ": " + host + "\r\n";
		request += std::string{HTTPRequest::UserAgent} + ": " + m_options.userAgent + "\r\n";
		request += std::string{HTTPRequest::AcceptEncoding} + ": identity\r\n";
		request += "Connection: close\r\n";
		request += "\r\n";

		TLSConnection connection{m_tlsContext, m_options.transportTimeouts};

		std::string proxyHost;
		uint16_t proxyPort = 0;

		const auto connected = this->resolveProxy(host, proxyHost, proxyPort)
			? connection.connectViaProxy(proxyHost, proxyPort, host, port)
			: connection.connect(host, port);

		if ( !connected )
		{
			return std::nullopt;
		}

		if ( !connection.write(request.data(), request.size()) )
		{
			return std::nullopt;
		}

		HTTPResponseParser parser{m_options.parserLimits};

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
				Logging::error(Tag, "performHop(), unable to open '" + filepath.string() + "' for writing.");

				return std::nullopt;
			}
		}

		std::array< char, TransportReadBufferSize > buffer{};

		auto result = HTTPResponseParser::Result::NeedMoreData;

		while ( result == HTTPResponseParser::Result::NeedMoreData )
		{
			if ( std::chrono::steady_clock::now() >= deadline )
			{
				Logging::error(Tag, "performHop(), the total time budget expired.");

				return std::nullopt;
			}

			const auto bytesRead = connection.read(buffer.data(), buffer.size());

			if ( !bytesRead.has_value() )
			{
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
						Logging::error(Tag, "performHop(), unable to write to '" + filepath.string() + "'.");

						return std::nullopt;
					}

					parser.body().clear();
				}
			}
		}

		if ( result != HTTPResponseParser::Result::Complete )
		{
			return std::nullopt;
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
