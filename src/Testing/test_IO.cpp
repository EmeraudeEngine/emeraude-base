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

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

/* Local inclusions. */
#include "IO/FileStream.hpp"
#include "IO/IO.hpp"
#include "IO/MemoryStream.hpp"
#include "Logging/Logging.hpp"
#include "Logging/Severity.hpp"

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
			std::error_code errorCode;

			return std::filesystem::temp_directory_path(errorCode) / name;
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
	const auto dir = std::filesystem::temp_directory_path(errorCode) / "emeraude_io_perms";
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

	const auto root = std::filesystem::temp_directory_path(errorCode) / "emeraude_forEachDirectoryEntry";
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
	const auto base = std::filesystem::temp_directory_path(errorCode) / "emeraude-io-long-path-test";
	std::filesystem::remove_all(base, errorCode);

	/* A short path is handed to the system as it is, on every platform. */
	EXPECT_EQ(EmEn::Base::IO::systemPath(base), base);

	/* A path well past MAX_PATH: every wrapper must work through it (Windows: through "\\?\"). */
	auto deep = base;

	for ( int level = 0; level < 6; ++level )
	{
		deep /= std::string(48, static_cast< char >('a' + level));
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

	EXPECT_TRUE(EmEn::Base::IO::eraseFile(renamed));
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
