/*
 * src/IO/ZipWriter.hpp
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

#pragma once

/* STL inclusions. */
#include <filesystem>
#include <memory>
#include <vector>

/* Third-party inclusions. */
#include <zip.h>

/* Local inclusions. */
#include "IO.hpp"

namespace EmEn::Base::IO
{
	/**
	 * @brief Class to create a ZIP archive.
	 */
	class ZipWriter final
	{
		public:

			static constexpr auto ClassId{"ZipWriter"};

			/**
			 * @brief Constructs a zip archive writer.
			 * @param filepath A reference to a path.
			 */
			explicit ZipWriter (std::filesystem::path filepath) noexcept;

			/**
			 * @brief Changes the zip archive filepath.
			 * @param filepath A reference to a path.
			 */
			void
			setFilepath (const std::filesystem::path & filepath) noexcept
			{
				this->discardArchive();

				m_filepath = filepath;
			}

			/**
			 * @brief Returns the filepath of the archive.
			 * @return const std::filesystem::path &
			 */
			[[nodiscard]]
			const std::filesystem::path &
			filepath () const noexcept
			{
				return m_filepath;
			}

			/**
			 * @brief Adds a file to compress into the archive with a custom entry name.
			 * @param path A reference to a path.
			 * @param entryName A reference to a string.
			 * @return bool
			 */
			bool addFilepathToSources (const std::filesystem::path & path, const std::string & entryName) noexcept;

			/**
			 * @brief Adds a file to compress into the archive.
			 * @param path A reference to a path.
			 * @return bool
			 */
			bool
			addFilepathToSources (const std::filesystem::path & path) noexcept
			{
				return this->addFilepathToSources(path, IO::toU8String(path.filename()));
			}

			/**
			 * @brief Adds a directory to compress into the archive.
			 * @param path A reference to a path.
			 * @return bool
			 */
			bool addDirectoryToSources (const std::filesystem::path & path) noexcept;

			/**
			 * @brief Creates the archive from the sources, then forgets them.
			 * @note The archive appears on disk only when it is written whole: a source that cannot be read, or a
			 * failed final write (the commit, zip_close()), leaves NO file (2026-10-18: a partial archive was left, and a
			 * failed commit answered true). Each source is read by libzip from its file while writing (memory stays
			 * bounded, owner decision, 2026-10-18), after an IO::isReadableRegularFile() check.
			 * @return bool
			 */
			bool create () noexcept;

		private:

			/**
			 * @brief Opens the ZIP archive.
			 * @return bool
			 */
			[[nodiscard]]
			bool openArchive () noexcept;

			/**
			 * @brief Writes the archive (zip_close()) and forgets the sources.
			 * @return bool False when the write fails: nothing is left on disk.
			 */
			[[nodiscard]]
			bool commitArchive () noexcept;

			/**
			 * @brief Drops the archive without writing anything, and forgets the sources.
			 */
			void discardArchive () noexcept;

			/**
			 * @brief Checks whether an entry name is already taken by a source.
			 * @param entryName A reference to a string.
			 * @return bool
			 */
			[[nodiscard]]
			bool entryNameExists (const std::string & entryName) const noexcept;

			/**
			 * @brief Checks whether a filepath exists in source.
			 * @param filepath A reference to a path.
			 * @return bool
			 */
			[[nodiscard]]
			bool sourceExists (const std::filesystem::path & filepath) const noexcept;

			/* NOTE: Released with zip_discard(): zip_close() WRITES the archive, it is called on purpose by commitArchive()
			 * only (a failed create() used to leave a partial archive through this deleter). */
			std::unique_ptr< zip_t, decltype(&zip_discard) > m_zip{nullptr, &zip_discard};
			std::filesystem::path m_filepath;
			std::vector< std::pair< std::filesystem::path, std::string > > m_sources;
	};
}
