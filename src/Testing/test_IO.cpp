/*
 * src/Testing/test_IO.cpp
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

/* Project configuration. */
#include "emeraude_platform.hpp"

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <stop_token>
#include <system_error>
#include <thread>
#include <vector>

/* Third-party inclusions. */
#if IS_LINUX || IS_MACOS
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

/* Local inclusions. */
#include "TemporaryPath.hpp"
#include "Animation/MD5AnimParser.hpp"
#include "FastJSON.hpp"
#include "INIParser.hpp"
#include "IO/FileStream.hpp"
#include "IO/IO.hpp"
#include "IO/MemoryStream.hpp"
#include "IO/ZipReader.hpp"
#include "IO/ZipWriter.hpp"
#include "Logging/Logging.hpp"
#include "Logging/Severity.hpp"
#include "PixelFactory/FileIO.hpp"
#include "VertexFactory/FileIO.hpp"
#include "VertexFactory/ShapeGenerator.hpp"
#include "WaveFactory/FileIO.hpp"

namespace EmEn::Base::IO
{
	/* ===== Nominal behaviour ===== */

	TEST(IOMemoryStream, writeThenReadRoundTrip)
	{
		const std::array< uint8_t, 4 > source{0x11, 0x22, 0x33, 0x44};

		std::vector< std::byte > buffer;
		MemoryStream writer{buffer};

		EXPECT_TRUE(writer.write(source.data(), source.size()));
		EXPECT_EQ(buffer.size(), source.size());
		EXPECT_EQ(writer.size(), source.size());
		EXPECT_EQ(writer.tell(), static_cast< int64_t >(source.size()));

		/* A non-const vector selects the write-mode ctor; bind via const ref for read mode. */
		const std::vector< std::byte > & readable = buffer;
		MemoryStream reader{readable};
		EXPECT_TRUE(reader.isMemoryBacked());
		EXPECT_TRUE(reader.isOpen());
		EXPECT_EQ(reader.size(), source.size());

		std::array< uint8_t, 4 > destination{};
		EXPECT_TRUE(reader.read(destination.data(), destination.size()));
		EXPECT_EQ(source, destination);
		EXPECT_EQ(reader.tell(), static_cast< int64_t >(source.size()));
	}

	TEST(IOMemoryStream, readPastEndFails)
	{
		const std::vector< std::byte > buffer(4);
		MemoryStream reader{buffer};

		std::array< uint8_t, 8 > destination{};
		EXPECT_FALSE(reader.read(destination.data(), destination.size()));
	}

	TEST(IOMemoryStream, seekSetCurEnd)
	{
		const std::array< uint8_t, 4 > source{0x10, 0x20, 0x30, 0x40};
		const std::vector< std::byte > buffer{
			std::byte{source[0]}, std::byte{source[1]}, std::byte{source[2]}, std::byte{source[3]}
		};
		MemoryStream reader{buffer};

		EXPECT_EQ(reader.seek(2, 0), 2);            /* SEEK_SET */
		uint8_t value{};
		EXPECT_TRUE(reader.read(&value, 1));
		EXPECT_EQ(value, source[2]);

		EXPECT_EQ(reader.seek(-1, 1), 2);           /* SEEK_CUR: now at 3, back 1 -> 2 */
		EXPECT_EQ(reader.seek(0, 2), 4);            /* SEEK_END */
		EXPECT_EQ(reader.seek(-10, 2), -1);         /* before the beginning -> error */
		EXPECT_EQ(reader.seek(99, 0), -1);          /* past the end (read mode) -> error */
	}

	/* ===== Malformed / hostile input (run under ASan/UBSan to prove no OOB) ===== */

	TEST(IOMemoryStream, readSizeOverflowIsRejected)
	{
		/* m_position becomes 1, then a near-SIZE_MAX size makes (m_position + size) wrap
		 * around to a small value that would bypass a naive bound check -> OOB read. */
		const std::vector< std::byte > buffer(8);
		MemoryStream reader{buffer};

		uint8_t first{};
		EXPECT_TRUE(reader.read(&first, 1));

		/* Runtime value (not a compile-time constant): models an untrusted size and is not
		 * folded away by the compiler's stringop-overflow check, so the overflow happens at
		 * run time and is caught by the bound check (and by ASan if the check is wrong). */
		volatile size_t overflowSize = std::numeric_limits< size_t >::max();
		uint8_t destination{};
		EXPECT_FALSE(reader.read(&destination, overflowSize));
	}

	TEST(IOMemoryStream, writeSizeOverflowIsRejected)
	{
		/* Same overflow class on the write path: (m_writePosition + size) wrapping would
		 * skip the resize and memcpy out of bounds. */
		std::vector< std::byte > buffer;
		MemoryStream writer{buffer};

		const uint8_t first{0x7F};
		EXPECT_TRUE(writer.write(&first, 1));

		volatile size_t overflowSize = std::numeric_limits< size_t >::max();
		const uint8_t source{0x00};
		EXPECT_FALSE(writer.write(&source, overflowSize));
	}

	namespace
	{
		/* noexcept filesystem helpers (the throwing overloads would terminate under -fno-exceptions). */
		std::filesystem::path
		tempFile (const char * name) noexcept
		{
			return EmEn::Base::Testing::uniqueTemporaryPath(name);
		}

		void
		removeQuietly (const std::filesystem::path & path) noexcept
		{
			std::error_code errorCode;

			std::filesystem::remove(path, errorCode);
		}
	}

	TEST(IOFileStream, writeThenReadRoundTrip)
	{
		const auto path = tempFile("emeraude_base_fs_roundtrip.bin");
		removeQuietly(path);

		const std::array< uint8_t, 4 > source{0xDE, 0xAD, 0xBE, 0xEF};
		{
			FileStream writer{path, FileStream::Mode::Write};
			ASSERT_TRUE(writer.isOpen());
			EXPECT_TRUE(writer.write(source.data(), source.size()));
		}
		{
			FileStream reader{path, FileStream::Mode::Read};
			ASSERT_TRUE(reader.isOpen());
			EXPECT_EQ(reader.size(), source.size());

			std::array< uint8_t, 4 > destination{};
			EXPECT_TRUE(reader.read(destination.data(), destination.size()));
			EXPECT_EQ(source, destination);
		}
		removeQuietly(path);
	}

	TEST(IOFileStream, openMissingFileFails)
	{
		const auto path = tempFile("emeraude_base_fs_missing.bin");
		removeQuietly(path);

		FileStream reader{path, FileStream::Mode::Read};
		EXPECT_FALSE(reader.isOpen());

		uint8_t value{};
		EXPECT_FALSE(reader.read(&value, 1));
	}

	TEST(IOFileStream, readPastEndFails)
	{
		const auto path = tempFile("emeraude_base_fs_pastend.bin");
		removeQuietly(path);

		const std::array< uint8_t, 4 > source{1, 2, 3, 4};
		{
			FileStream writer{path, FileStream::Mode::Write};
			ASSERT_TRUE(writer.write(source.data(), source.size()));
		}
		{
			FileStream reader{path, FileStream::Mode::Read};
			ASSERT_TRUE(reader.isOpen());

			std::array< uint8_t, 8 > destination{};
			EXPECT_FALSE(reader.read(destination.data(), destination.size())); /* only 4 bytes exist */
		}
		removeQuietly(path);
	}

	TEST(IOFileStream, modeEnforcement)
	{
		const auto path = tempFile("emeraude_base_fs_mode.bin");
		removeQuietly(path);

		{
			FileStream writer{path, FileStream::Mode::Write};
			ASSERT_TRUE(writer.isOpen());

			uint8_t value{};
			EXPECT_FALSE(writer.read(&value, 1)); /* read on a write-mode stream */
		}
		{
			FileStream reader{path, FileStream::Mode::Read};
			ASSERT_TRUE(reader.isOpen());

			const uint8_t value{0x42};
			EXPECT_FALSE(reader.write(&value, 1)); /* write on a read-mode stream */
		}
		removeQuietly(path);
	}

	TEST(IOFileStream, seekAndTell)
	{
		const auto path = tempFile("emeraude_base_fs_seek.bin");
		removeQuietly(path);

		const std::array< uint8_t, 4 > source{0x10, 0x20, 0x30, 0x40};
		{
			FileStream writer{path, FileStream::Mode::Write};
			ASSERT_TRUE(writer.write(source.data(), source.size()));
		}
		{
			FileStream reader{path, FileStream::Mode::Read};
			ASSERT_TRUE(reader.isOpen());
			EXPECT_EQ(reader.seek(2, 0), 2);
			EXPECT_EQ(reader.tell(), 2);

			uint8_t value{};
			EXPECT_TRUE(reader.read(&value, 1));
			EXPECT_EQ(value, source[2]);
		}
		removeQuietly(path);
	}

	TEST(IOFileUtils, putGetContentsRoundTrip)
	{
		const auto path = tempFile("emeraude_base_io_putget.bin");
		removeQuietly(path);

		const std::vector< uint8_t > source{0x01, 0x02, 0x03, 0x04, 0x05};
		EXPECT_TRUE(filePutContents(path, source));
		EXPECT_TRUE(fileExists(path));
		EXPECT_EQ(filesize(path), source.size());

		std::vector< uint8_t > readBack;
		EXPECT_TRUE(fileGetContents(path, readBack));
		EXPECT_EQ(readBack, source);

		removeQuietly(path);
	}

	TEST(IOFileUtils, getRangeReadsExactlyTheRange)
	{
		const auto path = tempFile("emeraude_base_io_range.bin");
		removeQuietly(path);

		std::vector< uint8_t > source(64);

		for ( size_t index = 0; index < source.size(); ++index )
		{
			source[index] = static_cast< uint8_t >(index);
		}

		EXPECT_TRUE(filePutContents(path, source));

		std::vector< std::byte > range;

		ASSERT_TRUE(fileGetRange(path, 10, 20, range));
		ASSERT_EQ(range.size(), 20U);

		for ( size_t index = 0; index < range.size(); ++index )
		{
			EXPECT_EQ(std::to_integer< uint8_t >(range[index]), 10 + index) << "byte " << index;
		}

		/* The whole file, and the last byte alone. */
		EXPECT_TRUE(fileGetRange(path, 0, 64, range));
		EXPECT_EQ(range.size(), 64U);
		EXPECT_TRUE(fileGetRange(path, 63, 1, range));
		EXPECT_EQ(std::to_integer< uint8_t >(range[0]), 63);

		removeQuietly(path);
	}

	TEST(IOFileUtils, getRangeRefusesEveryRangeOutOfTheFile)
	{
		const auto path = tempFile("emeraude_base_io_range_bad.bin");
		removeQuietly(path);

		const std::vector< uint8_t > source(16, 0x5A);
		EXPECT_TRUE(filePutContents(path, source));

		std::vector< std::byte > range;

		/* Past the end, an empty range, an offset beyond the file, an offset + length that overflows 64 bits. */
		EXPECT_FALSE(fileGetRange(path, 10, 7, range));
		EXPECT_TRUE(range.empty());
		EXPECT_FALSE(fileGetRange(path, 0, 0, range));
		EXPECT_FALSE(fileGetRange(path, 17, 1, range));
		EXPECT_FALSE(fileGetRange(path, 8, UINT64_MAX, range));
		EXPECT_FALSE(fileGetRange(tempFile("emeraude_base_io_range_missing.bin"), 0, 1, range));
		EXPECT_FALSE(fileGetRange({}, 0, 1, range));

		removeQuietly(path);
	}

	TEST(IOFileUtils, getContentsMissingFileFails)
	{
		const auto path = tempFile("emeraude_base_io_missing.bin");
		removeQuietly(path);
		EXPECT_FALSE(fileExists(path));

		std::vector< uint8_t > content;
		EXPECT_FALSE(fileGetContents(path, content));
	}

	TEST(IOFileUtils, getContentsRoundsUpPartialElements)
	{
		const auto path = tempFile("emeraude_base_io_partial.bin");
		removeQuietly(path);

		const std::vector< uint8_t > fiveBytes{0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
		EXPECT_TRUE(filePutContents(path, fiveBytes));

		/* 5 bytes / sizeof(uint32_t)=4 -> ceil = 2 words (the partial-element rounding). */
		std::vector< uint32_t > asWords;
		EXPECT_TRUE(fileGetContents(path, asWords));
		EXPECT_EQ(asWords.size(), 2U);

		removeQuietly(path);
	}

	TEST(IOFileUtils, errorsReachTheLoggingHook)
	{
		/* Proves the cerr -> Logging migration: a failed read must reach the sink. */
		Severity captured{Severity::Debug};
		std::string capturedTag;
		std::string capturedMessage;
		int calls{0};

		Logging::setSink([&] (Severity severity, const char * tag, std::string_view message) {
			captured = severity;
			capturedTag = tag != nullptr ? tag : "";
			capturedMessage = std::string{message};
			++calls;
		});

		const auto path = tempFile("emeraude_base_io_missing_for_log.bin");
		removeQuietly(path);

		std::vector< uint8_t > content;
		EXPECT_FALSE(fileGetContents(path, content));

		Logging::setSink(nullptr); /* reset before asserting (the lambda captures locals) */

		EXPECT_GE(calls, 1);
		EXPECT_EQ(captured, Severity::Error);
		EXPECT_EQ(capturedTag, "IO");
		EXPECT_NE(capturedMessage.find("fileGetContents"), std::string::npos);
	}
}

TEST(IOFileUtils, permissionsOnRealFile)
{
	/* Ave robustus! (Axis B): readable/writable were untested; the Windows AccessCheck path is real
	 * (docs corrected). Here on POSIX: a freshly written file is readable & writable; a missing path
	 * and the empty path are neither. */
	std::error_code errorCode;
	const auto dir = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_io_perms");
	std::filesystem::remove_all(dir, errorCode);
	std::filesystem::create_directories(dir, errorCode);

	const auto file = dir / "file.txt";
	{
		std::ofstream out{file, std::ios::binary | std::ios::trunc};
		out << "data";
	}

	EXPECT_TRUE(EmEn::Base::IO::readable(file));
	EXPECT_TRUE(EmEn::Base::IO::writable(file));

	const auto missing = dir / "does-not-exist.txt";
	EXPECT_FALSE(EmEn::Base::IO::readable(missing));
	EXPECT_FALSE(EmEn::Base::IO::writable(missing));
	EXPECT_FALSE(EmEn::Base::IO::executable(missing));

	EXPECT_FALSE(EmEn::Base::IO::readable(std::filesystem::path{}));
	EXPECT_FALSE(EmEn::Base::IO::writable(std::filesystem::path{}));
	EXPECT_FALSE(EmEn::Base::IO::executable(std::filesystem::path{}));

	std::filesystem::remove_all(dir, errorCode);
}

TEST(IOForEachDirectoryEntry, walksFlatAndRecursiveStopsEarlyAndNeverThrows)
{
	std::error_code errorCode;

	const auto root = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_forEachDirectoryEntry");
	std::filesystem::remove_all(root, errorCode);
	ASSERT_TRUE(std::filesystem::create_directories(root / "sub", errorCode));

	std::ofstream{root / "a.txt"} << "a";
	std::ofstream{root / "b.txt"} << "b";
	std::ofstream{root / "sub" / "c.txt"} << "c";

	size_t flat = 0;
	ASSERT_TRUE(EmEn::Base::IO::forEachDirectoryEntry(root, false, [&flat] (const std::filesystem::directory_entry &) { ++flat; return true; }));
	/* a.txt, b.txt and the sub directory itself. */
	EXPECT_EQ(flat, 3U);

	size_t regularFiles = 0;
	ASSERT_TRUE(EmEn::Base::IO::forEachDirectoryEntry(root, true, [&regularFiles] (const std::filesystem::directory_entry & entry) {
		std::error_code entryError;

		if ( entry.is_regular_file(entryError) )
		{
			++regularFiles;
		}

		return true;
	}));
	EXPECT_EQ(regularFiles, 3U);

	/* The visitor stops the walk: the walk still reports success. */
	size_t visited = 0;
	ASSERT_TRUE(EmEn::Base::IO::forEachDirectoryEntry(root, true, [&visited] (const std::filesystem::directory_entry &) { ++visited; return false; }));
	EXPECT_EQ(visited, 1U);

	/* A missing directory is a logged failure, never a terminate (the range-for over directory_iterator threw here). */
	size_t missing = 0;
	EXPECT_FALSE(EmEn::Base::IO::forEachDirectoryEntry(root / "does-not-exist", false, [&missing] (const std::filesystem::directory_entry &) { ++missing; return true; }));
	EXPECT_EQ(missing, 0U);
	EXPECT_TRUE(EmEn::Base::IO::directoryEntries(root / "does-not-exist").empty());

	std::filesystem::remove_all(root, errorCode);
}

TEST(IOConfinedPath, refusesEveryPathThatLeavesTheBase)
{
	const std::filesystem::path base{"/data/stores"};

	/* Accepted: a relative path that stays inside, "." and ".." segments that resolve inside included. */
	ASSERT_TRUE(EmEn::Base::IO::confinedPath(base, "Images/a.png").has_value());
	EXPECT_EQ(EmEn::Base::IO::confinedPath(base, "Images/a.png").value(), base / "Images/a.png");
	EXPECT_EQ(EmEn::Base::IO::confinedPath(base, "Images/../Meshes/./m.obj").value(), base / "Meshes/m.obj");

	/* Refused: empty, absolute (append() would REPLACE the base), escaping with "..". */
	EXPECT_FALSE(EmEn::Base::IO::confinedPath(base, "").has_value());
	EXPECT_FALSE(EmEn::Base::IO::confinedPath(base, "/etc/passwd").has_value());
	EXPECT_FALSE(EmEn::Base::IO::confinedPath(base, "../secret").has_value());
	EXPECT_FALSE(EmEn::Base::IO::confinedPath(base, "Images/../../secret").has_value());
	EXPECT_FALSE(EmEn::Base::IO::confinedPath(base, "a/b/../../../x").has_value());
#if IS_WINDOWS
	EXPECT_FALSE(EmEn::Base::IO::confinedPath(base, "C:\\Windows\\win.ini").has_value());
	EXPECT_FALSE(EmEn::Base::IO::confinedPath(base, "C:relative").has_value());
	EXPECT_FALSE(EmEn::Base::IO::confinedPath(base, "\\\\server\\share\\x").has_value());
	EXPECT_FALSE(EmEn::Base::IO::confinedPath(base, "..\\secret").has_value());
#endif
}

/* Windows paths of MAX_PATH (260) characters or more failed in every IO:: call (the Windows peer, 2026-10-01: 118
 * shader binaries under a ~142-character cache directory). Owner decision (2026-10-07): the IO:: wrappers use the
 * Win32 extended-length form ("\\?\") on Windows. The form itself is pure string work, checked on every platform. */
TEST(IOWindowsExtendedLengthPath, prefixesDriveAndUNCPathsAndLeavesTheRestAlone)
{
	using EmEn::Base::IO::windowsExtendedLengthPath;

	EXPECT_EQ(windowsExtendedLengthPath(std::string_view{"C:\\cache\\shader.bin"}), "\\\\?\\C:\\cache\\shader.bin");
	/* The prefix turns off every other normalisation: forward slashes must become backslashes. */
	EXPECT_EQ(windowsExtendedLengthPath(std::string_view{"d:/cache/shader.bin"}), "\\\\?\\d:\\cache\\shader.bin");
	EXPECT_EQ(windowsExtendedLengthPath(std::string_view{"\\\\server\\share\\a.bin"}), "\\\\?\\UNC\\server\\share\\a.bin");
	EXPECT_EQ(windowsExtendedLengthPath(std::wstring_view{L"C:\\x"}), L"\\\\?\\C:\\x");

	/* Already extended, or a device path: unchanged. */
	EXPECT_EQ(windowsExtendedLengthPath(std::string_view{"\\\\?\\C:\\x"}), "\\\\?\\C:\\x");
	EXPECT_EQ(windowsExtendedLengthPath(std::string_view{"\\\\.\\COM1"}), "\\\\.\\COM1");

	/* No extended form: relative, drive-relative, rooted without a drive, empty. */
	EXPECT_EQ(windowsExtendedLengthPath(std::string_view{"cache\\shader.bin"}), "cache\\shader.bin");
	EXPECT_EQ(windowsExtendedLengthPath(std::string_view{"C:cache"}), "C:cache");
	EXPECT_EQ(windowsExtendedLengthPath(std::string_view{"\\cache"}), "\\cache");
	EXPECT_EQ(windowsExtendedLengthPath(std::string_view{""}), "");
}

TEST(IOSystemPath, shortPathsAreUnchangedAndLongOnesStayUsable)
{
	std::error_code errorCode;
	const auto base = EmEn::Base::Testing::uniqueTemporaryPath("emeraude-io-long-path-test");
	std::filesystem::remove_all(base, errorCode);

	/* A short path is handed to the system as it is, on every platform. */
	EXPECT_EQ(EmEn::Base::IO::systemPath(base), base);

	/* A path well past MAX_PATH: every wrapper must work through it (Windows: through "\\?\"). */
	auto deep = base;

	for ( int level = 0; level < 6; ++level )
	{
		deep /= EmEn::Base::IO::u8path(std::string(48, static_cast< char >('a' + level)));
	}

	const auto file = deep / "shader-binary.bin";
	ASSERT_GT(file.native().size(), 300U);

	const std::string content{"long path content"};

	ASSERT_TRUE(EmEn::Base::IO::filePutContents(file, content, false, true));
	EXPECT_TRUE(EmEn::Base::IO::fileExists(file));
	EXPECT_EQ(EmEn::Base::IO::filesize(file), content.size());

	std::string readBack;
	ASSERT_TRUE(EmEn::Base::IO::fileGetContents(file, readBack));
	EXPECT_EQ(readBack, content);

	const auto renamed = deep / "shader-binary-committed.bin";
	ASSERT_TRUE(EmEn::Base::IO::renameFile(file, renamed));
	EXPECT_FALSE(EmEn::Base::IO::fileExists(file));
	EXPECT_TRUE(EmEn::Base::IO::fileExists(renamed));

	/* A recursive walk and a recursive removal from the SHORT root reach the long descendants (Windows: the root takes
	 * the extended form whatever its own length — eraseDirectory() failed with "145: The directory is not empty"). */
	size_t files = 0;
	size_t foreignEntries = 0;

	ASSERT_TRUE(EmEn::Base::IO::forEachDirectoryEntry(base, true, [&files, &foreignEntries, &base] (const std::filesystem::directory_entry & entry) {
		std::error_code entryError;

		if ( entry.is_regular_file(entryError) )
		{
			++files;
		}

		/* The entries come back in the CALLER's form: relative to the root it gave, never across "\\?\" (the
		 * resource scan computes exactly this, and found nothing on Windows when it was empty). A long entry may stay
		 * extended: only the short ones are checked. */
		if ( entry.path().native().size() < EmEn::Base::IO::WindowsLongPathThreshold && entry.path().lexically_relative(base).empty() )
		{
			++foreignEntries;
		}

		return true;
	}));
	EXPECT_EQ(files, 1U);
	EXPECT_EQ(foreignEntries, 0U);

	EXPECT_TRUE(EmEn::Base::IO::eraseDirectory(base, true));
	EXPECT_FALSE(EmEn::Base::IO::directoryExists(base));
}

TEST(IORenameFile, refusesEmptyAndMissingPaths)
{
	EXPECT_FALSE(EmEn::Base::IO::renameFile({}, "x"));
	EXPECT_FALSE(EmEn::Base::IO::renameFile("x", {}));
	std::error_code errorCode;
	const auto directory = std::filesystem::temp_directory_path(errorCode);

	EXPECT_FALSE(EmEn::Base::IO::renameFile(directory / "emeraude-io-no-such-file", directory / "emeraude-io-target"));
}

/* The coverage pass (2026-10-18): the contracts of filesize(), createDirectory(), isDirectoryContentEmpty(),
 * directoryExists() and fileGetContents(), and the readers refusing what is not a regular file. */
TEST(IOFileUtils, filesizeIsZeroOnAnError)
{
	/* file_size() answers static_cast< uintmax_t >(-1) on an error: a missing file was 18446744073709551615 bytes. */
	const auto missing = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_io_no_such_file.bin");

	EXPECT_EQ(EmEn::Base::IO::filesize(missing), 0U);
	EXPECT_EQ(EmEn::Base::IO::filesize(missing.parent_path()), 0U);
}

TEST(IOFileUtils, aBareFileNameNeedsNoDirectoryCreated)
{
	/* create_directories("") is EINVAL: filePutContents("out.json", …, createDirectories = true) failed. */
	std::error_code errorCode;
	const auto directory = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_io_bare_name");
	ASSERT_TRUE(std::filesystem::create_directories(directory, errorCode)) << errorCode.message();

	const auto previous = std::filesystem::current_path(errorCode);
	ASSERT_FALSE(errorCode) << errorCode.message();
	std::filesystem::current_path(directory, errorCode);
	ASSERT_FALSE(errorCode) << errorCode.message();

	const bool created = EmEn::Base::IO::createDirectory("out.json", true);
	const bool written = EmEn::Base::IO::filePutContents("out.json", std::string_view{"{}"}, false, true);

	std::filesystem::current_path(previous, errorCode);
	ASSERT_FALSE(errorCode) << errorCode.message();

	EXPECT_TRUE(created);
	EXPECT_TRUE(written);
	EXPECT_EQ(EmEn::Base::IO::filesize(directory / "out.json"), 2U);

	std::filesystem::remove_all(directory, errorCode);
}

TEST(IOFileUtils, anEmptyFileIsNoEmptyDirectory)
{
	std::error_code errorCode;
	const auto directory = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_io_empty_dir");
	ASSERT_TRUE(std::filesystem::create_directories(directory, errorCode)) << errorCode.message();

	const auto emptyFile = directory / "empty.bin";
	ASSERT_TRUE(EmEn::Base::IO::createFile(emptyFile));

	/* is_empty() is true for an empty FILE too. */
	EXPECT_FALSE(EmEn::Base::IO::isDirectoryContentEmpty(emptyFile));
	EXPECT_FALSE(EmEn::Base::IO::isDirectoryContentEmpty(directory));
	EXPECT_FALSE(EmEn::Base::IO::isDirectoryContentEmpty(directory / "missing"));

	ASSERT_TRUE(EmEn::Base::IO::eraseFile(emptyFile));
	EXPECT_TRUE(EmEn::Base::IO::isDirectoryContentEmpty(directory));

	std::filesystem::remove_all(directory, errorCode);
}

TEST(IOFileUtils, aPathThroughAFileIsSimplyNoDirectory)
{
	/* ENOTDIR (and Windows' ERROR_PATH_NOT_FOUND for a missing parent) used to be logged as errors. */
	std::error_code errorCode;
	const auto file = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_io_not_a_dir.bin");
	ASSERT_TRUE(EmEn::Base::IO::filePutContents(file, std::string_view{"x"}));

	int calls{0};
	EmEn::Base::Logging::setSink([&calls] (EmEn::Base::Severity /*severity*/, const char * /*tag*/, std::string_view /*message*/) {
		++calls;
	});

	const bool throughFile = EmEn::Base::IO::directoryExists(file / "child");
	const bool missingParents = EmEn::Base::IO::directoryExists(file.parent_path() / "emeraude_io_no_such_parent" / "a" / "b");

	EmEn::Base::Logging::setSink(nullptr);

	EXPECT_FALSE(throughFile);
	EXPECT_FALSE(missingParents);
	EXPECT_EQ(calls, 0);

	std::filesystem::remove(file, errorCode);
}

TEST(IOFileUtils, readersRefuseADirectory)
{
	std::error_code errorCode;
	const auto directory = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_io_dir_as_file");
	ASSERT_TRUE(std::filesystem::create_directories(directory, errorCode)) << errorCode.message();

	std::string text;
	std::vector< uint8_t > bytes;
	std::vector< std::byte > range;
	std::ifstream stream;

	EXPECT_FALSE(EmEn::Base::IO::isReadableRegularFile(directory));
	EXPECT_FALSE(EmEn::Base::IO::openInputFile(directory, stream));
	EXPECT_FALSE(EmEn::Base::IO::fileGetContents(directory, text));
	EXPECT_FALSE(EmEn::Base::IO::fileGetContents(directory, bytes));
	EXPECT_FALSE(EmEn::Base::IO::fileGetRange(directory, 0, 1, range));
	EXPECT_FALSE(EmEn::Base::IO::FileStream(directory, EmEn::Base::IO::FileStream::Mode::Read).isOpen());
	EXPECT_FALSE(EmEn::Base::FastJSON::getRootFromFile(directory, 16, true).has_value());
	EXPECT_FALSE(EmEn::Base::INIParser{}.read(directory));
	EXPECT_FALSE(EmEn::Base::IO::ZipReader::isArchiveFile(directory));

	std::filesystem::remove_all(directory, errorCode);
}

namespace
{
	/** @brief Counts the write-aside temporary files left in a directory. */
	[[nodiscard]]
	size_t
	countWritingLeftovers (const std::filesystem::path & directory)
	{
		size_t count = 0;

		static_cast< void >(EmEn::Base::IO::forEachDirectoryEntry(directory, false, [&count] (const std::filesystem::directory_entry & entry) {
			if ( EmEn::Base::IO::toU8String(entry.path().filename()).find(".writing-") != std::string::npos )
			{
				++count;
			}

			return true;
		}));

		return count;
	}
}

TEST(IOFileUtils, aWriteAsideReplacesWholeOrNotAtAll)
{
	std::error_code errorCode;
	const auto directory = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_io_aside");
	ASSERT_TRUE(std::filesystem::create_directories(directory, errorCode)) << errorCode.message();

	const auto target = directory / "target.bin";
	std::string content;

	ASSERT_TRUE(EmEn::Base::IO::filePutContentsAside(target, std::string_view{"first"}));
	ASSERT_TRUE(EmEn::Base::IO::fileGetContents(target, content));
	EXPECT_EQ(content, "first");

	ASSERT_TRUE(EmEn::Base::IO::filePutContentsAside(target, std::string_view{"second, longer"}));
	ASSERT_TRUE(EmEn::Base::IO::fileGetContents(target, content));
	EXPECT_EQ(content, "second, longer");

	/* A target that cannot be replaced (a directory holding a file): refused, the directory and its file untouched. */
	const auto occupied = directory / "occupied";
	ASSERT_TRUE(std::filesystem::create_directories(occupied, errorCode)) << errorCode.message();
	ASSERT_TRUE(EmEn::Base::IO::filePutContents(occupied / "inside.bin", std::string_view{"inside"}));

	EXPECT_FALSE(EmEn::Base::IO::filePutContentsAside(occupied, std::string_view{"x"}));
	EXPECT_TRUE(EmEn::Base::IO::directoryExists(occupied));
	EXPECT_EQ(EmEn::Base::IO::filesize(occupied / "inside.bin"), 6U);

	/* Each call writes aside under its own name, beside the target; nothing is left behind. */
	const auto first = EmEn::Base::IO::temporarySiblingPath(target);
	const auto second = EmEn::Base::IO::temporarySiblingPath(target);
	EXPECT_NE(first, second);
	EXPECT_EQ(first.parent_path(), directory);
	EXPECT_EQ(countWritingLeftovers(directory), 0U);

	std::filesystem::remove_all(directory, errorCode);
}

TEST(IOFileUtils, aRefusedWriteKeepsTheExistingFile)
{
	/* The three FileIO writers opened — truncated — the target before refusing an extension or failing to encode: an
	 * existing file was emptied, a new one left empty. They encode in memory and write aside now (owner decision). */
	using namespace EmEn::Base;

	std::error_code errorCode;
	const auto directory = Testing::uniqueTemporaryPath("emeraude_io_refused_writes");
	ASSERT_TRUE(std::filesystem::create_directories(directory, errorCode)) << errorCode.message();

	const std::string precious{"precious content"};
	std::vector< std::filesystem::path > kept;

	const auto keep = [&] (const char * name) {
		const auto path = directory / name;
		static_cast< void >(IO::filePutContents(path, std::string_view{precious}));
		kept.emplace_back(path);

		return path;
	};

	const PixelFactory::Pixmap< uint8_t > image{4U, 4U, PixelFactory::ChannelMode::RGB};
	const PixelFactory::Pixmap< float > floatImage{4U, 4U, PixelFactory::ChannelMode::RGB};
	const PixelFactory::Pixmap< uint8_t > invalidImage{};
	const auto cube = VertexFactory::ShapeGenerator::generateCuboid< float >(1.0F);
	const WaveFactory::Wave< int16_t > invalidWave{};

	EXPECT_FALSE(PixelFactory::FileIO::write(image, keep("image.bmp"), true));
	EXPECT_FALSE(PixelFactory::FileIO::write(floatImage, keep("image.png"), true));
	EXPECT_FALSE(PixelFactory::FileIO::write(invalidImage, keep("invalid.png"), true));
	EXPECT_FALSE(VertexFactory::FileIO::write(cube, keep("shape.xyz"), true));
	EXPECT_FALSE(VertexFactory::FileIO::write(cube, keep("shape.md2"), true));
	EXPECT_FALSE(WaveFactory::FileIO::write(invalidWave, keep("sound.wav"), true));

	for ( const auto & path : kept )
	{
		std::string content;
		EXPECT_TRUE(IO::fileGetContents(path, content));
		EXPECT_EQ(content, precious) << IO::toU8String(path);
	}

	/* A refused NEW file is not created at all. */
	EXPECT_FALSE(PixelFactory::FileIO::write(invalidImage, directory / "new.png", true));
	EXPECT_FALSE(IO::exists(directory / "new.png"));

	/* A successful overwrite replaces the content. */
	const auto replaced = keep("replaced.png");
	EXPECT_TRUE(PixelFactory::FileIO::write(image, replaced, true));

	PixelFactory::Pixmap< uint8_t > readBack;
	EXPECT_TRUE(PixelFactory::FileIO::read(replaced, readBack));
	EXPECT_EQ(readBack.width(), 4U);

	EXPECT_EQ(countWritingLeftovers(directory), 0U);

	std::filesystem::remove_all(directory, errorCode);
}

TEST(IOFileUtils, theRefusalsDataCanReach)
{
	/* The coverage pass (2026-10-18): the refusals a path or a range from data can trigger. */
	using namespace EmEn::Base;

	std::error_code errorCode;
	const auto directory = Testing::uniqueTemporaryPath("emeraude_io_refusals");
	ASSERT_TRUE(std::filesystem::create_directories(directory / "full", errorCode)) << errorCode.message();

	const auto file = directory / "full" / "data.bin";
	ASSERT_TRUE(IO::filePutContents(file, std::string_view{"0123456789"}));

	/* Erasing: a non-empty directory without recursion, a directory as a file, a file as a directory. */
	EXPECT_FALSE(IO::eraseDirectory(directory / "full", false));
	EXPECT_TRUE(IO::fileExists(file));
	EXPECT_FALSE(IO::eraseFile(directory / "full"));
	EXPECT_FALSE(IO::eraseDirectory(file, true));
	EXPECT_TRUE(IO::fileExists(file));

	/* Creating under a file, writing into a directory. */
	EXPECT_FALSE(IO::createDirectory(file / "child"));
	EXPECT_FALSE(IO::filePutContents(directory / "full", std::string_view{"x"}));

	/* Ranges: past the end, overflowing, empty. */
	std::vector< std::byte > range;
	EXPECT_TRUE(IO::fileGetRange(file, 2, 3, range));
	EXPECT_EQ(range.size(), 3U);
	EXPECT_FALSE(IO::fileGetRange(file, 8, 3, range));
	EXPECT_TRUE(range.empty());
	EXPECT_FALSE(IO::fileGetRange(file, std::numeric_limits< uint64_t >::max(), 2, range));
	EXPECT_FALSE(IO::fileGetRange(file, 0, 0, range));

	/* An empty file reads as empty, successfully. */
	ASSERT_TRUE(IO::createFile(directory / "empty.bin"));
	std::string content{"untouched"};
	EXPECT_TRUE(IO::fileGetContents(directory / "empty.bin", content));
	EXPECT_TRUE(content.empty());

	/* The FileIO writers: an existing file without overwrite, a read-only sound format. */
	const auto cube = VertexFactory::ShapeGenerator::generateCuboid< float >(1.0F);
	EXPECT_FALSE(VertexFactory::FileIO::write(cube, file, false));
	const WaveFactory::Wave< int16_t > wave{};
	EXPECT_FALSE(WaveFactory::FileIO::write(wave, file, false));
	EXPECT_FALSE(WaveFactory::FileIO::write(wave, directory / "sound.json", true));
	EXPECT_FALSE(WaveFactory::FileIO::write(wave, directory / "sound.mid", true));
	EXPECT_EQ(IO::filesize(file), 10U);
	EXPECT_FALSE(IO::exists(directory / "sound.json"));

	std::filesystem::remove_all(directory, errorCode);
}

TEST(IOFileUtils, filesizeOfADirectoryIsZero)
{
	/* MSVC's file_size() answers a directory's size with no error (Windows peer, 2026-10-18: 524288 for %TEMP%). */
	std::error_code errorCode;
	const auto directory = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_io_filesize_dir");
	ASSERT_TRUE(std::filesystem::create_directories(directory, errorCode)) << errorCode.message();

	EXPECT_EQ(EmEn::Base::IO::filesize(directory), 0U);

	std::filesystem::remove_all(directory, errorCode);
}

#if IS_LINUX
TEST(IOFileUtils, aPseudoFileIsRefusedNotReadEmpty)
{
	/* /proc claims 0 bytes or cannot seek to its end: refused explicitly (owner decision), never read as empty. */
	std::string text{"untouched"};

	EXPECT_FALSE(EmEn::Base::IO::fileGetContents("/proc/self/status", text));
	EXPECT_EQ(text, "untouched");
}
#endif

#if IS_LINUX || IS_MACOS
namespace
{
	/** @brief Which end of the FIFO the code under test would block on. */
	enum class BlockedEnd : uint8_t
	{
		Reader,
		Writer
	};

	/**
	 * @brief A FIFO for one test. If an open blocks on it, the releaser opens the other end (non-blocking) after one
	 * second: the former code fails the test instead of hanging the suite.
	 */
	class FifoForOneTest final
	{
		public:

			explicit
			FifoForOneTest (std::string_view leaf, BlockedEnd blockedEnd = BlockedEnd::Reader)
				: m_path{EmEn::Base::Testing::uniqueTemporaryPath(leaf)},
				m_created{::mkfifo(m_path.c_str(), 0600) == 0}
			{
				m_releaser = std::jthread{[path = m_path, blockedEnd] (const std::stop_token & stopToken) {
					const auto start = std::chrono::steady_clock::now();
					int heldReadEnd = -1;

					while ( !stopToken.stop_requested() )
					{
						if ( heldReadEnd < 0 && std::chrono::steady_clock::now() - start >= std::chrono::seconds{1} )
						{
							if ( blockedEnd == BlockedEnd::Reader )
							{
								/* NOTE: Succeeds only while a reader waits in its open(). */
								const int descriptor = ::open(path.c_str(), O_WRONLY | O_NONBLOCK);

								if ( descriptor >= 0 )
								{
									::close(descriptor);

									return;
								}
							}
							else
							{
								/* NOTE: A non-blocking read end opens at once and releases a writer waiting in its open().
								 * It stays open (drained) until the test ends: closing it under a writer would SIGPIPE. */
								heldReadEnd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
							}
						}

						if ( heldReadEnd >= 0 )
						{
							std::array< char, 256 > drain{};

							while ( ::read(heldReadEnd, drain.data(), drain.size()) > 0 )
							{
							}
						}

						std::this_thread::sleep_for(std::chrono::milliseconds{20});
					}

					if ( heldReadEnd >= 0 )
					{
						::close(heldReadEnd);
					}
				}};
			}

			FifoForOneTest (const FifoForOneTest &) = delete;
			FifoForOneTest (FifoForOneTest &&) = delete;
			FifoForOneTest & operator= (const FifoForOneTest &) = delete;
			FifoForOneTest & operator= (FifoForOneTest &&) = delete;

			~FifoForOneTest ()
			{
				m_releaser.request_stop();
				m_releaser.join();

				std::error_code errorCode;
				std::filesystem::remove(m_path, errorCode);
			}

			[[nodiscard]]
			const std::filesystem::path &
			path () const noexcept
			{
				return m_path;
			}

			[[nodiscard]]
			bool
			created () const noexcept
			{
				return m_created;
			}

		private:

			std::filesystem::path m_path;
			std::jthread m_releaser;
			bool m_created{false};
	};

	/** @brief Runs a read (or a write) on a fresh FIFO: it must be refused at once (a blocked open waits for the
	 * releaser, 1 s). */
	template< typename read_t >
	void
	expectRefusedAtOnce (std::string_view leaf, read_t && read, BlockedEnd blockedEnd = BlockedEnd::Reader)
	{
		const FifoForOneTest fifo{leaf, blockedEnd};
		ASSERT_TRUE(fifo.created());

		const auto start = std::chrono::steady_clock::now();
		const bool accepted = std::forward< read_t >(read)(fifo.path());
		const auto elapsed = std::chrono::steady_clock::now() - start;

		EXPECT_FALSE(accepted);
		EXPECT_LT(elapsed, std::chrono::milliseconds{500}) << "the open blocked on the FIFO";
	}
}

TEST(IOFileUtils, readersRefuseAFifoWithoutBlocking)
{
	/* A FIFO without a writer blocked std::ifstream::open() FOREVER: a path from data could freeze a loading thread. */
	expectRefusedAtOnce("emeraude_io_fifo_string", [] (const std::filesystem::path & path) {
		std::string content;
		return EmEn::Base::IO::fileGetContents(path, content);
	});
	expectRefusedAtOnce("emeraude_io_fifo_vector", [] (const std::filesystem::path & path) {
		std::vector< uint8_t > content;
		return EmEn::Base::IO::fileGetContents(path, content);
	});
	expectRefusedAtOnce("emeraude_io_fifo_range", [] (const std::filesystem::path & path) {
		std::vector< std::byte > content;
		return EmEn::Base::IO::fileGetRange(path, 0, 1, content);
	});
	expectRefusedAtOnce("emeraude_io_fifo_stream", [] (const std::filesystem::path & path) {
		return EmEn::Base::IO::FileStream{path, EmEn::Base::IO::FileStream::Mode::Read}.isOpen();
	});
	expectRefusedAtOnce("emeraude_io_fifo_json", [] (const std::filesystem::path & path) {
		return EmEn::Base::FastJSON::getRootFromFile(path, 16, true).has_value();
	});
	expectRefusedAtOnce("emeraude_io_fifo_ini", [] (const std::filesystem::path & path) {
		return EmEn::Base::INIParser{}.read(path);
	});
	expectRefusedAtOnce("emeraude_io_fifo_md5anim", [] (const std::filesystem::path & path) {
		return !EmEn::Base::Animation::MD5AnimParser< float >::parse(path, "clip").empty();
	});
	expectRefusedAtOnce("emeraude_io_fifo_zip", [] (const std::filesystem::path & path) {
		return EmEn::Base::IO::ZipReader::isArchiveFile(path);
	});
	expectRefusedAtOnce("emeraude_io_fifo_zip_open", [] (const std::filesystem::path & path) {
		EmEn::Base::IO::ZipReader reader{path};
		return reader.open();
	});
}
TEST(IOFileUtils, writersRefuseAFifoWithoutBlocking)
{
	/* Opening a FIFO for WRITING without a reader blocked std::ofstream::open() forever too (2026-10-18). */
	expectRefusedAtOnce("emeraude_io_wfifo_create", [] (const std::filesystem::path & path) {
		return EmEn::Base::IO::createFile(path);
	}, BlockedEnd::Writer);
	expectRefusedAtOnce("emeraude_io_wfifo_put", [] (const std::filesystem::path & path) {
		return EmEn::Base::IO::filePutContents(path, std::string_view{"x"});
	}, BlockedEnd::Writer);
	expectRefusedAtOnce("emeraude_io_wfifo_put_bytes", [] (const std::filesystem::path & path) {
		const std::vector< uint8_t > bytes{1, 2, 3};
		return EmEn::Base::IO::filePutContents(path, bytes);
	}, BlockedEnd::Writer);
	expectRefusedAtOnce("emeraude_io_wfifo_stream", [] (const std::filesystem::path & path) {
		return EmEn::Base::IO::FileStream{path, EmEn::Base::IO::FileStream::Mode::Write}.isOpen();
	}, BlockedEnd::Writer);
	expectRefusedAtOnce("emeraude_io_wfifo_ini", [] (const std::filesystem::path & path) {
		return EmEn::Base::INIParser{}.write(path);
	}, BlockedEnd::Writer);

	/* An archive entry extracted (overwrite) onto a FIFO of its destination: the entry takes the FIFO's own name. */
	expectRefusedAtOnce("emeraude_io_wfifo_zip_entry.txt", [] (const std::filesystem::path & path) {
		std::error_code errorCode;
		const auto archiveDirectory = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_io_wfifo_zip");
		std::filesystem::create_directories(archiveDirectory, errorCode);
		static_cast< void >(EmEn::Base::IO::filePutContents(archiveDirectory / "a.txt", std::string_view{"payload"}));

		const auto entryName = EmEn::Base::IO::toU8String(path.filename());
		bool accepted = false;

		{
			EmEn::Base::IO::ZipWriter writer{archiveDirectory / "one.zip"};

			if ( writer.addFilepathToSources(archiveDirectory / "a.txt", entryName) && writer.create() )
			{
				EmEn::Base::IO::ZipReader reader{archiveDirectory / "one.zip"};
				accepted = reader.open() && reader.extract(entryName, path.parent_path(), true);
			}
		}

		std::filesystem::remove_all(archiveDirectory, errorCode);

		return accepted;
	}, BlockedEnd::Writer);
}
#endif
