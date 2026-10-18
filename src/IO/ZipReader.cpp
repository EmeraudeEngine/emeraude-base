/*
 * src/IO/ZipReader.cpp
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

#include "ZipReader.hpp"

/* Project configuration. */
#include "emeraude_platform.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <utility>

/* Local inclusions. */
#include "IO.hpp"

namespace EmEn::Base::IO
{
	ZipReader::ZipReader (std::filesystem::path filepath) noexcept
		: m_filepath(std::move(filepath))
	{

	}

	ZipReader::~ZipReader ()
	{
		this->close();
	}

	bool
	ZipReader::isArchiveFile (const std::filesystem::path & filepath) noexcept
	{
		/* NOTE: libzip opens the path itself: a FIFO blocked that open forever (2026-10-18). */
		if ( !IO::isReadableRegularFile(filepath) )
		{
			return false;
		}

#if IS_WINDOWS
		zip_error_t zipError;
		zip_error_init(&zipError);

		auto * source = zip_source_win32w_create(filepath.wstring().c_str(), 0, -1, &zipError);

		if ( source == nullptr )
		{
			zip_error_fini(&zipError);

			return false;
		}

		auto * archive = zip_open_from_source(source, ZIP_RDONLY | ZIP_CHECKCONS, &zipError);

		if ( archive == nullptr )
		{
			zip_source_free(source);
			zip_error_fini(&zipError);

			return false;
		}

		zip_error_fini(&zipError);
#else
		int errorCode = 0;

		auto * archive = zip_open(toU8String(filepath).data(), ZIP_RDONLY | ZIP_CHECKCONS, &errorCode);

		if ( archive == nullptr )
		{
			return false;
		}
#endif

		zip_close(archive);

		return true;
	}

	bool
	ZipReader::open () noexcept
	{
		/* NOTE: A second open() starts from a closed reader: the entry list used to be read again on top of itself (each
		 * entry listed twice, 2026-10-18). */
		this->close();

		if ( !IO::exists(m_filepath) )
		{
			std::cerr << ClassId << " : The archive file " << toU8String(m_filepath) << " doesn't exists !" "\n";

			return false;
		}

		if ( !this->openArchive() )
		{
			std::cerr << ClassId << " : Unable to open the archive " << toU8String(m_filepath) << " !" "\n";

			return false;
		}

		this->readArchive();

		/* NOTE: A valid archive without an entry is open (owner decision, 2026-10-18): it used to answer false while
		 * staying open, against isArchiveFile(). */
		return true;
	}

	void
	ZipReader::close () noexcept
	{
		m_zip.reset();

		m_entryNames.clear();
	}

	bool
	ZipReader::extract (const std::string & entryName, std::vector< char > & buffer) const noexcept
	{
		if ( !this->isOpen() )
		{
			return false;
		}

		buffer.clear();

		struct zip_stat stat{};
		zip_stat_init(&stat);

		/* NOTE: A size of 0 is a legitimate empty file or a directory entry: only the call's result says "missing". */
		if ( zip_stat(m_zip.get(), entryName.c_str(), 0, &stat) != 0 )
		{
			std::cerr << ClassId << " : The entry " << entryName << " doesn't exists !" "\n";

			return false;
		}

		constexpr auto RequiredFields = ZIP_STAT_SIZE | ZIP_STAT_COMP_SIZE;

		if ( (stat.valid & RequiredFields) != RequiredFields )
		{
			std::cerr << ClassId << " : The entry " << entryName << " has no size information, refused !" "\n";

			return false;
		}

		if ( entryName.ends_with('/') )
		{
			return true;
		}

		/* NOTE: The declared size comes from the ARCHIVE. It is bounded before the allocation: a few bytes could
		 * declare a terabyte (std::bad_alloc -> abort under -fno-exceptions). Divided, never multiplied (overflow). */
		if ( stat.size / MaxCompressionRatio > stat.comp_size || stat.size > buffer.max_size() )
		{
			std::cerr << ClassId << " : The entry " << entryName << " declares " << stat.size << " bytes for " << stat.comp_size << " compressed, refused !" "\n";

			return false;
		}

		const std::unique_ptr< zip_file_t, decltype(&zip_fclose) > file{zip_fopen(m_zip.get(), entryName.c_str(), 0), &zip_fclose};

		if ( file == nullptr )
		{
			std::cerr << ClassId << " : Unable to open the entry " << entryName << " (" << zip_strerror(m_zip.get()) << ") !" "\n";

			return false;
		}

		buffer.resize(static_cast< size_t >(stat.size));

		const auto bytesRead = zip_fread(file.get(), buffer.data(), stat.size);

		/* NOTE: libzip verifies the CRC when it reaches the end of the data: one more read must report that end (0). */
		std::array< char, 1 > beyond{};

		if ( std::cmp_not_equal(bytesRead, stat.size) || zip_fread(file.get(), beyond.data(), beyond.size()) != 0 )
		{
			std::cerr << ClassId << " : Unable to read the entry " << entryName << " whole (" << zip_file_strerror(file.get()) << ") !" "\n";

			buffer.clear();

			return false;
		}

		return true;
	}

	bool
	ZipReader::extract (const std::string & entryName, const std::filesystem::path & destinationPath, bool overwrite) const noexcept
	{
		if ( std::ranges::find(m_entryNames, entryName) == m_entryNames.cend() )
		{
			std::cerr << ClassId << " : The entry " << entryName << " doesn't exists !" "\n";

			return false;
		}

		/* NOTE: Complete the final destination. This can hold subdirectories. The entry name comes from the ARCHIVE:
		 * "../" or an absolute name would write outside the destination ("Zip Slip"). */
		const auto confined = IO::confinedPath(destinationPath, u8path(entryName));

		if ( !confined )
		{
			std::cerr << ClassId << " : The entry " << entryName << " would be extracted outside " << toU8String(destinationPath) << ", refused !" "\n";

			return false;
		}

		const auto & filepath = *confined;

		/* NOTE: A directory entry ("name/") creates the directory. */
		if ( entryName.ends_with('/') )
		{
			if ( IO::directoryExists(filepath) )
			{
				return true;
			}

			if ( !IO::createDirectory(filepath) )
			{
				std::cerr << ClassId << " : Unable to create the directory " << toU8String(filepath) << " !" "\n";

				return false;
			}

			return true;
		}

		/* NOTE: Extracted in memory FIRST: a refused entry must leave no file behind. */
		std::vector< char > buffer;

		if ( !this->extract(entryName, buffer) )
		{
			std::cerr << ClassId << " : Unable to extract the content buffer !" "\n";

			return false;
		}

		/* NOTE: Check the final destination. */
		const auto finalDirectory = filepath.parent_path();

		if ( IO::exists(finalDirectory) )
		{
			if ( !IO::directoryExists(finalDirectory) )
			{
				std::cerr << ClassId << " : Unable to use the destination directory " << toU8String(finalDirectory) << " !" "\n";

				return false;
			}

			if ( IO::exists(filepath) && !overwrite )
			{
				std::cerr << ClassId << " : The destination file " << toU8String(filepath) << " already exists !" "\n";

				return false;
			}
		}
		else
		{
			if ( !IO::createDirectory(finalDirectory) )
			{
				std::cerr << ClassId << " : Unable to create the destination directory " << toU8String(finalDirectory) << " !" "\n";

				return false;
			}
		}

		/* NOTE: Through IO::openOutputFile(): a FIFO in the destination blocked the open forever (2026-10-18). */
		std::ofstream file;

		if ( !IO::openOutputFile(filepath, file) )
		{
			std::cerr << ClassId << " : Unable to open the file " << toU8String(filepath) << " for writing !" "\n";

			return false;
		}

		file.write(buffer.data(), static_cast< std::streamsize >(buffer.size()));
		file.close();

		if ( file.fail() )
		{
			std::cerr << ClassId << " : Unable to write the file " << toU8String(filepath) << " !" "\n";

			return false;
		}

		return true;
	}

	bool
	ZipReader::extractAll (const std::filesystem::path & destinationPath, bool overwrite) const noexcept
	{
		size_t fileExtracted = 0;

		/* NOTE: Check the main destination. */
		if ( IO::exists(destinationPath) )
		{
			if ( !IO::directoryExists(destinationPath) )
			{
				std::cerr << ClassId << " : Unable to use the destination directory " << toU8String(destinationPath) << " !" "\n";

				return false;
			}
		}
		else
		{
			if ( !IO::createDirectory(destinationPath) )
			{
				std::cerr << ClassId << " : Unable to create the destination directory " << toU8String(destinationPath) << " !" "\n";

				return false;
			}
		}

		for ( const auto & entryName : m_entryNames )
		{
			if ( this->extract(entryName, destinationPath, overwrite) )
			{
				fileExtracted++;
			}
		}

		return fileExtracted == m_entryNames.size();
	}

	bool
	ZipReader::openArchive () noexcept
	{
		/* NOTE: libzip opens the path itself: a FIFO blocked that open forever (2026-10-18). */
		if ( !IO::isReadableRegularFile(m_filepath) )
		{
			return false;
		}

#if IS_WINDOWS
		zip_error_t zipError;
		zip_error_init(&zipError);

		auto * source = zip_source_win32w_create(m_filepath.wstring().c_str(), 0, -1, &zipError);

		if ( source == nullptr )
		{
			std::cerr << ClassId << " : Unable to create LibZip source. Error : " << zip_error_strerror(&zipError) << " !" "\n";

			zip_error_fini(&zipError);

			return false;
		}

		m_zip.reset(zip_open_from_source(source, 0, &zipError));

		if ( !m_zip )
		{
			std::cerr << ClassId << " : Unable to init LibZip. Error : " << zip_error_strerror(&zipError) << " !" "\n";

			zip_source_free(source);
			zip_error_fini(&zipError);

			return false;
		}

		zip_error_fini(&zipError);
#else
		int errorCode = 0;

		m_zip.reset(zip_open(toU8String(m_filepath).data(), 0, &errorCode));

		if ( !m_zip )
		{
			zip_error_t error;
			zip_error_init_with_code(&error, errorCode);

			std::cerr << ClassId << " : Unable to init LibZip. Error : " << zip_error_strerror(&error) << " !" "\n";

			zip_error_fini(&error);

			return false;
		}
#endif

		return true;
	}

	void
	ZipReader::readArchive () noexcept
	{
		const auto entryCount = zip_get_num_entries(m_zip.get(), 0);

		struct zip_stat stat{};

		for ( zip_int64_t index = 0; index < entryCount; index++)
		{
			/* NOTE: index is in [0, entryCount[, never negative. */
			/* NOTE: The name is only valid when libzip says so (a null name would be undefined behaviour here). */
			if ( zip_stat_index(m_zip.get(), static_cast< zip_uint64_t >(index), 0, &stat ) == 0 && (stat.valid & ZIP_STAT_NAME) != 0 && stat.name != nullptr )
			{
				m_entryNames.emplace_back(stat.name);
			}
		}
	}
}
