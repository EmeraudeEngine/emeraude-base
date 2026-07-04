/*
 * src/Testing/test_NetworkTrustStore.cpp
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

#include <gtest/gtest.h>

/* STL inclusions. */
#include <cstdio>
#include <filesystem>
#include <fstream>

/* Local inclusions. */
#include "Network/TrustStore.hpp"
#include "Constants.hpp"

/* Third-party inclusions. LibreSSL, used directly to prove the store content. */
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

using namespace EmEn::Base;

namespace
{
	/**
	 * @brief Loads an X509 certificate from a PEM file (test helper).
	 * @param filepath The certificate file path.
	 * @return X509 * The certificate, or nullptr. Caller frees with X509_free().
	 */
	X509 *
	loadCertificate (const std::filesystem::path & filepath) noexcept
	{
		FILE * file = std::fopen(filepath.string().c_str(), "rb");

		if ( file == nullptr )
		{
			return nullptr;
		}

		X509 * certificate = PEM_read_X509(file, nullptr, nullptr, nullptr);

		std::fclose(file);

		return certificate;
	}

	/**
	 * @brief Verifies a leaf certificate against the trust store of a TLS context (test helper).
	 * @param context The configured TLS context.
	 * @param leafFilepath The leaf certificate to verify.
	 * @return bool True when the chain verification succeeds.
	 */
	bool
	verifyLeafAgainstContext (asio::ssl::context & context, const std::filesystem::path & leafFilepath) noexcept
	{
		X509 * leaf = loadCertificate(leafFilepath);

		if ( leaf == nullptr )
		{
			return false;
		}

		X509_STORE_CTX * verifyContext = X509_STORE_CTX_new();

		if ( verifyContext == nullptr )
		{
			X509_free(leaf);

			return false;
		}

		bool verified = false;

		if ( X509_STORE_CTX_init(verifyContext, SSL_CTX_get_cert_store(context.native_handle()), leaf, nullptr) == 1 )
		{
			verified = X509_verify_cert(verifyContext) == 1;
		}

		X509_STORE_CTX_free(verifyContext);
		X509_free(leaf);

		return verified;
	}
}

TEST(NetworkTrustStore, systemTrustStoreLoads)
{
	asio::ssl::context context{asio::ssl::context::tls_client};

	ASSERT_TRUE(Network::TrustStore::applySystemTrustStore(context));

	/* A real system store holds anchors: prove certificates actually landed. */
	EXPECT_GT(Network::TrustStore::certificateCount(context), 0U);
}

TEST(NetworkTrustStore, caBundleFileLoadsFixture)
{
	asio::ssl::context context{asio::ssl::context::tls_client};

	EXPECT_EQ(Network::TrustStore::certificateCount(context), 0U);

	ASSERT_TRUE(Network::TrustStore::applyCABundleFile(context, AssetsDirectory / "tls-test-ca.pem"));

	EXPECT_EQ(Network::TrustStore::certificateCount(context), 1U);
}

TEST(NetworkTrustStore, missingBundleFileFails)
{
	asio::ssl::context context{asio::ssl::context::tls_client};

	EXPECT_FALSE(Network::TrustStore::applyCABundleFile(context, AssetsDirectory / "does-not-exist.pem"));
	EXPECT_EQ(Network::TrustStore::certificateCount(context), 0U);
}

TEST(NetworkTrustStore, malformedBundleFileFails)
{
	const auto malformedFilepath = std::filesystem::temp_directory_path() / "emeraude-base-malformed-bundle.pem";

	{
		std::ofstream malformedFile{malformedFilepath, std::ios::trunc};
		malformedFile << "this is not a PEM certificate bundle";
	}

	asio::ssl::context context{asio::ssl::context::tls_client};

	EXPECT_FALSE(Network::TrustStore::applyCABundleFile(context, malformedFilepath));
	EXPECT_EQ(Network::TrustStore::certificateCount(context), 0U);

	std::filesystem::remove(malformedFilepath);
}

/* Utility proof (the plan's "blind its utility" criterion): a leaf certificate
 * signed by the fixture authority verifies once — and only once — that
 * authority is loaded through the override API. */
TEST(NetworkTrustStore, verifiesLeafSignedByLoadedCA)
{
	asio::ssl::context context{asio::ssl::context::tls_client};

	ASSERT_TRUE(Network::TrustStore::applyCABundleFile(context, AssetsDirectory / "tls-test-ca.pem"));

	EXPECT_TRUE(verifyLeafAgainstContext(context, AssetsDirectory / "tls-test-leaf-localhost.pem"));
}

TEST(NetworkTrustStore, rejectsLeafWithoutItsCA)
{
	asio::ssl::context context{asio::ssl::context::tls_client};

	/* Empty trust store: the exact same leaf must NOT verify. */
	EXPECT_FALSE(verifyLeafAgainstContext(context, AssetsDirectory / "tls-test-leaf-localhost.pem"));
}