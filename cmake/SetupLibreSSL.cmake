if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling LibreSSL library (libtls + libssl + libcrypto) ...")

# LibreSSL provides the OpenSSL-compatible API consumed through asio::ssl (see
# docs/plans/network-tls/README.md for the provider decision and the CA trust strategy).
# Static link order matters: consumers before providers (tls -> ssl -> crypto).
if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
		"${EMERAUDE_EXT_LIBS_PATH}/lib/tls.lib"
		"${EMERAUDE_EXT_LIBS_PATH}/lib/ssl.lib"
		"${EMERAUDE_EXT_LIBS_PATH}/lib/crypto.lib"
	)

	# LibreSSL static archives carry no MSVC autolink pragmas — the system libs must be
	# explicit: sockets (ws2_32), BCryptGenRandom for getentropy (bcrypt), and the
	# CryptoAPI used by the Network Windows trust-store import (crypt32).
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ws2_32 bcrypt crypt32)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
		"${EMERAUDE_EXT_LIBS_PATH}/lib/libtls.a"
		"${EMERAUDE_EXT_LIBS_PATH}/lib/libssl.a"
		"${EMERAUDE_EXT_LIBS_PATH}/lib/libcrypto.a"
	)
endif ()