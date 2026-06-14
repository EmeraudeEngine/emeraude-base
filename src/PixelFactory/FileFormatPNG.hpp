/*
 * src/PixelFactory/FileFormatPNG.hpp
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
#include <algorithm>
#include <array>
#include <csetjmp>
#include <cstdint>
#include <string>
#include <vector>

/* Third-party inclusions. */
#include <png.h>

/* Local inclusions for inheritances. */
#include "FileFormatInterface.hpp"
#include "Logging/Logging.hpp"

/* Local inclusions for usages. */
#include "IO/ByteStream.hpp"
#include "Pixmap.hpp"
#include "Types.hpp"

/* MSVC raises C4611 ("interaction between '_setjmp' and C++ object destruction is non-portable") for
 * any setjmp() in C++ code — even with no destructible object in scope — and /WX turns it into an
 * error. Here the longjmp resumes at the setjmp() in the same frame and the handler returns normally
 * (no destructor is skipped); with -fno-exceptions there is no unwinding either, so the warning is a
 * false positive. The macro suppresses it for the next line on MSVC only; it expands to nothing on
 * GCC/Clang, which never emit C4611. */
#ifndef EMERAUDE_BASE_SUPPRESS_SETJMP_C4611
	#if defined(_MSC_VER)
		#define EMERAUDE_BASE_SUPPRESS_SETJMP_C4611 __pragma(warning(suppress: 4611))
	#else
		#define EMERAUDE_BASE_SUPPRESS_SETJMP_C4611
	#endif
#endif

namespace EmEn::Base::PixelFactory
{
	/**
	 * @brief Error context for PNG operations, used to replace setjmp/longjmp mechanism.
	 * @note This struct is passed as user data to libPNG error callbacks.
	 */
	struct PNGErrorContext final
	{
		bool hasError{false};
		std::string errorMessage;
	};

	/**
	 * @brief Class for read and write PNG format via byte streams.
	 * @tparam pixel_data_t The pixel component type for the pixmap depth precision. Default uint8_t.
	 * @tparam dimension_t The type of unsigned integer used for pixmap dimension. Default uint32_t.
	 * @extends EmEn::Base::PixelFactory::FileFormatInterface The base IO class.
	 */
	template< typename pixel_data_t = uint8_t, typename dimension_t = uint32_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	class FileFormatPNG final : public FileFormatInterface< pixel_data_t, dimension_t >
	{
		public:

			FileFormatPNG () noexcept = default;

			/** @copydoc EmEn::Base::PixelFactory::FileFormatInterface::readStream() */
			[[nodiscard]]
			bool
			readStream (IO::ByteStream & stream, Pixmap< pixel_data_t, dimension_t > & pixmap) noexcept override
			{
				pixmap.clear();

				std::array< png_byte, 8 > signature{};

				/* Read signature and check it. */
				if ( !stream.read(signature.data(), sizeof(signature)) )
				{
					Logging::error("PixelFactory::FileFormatPNG", "readStream(), unable to read the PNG signature !");

					return false;
				}

				if ( !png_check_sig(signature.data(), sizeof(signature)) )
				{
					Logging::error("PixelFactory::FileFormatPNG", "readStream(), data is not a PNG stream !");

					return false;
				}

				/* libPNG reports a fatal error by calling pngErrorCallback which, per the libPNG contract,
				 * must not return (a returning handler triggers PNG_ABORT() -> abort()). The callback longjmps
				 * back to the setjmp() armed below; without it any malformed chunk aborts the process (fuzz_png).
				 * The read is split into a header phase and an image phase so the only RAII local the image
				 * phase needs (rowPointers) is filled BEFORE its setjmp is armed and never modified afterwards,
				 * keeping setjmp/longjmp free of skipped destructors and indeterminate objects. */
				PNGErrorContext errorContext{};

				auto * png = png_create_read_struct(PNG_LIBPNG_VER_STRING, &errorContext, pngErrorCallback, pngWarningCallback);

				if ( png == nullptr )
				{
					return false;
				}

				auto * pngInfo = png_create_info_struct(png);

				if ( pngInfo == nullptr )
				{
					png_destroy_read_struct(&png, nullptr, nullptr);

					return false;
				}

				/* Declared up-front so a longjmp from the image phase unwinds it normally. */
				std::vector< png_bytep > rowPointers;

				/* Phase 1 — header. */
				EMERAUDE_BASE_SUPPRESS_SETJMP_C4611
				if ( setjmp(png_jmpbuf(png)) )
				{
					Logging::error("PixelFactory::FileFormatPNG", std::string{"readStream(), PNG header error: "} + errorContext.errorMessage);

					png_destroy_read_struct(&png, &pngInfo, nullptr);

					return false;
				}

				/* Setup PNG for reading from ByteStream. */
				png_set_read_fn(png, &stream, customReadFunction);

				/* Tell PNG that we have already read the magic number. */
				png_set_sig_bytes(png, sizeof(signature));

				png_read_info(png, pngInfo);

				const auto width = png_get_image_width(png, pngInfo);
				const auto height = png_get_image_height(png, pngInfo);
				const auto bitDepth = png_get_bit_depth(png, pngInfo);

				auto pixmapAllocated = false;

				switch ( png_get_color_type(png, pngInfo) )
				{
					case PNG_COLOR_TYPE_GRAY :
						if ( bitDepth < 8 )
						{
							png_set_expand_gray_1_2_4_to_8(png);
						}
						else if ( bitDepth == 16 )
						{
							png_set_strip_16(png);
						}

						pixmapAllocated = pixmap.initialize(width, height, ChannelMode::Grayscale);
						break;

					case PNG_COLOR_TYPE_PALETTE :
					{
						png_set_palette_to_rgb(png);

						png_bytep transAlpha = nullptr;
						int count = 0;
						png_color_16p color = nullptr;

						png_get_tRNS(png, pngInfo, &transAlpha, &count, &color);

						if ( transAlpha != nullptr )
						{
							pixmapAllocated = pixmap.initialize(width, height, ChannelMode::RGBA);
						}
						else
						{
							pixmapAllocated = pixmap.initialize(width, height, ChannelMode::RGB);
						}
					}
						break;

					case PNG_COLOR_TYPE_RGB :
						if ( bitDepth < 8 )
						{
							png_set_packing(png);
						}
						else if ( bitDepth == 16 )
						{
							png_set_strip_16(png);
						}

						pixmapAllocated = pixmap.initialize(width, height, ChannelMode::RGB);
						break;

					case PNG_COLOR_TYPE_RGB_ALPHA :
						if ( bitDepth < 8 )
						{
							png_set_packing(png);
						}
						else if ( bitDepth == 16 )
						{
							png_set_strip_16(png);
						}

						pixmapAllocated = pixmap.initialize(width, height, ChannelMode::RGBA);
						break;

					case PNG_COLOR_TYPE_GRAY_ALPHA :
						if ( bitDepth < 8 )
						{
							png_set_packing(png);
						}
						else if ( bitDepth == 16 )
						{
							png_set_strip_16(png);
						}

						pixmapAllocated = pixmap.initialize(width, height, ChannelMode::GrayscaleAlpha);
						break;

					default:
						Logging::error("PixelFactory::FileFormatPNG", "readStream(), unhandled format !");

						png_destroy_read_struct(&png, &pngInfo, nullptr);

						return false;
				}

				if ( !pixmapAllocated )
				{
					png_destroy_read_struct(&png, &pngInfo, nullptr);

					return false;
				}

				/* Update info structure to apply transformations. */
				png_read_update_info(png, pngInfo);

				/* Set up row pointers for canonical top-left origin. Filled here, before the image-phase
				 * setjmp is armed, so rowPointers is never modified after that setjmp. */
				rowPointers.resize(pixmap.height(), nullptr);
				auto & buffer = pixmap.data();

				for ( size_t yIndex = 0; yIndex < pixmap.height(); ++yIndex )
				{
					const auto offset = yIndex * pixmap.width() * pixmap.colorCount();

					rowPointers.at(yIndex) = static_cast< png_bytep >(buffer.data() + offset);
				}

				/* Phase 2 — image. A corrupt IDAT longjmps here; rowPointers unwinds normally. */
				EMERAUDE_BASE_SUPPRESS_SETJMP_C4611
				if ( setjmp(png_jmpbuf(png)) )
				{
					Logging::error("PixelFactory::FileFormatPNG", std::string{"readStream(), PNG image error: "} + errorContext.errorMessage);

					png_destroy_read_struct(&png, &pngInfo, nullptr);

					return false;
				}

				png_read_image(png, rowPointers.data());

				png_read_end(png, nullptr);

				png_destroy_read_struct(&png, &pngInfo, nullptr);

				return true;
			}

			/** @copydoc EmEn::Base::PixelFactory::FileFormatInterface::writeStream() */
			[[nodiscard]]
			bool
			writeStream (IO::ByteStream & stream, const Pixmap< pixel_data_t, dimension_t > & pixmap, const WriteOptions & options = {}) const noexcept override
			{
				if ( !pixmap.isValid() )
				{
					Logging::error("PixelFactory::FileFormatPNG", "writeStream(), pixmap parameter is invalid !");

					return false;
				}

				const int bitDepth = 8;
				int colorType = 0;

				switch ( pixmap.channelMode() )
				{
					case ChannelMode::Grayscale :
						colorType = PNG_COLOR_TYPE_GRAY;
						break;

					case ChannelMode::GrayscaleAlpha :
						colorType = PNG_COLOR_TYPE_GRAY_ALPHA;
						break;

					case ChannelMode::RGB :
						colorType = PNG_COLOR_TYPE_RGB;
						break;

					case ChannelMode::RGBA :
						colorType = PNG_COLOR_TYPE_RGB_ALPHA;
						break;

					default:
						Logging::error("PixelFactory::FileFormatPNG", "writeStream(), invalid color count !");

						return false;
				}

				/* pngErrorCallback longjmps on a fatal libPNG error (a returning handler aborts the process),
				 * so the write path must arm setjmp too. Split into a setup phase and a write phase so the only
				 * RAII local (rowPointers) is filled before the write-phase setjmp and never modified after it. */
				PNGErrorContext errorContext{};

				auto * png = png_create_write_struct(PNG_LIBPNG_VER_STRING, &errorContext, pngErrorCallback, pngWarningCallback);

				if ( png == nullptr )
				{
					return false;
				}

				auto * pngInfo = png_create_info_struct(png);

				if ( pngInfo == nullptr )
				{
					png_destroy_write_struct(&png, nullptr);

					return false;
				}

				/* Declared up-front so a longjmp from the write phase unwinds it normally. */
				std::vector< png_bytep > rowPointers;

				/* Phase 1 — setup (IHDR / compression / filters). */
				EMERAUDE_BASE_SUPPRESS_SETJMP_C4611
				if ( setjmp(png_jmpbuf(png)) )
				{
					Logging::error("PixelFactory::FileFormatPNG", std::string{"writeStream(), PNG setup error: "} + errorContext.errorMessage);

					png_destroy_write_struct(&png, &pngInfo);

					return false;
				}

				/* Apply the interlace mode from options. */
				const auto interlaceType = (options.png.interlace == PngInterlace::Adam7) ? PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE;

				png_set_IHDR(
					png,
					pngInfo,
					static_cast< png_uint_32 >(pixmap.width()),
					static_cast< png_uint_32 >(pixmap.height()),
					bitDepth,
					colorType,
					interlaceType,
					PNG_COMPRESSION_TYPE_BASE,
					PNG_FILTER_TYPE_BASE
				);

				/* Apply compression level from options. */
				const auto compressionLevel = std::clamp(options.png.compressionLevel, 0, 9);
				png_set_compression_level(png, compressionLevel);

				/* Apply filter strategy from options. */
				int pngFilter = PNG_ALL_FILTERS;

				switch ( options.png.filterStrategy )
				{
					case PngFilterStrategy::None :
						pngFilter = PNG_FILTER_NONE;
						break;

					case PngFilterStrategy::Sub :
						pngFilter = PNG_FILTER_SUB;
						break;

					case PngFilterStrategy::Up :
						pngFilter = PNG_FILTER_UP;
						break;

					case PngFilterStrategy::Average :
						pngFilter = PNG_FILTER_AVG;
						break;

					case PngFilterStrategy::Paeth :
						pngFilter = PNG_FILTER_PAETH;
						break;

					case PngFilterStrategy::Adaptive :
						pngFilter = PNG_ALL_FILTERS;
						break;
				}

				png_set_filter(png, 0, pngFilter);

				/* Prepare row pointers with optional Y-axis inversion (filled before the write-phase setjmp). */
				rowPointers.resize(pixmap.height(), nullptr);

				for ( size_t yIndex = 0; yIndex < pixmap.height(); ++yIndex )
				{
					const auto rowIndex = options.invertYAxis ?
						static_cast< uint32_t >(pixmap.height() - 1 - yIndex) : static_cast< uint32_t >(yIndex);

					rowPointers[yIndex] = const_cast< png_bytep >(pixmap.rowPointer(rowIndex));
				}

				/* Setup PNG for writing to ByteStream. */
				png_set_write_fn(png, &stream, customWriteFunction, nullptr);

				png_set_rows(png, pngInfo, rowPointers.data());

				/* Phase 2 — write. */
				EMERAUDE_BASE_SUPPRESS_SETJMP_C4611
				if ( setjmp(png_jmpbuf(png)) )
				{
					Logging::error("PixelFactory::FileFormatPNG", std::string{"writeStream(), PNG write error: "} + errorContext.errorMessage);

					png_destroy_write_struct(&png, &pngInfo);

					return false;
				}

				png_write_png(png, pngInfo, PNG_TRANSFORM_IDENTITY, nullptr);

				png_destroy_write_struct(&png, &pngInfo);

				return true;
			}

		private:

			static
			void
			pngErrorCallback (png_structp pngPtr, png_const_charp message) noexcept
			{
				auto * errorContext = static_cast< PNGErrorContext * >(png_get_error_ptr(pngPtr));

				if ( errorContext != nullptr )
				{
					errorContext->hasError = true;
					errorContext->errorMessage = message;
				}

				/* A libPNG error handler MUST NOT return: doing so triggers PNG_ABORT() -> abort().
				 * Jump back to the setjmp() armed by the caller so the load/save is cancelled cleanly. */
				png_longjmp(pngPtr, 1);
			}

			static
			void
			pngWarningCallback (png_structp /* pngPtr */, png_const_charp message) noexcept
			{
				Logging::warning("PixelFactory::FileFormatPNG", message);
			}

			/**
			 * @brief Custom read function for libPNG using ByteStream.
			 * @param pngPtr The PNG structure pointer (io_ptr points to ByteStream).
			 * @param data Destination buffer.
			 * @param length Number of bytes to read.
			 */
			static
			void
			customReadFunction (png_structp pngPtr, png_bytep data, png_size_t length) noexcept
			{
				auto * stream = static_cast< IO::ByteStream * >(png_get_io_ptr(pngPtr));

				stream->read(data, length);
			}

			/**
			 * @brief Custom write function for libPNG using ByteStream.
			 * @param pngPtr The PNG structure pointer (io_ptr points to ByteStream).
			 * @param data Source buffer.
			 * @param length Number of bytes to write.
			 */
			static
			void
			customWriteFunction (png_structp pngPtr, png_bytep data, png_size_t length) noexcept
			{
				auto * stream = static_cast< IO::ByteStream * >(png_get_io_ptr(pngPtr));

				stream->write(data, length);
			}
	};
}
