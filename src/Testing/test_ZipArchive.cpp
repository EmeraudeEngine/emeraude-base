/*
 * src/Testing/test_ZipArchive.cpp
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
 */

#include <gtest/gtest.h>

/* STL inclusions. */
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

/* Local inclusions. */
#include "IO/ZipReader.hpp"
#include "IO/ZipWriter.hpp"

/* Ave robustus! (A.4 — RAII): ZipReader/ZipWriter own their libzip handle through a
 * std::unique_ptr<zip_t, decltype(&zip_close)>. These tests exercise a full write→read
 * round-trip and, critically, the abandon-without-close path: under ASan/UBSan the
 * destructor-driven release proves the owning handle never leaks. */

using namespace EmEn::Base::IO;

namespace
{
	std::filesystem::path
	freshTempDir (const char * leaf) noexcept
	{
		std::error_code errorCode;
		auto dir = std::filesystem::temp_directory_path(errorCode) / leaf;
		std::filesystem::remove_all(dir, errorCode);
		std::filesystem::create_directories(dir, errorCode);

		return dir;
	}

	/**
	 * @brief Removes a test directory and CHECKS it is gone. Windows cannot delete a file a handle still holds: a
	 * ZipReader alive at that point left its directory in %TEMP% silently (Windows peer, 2026-10-08). Every reader is
	 * scoped so it is destroyed first.
	 */
	void
	expectTempDirRemoved (const std::filesystem::path & dir)
	{
		std::error_code errorCode;

		std::filesystem::remove_all(dir, errorCode);

		EXPECT_FALSE(errorCode) << "cannot remove " << dir << ": " << errorCode.message();
		EXPECT_FALSE(std::filesystem::exists(dir, errorCode)) << dir << " is still there";
	}

	void
	writeFile (const std::filesystem::path & path, const std::string & payload) noexcept
	{
		std::ofstream out{path, std::ios::binary | std::ios::trunc};
		out.write(payload.data(), static_cast< std::streamsize >(payload.size()));
	}
}

TEST(ZipArchive, roundTripWriteThenRead)
{
	const auto dir = freshTempDir("emeraude_zip_roundtrip");
	const auto source = dir / "hello.txt";
	const std::string payload = "Ave robustus! Zip round-trip payload.";

	writeFile(source, payload);

	const auto archive = dir / "archive.zip";

	{
		ZipWriter writer{archive};
		ASSERT_TRUE(writer.addFilepathToSources(source, "hello.txt"));
		ASSERT_TRUE(writer.create());
	}

	ASSERT_TRUE(std::filesystem::exists(archive));
	EXPECT_TRUE(ZipReader::isArchiveFile(archive));

	{
		ZipReader reader{archive};
		ASSERT_TRUE(reader.open());
		ASSERT_EQ(reader.entries().size(), 1U);
		EXPECT_EQ(reader.entries().front(), "hello.txt");

		std::vector< char > buffer;
		ASSERT_TRUE(reader.extract("hello.txt", buffer));
		EXPECT_EQ(std::string(buffer.begin(), buffer.end()), payload);
	}

	expectTempDirRemoved(dir);
}

TEST(ZipArchive, addDirectoryToSourcesCreatesArchive)
{
	const auto dir = freshTempDir("emeraude_zip_directory");

	/* A small directory tree to zip, with a nested sub-directory to exercise the
	 * recursive walk and the relative-path entry naming. */
	const auto sourceRoot = dir / "to_zip";
	std::error_code errorCode;
	std::filesystem::create_directories(sourceRoot / "nested", errorCode);

	const std::string rootPayload = "Root file payload.";
	const std::string nestedPayload = "Nested file payload.";

	writeFile(sourceRoot / "root.txt", rootPayload);
	writeFile(sourceRoot / "nested" / "deep.txt", nestedPayload);

	const auto archive = dir / "archive.zip";

	{
		ZipWriter writer{archive};
		ASSERT_TRUE(writer.addDirectoryToSources(sourceRoot));
		ASSERT_TRUE(writer.create());
	}

	/* Primary goal: the archive file must have been created on disk. */
	ASSERT_TRUE(std::filesystem::exists(archive));
	EXPECT_GT(std::filesystem::file_size(archive, errorCode), 0U);
	EXPECT_TRUE(ZipReader::isArchiveFile(archive));

	/* Entries are named relative to the directory, with generic '/' separators,
	 * and the nested file keeps its sub-path. */
	{
		ZipReader reader{archive};
		ASSERT_TRUE(reader.open());

		const auto & entries = reader.entries();
		ASSERT_EQ(entries.size(), 2U);
		EXPECT_NE(std::ranges::find(entries, "root.txt"), entries.end());
		EXPECT_NE(std::ranges::find(entries, "nested/deep.txt"), entries.end());

		std::vector< char > buffer;
		ASSERT_TRUE(reader.extract("root.txt", buffer));
		EXPECT_EQ(std::string(buffer.begin(), buffer.end()), rootPayload);

		ASSERT_TRUE(reader.extract("nested/deep.txt", buffer));
		EXPECT_EQ(std::string(buffer.begin(), buffer.end()), nestedPayload);
	}

	expectTempDirRemoved(dir);
}

TEST(ZipArchive, readerDestructorReleasesHandleWithoutExplicitClose)
{
	const auto dir = freshTempDir("emeraude_zip_dtor");
	const auto source = dir / "data.bin";

	writeFile(source, "payload-bytes");

	const auto archive = dir / "archive.zip";

	{
		ZipWriter writer{archive};
		ASSERT_TRUE(writer.addFilepathToSources(source, "data.bin"));
		ASSERT_TRUE(writer.create());
	}

	{
		ZipReader reader{archive};
		ASSERT_TRUE(reader.open());
		EXPECT_TRUE(reader.isOpen());
		/* No reader.close() — the unique_ptr deleter must release the handle at scope exit.
		 * Leak-checked under ASan. */
	}

	expectTempDirRemoved(dir);
}

TEST(ZipArchive, writerAbandonedAfterAddDoesNotLeak)
{
	const auto dir = freshTempDir("emeraude_zip_writer_dtor");
	const auto source = dir / "entry.txt";

	writeFile(source, "entry-content");

	{
		ZipWriter writer{dir / "never-created.zip"};
		ASSERT_TRUE(writer.addFilepathToSources(source, "entry.txt"));
		/* Intentionally never call create(): the writer never opens a handle here, but the
		 * destructor path must stay clean (no dangling owning pointer). */
	}

	expectTempDirRemoved(dir);
}
TEST(ZipArchive, sourcesRefuseTheWrongKindOfPath)
{
	const auto dir = freshTempDir("emeraude_zip_kinds");
	writeFile(dir / "file.txt", "payload");

	{
		ZipWriter writer{dir / "archive.zip"};

		/* A directory is not a file, a file is not a directory ("&&" used to accept both), a missing path is neither. */
		EXPECT_FALSE(writer.addFilepathToSources(dir, "dir"));
		EXPECT_FALSE(writer.addDirectoryToSources(dir / "file.txt"));
		EXPECT_FALSE(writer.addFilepathToSources(dir / "missing.txt", "missing.txt"));
		EXPECT_FALSE(writer.addDirectoryToSources(dir / "missing"));
		EXPECT_TRUE(writer.addFilepathToSources(dir / "file.txt", "file.txt"));
	}

	expectTempDirRemoved(dir);
}

TEST(ZipArchive, extractionRefusesEntriesThatLeaveTheDestination)
{
	const auto dir = freshTempDir("emeraude_zip_slip");
	writeFile(dir / "payload.txt", "evil");

	const auto archive = dir / "slip.zip";

	{
		/* "Zip Slip": entry names written by whoever built the archive. */
		ZipWriter writer{archive};
		ASSERT_TRUE(writer.addFilepathToSources(dir / "payload.txt", "../escaped.txt"));
		ASSERT_TRUE(writer.create());
	}

	const auto destination = dir / "out";

	{
		ZipReader reader{archive};
		ASSERT_TRUE(reader.open());
		ASSERT_EQ(reader.entries().size(), 1U);

		EXPECT_FALSE(reader.extract("../escaped.txt", destination));
		EXPECT_FALSE(reader.extractAll(destination));
	}

	std::error_code errorCode;
	EXPECT_FALSE(std::filesystem::exists(dir / "escaped.txt", errorCode));

	expectTempDirRemoved(dir);
}
