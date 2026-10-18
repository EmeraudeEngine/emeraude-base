/*
 * src/IO/IO.cpp
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

#include "IO.hpp"

/* Local inclusions. */
#include "Logging/Logging.hpp"

/* Project configuration. */
#include "emeraude_platform.hpp"

/* STL inclusions. */
#include <algorithm>
#include <atomic>
#include <chrono>
#include <random>
#include <string>

/* Third-party inclusions. */
#if IS_LINUX || IS_MACOS
	#include <unistd.h>
#endif

#if IS_WINDOWS
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif

	#include <Windows.h>

	#pragma comment(lib, "advapi32.lib")
#endif

#if IS_WINDOWS
namespace
{
	bool
	checkWindowsAccess (const std::filesystem::path & path, DWORD desiredAccess) noexcept
	{
		/* Get the security descriptor size. */
		constexpr DWORD securityInfo = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
		DWORD sdLength = 0;

		GetFileSecurityW(path.c_str(), securityInfo, nullptr, 0, &sdLength);

		if ( GetLastError() != ERROR_INSUFFICIENT_BUFFER )
		{
			return false;
		}

		/* Allocate and retrieve the security descriptor. */
		std::vector< BYTE > sdBuffer(sdLength);
		auto * pSD = reinterpret_cast< PSECURITY_DESCRIPTOR >(sdBuffer.data());

		if ( !GetFileSecurityW(path.c_str(), securityInfo, pSD, sdLength, &sdLength) )
		{
			return false;
		}

		/* Open the process token. */
		HANDLE hToken = nullptr;

		if ( !OpenProcessToken(GetCurrentProcess(), TOKEN_IMPERSONATE | TOKEN_QUERY | TOKEN_DUPLICATE | STANDARD_RIGHTS_READ, &hToken) )
		{
			return false;
		}

		/* Duplicate to an impersonation token for AccessCheck(). */
		HANDLE hImpersonationToken = nullptr;

		if ( !DuplicateToken(hToken, SecurityImpersonation, &hImpersonationToken) )
		{
			CloseHandle(hToken);

			return false;
		}

		/* Set up generic mapping for file objects. */
		GENERIC_MAPPING mapping{};
		mapping.GenericRead = FILE_GENERIC_READ;
		mapping.GenericWrite = FILE_GENERIC_WRITE;
		mapping.GenericExecute = FILE_GENERIC_EXECUTE;
		mapping.GenericAll = FILE_ALL_ACCESS;

		MapGenericMask(&desiredAccess, &mapping);

		/* Perform the access check. */
		PRIVILEGE_SET privilegeSet{};
		DWORD privilegeSetLength = sizeof(PRIVILEGE_SET);
		DWORD grantedAccess = 0;
		BOOL accessStatus = FALSE;

		const BOOL result = AccessCheck(pSD, hImpersonationToken, desiredAccess, &mapping, &privilegeSet, &privilegeSetLength, &grantedAccess, &accessStatus);

		CloseHandle(hImpersonationToken);
		CloseHandle(hToken);

		return result != 0 && accessStatus != 0;
	}
}
#endif

namespace EmEn::Base::IO
{
	std::filesystem::path
	systemPath (const std::filesystem::path & path) noexcept
	{
#if IS_WINDOWS
		if ( path.empty() || (path.is_absolute() && path.native().size() < WindowsLongPathThreshold) )
		{
			return path;
		}

		std::error_code errorCode;
		const auto absolute = std::filesystem::absolute(path, errorCode);

		if ( errorCode || absolute.native().size() < WindowsLongPathThreshold )
		{
			return path;
		}

		auto normal = absolute.lexically_normal();
		normal.make_preferred();

		return std::filesystem::path{windowsExtendedLengthPath(std::wstring_view{normal.native()})};
#else
		return path;
#endif
	}

	std::filesystem::path
	systemTreePath (const std::filesystem::path & path) noexcept
	{
#if IS_WINDOWS
		if ( path.empty() )
		{
			return path;
		}

		std::error_code errorCode;
		const auto absolute = std::filesystem::absolute(path, errorCode);

		if ( errorCode )
		{
			return path;
		}

		auto normal = absolute.lexically_normal();
		normal.make_preferred();

		return std::filesystem::path{windowsExtendedLengthPath(std::wstring_view{normal.native()})};
#else
		return path;
#endif
	}

	bool
	renameFile (const std::filesystem::path & from, const std::filesystem::path & to) noexcept
	{
		if ( from.empty() || to.empty() ) [[unlikely]]
		{
			return false;
		}

		std::error_code errorCode;

		std::filesystem::rename(systemPath(from), systemPath(to), errorCode);

		if ( errorCode ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::renameFile(), unable to rename "} + toU8String(from) + " to " + toU8String(to) + " (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");

			return false;
		}

		return true;
	}

	std::filesystem::path
	temporarySiblingPath (const std::filesystem::path & filepath) noexcept
	{
		/* NOTE: Drawn once per process from std::random_device and the clock (no seeded draw: no PortableRandom needed). */
		static const std::string processToken = [] {
			std::random_device device;
			const auto clockBits = static_cast< uint64_t >(std::chrono::steady_clock::now().time_since_epoch().count());
			const auto value = ((static_cast< uint64_t >(device()) << 32U) | static_cast< uint64_t >(device())) ^ clockBits;
			constexpr std::string_view Digits{"0123456789abcdef"};
			std::string token(16, '0');

			for ( size_t index = 0; index < token.size(); ++index )
			{
				token[index] = Digits[(value >> (index * 4U)) & 0xFU];
			}

			return token;
		}();

		static std::atomic< uint64_t > counter{0};

		return filepath.parent_path() / u8path(toU8String(filepath.filename()) + ".writing-" + processToken + "-" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed)));
	}

	bool
	fileExists (const std::filesystem::path & filepath) noexcept
	{
		if ( filepath.empty() ) [[unlikely]]
		{
			return false;
		}

		std::error_code errorCode;
		const auto systemFilepath = systemPath(filepath);

		if ( !std::filesystem::exists(systemFilepath, errorCode) ) [[unlikely]]
		{
			return false;
		}

		const auto result = std::filesystem::is_regular_file(systemFilepath, errorCode);

		if ( errorCode.value() > 0 ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::fileExists(), unable to check the existence of the file "} + toU8String(filepath) + " (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");

			return false;
		}

		return result;
	}

	bool
	isReadableRegularFile (const std::filesystem::path & filepath) noexcept
	{
		if ( filepath.empty() ) [[unlikely]]
		{
			return false;
		}

		std::error_code errorCode;
		const auto status = std::filesystem::status(systemPath(filepath), errorCode);

		/* NOTE: A missing file or an inaccessible path is the caller's to report (an optional file is no anomaly). */
		if ( !std::filesystem::exists(status) || errorCode ) [[unlikely]]
		{
			return false;
		}

		if ( !std::filesystem::is_regular_file(status) ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::isReadableRegularFile(), "} + toU8String(filepath) + " is not a regular file (a FIFO, a device, a socket or a directory): refused.");

			return false;
		}

		return true;
	}

	bool
	openInputFile (const std::filesystem::path & filepath, std::ifstream & stream, std::ios::openmode mode) noexcept
	{
		if ( stream.is_open() )
		{
			stream.close();
		}

		if ( !isReadableRegularFile(filepath) )
		{
			return false;
		}

		stream.open(systemPath(filepath), mode | std::ios::in);

		return stream.is_open();
	}

	bool
	isWritableTarget (const std::filesystem::path & filepath) noexcept
	{
		if ( filepath.empty() ) [[unlikely]]
		{
			return false;
		}

		std::error_code errorCode;
		const auto status = std::filesystem::status(systemPath(filepath), errorCode);

		/* NOTE: A missing path is created by the open; an inaccessible one fails there, the caller reports it. */
		if ( !std::filesystem::exists(status) )
		{
			return true;
		}

		if ( !std::filesystem::is_regular_file(status) ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::isWritableTarget(), "} + toU8String(filepath) + " is not a regular file (a FIFO, a device, a socket or a directory): refused.");

			return false;
		}

		return true;
	}

	bool
	openOutputFile (const std::filesystem::path & filepath, std::ofstream & stream, std::ios::openmode mode) noexcept
	{
		if ( stream.is_open() )
		{
			stream.close();
		}

		if ( !isWritableTarget(filepath) )
		{
			return false;
		}

		stream.open(systemPath(filepath), mode | std::ios::out);

		return stream.is_open();
	}

	std::streamoff
	fileSizeFromStream (std::ifstream & stream) noexcept
	{
		stream.seekg(0, std::ios::end);

		const std::streamoff bytes = stream.tellg();

		stream.clear();
		stream.seekg(0, std::ios::beg);

		if ( bytes < 0 || !stream ) [[unlikely]]
		{
			return -1;
		}

		/* NOTE: A pseudo-file can claim 0 bytes and hold some. */
		if ( bytes == 0 && stream.peek() != std::ifstream::traits_type::eof() ) [[unlikely]]
		{
			return -1;
		}

		stream.clear();

		return bytes;
	}

	size_t
	filesize (const std::filesystem::path & filepath) noexcept
	{
		std::error_code errorCode;
		const auto systemFilepath = systemPath(filepath);

		/* NOTE: file_size() of an existing path that is not a regular file is implementation-defined: libstdc++ fails
		 * with EISDIR, MSVC answers a DIRECTORY's size with no error (Windows peer, 2026-10-18: 524288 for %TEMP%). */
		if ( std::filesystem::exists(systemFilepath, errorCode) && !std::filesystem::is_regular_file(systemFilepath, errorCode) ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::filesize(), "} + toU8String(filepath) + " is not a regular file.");

			return 0;
		}

		const auto size = std::filesystem::file_size(systemFilepath, errorCode);

		/* NOTE: On an error, file_size() answers static_cast< uintmax_t >(-1): the contract is 0 (2026-10-18: a missing
		 * file was 18446744073709551615 bytes). */
		if ( errorCode ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::filesize(), unable to get the size of the file "} + toU8String(filepath) + " (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");

			return 0;
		}

		return static_cast< size_t >(size);
	}

	bool
	createFile (const std::filesystem::path & filepath) noexcept
	{
		if ( filepath.empty() ) [[unlikely]]
		{
			return false;
		}

		/* NOTE: Through openOutputFile(): a FIFO blocked the open forever (2026-10-18). Text mode, truncated, as before. */
		std::ofstream file;

		return openOutputFile(filepath, file, std::ios::trunc);
	}

	bool
	eraseFile (const std::filesystem::path & filepath) noexcept
	{
		if ( filepath.empty() ) [[unlikely]]
		{
			return false;
		}

		std::error_code errorCode;
		const auto systemFilepath = systemPath(filepath);

		if ( !std::filesystem::is_regular_file(systemFilepath, errorCode) ) [[unlikely]]
		{
			if ( errorCode.value() > 0 ) [[unlikely]]
			{
				Logging::error("IO", std::string{"IO::eraseFile(), unable to check the path "} + toU8String(filepath) + " (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");
			}
			else
			{
				Logging::error("IO", std::string{"IO::eraseFile(), the path "} + toU8String(filepath) + " is not a regular file !");
			}

			return false;
		}

		std::filesystem::remove(systemFilepath, errorCode);

		if ( errorCode.value() > 0 ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::eraseFile(), unable to delete the file "} + toU8String(filepath) + " (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");

			return false;
		}

		return true;
	}

	bool
	directoryExists (const std::filesystem::path & path) noexcept
	{
		if ( path.empty() ) [[unlikely]]
		{
			return false;
		}

		std::error_code errorCode;

		const auto result = std::filesystem::is_directory(systemPath(path), errorCode);

		if ( errorCode ) [[unlikely]]
		{
			/* NOTE: A missing path, or one crossing a file (ENOTDIR; Windows answers ERROR_PATH_NOT_FOUND for a missing
			 * parent), is simply no directory: no error to print. */
			if ( errorCode != std::errc::no_such_file_or_directory && errorCode != std::errc::not_a_directory ) [[unlikely]]
			{
				Logging::error("IO", std::string{"IO::directoryExists(), unable to check if the path "} + toU8String(path) + " is a directory (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");
			}

			return false;
		}

		return result;
	}

	bool
	isDirectoryContentEmpty (const std::filesystem::path & path) noexcept
	{
		if ( path.empty() ) [[unlikely]]
		{
			return false;
		}

		/* NOTE: is_empty() answers true for an empty FILE too: an empty file is no empty directory. */
		if ( !directoryExists(path) )
		{
			return false;
		}

		std::error_code errorCode;

		const auto result = std::filesystem::is_empty(systemPath(path), errorCode);

		if ( errorCode ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::isDirectoryContentEmpty(), unable to check the content of directory "} + toU8String(path) + " (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");

			return false;
		}

		return result;
	}

	std::vector< std::filesystem::path >
	directoryEntries (const std::filesystem::path & path) noexcept
	{
		std::vector< std::filesystem::path > entries{};

		/* NOTE: a failed walk is logged by forEachDirectoryEntry(); the entries read before the error are returned. */
		static_cast< void >(forEachDirectoryEntry(path, false, [&entries] (const std::filesystem::directory_entry & entry) {
			entries.emplace_back(entry.path());

			return true;
		}));

		return entries;
	}

	std::optional< std::filesystem::path >
	confinedPath (const std::filesystem::path & base, const std::filesystem::path & relative) noexcept
	{
		if ( relative.empty() || relative.is_absolute() || relative.has_root_name() || relative.has_root_directory() )
		{
			return std::nullopt;
		}

		const auto normalized = relative.lexically_normal();

		if ( normalized.empty() || *normalized.begin() == ".." )
		{
			return std::nullopt;
		}

		return base / normalized;
	}

	void
	logDirectoryWalkError (const std::filesystem::path & path, const std::error_code & errorCode) noexcept
	{
		Logging::error("IO", std::string{"IO::forEachDirectoryEntry(), unable to walk the directory "} + toU8String(path) + " (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");
	}

	bool
	createDirectory (const std::filesystem::path & path, bool removeFileSection) noexcept
	{
		if ( path.empty() ) [[unlikely]]
		{
			return false;
		}

		std::error_code errorCode;

		if ( removeFileSection )
		{
			const auto parentPath = path.parent_path();

			/* NOTE: A bare file name lives in the current directory: nothing to create (create_directories("") is
			 * EINVAL, 2026-10-18: filePutContents("out.json", …, createDirectories = true) failed). */
			if ( parentPath.empty() )
			{
				return true;
			}

			std::filesystem::create_directories(systemPath(parentPath), errorCode);
		}
		else
		{
			std::filesystem::create_directories(systemPath(path), errorCode);
		}

		if ( errorCode.value() > 0 ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::createDirectory(), unable to create the path "} + toU8String(path) + " (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");

			return false;
		}

		return true;
	}

	bool
	eraseDirectory (const std::filesystem::path & path, bool recursive) noexcept
	{
		if ( path.empty() ) [[unlikely]]
		{
			return false;
		}

		std::error_code errorCode;
		const auto systemDirectory = systemPath(path);

		if ( !std::filesystem::is_directory(systemDirectory, errorCode) ) [[unlikely]]
		{
			if ( errorCode.value() > 0 ) [[unlikely]]
			{
				Logging::error("IO", std::string{"IO::eraseDirectory(), unable to check the path "} + toU8String(path) + " (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");
			}
			else
			{
				Logging::error("IO", std::string{"IO::eraseDirectory(), the path "} + toU8String(path) + " is not a directory !");
			}

			return false;
		}

		if ( recursive )
		{
			/* The DESCENDANTS' length decides: a short root walked in its short form stops past MAX_PATH (Windows,
			 * 2026-10-07: "145: The directory is not empty" on a 300-character tree). */
			std::filesystem::remove_all(systemTreePath(path), errorCode);
		}
		else
		{
			std::filesystem::remove(systemDirectory, errorCode);
		}

		if ( errorCode.value() > 0 ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::eraseDirectory(), Unable to delete the directory "} + toU8String(path) + " (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");

			return false;
		}

		return true;
	}

	std::filesystem::path
	getCurrentWorkingDirectory () noexcept
	{
		std::error_code errorCode;

		auto path = std::filesystem::current_path(errorCode);

		if ( errorCode.value() > 0 ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::getCurrentWorkingDirectory(), unable to get the current working directory ("} + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");

			return {};
		}

		return path;
	}

	bool
	exists (const std::filesystem::path & path) noexcept
	{
		if ( path.empty() ) [[unlikely]]
		{
			return false;
		}

		std::error_code errorCode;

		const auto result = std::filesystem::exists(systemPath(path), errorCode);

		if ( errorCode.value() > 0 ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::exists(), unable to check the existence of the entry "} + toU8String(path) + " (" + std::to_string(errorCode.value()) + ": " + errorCode.message() + ")");

			return false;
		}

		return result;
	}

	bool
	readable (const std::filesystem::path & path) noexcept
	{
		if ( path.empty() )
		{
			return false;
		}

#if IS_LINUX || IS_MACOS
		return access(path.c_str(), R_OK) == 0;
#elif IS_WINDOWS
		return checkWindowsAccess(systemPath(path), GENERIC_READ);
#else
		Logging::error("IO", "IO::readable(), unable to check permission !");

		return false;
#endif
	}

	bool
	writable (const std::filesystem::path & path) noexcept
	{
		if ( path.empty() )
		{
			return false;
		}

#if IS_LINUX || IS_MACOS
		return access(path.c_str(), W_OK) == 0;
#elif IS_WINDOWS
		return checkWindowsAccess(systemPath(path), GENERIC_WRITE);
#else
		Logging::error("IO", "IO::writable(), unable to check permission !");

		return false;
#endif
	}

	bool
	executable (const std::filesystem::path & path) noexcept
	{
		if ( path.empty() )
		{
			return false;
		}

#if IS_LINUX || IS_MACOS
		return access(path.c_str(), X_OK) == 0;
#elif IS_WINDOWS
		return checkWindowsAccess(systemPath(path), GENERIC_EXECUTE);
#else
		Logging::error("IO", "IO::executable(), unable to check permission !");

		return false;
#endif
	}

	std::string
	getFileExtension (const std::filesystem::path & filepath, bool forceToLower) noexcept
	{
		if ( !filepath.has_extension() )
		{
			return {};
		}

		/* NOTE: On Windows, path::string() converts the native wide path to the system
		 * ANSI code page, which collapses non-ASCII characters (e.g. 'É' → single byte).
		 * toU8String() guarantees a UTF-8 result on every platform. */
		auto extension = toU8String(filepath.extension()).substr(1);

		if ( forceToLower )
		{
			std::ranges::transform(extension, extension.begin(), [] (char character) {
				return static_cast< char >(::tolower(static_cast< unsigned char >(character)));
			});
		}

		return extension;
	}

	bool
	fileGetContents (const std::filesystem::path & filepath, std::string & content) noexcept
	{
		if ( filepath.empty() ) [[unlikely]]
		{
			return false;
		}

		std::ifstream file;

		if ( !openInputFile(filepath, file) ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::fileGetContents(), unable to read "} + toU8String(filepath) + " file.");

			return false;
		}

		const auto bytes = fileSizeFromStream(file);

		if ( bytes < 0 ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::fileGetContents(), the size of "} + toU8String(filepath) + " is unknown (a pseudo-file is not read whole).");

			return false;
		}

		content.resize(static_cast< size_t >(bytes));

		file.read(content.data(), static_cast< std::streamsize >(content.size()));

		if ( !file ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::fileGetContents(), error reading "} + toU8String(filepath) + " file.");

			return false;
		}

		return true;
	}

	bool
	filePutContents (const std::filesystem::path & filepath, std::string_view content, bool append, bool createDirectories) noexcept
	{
		if ( filepath.empty() ) [[unlikely]]
		{
			return false;
		}

		if ( createDirectories && !IO::createDirectory(filepath, true) ) [[unlikely]]
		{
			return false;
		}

		/* NOTE: Through openOutputFile(): a FIFO blocked the open forever (2026-10-18). */
		std::ofstream file;

		if ( !openOutputFile(filepath, file, std::ios::binary | (append ? std::ios::app : std::ios::trunc)) ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::filePutContents(), unable to write into "} + toU8String(filepath) + " file.");

			return false;
		}

		file.write(content.data(), static_cast< std::streamsize >(content.size()));

		if ( !file ) [[unlikely]]
		{
			Logging::error("IO", std::string{"IO::filePutContents(), error writing to "} + toU8String(filepath) + " file.");

			return false;
		}

		return true;
	}
}
