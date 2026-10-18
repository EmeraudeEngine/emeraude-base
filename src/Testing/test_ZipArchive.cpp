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
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

/* Third-party inclusions. */
#include <zlib.h>

/* Local inclusions. */
#include "IO/IO.hpp"
#include "TemporaryPath.hpp"
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
		auto dir = EmEn::Base::Testing::uniqueTemporaryPath(leaf);
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

		EXPECT_FALSE(errorCode) << "cannot remove " << EmEn::Base::IO::toU8String(dir) << ": " << errorCode.message();
		EXPECT_FALSE(std::filesystem::exists(dir, errorCode)) << EmEn::Base::IO::toU8String(dir) << " is still there";
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

/* The coverage pass (2026-10-17): archives built byte by byte, for the cases a ZipWriter never produces — an empty entry
 * and a directory entry, an unknown compression method, an encrypted entry, corrupted data, a declared size the payload
 * cannot back. */
namespace
{
	/* One stored entry of a hand-built archive; the overrides forge what a ZipWriter never writes. */
	struct ZipEntrySpec
	{
		std::string name;
		std::string data;
		uint16_t method{0};
		uint16_t flags{0};
		bool corruptCRC{false};
		uint64_t declaredSize{0}; /* 0 = the data size; otherwise written in a Zip64 extra field of the central directory. */
	};

	void
	putU16 (std::string & out, uint32_t value)
	{
		out.push_back(static_cast< char >(value & 0xFFU));
		out.push_back(static_cast< char >((value >> 8U) & 0xFFU));
	}

	void
	putU32 (std::string & out, uint32_t value)
	{
		putU16(out, value & 0xFFFFU);
		putU16(out, value >> 16U);
	}

	void
	putU64 (std::string & out, uint64_t value)
	{
		putU32(out, static_cast< uint32_t >(value & 0xFFFFFFFFU));
		putU32(out, static_cast< uint32_t >(value >> 32U));
	}

	/* A ZIP archive (local headers, data, central directory, end record) written to a file. */
	void
	writeZip (const std::filesystem::path & path, const std::vector< ZipEntrySpec > & entries)
	{
		std::string archive;
		std::string central;

		for ( const auto & entry : entries )
		{
			const std::vector< Bytef > bytes(entry.data.cbegin(), entry.data.cend());
			auto crc = static_cast< uint32_t >(::crc32(0, bytes.data(), static_cast< uInt >(bytes.size())));

			if ( entry.corruptCRC )
			{
				crc ^= 0xFFFFFFFFU;
			}

			const auto size = static_cast< uint32_t >(entry.data.size());
			const auto offset = static_cast< uint32_t >(archive.size());

			putU32(archive, 0x04034B50U);
			putU16(archive, 20);
			putU16(archive, entry.flags);
			putU16(archive, entry.method);
			putU16(archive, 0);
			putU16(archive, 0x21);
			putU32(archive, crc);
			putU32(archive, size);
			putU32(archive, size);
			putU16(archive, static_cast< uint32_t >(entry.name.size()));
			putU16(archive, 0);
			archive += entry.name;
			archive += entry.data;

			const bool zip64 = entry.declaredSize != 0;

			putU32(central, 0x02014B50U);
			putU16(central, zip64 ? 45 : 20);
			putU16(central, zip64 ? 45 : 20);
			putU16(central, entry.flags);
			putU16(central, entry.method);
			putU16(central, 0);
			putU16(central, 0x21);
			putU32(central, crc);
			putU32(central, size);
			putU32(central, zip64 ? 0xFFFFFFFFU : size);
			putU16(central, static_cast< uint32_t >(entry.name.size()));
			putU16(central, zip64 ? 12 : 0);
			putU16(central, 0);
			putU16(central, 0);
			putU16(central, 0);
			putU32(central, entry.name.ends_with('/') ? 0x10U : 0U);
			putU32(central, offset);
			central += entry.name;

			if ( zip64 )
			{
				putU16(central, 0x0001);
				putU16(central, 8);
				putU64(central, entry.declaredSize);
			}
		}

		const auto centralOffset = static_cast< uint32_t >(archive.size());

		archive += central;
		putU32(archive, 0x06054B50U);
		putU16(archive, 0);
		putU16(archive, 0);
		putU16(archive, static_cast< uint32_t >(entries.size()));
		putU16(archive, static_cast< uint32_t >(entries.size()));
		putU32(archive, static_cast< uint32_t >(central.size()));
		putU32(archive, centralOffset);
		putU16(archive, 0);

		writeFile(path, archive);
	}
}

TEST(ZipArchive, emptyEntriesAndDirectoryEntriesExtract)
{
	const auto dir = freshTempDir("emeraude_zip_empty");
	const auto archive = dir / "empty.zip";

	writeZip(archive, {{.name = "empty.txt", .data = ""}, {.name = "folder/", .data = ""}, {.name = "folder/a.txt", .data = "abc"}});

	{
		ZipReader reader{archive};
		ASSERT_TRUE(reader.open());
		ASSERT_EQ(reader.entries().size(), 3U);

		std::vector< char > buffer{'x'};
		EXPECT_TRUE(reader.extract("empty.txt", buffer));
		EXPECT_TRUE(buffer.empty());

		const auto destination = dir / "out";
		EXPECT_TRUE(reader.extractAll(destination));

		std::error_code errorCode;
		EXPECT_TRUE(std::filesystem::is_directory(destination / "folder", errorCode));
		EXPECT_EQ(std::filesystem::file_size(destination / "empty.txt", errorCode), 0U);
		EXPECT_EQ(std::filesystem::file_size(destination / "folder" / "a.txt", errorCode), 3U);
	}

	expectTempDirRemoved(dir);
}

TEST(ZipArchive, entriesThatCannotBeReadAreRefused)
{
	const auto dir = freshTempDir("emeraude_zip_unreadable");
	const auto archive = dir / "unreadable.zip";

	writeZip(archive, {
		{.name = "unknown-method.bin", .data = "payload", .method = 17},
		{.name = "encrypted.bin", .data = "payload", .flags = 1},
		{.name = "corrupted.bin", .data = "payload", .corruptCRC = true},
		{.name = "good.txt", .data = "fine"}
	});

	{
		ZipReader reader{archive};
		ASSERT_TRUE(reader.open());

		std::vector< char > buffer;
		EXPECT_FALSE(reader.extract("unknown-method.bin", buffer));
		EXPECT_FALSE(reader.extract("encrypted.bin", buffer));
		EXPECT_FALSE(reader.extract("corrupted.bin", buffer));

		/* A failed extraction leaves no file behind. */
		EXPECT_FALSE(reader.extract("corrupted.bin", dir / "out"));
		std::error_code errorCode;
		EXPECT_FALSE(std::filesystem::exists(dir / "out" / "corrupted.bin", errorCode));

		ASSERT_TRUE(reader.extract("good.txt", buffer));
		EXPECT_EQ(std::string(buffer.begin(), buffer.end()), "fine");
	}

	expectTempDirRemoved(dir);
}

TEST(ZipArchive, declaredSizeThePayloadCannotBackIsRefused)
{
	/* The central directory declares 1 TiB for 7 stored bytes: the buffer used to be resized to the declared size
	 * (std::bad_alloc -> abort under -fno-exceptions). */
	const auto dir = freshTempDir("emeraude_zip_declared_size");
	const auto archive = dir / "bomb.zip";

	writeZip(archive, {{.name = "bomb.bin", .data = "payload", .declaredSize = uint64_t{1} << 40U}});

	{
		ZipReader reader{archive};
		ASSERT_TRUE(reader.open());

		std::vector< char > buffer;
		EXPECT_FALSE(reader.extract("bomb.bin", buffer));
	}

	expectTempDirRemoved(dir);
}

TEST(ZipArchive, reopeningListsEachEntryOnceAndAnEmptyArchiveOpens)
{
	/* A second open() read the entry list again on top of itself; a valid empty archive answered false while staying
	 * open (owner decision: it opens). */
	const auto dir = freshTempDir("emeraude_zip_reopen");
	const auto archive = dir / "one.zip";
	const auto empty = dir / "empty.zip";

	writeZip(archive, {{.name = "a.txt", .data = "hello"}});
	writeZip(empty, {});

	{
		ZipReader reader{archive};
		ASSERT_TRUE(reader.open());
		ASSERT_TRUE(reader.open());
		EXPECT_EQ(reader.entries().size(), 1U);

		ZipReader emptyReader{empty};
		EXPECT_TRUE(ZipReader::isArchiveFile(empty));
		EXPECT_TRUE(emptyReader.open());
		EXPECT_TRUE(emptyReader.isOpen());
		EXPECT_TRUE(emptyReader.entries().empty());
		EXPECT_TRUE(emptyReader.extractAll(dir / "out", false));
	}

	expectTempDirRemoved(dir);
}

