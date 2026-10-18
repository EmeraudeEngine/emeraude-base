/*
 * src/IO/ZipWriter.cpp
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

#include "ZipWriter.hpp"

/* Project configuration. */
#include "emeraude_platform.hpp"

/* STL inclusions. */
#include <algorithm>
#include <iostream>
#include <tuple>
#include <utility>

/* Local inclusions. */
#include "IO.hpp"

namespace EmEn::Base::IO
{
	ZipWriter::ZipWriter (std::filesystem::path filepath) noexcept
		: m_filepath(std::move(filepath))
	{
	}

	bool
	ZipWriter::sourceExists (const std::filesystem::path & filepath) const noexcept
	{
		return std::ranges::any_of(m_sources, [&filepath] (const std::pair< std::filesystem::path, std::string > & source) {
			return source.first == filepath;
		});
	}

	bool
	ZipWriter::addFilepathToSources (const std::filesystem::path & path, const std::string & entryName) noexcept
	{
		/* NOTE: "&&" here used to accept an existing DIRECTORY as a file (the message always said "or"). */
		if ( !IO::fileExists(path) )
		{
			std::cerr << ClassId << " : The path " << toU8String(path) << " is not a regular file or doesn't exists !" "\n";

			return false;
		}

		if ( ZipWriter::sourceExists(path) )
		{
			std::cerr << ClassId << " : The file " << toU8String(path) << " already exists in source !" "\n";

			return false;
		}

		/* NOTE: Two sources under one entry name used to keep the LAST one silently (ZIP_FL_OVERWRITE): refused (owner
		 * decision, 2026-10-18). */
		if ( this->entryNameExists(entryName) )
		{
			std::cerr << ClassId << " : The entry name " << entryName << " is already taken in source !" "\n";

			return false;
		}

		m_sources.emplace_back(path, entryName);

		return true;
	}

	bool
	ZipWriter::addDirectoryToSources (const std::filesystem::path & path) noexcept
	{
		/* NOTE: "&&" here used to accept an existing regular FILE, then walk it (the message always said "or"). */
		if ( !IO::directoryExists(path) )
		{
			std::cerr << ClassId << " : The path " << toU8String(path) << " is not a directory or doesn't exists !" "\n";

			return false;
		}

		bool duplicate = false;

		const auto walked = IO::forEachDirectoryEntry(path, true, [this, &path, &duplicate] (const std::filesystem::directory_entry & entry) {
			std::error_code errorCode;

			if ( !entry.is_regular_file(errorCode) )
			{
				return true;
			}

			if ( ZipWriter::sourceExists(entry.path()) )
			{
				std::cerr << ClassId << " : The file " << toU8String(entry.path()) << " already exists in source !" "\n";

				duplicate = true;

				return false;
			}

			const auto relativePath = std::filesystem::relative(entry.path(), path, errorCode);

			if ( errorCode )
			{
				std::cerr << ClassId << " : Unable to name the entry " << toU8String(entry.path()) << " relative to " << toU8String(path) << " (" << errorCode.message() << ") !" "\n";

				return true;
			}

			auto entryName = toGenericU8String(relativePath);

			if ( this->entryNameExists(entryName) )
			{
				std::cerr << ClassId << " : The entry name " << entryName << " is already taken in source !" "\n";

				duplicate = true;

				return false;
			}

			m_sources.emplace_back(entry.path(), std::move(entryName));

			return true;
		});

		return walked && !duplicate;
	}

	bool
	ZipWriter::create () noexcept
	{
		if ( IO::exists(m_filepath) )
		{
			std::cerr << ClassId << " : The archive file " << toU8String(m_filepath) << " already exists !" "\n";

			return false;
		}

		if ( !this->openArchive() )
		{
			std::cerr << ClassId << " : Unable to create the archive " << toU8String(m_filepath) << " !" "\n";

			return false;
		}

		for ( const auto & [filepath, entryName] : m_sources )
		{
			/* NOTE: libzip opens the file itself when it writes: the regular-file check comes right before. */
			if ( !IO::isReadableRegularFile(filepath) )
			{
				std::cerr << ClassId << " : Unable to read the source " << toU8String(filepath) << " !" "\n";

				this->discardArchive();

				return false;
			}

			/* NOTE: A file source: libzip reads it while writing the archive, one source at a time (every source used to
			 * be read into memory first and kept until the end). */
#if IS_WINDOWS
			/* NOTE: The temporary wstring lives for this statement only: libzip copies the name when it creates the
			 * source. Never keep that c_str() (Windows peer, 2026-10-18). */
			zip_source_t * source = zip_source_win32w(m_zip.get(), filepath.wstring().c_str(), 0, ZIP_LENGTH_TO_END);
#else
			zip_source_t * source = zip_source_file(m_zip.get(), toU8String(filepath).c_str(), 0, ZIP_LENGTH_TO_END);
#endif

			if ( source == nullptr )
			{
				std::cerr << ClassId << " : Failed to create the source of " << toU8String(filepath) << " ! Error : " << zip_strerror(m_zip.get()) << "\n";

				this->discardArchive();

				return false;
			}

			/* NOTE: The entry names are unique (checked when the sources are added): no overwrite flag. */
			if ( zip_file_add(m_zip.get(), entryName.c_str(), source, 0) < 0 )
			{
				std::cerr << ClassId << " : Failed to add " << entryName << " to the archive ! Error : " << zip_strerror(m_zip.get()) << "\n";

				/* NOTE: A source that was not added still belongs to the caller. */
				zip_source_free(source);

				this->discardArchive();

				return false;
			}
		}

		return this->commitArchive();
	}

	bool
	ZipWriter::openArchive () noexcept
	{
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

		m_zip.reset(zip_open_from_source(source, ZIP_CREATE, &zipError));

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

		m_zip.reset(zip_open(toU8String(m_filepath).data(), ZIP_CREATE, &errorCode));

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

	bool
	ZipWriter::commitArchive () noexcept
	{
		m_sources.clear();

		if ( !m_zip )
		{
			return false;
		}

		/* NOTE: zip_close() writes the archive: on success it frees the handle, on failure the handle stays ours (freed
		 * by zip_discard() through the deleter) and libzip leaves no file. Its result used to be ignored: a write that
		 * failed answered true (2026-10-18). */
		if ( zip_close(m_zip.get()) != 0 )
		{
			std::cerr << ClassId << " : Unable to write the archive " << toU8String(m_filepath) << " ! Error : " << zip_strerror(m_zip.get()) << "\n";

			m_zip.reset();

			return false;
		}

		/* NOTE: A successful zip_close() freed the handle: the pointer is dropped, not freed again. */
		std::ignore = m_zip.release();

		return true;
	}

	void
	ZipWriter::discardArchive () noexcept
	{
		m_zip.reset();

		m_sources.clear();
	}

	bool
	ZipWriter::entryNameExists (const std::string & entryName) const noexcept
	{
		return std::ranges::any_of(m_sources, [&entryName] (const std::pair< std::filesystem::path, std::string > & source) {
			return source.second == entryName;
		});
	}
}
