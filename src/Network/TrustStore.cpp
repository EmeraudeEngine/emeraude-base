/*
 * src/Network/TrustStore.cpp
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

#include "TrustStore.hpp"

/* Project configuration. */
#include "emeraude_platform.hpp"

/* STL inclusions. */
#include <array>
#include <string>
#include <system_error>

/* Third-party inclusions. LibreSSL, through its OpenSSL-compatible API. */
#include <openssl/ssl.h>
#include <openssl/x509.h>

#if IS_WINDOWS
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif

	#include <Windows.h>
	#include <wincrypt.h>
#endif

/* Local inclusions. */
#include "Logging/Logging.hpp"

namespace EmEn::Base::Network::TrustStore
{
	namespace
	{
		constexpr auto Tag{"Network::TrustStore"};

#if IS_LINUX
		/* Well-known distribution CA bundle locations (the curl/Go probing practice).
		 * A custom-built LibreSSL has no usable compiled-in default, so the host
		 * distribution bundle is located explicitly. First match wins. */
		constexpr std::array< const char *, 6 > LinuxBundleCandidates{
			"/etc/ssl/certs/ca-certificates.crt",                /* Debian/Ubuntu/Gentoo/Arch */
			"/etc/pki/tls/certs/ca-bundle.crt",                  /* Fedora/RHEL 6 */
			"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem", /* RHEL 7+/CentOS */
			"/etc/ssl/ca-bundle.pem",                            /* openSUSE */
			"/etc/pki/tls/cacert.pem",                           /* OpenELEC */
			"/etc/ssl/cert.pem"                                  /* Alpine */
		};

		/* Hashed certificates directory fallback when no bundle file exists. */
		constexpr auto LinuxHashedCertsDirectory{"/etc/ssl/certs"};
#endif

#if IS_MACOS
		/* Apple-maintained bundle extracted from the system Keychain, present on
		 * every macOS. User/enterprise Keychain additions are deliberately not
		 * imported (owner-ruled over the Security.framework extraction); the
		 * applyCABundleFile() override covers those setups. */
		constexpr auto MacOSBundleFile{"/etc/ssl/cert.pem"};
#endif

#if IS_WINDOWS
		/**
		 * @brief Imports every certificate of a Windows system store into an X509 store.
		 * @note LibreSSL does not ship OpenSSL >= 3.2 'winstore' loader, so the
		 * CryptoAPI import is done by hand (Windows-only code path by design).
		 * @param store The LibreSSL X509 store receiving the certificates.
		 * @param systemStoreName The Windows system store name ("ROOT", "CA").
		 * @param importedCount The number of successfully imported certificates [in/out].
		 * @return bool False when the Windows system store could not be opened.
		 */
		bool
		importWindowsSystemStore (X509_STORE * store, const wchar_t * systemStoreName, size_t & importedCount) noexcept
		{
			HCERTSTORE systemStore = CertOpenSystemStoreW(0, systemStoreName);

			if ( systemStore == nullptr )
			{
				return false;
			}

			PCCERT_CONTEXT certificateContext = nullptr;

			while ( (certificateContext = CertEnumCertificatesInStore(systemStore, certificateContext)) != nullptr )
			{
				/* d2i_X509() advances the buffer pointer: work on a copy. */
				const unsigned char * encodedBytes = certificateContext->pbCertEncoded;

				X509 * certificate = d2i_X509(nullptr, &encodedBytes, static_cast< long >(certificateContext->cbCertEncoded));

				if ( certificate == nullptr )
				{
					/* Undecodable entry: skip it, the remaining store is still valuable. */
					continue;
				}

				/* Returns 0 on duplicates: not an error, simply not counted. */
				if ( X509_STORE_add_cert(store, certificate) == 1 )
				{
					++importedCount;
				}

				X509_free(certificate);
			}

			CertCloseStore(systemStore, 0);

			return true;
		}
#endif
	}

	bool
	applyCABundleFile (asio::ssl::context & context, const std::filesystem::path & bundleFilepath) noexcept
	{
		if ( std::error_code fileError; !std::filesystem::is_regular_file(bundleFilepath, fileError) )
		{
			Logging::error(Tag, "applyCABundleFile(), the CA bundle file '" + bundleFilepath.string() + "' does not exist !");

			return false;
		}

		asio::error_code error;

		context.load_verify_file(bundleFilepath.string(), error);

		if ( error )
		{
			Logging::error(Tag, "applyCABundleFile(), unable to load the CA bundle file '" + bundleFilepath.string() + "' : " + error.message());

			return false;
		}

		Logging::info(Tag, "CA bundle file '" + bundleFilepath.string() + "' loaded.");

		return true;
	}

	bool
	applySystemTrustStore (asio::ssl::context & context) noexcept
	{
#if IS_WINDOWS
		auto * store = SSL_CTX_get_cert_store(context.native_handle());

		if ( store == nullptr )
		{
			Logging::error(Tag, "applySystemTrustStore(), the TLS context has no certificate store !");

			return false;
		}

		size_t importedCount = 0;

		/* ROOT holds the trusted anchors; CA holds intermediates that help
		 * building chains when a server sends an incomplete one. */
		const auto rootOpened = importWindowsSystemStore(store, L"ROOT", importedCount);
		const auto caOpened = importWindowsSystemStore(store, L"CA", importedCount);

		if ( !rootOpened && !caOpened )
		{
			Logging::error(Tag, "applySystemTrustStore(), unable to open the Windows system certificate stores !");

			return false;
		}

		if ( importedCount == 0 )
		{
			Logging::error(Tag, "applySystemTrustStore(), no certificate imported from the Windows system stores !");

			return false;
		}

		Logging::info(Tag, "Windows system trust store imported (" + std::to_string(importedCount) + " certificates).");

		return true;
#elif IS_MACOS
		return applyCABundleFile(context, MacOSBundleFile);
#else
		for ( const auto * candidate : LinuxBundleCandidates )
		{
			if ( std::error_code fileError; std::filesystem::is_regular_file(candidate, fileError) )
			{
				return applyCABundleFile(context, candidate);
			}
		}

		/* No bundle file: fall back to the hashed certificates directory. */
		if ( std::error_code fileError; std::filesystem::is_directory(LinuxHashedCertsDirectory, fileError) )
		{
			asio::error_code error;

			context.add_verify_path(LinuxHashedCertsDirectory, error);

			if ( !error )
			{
				std::string message{"applySystemTrustStore(), no CA bundle file found, using the hashed directory '"};
				message += LinuxHashedCertsDirectory;
				message += "'.";

				Logging::warning(Tag, message);

				return true;
			}
		}

		Logging::error(Tag, "applySystemTrustStore(), no system CA bundle found on this host !");

		return false;
#endif
	}

	size_t
	certificateCount (asio::ssl::context & context) noexcept
	{
		auto * store = SSL_CTX_get_cert_store(context.native_handle());

		if ( store == nullptr )
		{
			return 0;
		}

		auto * objects = X509_STORE_get0_objects(store);

		if ( objects == nullptr )
		{
			return 0;
		}

		size_t count = 0;

		for ( int index = 0; index < sk_X509_OBJECT_num(objects); ++index )
		{
			const auto * object = sk_X509_OBJECT_value(objects, index);

			if ( X509_OBJECT_get_type(object) == X509_LU_X509 )
			{
				++count;
			}
		}

		return count;
	}
}