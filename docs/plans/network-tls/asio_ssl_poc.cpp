/*
 * PoC — does asio::ssl over system OpenSSL compile + link under the base's
 * regime (-fno-exceptions, ASIO_NO_EXCEPTIONS)? Mirrors emeraude-base's
 * Network include ordering: the no-exceptions hook is included BEFORE asio.
 */

#include "asio_throw_exception.hpp"   /* defines ASIO_* + the throw_exception hook */
#include "asio.hpp"
#include "asio/ssl.hpp"

#include <openssl/crypto.h>

#include <iostream>

int
main (int argc, char ** /*argv*/)
{
	asio::io_context io;

	/* 1) A TLS client context backed by OpenSSL — this alone forces the
	 *    libssl/libcrypto init path to be instantiated and linked. */
	asio::ssl::context ctx{asio::ssl::context::tls_client};
	ctx.set_default_verify_paths();
	ctx.set_verify_mode(asio::ssl::verify_peer);

	/* 2) The HTTPS building block: an SSL stream over a TCP socket. */
	asio::ssl::stream< asio::ip::tcp::socket > stream{io, ctx};

	/* 3) Instantiate the no-throw (error_code) handshake codepath under
	 *    ASIO_NO_EXCEPTIONS. Guarded so the PoC links without real I/O. */
	if ( argc < 0 )
	{
		asio::error_code ec;
		stream.handshake(asio::ssl::stream_base::client, ec);
		std::cout << ec.message() << '\n';
	}

	std::cout << "asio::ssl + OpenSSL PoC: compiled and linked under -fno-exceptions.\n";
	std::cout << "OpenSSL: " << OpenSSL_version(OPENSSL_VERSION) << '\n';

	return 0;
}
