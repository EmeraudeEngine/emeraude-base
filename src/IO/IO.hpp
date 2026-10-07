/*
 * src/IO/IO.hpp
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

/* Project configuration. */
#include "emeraude_platform.hpp"

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "String.hpp"

namespace EmEn::Base::IO
{
#if IS_WINDOWS
	constexpr char Separator = '\\';
#else
	constexpr char Separator = '/';
#endif

	/**
	 * @brief From this length (native characters), systemPath() gives a path the Win32 extended-length form on Windows:
	 * 248 = MAX_PATH − 12, the CreateDirectory() limit (room for an 8.3 file name).
	 */
	constexpr size_t WindowsLongPathThreshold{248};

	/**
	 * @brief Returns the Win32 extended-length form of an ABSOLUTE Windows path: C:\a → \\?\C:\a,
	 * \\server\share\a → \\?\UNC\server\share\a. Forward slashes become backslashes: the prefix turns
	 * off every other normalisation, so the path must also be lexically normal (no "." / "..").
	 * @note Pure string work, compiled on every platform for its tests; systemPath() applies it on Windows only. A path
	 * already extended (or a \\.\ device path) comes back unchanged; so does a relative, drive-relative or
	 * drive-less rooted one, which has no extended form.
	 * @tparam char_t The character type (wchar_t for a Windows native path).
	 * @param absolutePath An absolute, lexically normal path.
	 * @return std::basic_string< char_t >
	 */
	template< typename char_t >
	[[nodiscard]]
	std::basic_string< char_t >
	windowsExtendedLengthPath (std::basic_string_view< char_t > absolutePath) noexcept
	{
		const auto at = [absolutePath] (size_t index) noexcept -> char_t {
			return index < absolutePath.size() ? absolutePath[index] : char_t{0};
		};
		const auto isSeparator = [] (char_t character) noexcept {
			return character == static_cast< char_t >('\\') || character == static_cast< char_t >('/');
		};
		const auto isDriveLetter = [] (char_t character) noexcept {
			return (character >= static_cast< char_t >('A') && character <= static_cast< char_t >('Z')) || (character >= static_cast< char_t >('a') && character <= static_cast< char_t >('z'));
		};

		/* Already extended ("\\?\"), or a device path ("\\.\"). */
		if ( isSeparator(at(0)) && isSeparator(at(1)) && (at(2) == static_cast< char_t >('?') || at(2) == static_cast< char_t >('.')) && isSeparator(at(3)) )
		{
			return std::basic_string< char_t >{absolutePath};
		}

		std::string_view prefix;
		size_t skipped = 0;

		if ( isSeparator(at(0)) && isSeparator(at(1)) )
		{
			/* UNC: "\\server\share" → "\\?\UNC\server\share". */
			prefix = R"(\\?\UNC\)";
			skipped = 2;
		}
		else if ( isDriveLetter(at(0)) && at(1) == static_cast< char_t >(':') && isSeparator(at(2)) )
		{
			prefix = R"(\\?\)";
		}
		else
		{
			return std::basic_string< char_t >{absolutePath};
		}

		std::basic_string< char_t > result;
		result.reserve(prefix.size() + absolutePath.size() - skipped);

		for ( const auto character : prefix )
		{
			result.push_back(static_cast< char_t >(character));
		}

		for ( const auto character : absolutePath.substr(skipped) )
		{
			result.push_back(isSeparator(character) ? static_cast< char_t >('\\') : character);
		}

		return result;
	}

	/**
	 * @brief Returns the form of a path every IO:: wrapper hands to the system.
	 * @note Windows: a path whose absolute form reaches WindowsLongPathThreshold characters is made absolute, lexically
	 * normal, and given the extended-length form (\\?\), so it works past MAX_PATH (260) without the system's
	 * LongPathsEnabled. Any other path, and every path elsewhere, comes back unchanged. A walk of an extended directory
	 * (forEachDirectoryEntry()) yields extended entry paths.
	 * @param path A path.
	 * @return std::filesystem::path
	 */
	[[nodiscard]]
	std::filesystem::path systemPath (const std::filesystem::path & path) noexcept;

	/**
	 * @brief Renames (moves) a file or a directory, replacing an existing target file — the commit of a "write aside,
	 * then rename" save. Never throws (std::error_code overload), works past MAX_PATH on Windows (systemPath()).
	 * @param from The current path.
	 * @param to The new path.
	 * @return bool False when a path is empty or the system refuses (logged).
	 */
	[[nodiscard]]
	bool renameFile (const std::filesystem::path & from, const std::filesystem::path & to) noexcept;

	/**
	 * @brief Checks if a file exists on disk.
	 *
	 * Verifies that the specified path exists and is a regular file (not a directory,
	 * symlink, or other special file type).
	 *
	 * @param filepath Path to the file to check.
	 * @return True if the file exists and is a regular file, false otherwise.
	 * @note Returns false if the path is empty or if an error occurs during the check.
	 */
	[[nodiscard]]
	bool fileExists (const std::filesystem::path & filepath) noexcept;

	/**
	 * @brief Returns the size of a file in bytes.
	 *
	 * Retrieves the size of the specified file using the filesystem API.
	 * Errors are logged to stderr.
	 *
	 * @param filepath Path to the file.
	 * @return Size of the file in bytes, or 0 if an error occurs.
	 * @note Returns 0 if the file does not exist or cannot be accessed.
	 */
	[[nodiscard]]
	size_t filesize (const std::filesystem::path & filepath) noexcept;

	/**
	 * @brief Creates an empty file at the specified location.
	 *
	 * Creates a new empty file at the given path. If the file already exists,
	 * it will be truncated (emptied).
	 *
	 * @param filepath Path where the file should be created.
	 * @return True if the file was successfully created or opened, false otherwise.
	 * @note Returns false if the path is empty. Parent directories must exist.
	 */
	bool createFile (const std::filesystem::path & filepath) noexcept;

	/**
	 * @brief Deletes a file from disk.
	 *
	 * Permanently removes the specified file. The path must point to a regular file,
	 * not a directory or special file.
	 *
	 * @param filepath Path to the file to delete.
	 * @return True if the file was successfully deleted, false otherwise.
	 * @warning This is a destructive operation that cannot be undone.
	 * @note Returns false if the path is empty, not a regular file, or deletion fails.
	 */
	bool eraseFile (const std::filesystem::path & filepath) noexcept;

	/**
	 * @brief Checks if a directory exists on disk.
	 *
	 * Verifies that the specified path exists and is a directory.
	 *
	 * @param path Path to the directory to check.
	 * @return True if the path exists and is a directory, false otherwise.
	 * @note Returns false if the path is empty or points to a non-directory.
	 *	   Error messages are suppressed for non-existent directories (error code 2).
	 */
	[[nodiscard]]
	bool directoryExists (const std::filesystem::path & path) noexcept;

	/**
	 * @brief Checks whether a directory is empty.
	 *
	 * Determines if the specified directory contains no files or subdirectories.
	 *
	 * @param path Path to the directory to check.
	 * @return True if the directory is empty, false otherwise.
	 * @note Returns false if the path is empty or an error occurs.
	 *	   The directory must exist for this function to work correctly.
	 */
	[[nodiscard]]
	bool isDirectoryContentEmpty (const std::filesystem::path & path) noexcept;
	
	/**
	 * @brief Returns a list of all entries in a directory.
	 *
	 * Retrieves all files and subdirectories within the specified directory.
	 * This is a non-recursive operation (does not traverse subdirectories).
	 *
	 * @param path Path to the directory to list.
	 * @return Vector containing paths to all entries in the directory. Returns empty vector if an error occurs.
	 * @note Errors during iteration are logged to stderr. The directory must exist.
	 */
	[[nodiscard]]
	std::vector< std::filesystem::path > directoryEntries (const std::filesystem::path & path) noexcept;

	/**
	 * @brief Joins a RELATIVE path under a base directory, refusing any path that could leave it (lexical check).
	 * @note Refused: an empty path, an absolute path, a root name or root directory (C:, \\server, /), and a path whose
	 * lexically_normal() form starts with "..". std::filesystem::path::append() REPLACES the base with an absolute
	 * path, and "../" walks out of it: every path that comes from data (a resource definition, a ZIP entry name)
	 * goes through here. A symbolic link placed INSIDE the base is not resolved (no system call).
	 * @param base The directory the result must stay in.
	 * @param relative The path to join, as written in the data.
	 * @return std::optional< std::filesystem::path > The joined path, or nothing when it would leave the base.
	 */
	[[nodiscard]]
	std::optional< std::filesystem::path > confinedPath (const std::filesystem::path & base, const std::filesystem::path & relative) noexcept;

	/**
	 * @brief Logs a failed directory walk (the non-template half of forEachDirectoryEntry()).
	 * @param path The walked directory.
	 * @param errorCode The error.
	 * @return void
	 */
	void logDirectoryWalkError (const std::filesystem::path & path, const std::error_code & errorCode) noexcept;

	/**
	 * @brief Visits every entry of a directory — recursively on demand — WITHOUT EVER THROWING.
	 * @note Under -fno-exceptions a range-for over std::filesystem::directory_iterator terminates the process on the
	 * first filesystem error: its constructor without std::error_code AND its operator++ throw. This walk uses the
	 * error_code constructor and increment(error_code) only. A recursive walk skips the directories it may not enter
	 * (directory_options::skip_permission_denied). A directory_entry's own queries (is_regular_file(), file_size(), …)
	 * throw too: call their std::error_code overloads in the visitor.
	 * @tparam visitor_t A callable `bool (const std::filesystem::directory_entry &)`: return false to stop the walk.
	 * @param path The directory.
	 * @param recursive Whether to enter the sub-directories.
	 * @param visitor The visitor.
	 * @return bool True when the walk reached its end or the visitor stopped it, false on a filesystem error (logged).
	 */
	template< typename visitor_t >
	requires std::is_invocable_r_v< bool, visitor_t &, const std::filesystem::directory_entry & >
	[[nodiscard]]
	bool
	forEachDirectoryEntry (const std::filesystem::path & path, bool recursive, visitor_t && visitor) noexcept
	{
		std::error_code errorCode;

		const auto walk = [&] (auto iterator) -> bool {
			if ( errorCode ) [[unlikely]]
			{
				logDirectoryWalkError(path, errorCode);

				return false;
			}

			const decltype(iterator) end{};

			while ( iterator != end )
			{
				if ( !visitor(*iterator) )
				{
					return true;
				}

				iterator.increment(errorCode);

				if ( errorCode ) [[unlikely]]
				{
					logDirectoryWalkError(path, errorCode);

					return false;
				}
			}

			return true;
		};

		const auto directory = systemPath(path);

		if ( recursive )
		{
			return walk(std::filesystem::recursive_directory_iterator{directory, std::filesystem::directory_options::skip_permission_denied, errorCode});
		}

		return walk(std::filesystem::directory_iterator{directory, errorCode});
	}

	/**
	 * @brief Creates a directory and all necessary parent directories.
	 *
	 * Creates the specified directory along with any missing parent directories
	 * in the path. If the directory already exists, the operation succeeds.
	 *
	 * @param path Path to the directory to create.
	 * @param removeFileSection If true, treats the last component as a filename and creates
	 *		only the parent directory path. Useful when path includes a filename. Default false.
	 * @return True if the directory was created or already exists, false on error.
	 * @note Returns false if the path is empty or directory creation fails.
	 */
	bool createDirectory (const std::filesystem::path & path, bool removeFileSection = false) noexcept;

	/**
	 * @brief Deletes a directory from disk.
	 *
	 * Removes the specified directory. By default, only empty directories can be removed.
	 * With recursive mode enabled, all contents are deleted.
	 *
	 * @param path Path to the directory to delete.
	 * @param recursive If true, deletes the directory and all its contents recursively.
	 *		If false, only deletes empty directories. Default false.
	 * @return True if the directory was successfully deleted, false otherwise.
	 * @warning This is a destructive operation that cannot be undone. Use recursive mode with caution.
	 * @note Returns false if the path is empty, not a directory, or deletion fails.
	 */
	bool eraseDirectory (const std::filesystem::path & path, bool recursive = false) noexcept;

	/**
	 * @brief Returns the current working directory of the process.
	 *
	 * Retrieves the absolute path of the directory from which the application
	 * was launched or to which it has changed.
	 *
	 * @return Path to the current working directory. Returns empty path if an error occurs.
	 * @note Errors are logged to stderr.
	 */
	[[nodiscard]]
	std::filesystem::path getCurrentWorkingDirectory () noexcept;

	/**
	 * @brief Checks if a path exists on disk.
	 *
	 * Verifies that the specified path exists, regardless of whether it is a file,
	 * directory, symlink, or other filesystem entity.
	 *
	 * @param path Path to check for existence.
	 * @return True if the path exists, false otherwise.
	 * @note Returns false if the path is empty or an error occurs.
	 * @see fileExists, directoryExists
	 */
	[[nodiscard]]
	bool exists (const std::filesystem::path & path) noexcept;

	/**
	 * @brief Checks whether the application has read permission for the path.
	 *
	 * Tests if the current process has permission to read from the specified path.
	 *
	 * @param path Path to check for read permission.
	 * @return True if the path is readable by the application, false otherwise.
	 * @note Returns false if the path is empty. On Linux/macOS, uses access() with R_OK;
	 * on Windows, an AccessCheck against the file's security descriptor (GENERIC_READ).
	 * @see writable, executable
	 */
	[[nodiscard]]
	bool readable (const std::filesystem::path & path) noexcept;

	/**
	 * @brief Checks whether the application has write permission for the path.
	 *
	 * Tests if the current process has permission to write to the specified path.
	 *
	 * @param path Path to check for write permission.
	 * @return True if the path is writable by the application, false otherwise.
	 * @note Returns false if the path is empty. On Linux/macOS, uses access() with W_OK;
	 * on Windows, an AccessCheck against the file's security descriptor (GENERIC_WRITE).
	 * @see readable, executable
	 */
	[[nodiscard]]
	bool writable (const std::filesystem::path & path) noexcept;

	/**
	 * @brief Checks whether the application has execute permission for the path.
	 *
	 * Tests if the current process has permission to execute the specified path.
	 *
	 * @param path Path to check for execute permission.
	 * @return True if the path is executable by the application, false otherwise.
	 * @note Returns false if the path is empty. On Linux/macOS, uses access() with X_OK;
	 * on Windows, an AccessCheck against the file's security descriptor (GENERIC_EXECUTE).
	 * @see readable, writable
	 */
	[[nodiscard]]
	bool executable (const std::filesystem::path & path) noexcept;

	/**
	 * @brief Extracts the file extension from a path.
	 *
	 * Returns the file extension without the leading dot. For example,
	 * "file.txt" returns "txt", "archive.tar.gz" returns "gz".
	 *
	 * @param filepath Path to extract the extension from.
	 * @param forceToLower If true, converts the extension to lowercase. Default false.
	 * @return The file extension as a string, or empty string if no extension exists.
	 * @note The leading dot is automatically removed from the extension.
	 */
	[[nodiscard]]
	std::string getFileExtension (const std::filesystem::path & filepath, bool forceToLower = false) noexcept;

	/**
	 * @brief Reads a file and returns its content as a string.
	 *
	 * Reads the entire file into memory and stores it in the provided string.
	 * The file is opened in binary mode to preserve exact content.
	 *
	 * @param filepath Path to the file to read.
	 * @param[out] content String that will be filled with the file contents.
	 *		The string is resized to match the file size.
	 * @return True if the file was successfully read, false otherwise.
	 * @note Returns false if the path is empty, file cannot be opened, or read fails.
	 *	   Errors are logged to stderr.
	 * @see fileGetContents(const std::filesystem::path&, std::vector<data_t>&)
	 */
	bool fileGetContents (const std::filesystem::path & filepath, std::string & content) noexcept;

	/**
	 * @brief Writes a string to a file.
	 *
	 * Writes the provided string content to a file. Can optionally append to existing
	 * content or create parent directories as needed. The file is opened in binary mode.
	 *
	 * @param filepath Path to the file to write.
	 * @param content String view containing the data to write.
	 * @param append If true, appends to the file instead of overwriting. Default false.
	 * @param createDirectories If true, creates parent directories if they don't exist. Default false.
	 * @return True if the content was successfully written, false otherwise.
	 * @note Returns false if the path is empty, directory creation fails (when requested),
	 *	   file cannot be opened, or write fails. Errors are logged to stderr.
	 * @see filePutContents(const std::filesystem::path&, const container_t&, bool, bool)
	 */
	bool filePutContents (const std::filesystem::path & filepath, std::string_view content, bool append = false, bool createDirectories = false) noexcept;

	/**
	 * @brief Reads a file and returns its binary content in a vector.
	 *
	 * Template function that reads a file as binary data and stores it in a vector
	 * of the specified type. The vector is automatically resized to accommodate the
	 * file contents, with proper alignment for the data type.
	 *
	 * @tparam data_t The type of data elements to store (e.g., uint32_t, char, std::byte).
	 * @param filepath Path to the file to read.
	 * @param[out] content Vector that will be filled with the file contents.
	 *		The vector is resized to fit the file data, accounting for element size.
	 * @return True if the file was successfully read, false otherwise.
	 * @note Returns false if the path is empty, file cannot be opened, or read fails.
	 *	   If file size is not evenly divisible by sizeof(data_t), the vector is
	 *	   sized up to accommodate partial elements. Errors are logged to stderr.
	 * @see fileGetContents(const std::filesystem::path&, std::string&)
	 */
	template< typename data_t >
	bool
	fileGetContents (const std::filesystem::path & filepath, std::vector< data_t > & content) noexcept
	{
		if ( filepath.empty() ) [[unlikely]]
		{
			return false;
		}

		std::ifstream file{systemPath(filepath), std::ios::binary | std::ios::ate};

		if ( !file.is_open() ) [[unlikely]]
		{
			Logging::error("IO", "fileGetContents: cannot open " + filepath.string());

			return false;
		}

		/* NOTE: Read the file size. */
		const auto bytes = file.tellg();

		if ( bytes < 0 ) [[unlikely]]
		{
			Logging::error("IO", "fileGetContents: cannot read the size of " + filepath.string());

			return false;
		}

		file.seekg(0, std::ifstream::beg);

		content.resize((static_cast< size_t >(bytes) / sizeof(data_t)) + (static_cast< size_t >(bytes) % sizeof(data_t) != 0 ? 1U : 0U));

		file.read(reinterpret_cast< char * >(content.data()), bytes);

		if ( !file ) [[unlikely]]
		{
			Logging::error("IO", "fileGetContents: read error on " + filepath.string());

			return false;
		}

		return true;
	}

	/**
	 * @brief Reads a byte range of a file: `length` bytes from `offset`.
	 * @note For data stored inside a container file (an image in a .glb's BIN chunk, an uncompressed archive entry):
	 * only the range is read. A range reaching past the end of the file is refused (the file changed, or the range is
	 * wrong), never shortened.
	 * @param filepath Path to the file to read.
	 * @param offset The first byte of the range.
	 * @param length The number of bytes. 0 is refused.
	 * @param[out] content The bytes, exactly `length` of them.
	 * @return bool False when the path is empty, the file cannot be opened, the range is empty or out of the file, or
	 * the read fails; the content is then left empty.
	 */
	inline
	bool
	fileGetRange (const std::filesystem::path & filepath, uint64_t offset, uint64_t length, std::vector< std::byte > & content) noexcept
	{
		content.clear();

		if ( filepath.empty() || length == 0 ) [[unlikely]]
		{
			return false;
		}

		std::ifstream file{systemPath(filepath), std::ios::binary | std::ios::ate};

		if ( !file.is_open() ) [[unlikely]]
		{
			Logging::error("IO", "fileGetRange: cannot open " + filepath.string());

			return false;
		}

		const std::streamoff fileSize = file.tellg();

		if ( fileSize < 0 ) [[unlikely]]
		{
			Logging::error("IO", "fileGetRange: cannot read the size of " + filepath.string());

			return false;
		}

		const auto fileBytes = static_cast< uint64_t >(fileSize);

		/* NOTE: Overflow-safe: offset <= size and length <= size - offset. */
		if ( offset > fileBytes || length > fileBytes - offset ) [[unlikely]]
		{
			Logging::error("IO", "fileGetRange: the range [" + std::to_string(offset) + ", +" + std::to_string(length) + ") is out of " + filepath.string());

			return false;
		}

		if ( length > content.max_size() ) [[unlikely]]
		{
			return false;
		}

		content.resize(static_cast< size_t >(length));

		file.seekg(static_cast< std::streamoff >(offset), std::ifstream::beg);
		file.read(reinterpret_cast< char * >(content.data()), static_cast< std::streamsize >(length));

		if ( !file ) [[unlikely]]
		{
			Logging::error("IO", "fileGetRange: read error on " + filepath.string());

			content.clear();

			return false;
		}

		return true;
	}

	/**
	 * @brief Writes binary data from a container to a file.
	 *
	 * Template function that writes binary data from any contiguous container
	 * (vector, array, span, etc.) to a file. Can optionally append to existing
	 * content or create parent directories as needed.
	 *
	 * @tparam container_t A contiguous range type (std::vector, std::array, std::span, C-array, etc.).
	 * @param filepath Path to the file to write.
	 * @param content Container holding the binary data to write.
	 * @param append If true, appends to the file instead of overwriting. Default false.
	 * @param createDirectories If true, creates parent directories if they don't exist. Default false.
	 * @return True if the data was successfully written, false otherwise.
	 * @note Returns false if the path is empty, directory creation fails (when requested),
	 *	   file cannot be opened, or write fails. Errors are logged to stderr.
	 *	   The total bytes written equals container size multiplied by element size.
	 * @see filePutContents(const std::filesystem::path&, std::string_view, bool, bool)
	 */
	template< std::ranges::contiguous_range container_t >
	bool
	filePutContents (const std::filesystem::path & filepath, const container_t & content, bool append = false, bool createDirectories = false) noexcept
	{
		if ( filepath.empty() ) [[unlikely]]
		{
			return false;
		}

		if ( createDirectories && !IO::createDirectory(filepath, true) ) [[unlikely]]
		{
			return false;
		}

		std::ofstream file{systemPath(filepath), std::ios::binary | (append ? std::ios::app : std::ios::trunc)};

		if ( !file.is_open() ) [[unlikely]]
		{
			Logging::error("IO", "filePutContents: cannot open " + filepath.string() + " for writing");

			return false;
		}

		file.write(reinterpret_cast< const char * >(std::ranges::data(content)), static_cast< std::streamsize >(std::ranges::size(content) * sizeof(std::ranges::range_value_t< container_t >)));

		if ( !file ) [[unlikely]]
		{
			Logging::error("IO", "filePutContents: write error on " + filepath.string());

			return false;
		}

		return true;
	}

	/**
	 * @brief Checks if a directory is ready to use and writable.
	 *
	 * Convenience function that verifies a directory exists and is writable,
	 * or creates it if it doesn't exist. Useful for ensuring cache and output
	 * directories are accessible before use.
	 *
	 * @param path Path to the directory to check or create.
	 * @return True if the directory exists and is writable, or was successfully created.
	 *		 False if creation fails or the directory is not writable.
	 * @note This function combines directoryExists(), writable(), and createDirectory().
	 *	   Particularly useful for cache directories and temporary storage locations.
	 * @see directoryExists, writable, createDirectory
	 */
	[[nodiscard]]
	inline
	bool
	isDirectoryUsable (const std::filesystem::path & path) noexcept
	{
		if ( IO::directoryExists(path) )
		{
			return IO::writable(path);
		}

		return IO::createDirectory(path);
	}

	/**
	 * @brief Construct a std::filesystem::path from a UTF-8 encoded std::string.
	 * On Windows, through String::utf8ToUTF16() into the native wide path: the ANSI code page is bypassed, and invalid
	 * UTF-8 becomes U+FFFD (the char8_t constructor THREW std::system_error on it, an abort — triad 15, 2026-10-01).
	 * On POSIX, direct construction (native encoding is already UTF-8).
	 * @param UTF8String A reference to a string.
	 * @return std::filesystem::path
	 */
	[[nodiscard]]
	inline
	std::filesystem::path
	u8path (const std::string & UTF8String)
	{
		if constexpr ( IsWindows )
		{
			const auto UTF16String = String::utf8ToUTF16(UTF8String);

			return std::filesystem::path{std::wstring{UTF16String.begin(), UTF16String.end()}};
		}
		else
		{
			return {UTF8String};
		}
	}

	/**
	 * @brief Convert a path to a UTF-8 encoded std::string (native separators).
	 * @note On Windows, from the native wide string through String::utf16ToUTF8(): a lone surrogate (NTFS allows one)
	 * becomes U+FFFD (path::u8string() THREW on it, an abort — triad 15, 2026-10-01).
	 * @param path A reference to a filesystem path.
	 * @return std::string
	 */
	[[nodiscard]]
	inline
	std::string
	toU8String (const std::filesystem::path & path)
	{
		if constexpr ( IsWindows )
		{
			const auto & native = path.native();

			return String::utf16ToUTF8(std::u16string{native.begin(), native.end()});
		}
		else
		{
			return path.string();
		}
	}

	/**
	 * @brief Convert a path to a UTF-8 encoded std::string with forward slashes.
	 * @param path A reference to a filesystem path.
	 * @return std::string
	 */
	[[nodiscard]]
	inline
	std::string
	toGenericU8String (const std::filesystem::path & path)
	{
		if constexpr ( IsWindows )
		{
			/* NOTE: generic_wstring() only swaps the separators of the native wide string (no conversion, no throw). */
			const auto generic = path.generic_wstring();

			return String::utf16ToUTF8(std::u16string{generic.begin(), generic.end()});
		}
		else
		{
			return path.generic_string();
		}
	}
}
