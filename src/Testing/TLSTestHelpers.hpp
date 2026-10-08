/*
 * src/Testing/TLSTestHelpers.hpp
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

/* Shared TLS test infrastructure: runtime-generated server credentials (no
 * private key is ever committed) and a hermetic multi-connection HTTPS test
 * server on 127.0.0.1. Test-binary only — never part of the library. */

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

/* Third-party inclusions.
 * NOTE: the no-exceptions hook MUST be included before any asio header. */
#include "Network/asio_throw_exception.hpp"
#include "asio.hpp"
#include "asio/ssl.hpp"

/* LibreSSL, used to generate the ephemeral credentials at test runtime. */
#include <openssl/bio.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace EmEn::Base::Testing
{
	/**
	 * @brief Returns the Content-Length a raw request announced, 0 when it declared none.
	 * @note Hand-rolled and case-insensitive: the header codec under test must not be the thing
	 * that decides what the test server reads.
	 * @param rawRequest The raw request text: headers, then possibly a partial body.
	 * @return size_t
	 */
	[[nodiscard]]
	inline
	size_t
	declaredContentLength (const std::string & rawRequest) noexcept
	{
		const auto headerEnd = rawRequest.find("\r\n\r\n");

		if ( headerEnd == std::string::npos )
		{
			return 0;
		}

		std::string headers{rawRequest, 0, headerEnd};

		for ( auto & character : headers )
		{
			character = static_cast< char >(std::tolower(static_cast< unsigned char >(character)));
		}

		constexpr std::string_view Needle{"\r\ncontent-length:"};

		const auto position = headers.find(Needle);

		if ( position == std::string::npos )
		{
			return 0;
		}

		size_t length = 0;
		bool digitSeen = false;

		for ( auto index = position + Needle.size(); index < headers.size(); ++index )
		{
			const auto character = headers[index];

			if ( character == ' ' || character == '\t' )
			{
				if ( digitSeen )
				{
					break;
				}

				continue;
			}

			if ( character < '0' || character > '9' )
			{
				break;
			}

			length = (length * 10) + static_cast< size_t >(character - '0');
			digitSeen = true;
		}

		return length;
	}

	/** @brief Ephemeral PEM credentials for a hermetic TLS test server. */
	struct ServerCredentials final
	{
		std::string certificatePEM;
		std::string privateKeyPEM;
		bool valid{false};
	};

	/**
	 * @brief Generates a self-signed EC P-256 certificate at runtime (test helper).
	 * @param subjectAltName The SAN extension value (controls hostname verification).
	 * @return ServerCredentials
	 */
	inline ServerCredentials
	generateServerCredentials (const char * subjectAltName) noexcept
	{
		ServerCredentials credentials;

		/* EC P-256 key pair. */
		EC_KEY * ecKey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

		if ( ecKey == nullptr || EC_KEY_generate_key(ecKey) != 1 )
		{
			EC_KEY_free(ecKey);

			return credentials;
		}

		EVP_PKEY * key = EVP_PKEY_new();

		if ( key == nullptr || EVP_PKEY_assign_EC_KEY(key, ecKey) != 1 )
		{
			EC_KEY_free(ecKey);
			EVP_PKEY_free(key);

			return credentials;
		}

		/* Self-signed X509 certificate, CN=localhost. */
		X509 * certificate = X509_new();

		if ( certificate == nullptr )
		{
			EVP_PKEY_free(key);

			return credentials;
		}

		X509_set_version(certificate, 2);
		ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1);
		X509_gmtime_adj(X509_get_notBefore(certificate), -3600);
		X509_gmtime_adj(X509_get_notAfter(certificate), 3600L * 24 * 365);
		X509_set_pubkey(certificate, key);

		auto * subject = X509_get_subject_name(certificate);
		X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, reinterpret_cast< const unsigned char * >("localhost"), -1, -1, 0);
		X509_set_issuer_name(certificate, subject);

		/* SAN extension: this is what hostname verification checks. */
		X509V3_CTX extensionContext;
		X509V3_set_ctx(&extensionContext, certificate, certificate, nullptr, nullptr, 0);

		auto * extension = X509V3_EXT_conf_nid(nullptr, &extensionContext, NID_subject_alt_name, subjectAltName);

		bool built = extension != nullptr && X509_add_ext(certificate, extension, -1) == 1;

		X509_EXTENSION_free(extension);

		built = built && X509_sign(certificate, key, EVP_sha256()) != 0;

		if ( built )
		{
			/* Serialize both to PEM through memory BIOs. */
			BIO * certificateBIO = BIO_new(BIO_s_mem());
			BIO * keyBIO = BIO_new(BIO_s_mem());

			if ( certificateBIO != nullptr && keyBIO != nullptr
				&& PEM_write_bio_X509(certificateBIO, certificate) == 1
				&& PEM_write_bio_PrivateKey(keyBIO, key, nullptr, nullptr, 0, nullptr, nullptr) == 1 )
			{
				char * bytes = nullptr;

				/* NOTE: BIO_ctrl(BIO_CTRL_INFO) is what the BIO_get_mem_data() macro expands to, without
				 * the macro's C-style cast of the out-pointer (the char ** out-pointer is passed as void * explicitly). */
				const auto certificateLength = BIO_ctrl(certificateBIO, BIO_CTRL_INFO, 0, static_cast< void * >(&bytes));
				credentials.certificatePEM.assign(bytes, static_cast< size_t >(certificateLength));

				const auto keyLength = BIO_ctrl(keyBIO, BIO_CTRL_INFO, 0, static_cast< void * >(&bytes));
				credentials.privateKeyPEM.assign(bytes, static_cast< size_t >(keyLength));

				credentials.valid = !credentials.certificatePEM.empty() && !credentials.privateKeyPEM.empty();
			}

			BIO_free(certificateBIO);
			BIO_free(keyBIO);
		}

		X509_free(certificate);
		EVP_PKEY_free(key);

		return credentials;
	}

	/**
	 * @brief A hermetic HTTPS/1.1 test server on 127.0.0.1 (ephemeral port).
	 * @note Serves sequential connections until destroyed. For each connection it
	 * reads one request (until the header terminator, bounded), hands the raw text
	 * to the handler, writes back whatever the handler returns, then closes — which
	 * matches the client's one-connection-per-hop, Connection-close behavior.
	 * @note Server-side asio calls use ONLY the error_code overloads: a throwing
	 * overload would abort the process under ASIO_NO_EXCEPTIONS.
	 */
	class HTTPSTestServer final
	{
		public:

			using RequestHandler = std::function< std::string (const std::string & rawRequest) >;

			/**
			 * @brief Makes every session end by dropping the TCP connection WITHOUT a TLS
			 * close_notify — the truncation-attack signature a client must not accept as a
			 * clean end of stream.
			 */
			void
			setAbortWithoutCloseNotify (bool state) noexcept
			{
				m_abortWithoutCloseNotify = state;
			}

			/**
			 * @brief Constructs and starts the server.
			 * @param credentials The PEM credentials (see generateServerCredentials()).
			 * @param handler Builds the raw response bytes for a raw request [std::move].
			 * @param proxyMode Answer an HTTP CONNECT first (plaintext), then act as the tunnelled target. Default false.
			 * @param listenOnIPv6 Listen on ::1 too, same port. Default true.
			 */
			HTTPSTestServer (const ServerCredentials & credentials, RequestHandler handler, bool proxyMode = false, bool listenOnIPv6 = true) noexcept
				: m_handler(std::move(handler)),
				m_proxyMode(proxyMode)
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
					m_acceptor.listen(4, error);
				}

				if ( error )
				{
					return;
				}

				m_port = m_acceptor.local_endpoint(error).port();

				/* Dual stack: the same port on ::1 too. A client resolving "localhost" tries ::1 first on Windows, and a
				 * refused connect costs ~2 s there (the SYN is retried after the RST) — every hermetic HTTPS test paid it
				 * (Windows peer, 2026-10-07). Best effort: without IPv6 the server stays IPv4-only, as before.
				 * `listenOnIPv6 = false` keeps it IPv4-only on purpose (the client's Happy Eyeballs test). */
				if ( listenOnIPv6 )
				{
					asio::error_code ipv6Error;
					const auto loopback6 = asio::ip::make_address("::1", ipv6Error);

					if ( !ipv6Error )
					{
						m_acceptor6.open(asio::ip::tcp::v6(), ipv6Error);
					}

					if ( !ipv6Error )
					{
						m_acceptor6.set_option(asio::ip::v6_only{true}, ipv6Error);
					}

					if ( !ipv6Error )
					{
						m_acceptor6.bind(asio::ip::tcp::endpoint{loopback6, m_port}, ipv6Error);
					}

					if ( !ipv6Error )
					{
						m_acceptor6.listen(4, ipv6Error);
					}

					if ( ipv6Error && m_acceptor6.is_open() )
					{
						asio::error_code ignored;
						m_acceptor6.close(ignored);
					}
				}

				/* Async accept chain so the destructor can cancel cleanly: a
				 * blocking synchronous accept() on a worker thread cannot be woken
				 * reliably by closing the acceptor (the thread would never join). */
				this->scheduleAccept(m_acceptor);

				if ( m_acceptor6.is_open() )
				{
					this->scheduleAccept(m_acceptor6);
				}

				m_thread = std::thread{[this] () {
					m_ioContext.run();
				}};
			}

			~HTTPSTestServer ()
			{
				/* Stop the io_context from this thread (thread-safe) so run()
				 * returns, then join. */
				m_ioContext.stop();

				if ( m_thread.joinable() )
				{
					m_thread.join();
				}

				/* Keep-alive connections: their blocking reads end when their socket is shut down. */
				{
					const std::scoped_lock lock{m_keepAliveAccess};

					for ( const auto & stream : m_keepAliveStreams )
					{
						asio::error_code ignored;
						stream->lowest_layer().shutdown(asio::socket_base::shutdown_both, ignored);
					}
				}

				for ( auto & connectionThread : m_keepAliveThreads )
				{
					if ( connectionThread.joinable() )
					{
						connectionThread.join();
					}
				}
			}

			/**
			 * @brief Serves several requests per connection (keep-alive), each connection on its own thread, until the
			 * client closes it or a response carries 'Connection: close'. For HTTPSClient's connection reuse.
			 * @param state The state.
			 * @param closeAfterEachResponse When true, the server still CLOSES each connection after one response,
			 * WITHOUT announcing it — a server that dropped an idle connection, as seen by a pooled client.
			 */
			void
			setKeepAlive (bool state, bool closeAfterEachResponse = false) noexcept
			{
				m_keepAlive = state;
				m_closeAfterEachResponse = closeAfterEachResponse;
			}

			/**
			 * @brief Keep-alive mode: whether the server answers the client's close_notify with its own (real servers do,
			 * or drop the TCP connection). Off = a peer that keeps the connection open and silent.
			 * @param state The state. Default on.
			 */
			void
			setAnswerCloseNotify (bool state) noexcept
			{
				m_answerCloseNotify = state;
			}

			/** @brief Returns the number of TLS connections accepted (handshake done). */
			[[nodiscard]]
			size_t
			connectionCount () const noexcept
			{
				return m_connectionCount.load();
			}

			HTTPSTestServer (const HTTPSTestServer & copy) noexcept = delete;
			HTTPSTestServer (HTTPSTestServer && move) noexcept = delete;
			HTTPSTestServer & operator= (const HTTPSTestServer & copy) noexcept = delete;
			HTTPSTestServer & operator= (HTTPSTestServer && move) noexcept = delete;

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

			/** @brief Returns the number of requests fully served. */
			[[nodiscard]]
			size_t
			requestCount () const noexcept
			{
				return m_requestCount.load();
			}

			/** @brief Returns the number of CONNECT tunnels accepted (proxy mode). */
			[[nodiscard]]
			size_t
			tunnelCount () const noexcept
			{
				return m_tunnelCount.load();
			}

		private:

			/**
			 * @brief Queues an asynchronous accept; each accepted connection is served
			 * synchronously in the handler, then the next accept is queued.
			 */
			void
			scheduleAccept (asio::ip::tcp::acceptor & acceptor) noexcept
			{
				acceptor.async_accept([this, &acceptor] (const asio::error_code & acceptError, asio::ip::tcp::socket socket) {
					if ( acceptError )
					{
						/* Acceptor closed / io_context stopped: end the chain. */
						return;
					}

					if ( m_keepAlive )
					{
						const std::scoped_lock lock{m_keepAliveAccess};

						auto stream = std::make_shared< asio::ssl::stream< asio::ip::tcp::socket > >(std::move(socket), m_serverContext);

						m_keepAliveStreams.push_back(stream);
						m_keepAliveThreads.emplace_back([this, stream] () {
							this->serveKeepAliveConnection(*stream);
						});
					}
					else
					{
						this->serveConnection(std::move(socket));
					}

					this->scheduleAccept(acceptor);
				});
			}

			/**
			 * @brief Serves one accepted connection over TLS (synchronous, sequential).
			 * @param socket The accepted TCP socket [std::move].
			 */
			void
			serveConnection (asio::ip::tcp::socket socket) noexcept
			{
				asio::error_code error;

				/* Proxy mode: first the PLAINTEXT CONNECT dance on the raw socket,
				 * then behave as the tunnelled target (we present the target cert). */
				if ( m_proxyMode )
				{
					std::string connectRequest;

					while ( connectRequest.find("\r\n\r\n") == std::string::npos && connectRequest.size() < MaxRequestSize )
					{
						std::array< char, 512 > buffer{};

						const auto bytesRead = socket.read_some(asio::buffer(buffer), error);

						if ( error )
						{
							break;
						}

						connectRequest.append(buffer.data(), bytesRead);
					}

					if ( error || connectRequest.rfind("CONNECT ", 0) != 0 )
					{
						return;
					}

					const std::string established{"HTTP/1.1 200 Connection established\r\n\r\n"};

					asio::write(socket, asio::buffer(established), error);

					if ( error )
					{
						return;
					}

					++m_tunnelCount;
				}

				asio::ssl::stream< asio::ip::tcp::socket > stream{std::move(socket), m_serverContext};

				stream.handshake(asio::ssl::stream_base::server, error);

				if ( error )
				{
					/* Expected when a test client rejects our certificate. */
					return;
				}

				++m_connectionCount;

				/* Read one request, up to the header terminator (bounded). */
				std::string request;

				while ( request.find("\r\n\r\n") == std::string::npos && request.size() < MaxRequestSize )
				{
					std::array< char, 2048 > buffer{};

					const auto bytesRead = stream.read_some(asio::buffer(buffer), error);

					if ( error )
					{
						break;
					}

					request.append(buffer.data(), bytesRead);
				}

				if ( error || request.find("\r\n\r\n") == std::string::npos )
				{
					return;
				}

				/* ⚠️ The loop above stops at the header terminator, so a request BODY is only
				 * whatever happened to share the last TLS record — a test asserting on it would
				 * pass or fail by timing. Read out what Content-Length announced before handing
				 * the request to the handler. */
				if ( const auto announced = declaredContentLength(request); announced > 0 )
				{
					const auto bodyStart = request.find("\r\n\r\n") + 4;

					while ( request.size() - bodyStart < announced && request.size() < MaxRequestSize )
					{
						std::array< char, 2048 > buffer{};

						const auto bytesRead = stream.read_some(asio::buffer(buffer), error);

						if ( error )
						{
							break;
						}

						request.append(buffer.data(), bytesRead);
					}

					if ( error )
					{
						return;
					}
				}

				const auto response = m_handler(request);

				/* Count the served request BEFORE writing: the client can read the
				 * body and tear down before a post-write increment becomes visible
				 * to the main thread, so a post-write count races low. */
				++m_requestCount;

				asio::write(stream, asio::buffer(response), error);

				if ( m_abortWithoutCloseNotify )
				{
					/* Hard reset: no close_notify, no FIN — exactly what a truncation attack (or a
					 * crashing origin) looks like on the wire. */
					asio::error_code ignored;
					stream.lowest_layer().set_option(asio::socket_base::linger{true, 0}, ignored);
					stream.lowest_layer().close(ignored);

					return;
				}

				stream.shutdown(error);
			}

			/**
			 * @brief Serves a keep-alive connection: request after request until the client closes, an error, or a
			 * 'Connection: close' response.
			 * @param stream The connection.
			 */
			void
			serveKeepAliveConnection (asio::ssl::stream< asio::ip::tcp::socket > & stream) noexcept
			{
				asio::error_code error;

				stream.handshake(asio::ssl::stream_base::server, error);

				if ( error )
				{
					return;
				}

				++m_connectionCount;

				std::string pending;

				while ( true )
				{
					while ( pending.find("\r\n\r\n") == std::string::npos && pending.size() < MaxRequestSize )
					{
						std::array< char, 2048 > buffer{};

						const auto bytesRead = stream.read_some(asio::buffer(buffer), error);

						if ( error )
						{
							/* The client closed (or the server is shutting down). Its close_notify is answered, unless the
							 * test wants a silent peer. */
							if ( error == asio::error::eof && m_answerCloseNotify )
							{
								asio::error_code ignored;
								stream.shutdown(ignored);
							}
							else if ( error == asio::error::eof )
							{
								/* Silent: hold the connection until the server is destroyed. */
								std::array< char, 64 > sink{};
								asio::error_code ignored;

								static_cast< void >(stream.next_layer().read_some(asio::buffer(sink), ignored));
							}

							return;
						}

						pending.append(buffer.data(), bytesRead);
					}

					const auto headerEnd = pending.find("\r\n\r\n");

					if ( headerEnd == std::string::npos )
					{
						return;
					}

					size_t requestEnd = headerEnd + 4 + declaredContentLength(pending.substr(0, headerEnd + 4));

					while ( pending.size() < requestEnd && pending.size() < MaxRequestSize )
					{
						std::array< char, 2048 > buffer{};

						const auto bytesRead = stream.read_some(asio::buffer(buffer), error);

						if ( error )
						{
							return;
						}

						pending.append(buffer.data(), bytesRead);
					}

					requestEnd = std::min(requestEnd, pending.size());

					const auto request = pending.substr(0, requestEnd);
					pending.erase(0, requestEnd);

					const auto response = m_handler(request);

					++m_requestCount;

					asio::write(stream, asio::buffer(response), error);

					if ( error || m_closeAfterEachResponse || response.find("Connection: close") != std::string::npos )
					{
						stream.shutdown(error);

						return;
					}
				}
			}

			static constexpr size_t MaxRequestSize{16384};

			asio::io_context m_ioContext;
			asio::ssl::context m_serverContext{asio::ssl::context::tls_server};
			asio::ip::tcp::acceptor m_acceptor{m_ioContext};
			asio::ip::tcp::acceptor m_acceptor6{m_ioContext};
			RequestHandler m_handler;
			std::thread m_thread;
			std::atomic< size_t > m_requestCount{0};
			std::atomic< size_t > m_tunnelCount{0};
			uint16_t m_port{0};
			bool m_proxyMode{false};
			std::atomic< bool > m_abortWithoutCloseNotify{false};
			std::atomic< size_t > m_connectionCount{0};
			std::mutex m_keepAliveAccess;
			std::vector< std::shared_ptr< asio::ssl::stream< asio::ip::tcp::socket > > > m_keepAliveStreams;
			std::vector< std::thread > m_keepAliveThreads;
			std::atomic< bool > m_keepAlive{false};
			std::atomic< bool > m_closeAfterEachResponse{false};
			std::atomic< bool > m_answerCloseNotify{true};
	};
}
