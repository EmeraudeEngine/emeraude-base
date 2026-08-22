/*
 * src/PixelFactory/FileFormatJpeg.cpp
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

#include "FileFormatJpeg.hpp"

/* Project configuration. */
#include "emeraude_base_config.hpp"

/* STL inclusions. */
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <csetjmp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

/* Third-party inclusions. */
#include <jpeglib.h>

/* Local inclusions. */
#include "Logging/Logging.hpp"

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
	namespace
	{
		/* libjpeg reports fatal errors via error_exit, whose default implementation calls exit().
		 * This manager (jpeg_error_mgr MUST stay the first member for the libjpeg cast) replaces it
		 * with a longjmp so malformed input cancels the operation instead of killing the process. */
		struct ErrorManager
		{
			jpeg_error_mgr pub;
			std::jmp_buf escape;
			char message[JMSG_LENGTH_MAX];
		};

		void
		errorExit (j_common_ptr cinfo) noexcept
		{
			auto * manager = reinterpret_cast< ErrorManager * >(cinfo->err);

			(*cinfo->err->format_message)(cinfo, manager->message);

			std::longjmp(manager->escape, 1);
		}
	}

	template< typename pixel_data_t, typename dimension_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	bool
	FileFormatJpeg< pixel_data_t, dimension_t >::readStream (IO::ByteStream & stream, Pixmap< pixel_data_t, dimension_t > & pixmap) noexcept
	{
		pixmap.clear();

		/* JPEG requires all data in memory for jpeg_mem_src. */
		std::vector< unsigned char > inputBuffer;
		const unsigned char * sourcePtr = nullptr;
		unsigned long sourceSize = 0;

		if ( stream.isMemoryBacked() && stream.data() != nullptr )
		{
			sourcePtr = reinterpret_cast< const unsigned char * >(stream.data());
			sourceSize = static_cast< unsigned long >(stream.size());
		}
		else
		{
			const auto totalSize = stream.size();

			if ( totalSize == 0 )
			{
				Logging::error("PixelFactory::FileFormatJpeg", "readStream(), empty stream !");

				return false;
			}

			inputBuffer.resize(totalSize);

			if ( !stream.read(inputBuffer.data(), totalSize) )
			{
				Logging::error("PixelFactory::FileFormatJpeg", "readStream(), failed to read stream data !");

				return false;
			}

			sourcePtr = inputBuffer.data();
			sourceSize = static_cast< unsigned long >(totalSize);
		}

		jpeg_decompress_struct info{};
		ErrorManager error{};

		info.err = jpeg_std_error(&error.pub);
		error.pub.error_exit = errorExit;

		jpeg_create_decompress(&info);

		jpeg_mem_src(&info, sourcePtr, sourceSize);

		/* libjpeg's default error_exit calls exit() on malformed input (fuzz_jpeg). errorExit longjmps
		 * here instead. The setjmp is placed AFTER jpeg_mem_src so sourcePtr/sourceSize (passed by value)
		 * are fully consumed before it and cannot be clobbered by the longjmp (-Wclobbered). inputBuffer,
		 * which backs sourcePtr, is filled before the setjmp and never modified afterwards. */
		EMERAUDE_BASE_SUPPRESS_SETJMP_C4611
		if ( setjmp(error.escape) )
		{
			Logging::error("PixelFactory::FileFormatJpeg", std::string{"readStream(), "} + error.message);

			jpeg_destroy_decompress(&info);

			return false;
		}

		jpeg_read_header(&info, 1);
		jpeg_start_decompress(&info);

		if constexpr ( PixelFactoryDebugEnabled )
		{
			std::cout <<
				"[Jpeg_DEBUG] Reading header." << '\n' <<
				"\tWidth : " << info.output_width << '\n' <<
				"\tHeight : " << info.output_height << '\n' <<
				"\tComponents : " << info.output_components << '\n';
		}

		auto pixmapAllocated = false;

		switch ( info.output_components )
		{
			case 1:
				pixmapAllocated = pixmap.initialize(info.output_width, info.output_height, ChannelMode::Grayscale);
				break;

			case 2:
				pixmapAllocated = pixmap.initialize(info.output_width, info.output_height, ChannelMode::GrayscaleAlpha);
				break;

			case 3:
				pixmapAllocated = pixmap.initialize(info.output_width, info.output_height, ChannelMode::RGB);
				break;

			case 4:
				pixmapAllocated = pixmap.initialize(info.output_width, info.output_height, ChannelMode::RGBA);
				break;

			default:
				break;
		}

		if ( pixmapAllocated )
		{
			/* Always decode to canonical top-left origin. */
			size_t rowIndex = 0;

			while ( info.output_scanline < info.output_height )
			{
				auto rowData = pixmap.rowPointer(rowIndex);

				jpeg_read_scanlines(&info, &rowData, 1);

				rowIndex++;
			}
		}

		jpeg_finish_decompress(&info);
		jpeg_destroy_decompress(&info);

		return pixmapAllocated;
	}

	template< typename pixel_data_t, typename dimension_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	bool
	FileFormatJpeg< pixel_data_t, dimension_t >::writeStream (IO::ByteStream & stream, const Pixmap< pixel_data_t, dimension_t > & pixmap, const WriteOptions & options) const noexcept
	{
		if ( !pixmap.isValid() )
		{
			Logging::error("PixelFactory::FileFormatJpeg", "writeStream(), pixmap parameter is invalid !");

			return false;
		}

		if ( pixmap.colorCount() != 3 && pixmap.colorCount() != 1 )
		{
			Logging::error("PixelFactory::FileFormatJpeg", "writeStream(), only rgb and grayscale format is supported for now !");

			return false;
		}

		jpeg_compress_struct info{};
		ErrorManager error{};

		info.err = jpeg_std_error(&error.pub);
		error.pub.error_exit = errorExit;

		jpeg_create_compress(&info);

		/* Use memory destination, write to stream afterwards. */
		unsigned char * outBuffer = nullptr;
		unsigned long outSize = 0;

		/* Route fatal libjpeg errors through longjmp instead of exit(); free the libjpeg buffer here. */
		EMERAUDE_BASE_SUPPRESS_SETJMP_C4611
		if ( setjmp(error.escape) )
		{
			Logging::error("PixelFactory::FileFormatJpeg", std::string{"writeStream(), "} + error.message);

			jpeg_destroy_compress(&info);
			free(outBuffer);

			return false;
		}

		jpeg_mem_dest(&info, &outBuffer, &outSize);

		info.image_width = static_cast< JDIMENSION >(pixmap.width());
		info.image_height = static_cast< JDIMENSION >(pixmap.height());

		switch ( pixmap.channelMode() )
		{
			case ChannelMode::RGB :
			case ChannelMode::RGBA :
				info.input_components = 3;
				info.in_color_space = JCS_RGB;
				break;

			case ChannelMode::Grayscale :
			case ChannelMode::GrayscaleAlpha :
				info.input_components = 1;
				info.in_color_space = JCS_GRAYSCALE;
				break;

			default:
				Logging::error("PixelFactory::FileFormatJpeg", "writeStream(), unhandled format !");

				jpeg_destroy_compress(&info);
				free(outBuffer);

				return false;
		}

		if constexpr ( PixelFactoryDebugEnabled )
		{
			std::cout <<
				"[Jpeg_DEBUG] Writing header." << '\n' <<
				"\tWidth : " << info.image_width << '\n' <<
				"\tHeight : " << info.image_height << '\n' <<
				"\tComponents : " << info.input_components << '\n';
		}

		jpeg_set_defaults(&info);

		/* Apply JPEG options. */
		const auto quality = std::clamp(options.jpeg.quality, 0, 100);
		jpeg_set_quality(&info, quality, 1);

		if ( options.jpeg.optimizeHuffman )
		{
			info.optimize_coding = TRUE;
		}

		if ( options.jpeg.progressive )
		{
			jpeg_simple_progression(&info);
		}

		/* Apply chroma subsampling for color images. */
		if ( options.jpeg.subsampling != ChromaSubsampling::Auto && info.input_components == 3 )
		{
			switch ( options.jpeg.subsampling )
			{
				case ChromaSubsampling::Sample444 :
					info.comp_info[0].h_samp_factor = 1;
					info.comp_info[0].v_samp_factor = 1;
					break;

				case ChromaSubsampling::Sample422 :
					info.comp_info[0].h_samp_factor = 2;
					info.comp_info[0].v_samp_factor = 1;
					break;

				case ChromaSubsampling::Sample420 :
					info.comp_info[0].h_samp_factor = 2;
					info.comp_info[0].v_samp_factor = 2;
					break;

				default:
					break;
			}

			info.comp_info[1].h_samp_factor = 1;
			info.comp_info[1].v_samp_factor = 1;
			info.comp_info[2].h_samp_factor = 1;
			info.comp_info[2].v_samp_factor = 1;
		}

		jpeg_start_compress(&info, 1);

		if ( options.invertYAxis )
		{
			auto rowIndex = static_cast< size_t >(pixmap.height() - 1);

			while ( info.next_scanline < info.image_height )
			{
				auto * rowData = const_cast< JSAMPROW >(pixmap.rowPointer(rowIndex));

				jpeg_write_scanlines(&info, &rowData, 1);

				--rowIndex;
			}
		}
		else
		{
			size_t rowIndex = 0;

			while ( info.next_scanline < info.image_height )
			{
				auto * rowData = const_cast< JSAMPROW >(pixmap.rowPointer(rowIndex));

				jpeg_write_scanlines(&info, &rowData, 1);

				rowIndex++;
			}
		}

		jpeg_finish_compress(&info);
		jpeg_destroy_compress(&info);

		/* Write compressed data to the stream. */
		const auto result = stream.write(outBuffer, static_cast< size_t >(outSize));

		/* Free the buffer allocated by jpeg_mem_dest. */
		free(outBuffer);

		return result;
	}

	/* Explicit instantiations — the ONLY place libjpeg symbols enter the binary.
	 * libjpeg is an 8-bit API, and PixelFactory::FileIO already guards its dispatch with
	 * `if constexpr ( std::is_same_v< pixel_data_t, uint8_t > )`, so no other pixel type can reach
	 * this codec. Add a line here if a consumer ever needs another dimension type; the linker names
	 * exactly what is missing when an instantiation is absent. */
	template class FileFormatJpeg< uint8_t, uint32_t >;
}