/*
 * src/Network/HTTPServer.cpp
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

#include "HTTPServer.hpp"

/* STL inclusions. */
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "String.hpp"

namespace EmEn::Base::Network
{
	namespace
	{
		/** @brief The size of one chunk of a streamed file. */
		constexpr size_t FileChunkBytes{size_t{256} * 1024};

		/**
		 * @brief Returns a lowercase copy of an ASCII string.
		 * @param text The text.
		 * @return std::string
		 */
		[[nodiscard]]
		std::string
		lowercase (std::string text) noexcept
		{
			std::ranges::transform(text, text.begin(), [] (char character) {
				return ( character >= 'A' && character <= 'Z' ) ? static_cast< char >(character - 'A' + 'a') : character;
			});

			return text;
		}

		/**
		 * @brief Parses a decimal unsigned integer: digits only, no sign, no overflow, no locale.
		 * @param text The text.
		 * @return std::optional< uint64_t >
		 */
		[[nodiscard]]
		std::optional< uint64_t >
		parseDecimal (std::string_view text) noexcept
		{
			if ( text.empty() || text.size() > 19 || !std::ranges::all_of(text, [] (char character) { return character >= '0' && character <= '9'; }) )
			{
				return std::nullopt;
			}

			uint64_t value = 0;

			for ( const auto character : text )
			{
				value = (value * 10) + static_cast< uint64_t >(character - '0');
			}

			return value;
		}

		/**
		 * @brief Parses the request line and the headers.
		 * @param head The bytes before the empty line.
		 * @param request Receives the method, target, version and headers.
		 * @return bool False on a malformed request (or a smuggling attempt: duplicated framing headers).
		 */
		[[nodiscard]]
		bool
		parseHead (const std::string & head, HTTPServerRequest & request) noexcept
		{
			std::istringstream stream{head};
			std::string line;

			if ( !std::getline(stream, line) )
			{
				return false;
			}

			if ( !line.empty() && line.back() == '\r' )
			{
				line.pop_back();
			}

			const auto firstSpace = line.find(' ');

			if ( firstSpace == std::string::npos )
			{
				return false;
			}

			const auto secondSpace = line.find(' ', firstSpace + 1);

			if ( secondSpace == std::string::npos )
			{
				return false;
			}

			request.method = line.substr(0, firstSpace);
			request.target = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
			request.version = line.substr(secondSpace + 1);

			if ( request.method.empty() || request.target.empty() || ( request.version != "HTTP/1.1" && request.version != "HTTP/1.0" ) )
			{
				return false;
			}

			while ( std::getline(stream, line) )
			{
				if ( !line.empty() && line.back() == '\r' )
				{
					line.pop_back();
				}

				if ( line.empty() )
				{
					break;
				}

				const auto colon = line.find(':');

				if ( colon == std::string::npos || colon == 0 )
				{
					return false;
				}

				auto name = lowercase(line.substr(0, colon));
				auto value = String::trim(line.substr(colon + 1));

				/* NOTE: two framing or identity headers that disagree are how requests are smuggled past a
				 * check: refuse any duplicate of them instead of choosing one. */
				if ( request.headers.contains(name) )
				{
					if ( name == "content-length" || name == "host" || name == "authorization" || name == "origin" || name == "transfer-encoding" || name == "range" )
					{
						return false;
					}

					continue;
				}

				request.headers.emplace(std::move(name), std::move(value));
			}

			return true;
		}
	}

	std::string
	HTTPServerRequest::header (const std::string & lowercaseName) const noexcept
	{
		const auto headerIt = headers.find(lowercaseName);

		return headerIt != headers.end() ? headerIt->second : std::string{};
	}

	std::string
	HTTPServerRequest::path () const noexcept
	{
		const auto query = target.find('?');

		return query == std::string::npos ? target : target.substr(0, query);
	}

	const char *
	reasonPhrase (int status) noexcept
	{
		switch ( status )
		{
			case 200 : return "OK";
			case 202 : return "Accepted";
			case 206 : return "Partial Content";
			case 400 : return "Bad Request";
			case 401 : return "Unauthorized";
			case 403 : return "Forbidden";
			case 404 : return "Not Found";
			case 405 : return "Method Not Allowed";
			case 408 : return "Request Timeout";
			case 411 : return "Length Required";
			case 413 : return "Content Too Large";
			case 415 : return "Unsupported Media Type";
			case 416 : return "Range Not Satisfiable";
			case 431 : return "Request Header Fields Too Large";
			case 500 : return "Internal Server Error";
			case 501 : return "Not Implemented";
			case 503 : return "Service Unavailable";
			default : return "Error";
		}
	}

	std::optional< std::pair< uint64_t, uint64_t > >
	parseByteRange (std::string_view value, uint64_t size, bool & unsatisfiable) noexcept
	{
		unsatisfiable = false;

		constexpr std::string_view Unit{"bytes="};

		if ( !value.starts_with(Unit) )
		{
			return std::nullopt;
		}

		const auto spec = value.substr(Unit.size());

		/* NOTE: several ranges would need a multipart answer: ignored, the whole file is served (RFC 9110 § 14.2). */
		if ( spec.find(',') != std::string_view::npos )
		{
			return std::nullopt;
		}

		const auto dash = spec.find('-');

		if ( dash == std::string_view::npos )
		{
			return std::nullopt;
		}

		const auto firstText = spec.substr(0, dash);
		const auto lastText = spec.substr(dash + 1);

		/* A suffix range: the last N bytes. */
		if ( firstText.empty() )
		{
			const auto suffix = parseDecimal(lastText);

			if ( !suffix.has_value() )
			{
				return std::nullopt;
			}

			if ( *suffix == 0 || size == 0 )
			{
				unsatisfiable = true;

				return std::nullopt;
			}

			const auto length = std::min(*suffix, size);

			return std::pair{size - length, length};
		}

		const auto first = parseDecimal(firstText);

		if ( !first.has_value() )
		{
			return std::nullopt;
		}

		uint64_t last = size > 0 ? size - 1 : 0;

		if ( !lastText.empty() )
		{
			const auto parsedLast = parseDecimal(lastText);

			if ( !parsedLast.has_value() || *parsedLast < *first )
			{
				return std::nullopt;
			}

			last = std::min(*parsedLast, last);
		}

		if ( *first >= size )
		{
			unsatisfiable = true;

			return std::nullopt;
		}

		return std::pair{*first, last - *first + 1};
	}

	HTTPServerConnection::HTTPServerConnection (HTTPServer & server, asio::ip::tcp::socket socket, uint64_t id) noexcept
		: m_server{server},
		m_socket{std::move(socket)},
		m_timer{m_socket.get_executor()},
		m_buffer{server.options().maxHeaderBytes + server.options().maxBodyBytes},
		m_id{id}
	{

	}

	void
	HTTPServerConnection::start () noexcept
	{
		this->readRequest();
	}

	void
	HTTPServerConnection::close () noexcept
	{
		if ( m_closed )
		{
			return;
		}

		m_closed = true;
		m_fileBody.reset();

		asio::error_code ec;
		m_timer.cancel();
		m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		m_socket.close(ec);

		m_server.removeConnection(this->shared_from_this());
	}

	std::string
	HTTPServerConnection::statusLine (int status) noexcept
	{
		return "HTTP/1.1 " + std::to_string(status) + " " + reasonPhrase(status) + "\r\n";
	}

	std::string
	HTTPServerConnection::connectionHeader () const noexcept
	{
		return m_keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
	}

	void
	HTTPServerConnection::respond (int status, std::string_view contentType, std::string_view body, std::string_view extraHeaders) noexcept
	{
		auto response = HTTPServerConnection::statusLine(status);
		response.append("Content-Type: ").append(contentType).append("\r\n");
		response.append(extraHeaders);
		response.append("Content-Length: ").append(std::to_string(body.size())).append("\r\n");
		response.append(this->connectionHeader());
		response.append("\r\n");

		if ( m_request.method != "HEAD" )
		{
			response.append(body);
		}

		this->finishWith(std::move(response));
	}

	void
	HTTPServerConnection::respondEmpty (int status, std::string_view extraHeaders) noexcept
	{
		auto response = HTTPServerConnection::statusLine(status);
		response.append(extraHeaders);
		response.append("Content-Length: 0\r\n");
		response.append(this->connectionHeader());
		response.append("\r\n");

		this->finishWith(std::move(response));
	}

	void
	HTTPServerConnection::respondFile (const std::filesystem::path & filepath, std::string_view contentType, std::string_view extraHeaders) noexcept
	{
		std::error_code fileError;

		if ( !std::filesystem::is_regular_file(filepath, fileError) || fileError )
		{
			this->respondEmpty(404);

			return;
		}

		const auto size = std::filesystem::file_size(filepath, fileError);

		if ( fileError )
		{
			this->respondEmpty(404);

			return;
		}

		uint64_t offset = 0;
		uint64_t length = size;
		auto status = 200;

		/* NOTE: an If-Range would need a validator this server does not emit: the whole file is the safe answer. */
		if ( const auto range = m_request.header("range"); !range.empty() && m_request.header("if-range").empty() )
		{
			bool unsatisfiable = false;

			if ( const auto byteRange = parseByteRange(range, size, unsatisfiable); byteRange.has_value() )
			{
				offset = byteRange->first;
				length = byteRange->second;
				status = 206;
			}
			else if ( unsatisfiable )
			{
				this->respondEmpty(416, "Content-Range: bytes */" + std::to_string(size) + "\r\n");

				return;
			}
		}

		auto response = HTTPServerConnection::statusLine(status);
		response.append("Content-Type: ").append(contentType).append("\r\n");
		response.append("Accept-Ranges: bytes\r\n");

		if ( status == 206 )
		{
			response.append("Content-Range: bytes ").append(std::to_string(offset)).append("-").append(std::to_string(offset + length - 1)).append("/").append(std::to_string(size)).append("\r\n");
		}

		response.append(extraHeaders);
		response.append("Content-Length: ").append(std::to_string(length)).append("\r\n");
		response.append(this->connectionHeader());
		response.append("\r\n");

		if ( m_request.method != "HEAD" && length > 0 )
		{
			FileBody fileBody;
			fileBody.file.open(filepath, std::ios::binary);
			fileBody.remaining = length;

			if ( !fileBody.file.is_open() )
			{
				this->respondEmpty(404);

				return;
			}

			if ( !fileBody.file.seekg(static_cast< std::streamoff >(offset)) )
			{
				this->respondEmpty(500);

				return;
			}

			m_fileBody.emplace(std::move(fileBody));
		}

		this->finishWith(std::move(response));
	}

	void
	HTTPServerConnection::startStream (std::string_view contentType, std::string_view extraHeaders, std::string keepAlivePayload, uint32_t keepAliveSeconds) noexcept
	{
		m_streaming = true;
		m_keepAlivePayload = std::move(keepAlivePayload);
		m_keepAliveSeconds = keepAliveSeconds;

		std::string head{"HTTP/1.1 200 OK\r\n"};
		head.append("Content-Type: ").append(contentType).append("\r\n");
		head.append(extraHeaders);
		head.append("Connection: keep-alive\r\n\r\n");

		this->write(std::move(head));

		if ( !m_keepAlivePayload.empty() && m_keepAliveSeconds > 0 )
		{
			this->armTimer(m_keepAliveSeconds, TimerRole::KeepAlive);
		}

		this->watchForClose();
	}

	void
	HTTPServerConnection::sendStream (std::string bytes) noexcept
	{
		if ( m_streaming && !m_closed )
		{
			this->write(std::move(bytes));
		}
	}

	void
	HTTPServerConnection::writeNowAndClose (std::string_view bytes) noexcept
	{
		if ( !m_closed && !bytes.empty() )
		{
			asio::error_code ec;
			static_cast< void >(asio::write(m_socket, asio::buffer(bytes.data(), bytes.size()), ec));
		}

		this->close();
	}

	void
	HTTPServerConnection::armTimer (uint32_t seconds, TimerRole role) noexcept
	{
		m_timerRole = role;
		m_timer.expires_after(std::chrono::seconds{seconds});

		auto self = this->shared_from_this();

		m_timer.async_wait([self, role] (const asio::error_code & ec) {
			if ( ec || self->m_closed || self->m_timerRole != role )
			{
				return;
			}

			if ( role == TimerRole::KeepAlive )
			{
				self->write(self->m_keepAlivePayload);
				self->armTimer(self->m_keepAliveSeconds, TimerRole::KeepAlive);

				return;
			}

			self->close();
		});
	}

	void
	HTTPServerConnection::readRequest () noexcept
	{
		if ( m_closed )
		{
			return;
		}

		const auto & options = m_server.options();

		/* Idle until the first byte, then the whole request must arrive in time (slowloris). */
		if ( m_buffer.size() > 0 )
		{
			this->armTimer(options.requestTimeoutSeconds, TimerRole::RequestDeadline);
		}
		else
		{
			this->armTimer(options.idleTimeoutSeconds, TimerRole::Idle);
		}

		auto self = this->shared_from_this();

		asio::async_read_until(m_socket, m_buffer, "\r\n\r\n", [self] (const asio::error_code & ec, std::size_t headBytes) {
			if ( self->m_closed )
			{
				return;
			}

			if ( ec )
			{
				if ( ec == asio::error::not_found )
				{
					self->m_keepAlive = false;
					self->respondEmpty(431);

					return;
				}

				self->close();

				return;
			}

			const auto & serverOptions = self->m_server.options();

			self->armTimer(serverOptions.requestTimeoutSeconds, TimerRole::RequestDeadline);

			if ( headBytes > serverOptions.maxHeaderBytes )
			{
				self->m_keepAlive = false;
				self->respondEmpty(431);

				return;
			}

			const std::string head{asio::buffers_begin(self->m_buffer.data()), asio::buffers_begin(self->m_buffer.data()) + static_cast< std::ptrdiff_t >(headBytes)};
			self->m_buffer.consume(headBytes);

			self->m_request = HTTPServerRequest{};

			if ( !parseHead(head, self->m_request) )
			{
				self->m_keepAlive = false;
				self->respondEmpty(400);

				return;
			}

			self->m_keepAlive = self->m_request.version == "HTTP/1.1" && lowercase(self->m_request.header("connection")) != "close";

			if ( !self->m_request.header("transfer-encoding").empty() )
			{
				self->m_keepAlive = false;
				self->respondEmpty(501);

				return;
			}

			size_t contentLength = 0;

			if ( const auto lengthText = self->m_request.header("content-length"); !lengthText.empty() )
			{
				const auto parsed = parseDecimal(lengthText);

				if ( !parsed.has_value() )
				{
					self->m_keepAlive = false;
					self->respondEmpty(400);

					return;
				}

				if ( *parsed > serverOptions.maxBodyBytes )
				{
					self->m_keepAlive = false;
					self->respondEmpty(413);

					return;
				}

				contentLength = static_cast< size_t >(*parsed);
			}
			else if ( self->m_request.method == "POST" )
			{
				self->m_keepAlive = false;
				self->respondEmpty(411);

				return;
			}

			self->readBody(contentLength);
		});
	}

	void
	HTTPServerConnection::readBody (size_t contentLength) noexcept
	{
		if ( m_buffer.size() >= contentLength )
		{
			m_request.body.assign(asio::buffers_begin(m_buffer.data()), asio::buffers_begin(m_buffer.data()) + static_cast< std::ptrdiff_t >(contentLength));
			m_buffer.consume(contentLength);

			this->handleRequest();

			return;
		}

		auto self = this->shared_from_this();

		asio::async_read(m_socket, m_buffer, asio::transfer_exactly(contentLength - m_buffer.size()), [self, contentLength] (const asio::error_code & ec, std::size_t /*bytes*/) {
			if ( self->m_closed )
			{
				return;
			}

			if ( ec )
			{
				self->close();

				return;
			}

			self->readBody(contentLength);
		});
	}

	void
	HTTPServerConnection::handleRequest () noexcept
	{
		m_timer.cancel();

		/* ⚠️ Host and Origin BEFORE anything else: a page loaded in a browser — CEF included, it can run in the very
		 * process that hosts this server — can reach 127.0.0.1, and DNS rebinding can make it look same-origin. */
		if ( !m_server.isAcceptedHost(m_request.header("host")) )
		{
			this->respondEmpty(403);

			return;
		}

		if ( const auto origin = m_request.header("origin"); !origin.empty() && !m_server.isAcceptedOrigin(origin) )
		{
			this->respondEmpty(403);

			return;
		}

		if ( !m_server.isAuthorized(m_request.header("authorization")) )
		{
			this->respondEmpty(401, "WWW-Authenticate: Bearer\r\n");

			return;
		}

		if ( m_server.m_onRequest )
		{
			m_server.m_onRequest(this->shared_from_this());

			return;
		}

		this->respondEmpty(404);
	}

	void
	HTTPServerConnection::watchForClose () noexcept
	{
		auto self = this->shared_from_this();

		m_socket.async_read_some(asio::buffer(m_scratch), [self] (const asio::error_code & ec, std::size_t /*bytes*/) {
			if ( self->m_closed )
			{
				return;
			}

			if ( ec )
			{
				self->close();

				return;
			}

			self->watchForClose();
		});
	}

	void
	HTTPServerConnection::finishWith (std::string response) noexcept
	{
		m_closeAfterFlush = !m_keepAlive;
		m_readAfterFlush = m_keepAlive;

		this->write(std::move(response));
	}

	void
	HTTPServerConnection::write (std::string bytes) noexcept
	{
		if ( m_closed )
		{
			return;
		}

		m_writeQueue.emplace_back(std::move(bytes));

		if ( !m_writing )
		{
			this->writeNext();
		}
	}

	void
	HTTPServerConnection::writeNext () noexcept
	{
		/* A file body follows its head, one chunk at a time: one chunk in memory per connection. */
		if ( m_writeQueue.empty() && m_fileBody.has_value() )
		{
			const auto chunkSize = static_cast< size_t >(std::min< uint64_t >(m_fileBody->remaining, FileChunkBytes));
			std::string chunk(chunkSize, '\0');

			m_fileBody->file.read(chunk.data(), static_cast< std::streamsize >(chunkSize));

			if ( std::cmp_not_equal(m_fileBody->file.gcount(), chunkSize) )
			{
				/* The length was announced: a short file cannot be answered any more, only cut. */
				Logging::error("HTTPServer", m_server.options().name + ": a file shrank or failed while it was being sent; the connection is closed.");

				this->close();

				return;
			}

			m_fileBody->remaining -= chunkSize;

			if ( m_fileBody->remaining == 0 )
			{
				m_fileBody.reset();
			}

			m_writeQueue.emplace_back(std::move(chunk));

			/* A reader that stops reading must not hold the connection forever. */
			this->armTimer(m_server.options().requestTimeoutSeconds, TimerRole::WriteDeadline);
		}

		if ( m_writeQueue.empty() )
		{
			m_writing = false;

			if ( m_timerRole == TimerRole::WriteDeadline )
			{
				m_timer.cancel();
			}

			if ( m_closeAfterFlush )
			{
				this->close();
			}
			else if ( m_readAfterFlush )
			{
				m_readAfterFlush = false;

				this->readRequest();
			}

			return;
		}

		m_writing = true;

		auto self = this->shared_from_this();

		asio::async_write(m_socket, asio::buffer(m_writeQueue.front()), [self] (const asio::error_code & ec, std::size_t /*bytes*/) {
			if ( self->m_closed )
			{
				return;
			}

			if ( ec )
			{
				self->close();

				return;
			}

			self->m_writeQueue.pop_front();
			self->writeNext();
		});
	}

	HTTPServer::HTTPServer (HTTPServerOptions options) noexcept
		: m_options{std::move(options)}
	{

	}

	HTTPServer::~HTTPServer ()
	{
		this->stop();
	}

	bool
	HTTPServer::start (RequestHandler onRequest, StreamShutdownHandler onStreamShutdown, ClosedHandler onClosed) noexcept
	{
		if ( m_running )
		{
			return true;
		}

		const auto & name = m_options.name;
		asio::error_code ec;

		const auto bindAddress = asio::ip::make_address(m_options.address, ec);

		if ( ec )
		{
			Logging::error("HTTPServer", name + ": invalid bind address '" + m_options.address + "': " + ec.message() + ". It stays closed.");

			return false;
		}

		m_loopback = bindAddress.is_loopback();

		if ( !m_loopback && m_options.bearerToken.empty() )
		{
			Logging::error("HTTPServer", name + ": refuses to listen on the non-loopback address '" + m_options.address + "' without a bearer token. It stays closed.");

			return false;
		}

		const asio::ip::tcp::endpoint endpoint{bindAddress, m_options.port};

		m_acceptor = std::make_unique< asio::ip::tcp::acceptor >(m_ioContext);
		m_acceptor->open(endpoint.protocol(), ec);

		if ( !ec )
		{
			/* NOTE: a failure here only costs the fast restart on the same port: a warning, then bind. */
			m_acceptor->set_option(asio::socket_base::reuse_address(true), ec);

			if ( ec )
			{
				Logging::warning("HTTPServer", name + ": unable to set reuse_address: " + ec.message());

				ec.clear();
			}

			m_acceptor->bind(endpoint, ec);
		}

		if ( !ec )
		{
			m_acceptor->listen(asio::socket_base::max_listen_connections, ec);
		}

		if ( !ec )
		{
			m_boundPort = m_acceptor->local_endpoint(ec).port();
		}

		if ( ec )
		{
			/* ⚠️ Windows reports a port another process holds exclusively (SO_EXCLUSIVEADDRUSE) as ACCESS DENIED,
			 * not "address in use": measured on an ASUS laptop whose Armoury Crate listens on 127.0.0.1:7778. */
			const auto * const hint = ( ec == asio::error::address_in_use || ec == asio::error::access_denied ) ?
				" (the port is most likely taken by another process; Windows reports that as access denied)" :
				"";

			Logging::error("HTTPServer", name + ": unable to listen on " + m_options.address + ':' + std::to_string(m_options.port) + ": " + ec.message() + hint + ". It stays closed.");

			m_acceptor.reset();

			return false;
		}

		m_onRequest = std::move(onRequest);
		m_onStreamShutdown = std::move(onStreamShutdown);
		m_onClosed = std::move(onClosed);

		m_ioContext.restart();
		m_workGuard.emplace(asio::make_work_guard(m_ioContext));
		m_running = true;

		this->accept();

		m_networkThread = std::thread([this] () {
			m_ioContext.run();
		});

		return true;
	}

	void
	HTTPServer::stop () noexcept
	{
		if ( !m_running )
		{
			return;
		}

		/* Streams get their last words and every socket closes ON the network thread (the only one that touches
		 * them), then the loop is released. Bounded: a stuck peer cannot hold the shutdown. */
		std::mutex doneMutex;
		std::condition_variable doneSignal;
		bool done = false;

		asio::post(m_ioContext, [this, &doneMutex, &doneSignal, &done] () {
			asio::error_code ec;

			if ( m_acceptor != nullptr )
			{
				m_acceptor->cancel(ec);
				m_acceptor->close(ec);
			}

			/* NOTE: a copy — closing a connection removes it from the set. */
			const auto connections = m_connections;

			for ( const auto & connection : connections )
			{
				if ( connection->isStreaming() && m_onStreamShutdown )
				{
					m_onStreamShutdown(*connection);
				}

				connection->close();
			}

			{
				const std::scoped_lock lock{doneMutex};

				done = true;
			}

			doneSignal.notify_one();
		});

		{
			std::unique_lock< std::mutex > lock{doneMutex};

			doneSignal.wait_for(lock, std::chrono::seconds{3}, [&done] () {
				return done;
			});
		}

		m_workGuard.reset();
		m_ioContext.stop();

		if ( m_networkThread.joinable() )
		{
			m_networkThread.join();
		}

		/* Handlers that never ran hold connections; drop them now, on this (the last) thread. */
		m_connections.clear();
		m_acceptor.reset();
		m_running = false;
	}

	std::string
	HTTPServer::baseURL () const noexcept
	{
		if ( !m_running )
		{
			return {};
		}

		const auto host = m_options.address.find(':') != std::string::npos ? "[" + m_options.address + "]" : m_options.address;

		return "http://" + host + ":" + std::to_string(m_boundPort);
	}

	void
	HTTPServer::post (std::function< void () > task) noexcept
	{
		if ( !m_running || !task )
		{
			return;
		}

		asio::post(m_ioContext, std::move(task));
	}

	void
	HTTPServer::forEachConnection (const std::function< void (HTTPServerConnection &) > & function) const noexcept
	{
		/* NOTE: a copy — the function may close a connection, which removes it from the set. */
		const auto connections = m_connections;

		for ( const auto & connection : connections )
		{
			function(*connection);
		}
	}

	void
	HTTPServer::accept () noexcept
	{
		m_acceptor->async_accept([this] (const asio::error_code & ec, asio::ip::tcp::socket socket) {
			if ( ec )
			{
				if ( ec == asio::error::operation_aborted )
				{
					return;
				}

				Logging::error("HTTPServer", m_options.name + ": accept failed: " + ec.message() + ".");

				/* Never re-arm blindly on a permanent failure: it would spin this thread. */
				if ( ec == asio::error::no_descriptors || ec == asio::error::no_memory )
				{
					return;
				}

				this->accept();

				return;
			}

			asio::error_code optionError;
			socket.set_option(asio::ip::tcp::no_delay{true}, optionError);

			if ( optionError )
			{
				Logging::warning("HTTPServer", m_options.name + ": unable to disable Nagle's algorithm for a client: " + optionError.message());
			}

			if ( m_connections.size() >= m_options.maxConnections )
			{
				constexpr std::string_view Refusal{"HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"};

				asio::error_code writeError;
				static_cast< void >(asio::write(socket, asio::buffer(Refusal), writeError));
				socket.close(writeError);
			}
			else
			{
				auto connection = std::make_shared< HTTPServerConnection >(*this, std::move(socket), m_nextConnectionId++);

				m_connections.insert(connection);

				connection->start();
			}

			this->accept();
		});
	}

	void
	HTTPServer::removeConnection (const std::shared_ptr< HTTPServerConnection > & connection) noexcept
	{
		if ( m_connections.erase(connection) > 0 && m_onClosed )
		{
			m_onClosed(connection->id());
		}
	}

	bool
	HTTPServer::isAcceptedHost (const std::string & host) const noexcept
	{
		/* NOTE: only a loopback binding can know its legitimate names; a non-loopback one is protected by
		 * its mandatory bearer token instead. */
		if ( !m_loopback )
		{
			return true;
		}

		const auto port = std::to_string(m_boundPort);

		return host == "127.0.0.1:" + port || host == "localhost:" + port || host == "[::1]:" + port;
	}

	bool
	HTTPServer::isAcceptedOrigin (const std::string & origin) const noexcept
	{
		const auto port = std::to_string(m_boundPort);

		if ( origin == "http://127.0.0.1:" + port || origin == "http://localhost:" + port || origin == "http://[::1]:" + port )
		{
			return true;
		}

		/* A non-loopback binding: its own address is its own origin. */
		return !m_loopback && origin == this->baseURL();
	}

	bool
	HTTPServer::isAuthorized (const std::string & authorization) const noexcept
	{
		if ( m_options.bearerToken.empty() )
		{
			return true;
		}

		const std::string expected = "Bearer " + m_options.bearerToken;

		/* Constant time over the expected length: the comparison must not leak how many characters matched. */
		unsigned char difference = authorization.size() == expected.size() ? 0 : 1;

		for ( size_t index = 0; index < expected.size(); ++index )
		{
			const auto actual = index < authorization.size() ? authorization[index] : '\0';

			difference |= static_cast< unsigned char >(actual ^ expected[index]);
		}

		return difference == 0;
	}
}
