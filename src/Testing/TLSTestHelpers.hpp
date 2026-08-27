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
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

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

				const auto certificateLength = BIO_get_mem_data(certificateBIO, &bytes);
				credentials.certificatePEM.assign(bytes, static_cast< size_t >(certificateLength));

				const auto keyLength = BIO_get_mem_data(keyBIO, &bytes);
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
			 * @brief Constructs and starts the server.
			 * @param credentials The PEM credentials (see generateServerCredentials()).
			 * @param handler Builds the raw response bytes for a raw request [std::move].
			 */
			/**
			 * @brief Makes every session end by dropping the TCP connection WITHOUT a TLS
			 * close_notify — the truncation-attack signature a client must not accept as a
			 * clean end of stream.
			 * @return void
			 */
			void
			setAbortWithoutCloseNotify (bool state) noexcept
			{
				m_abortWithoutCloseNotify = state;
			}

			HTTPSTestServer (const ServerCredentials & credentials, RequestHandler handler, bool proxyMode = false) noexcept
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

				/* Async accept chain so the destructor can cancel cleanly: a
				 * blocking synchronous accept() on a worker thread cannot be woken
				 * reliably by closing the acceptor (the thread would never join). */
				this->scheduleAccept();

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
			 * @return void
			 */
			void
			scheduleAccept () noexcept
			{
				m_acceptor.async_accept([this] (const asio::error_code & acceptError, asio::ip::tcp::socket socket) {
					if ( acceptError )
					{
						/* Acceptor closed / io_context stopped: end the chain. */
						return;
					}

					this->serveConnection(std::move(socket));

					this->scheduleAccept();
				});
			}

			/**
			 * @brief Serves one accepted connection over TLS (synchronous, sequential).
			 * @param socket The accepted TCP socket [std::move].
			 * @return void
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

			static constexpr size_t MaxRequestSize{16384};

			asio::io_context m_ioContext;
			asio::ssl::context m_serverContext{asio::ssl::context::tls_server};
			asio::ip::tcp::acceptor m_acceptor{m_ioContext};
			RequestHandler m_handler;
			std::thread m_thread;
			std::atomic< size_t > m_requestCount{0};
			std::atomic< size_t > m_tunnelCount{0};
			uint16_t m_port{0};
			bool m_proxyMode{false};
			std::atomic< bool > m_abortWithoutCloseNotify{false};
	};
}