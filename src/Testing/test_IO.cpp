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
#include <limits>
#include <string>
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
