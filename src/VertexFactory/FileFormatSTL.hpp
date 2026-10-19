/*
 * src/VertexFactory/FileFormatSTL.hpp
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
#include <bit>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

/* Local inclusions for inheritances. */
#include "FileFormatInterface.hpp"

/* Local inclusions for usages. */
#include "emeraude_platform.hpp"
#include "Logging/Logging.hpp"

/* Local inclusions for usages. */
#include "Math/Vector.hpp"
#include "Shape.hpp"
#include "ShapeTriangle.hpp"
#include "ShapeVertex.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief The FileFormatSTL class.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 * @tparam index_data_t The precision type of index data. Default uint32_t.
	 * @note https://en.wikipedia.org/wiki/STL_(file_format)
	 * @extends EmEn::Base::VertexFactory::FileFormatInterface
	 */
	template< typename vertex_data_t = float, typename index_data_t = uint32_t >
	requires (std::is_floating_point_v< vertex_data_t > && std::is_unsigned_v< index_data_t > )
	class FileFormatSTL final : public FileFormatInterface< vertex_data_t, index_data_t >
	{
		public:

			/**
			 * @brief Constructs an STL file format.
			 */
			FileFormatSTL () noexcept = default;

			/** @copydoc EmEn::Base::VertexFactory::FileFormatInterface::readStream() */
			[[nodiscard]]
			bool
			readStream (IO::ByteStream & stream, ShapeLoadResult< vertex_data_t, index_data_t > & result, const ReadOptions & /*readOptions*/) noexcept override
			{
				auto & geometry = result.shape;

				if ( !stream.isOpen() )
				{
					Logging::error("VertexFactory::FileFormatSTL", "readStream(), stream is not open !");
					return false;
				}

				const auto dataSize = stream.size();
				std::string buffer(dataSize, '\0');
				if ( !stream.read(buffer.data(), dataSize) )
				{
					Logging::error("VertexFactory::FileFormatSTL", "readStream(), failed to read stream data !");
					return false;
				}

				std::istringstream input(buffer, std::ios::binary);

				if ( FileFormatSTL::isAscii(input) )
				{
					return this->readAscii(input, geometry);
				}

				return this->readBinary(input, geometry);
			}

			/** @copydoc EmEn::Base::VertexFactory::FileFormatInterface::writeStream() */
			[[nodiscard]]
			bool
			writeStream (IO::ByteStream & stream, const Shape< vertex_data_t, index_data_t > & geometry, const WriteOptions & /*writeOptions*/) const noexcept override
			{
				if ( !geometry.isValid() )
				{
					Logging::error("VertexFactory::FileFormatSTL", "writeStream(), geometry is invalid !");
					return false;
				}

				/* NOTE: We default to Binary STL for saving as it is more compact. */

				/* The count field is 32 bits. */
				if ( geometry.triangles().size() > std::numeric_limits< uint32_t >::max() )
				{
					Logging::error("VertexFactory::FileFormatSTL", "writeStream(), more triangles than the format can count !");

					return false;
				}

				/* Header (80 bytes), then the triangle count (4 bytes). */
				std::array< char, 80 > header{};
				constexpr std::string_view Signature{"Exported by Emeraude-Engine"};
				std::ranges::copy(Signature, header.begin());

				const auto triangleCount = static_cast< uint32_t >(geometry.triangles().size());
				std::array< char, sizeof(uint32_t) > countBytes{};
				std::memcpy(countBytes.data(), &triangleCount, sizeof(uint32_t));

				if ( !stream.write(header.data(), header.size()) || !stream.write(countBytes.data(), countBytes.size()) )
				{
					Logging::error("VertexFactory::FileFormatSTL", "writeStream(), unable to write the header !");

					return false;
				}

				/* One 50-byte record per triangle: a zero normal (the format allows it, readers recompute it from the
				 * winding), the 3 vertices, a zero attribute byte count. */
				std::array< char, 50 > record{};

				for ( const auto & triangle : geometry.triangles() )
				{
					record.fill(0);

					for ( index_data_t corner = 0; corner < 3; ++corner )
					{
						const auto & position = geometry.vertex(triangle.vertexIndex(corner)).position();
						const auto first = 3 + (static_cast< size_t >(corner) * 3);

						writeFloat(record, first, static_cast< float >(position[Math::X]));
						writeFloat(record, first + 1, static_cast< float >(position[Math::Y]));
						writeFloat(record, first + 2, static_cast< float >(position[Math::Z]));
					}

					if ( !stream.write(record.data(), record.size()) )
					{
						Logging::error("VertexFactory::FileFormatSTL", "writeStream(), unable to write a triangle !");

						return false;
					}
				}

				return true;
			}

		private:

			/* NOTE: The binary STL format is little-endian; the floats are copied as the host stores them. Every target
			 * of the cascade (x86-64, ARM64) is little-endian: a big-endian one must swap the bytes here first. */
			static_assert(std::endian::native == std::endian::little, "FileFormatSTL reads and writes little-endian floats.");

			/**
			 * @brief Returns the n-th float of a binary STL triangle record.
			 * @param record The 50-byte record.
			 * @param index The float index, 0 to 11.
			 * @return float
			 */
			[[nodiscard]]
			static
			float
			readFloat (const std::array< char, 50 > & record, size_t index) noexcept
			{
				float value = 0.0F;

				std::memcpy(&value, record.data() + (index * sizeof(float)), sizeof(float));

				return value;
			}

			/**
			 * @brief Writes the n-th float of a binary STL triangle record.
			 * @param record The 50-byte record.
			 * @param index The float index, 0 to 11.
			 * @param value The value.
			 */
			static
			void
			writeFloat (std::array< char, 50 > & record, size_t index, float value) noexcept
			{
				std::memcpy(record.data() + (index * sizeof(float)), &value, sizeof(float));
			}

			/**
			 * @brief Check if the file is ASCII or Binary.
			 * @param file A reference to the input stream.
			 * @return bool True if ASCII, False if Binary.
			 */
			static
			bool
			isAscii (std::istream & file) noexcept
			{
				/* STL rules:
				 * - ASCII starts with "solid"
				 * - Binary has an 80-byte header, generally not starting with "solid" (but it can!)
				 * - Ideally we check file size vs expected binary size: 80 + 4 + (50 * triangle_count)
				 */

				char header[6] = {0}; // "solid" + 1
				file.read(header, 5);
				header[5] = '\0';

				/* If it does not start with solid, it is binary (or invalid). */
				if ( const bool startsWithSolid = (std::string(header) == "solid"); !startsWithSolid )
				{
					file.seekg(0);

					return false;
				}

				/* If it starts with solid, it MIGHT be ascii, or a binary file with "solid" in header. */
				/* Check file size. */
				file.seekg(0, std::ios::end);

				const auto fileSize = file.tellg();

				file.seekg(80);
				uint32_t count = 0;
				if ( file.read(reinterpret_cast< char * >(&count), 4) )
				{
					const auto expectedSize = 80 + 4 + (static_cast< std::streamoff >(count) * 50);

					if ( fileSize == expectedSize )
					{
						/* It matches binary size perfectly, so assume binary. */
						file.seekg(0);

						return false;
					}
				}

				/* Assume ASCII */
				file.seekg(0);

				return true;
			}

			/**
			 * @brief Parses exactly three finite floats separated by blanks ("ni nj nk", "x y z").
			 * @note Owner decision (2026-10-08): a malformed or non-finite triplet REFUSES the file — it used to become
			 * (0, 0, 0) silently, a 'nan' or an 'inf' passed as is (std::sscanf, its result unchecked; deprecated by the
			 * MSVC CRT, C4996). Trailing blanks are accepted, any other trailing text is not.
			 * @note strtof on macOS (libc++ lacks std::from_chars for floats, as in FileFormatOBJ::parseFloat()); the view
			 * always ends a NUL-terminated line.
			 * @param text The text after the keyword.
			 * @param values The three values.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			parseThreeFloats (std::string_view text, std::array< float, 3 > & values) noexcept
			{
				size_t position = 0;

				const auto skipBlanks = [&text, &position] () noexcept {
					while ( position < text.size() && (text[position] == ' ' || text[position] == '\t' || text[position] == '\r') )
					{
						++position;
					}
				};

				for ( auto & value : values )
				{
					skipBlanks();

					/* std::from_chars() refuses the leading '+' that std::sscanf() accepted (some exporters write it). */
					if ( position + 1 < text.size() && text[position] == '+' && (std::isdigit(static_cast< unsigned char >(text[position + 1])) != 0 || text[position + 1] == '.') )
					{
						++position;
					}

					if ( position >= text.size() )
					{
						return false;
					}

					const auto * const first = text.data() + position;
#if IS_MACOS
					char * end = nullptr;
					value = std::strtof(first, &end);

					if ( end == first )
					{
						return false;
					}

					const auto * const last = end;
#else
					const auto [last, error] = std::from_chars(first, text.data() + text.size(), value);

					if ( error != std::errc{} )
					{
						return false;
					}
#endif
					if ( !std::isfinite(value) || last > text.data() + text.size() )
					{
						return false;
					}

					position = static_cast< size_t >(last - text.data());
				}

				skipBlanks();

				return position == text.size();
			}

			/**
			 * @brief Reads an ASCII STL file.
			 * @param file A reference to the input stream.
			 * @param geometry A reference to the geometry to fill.
			 * @return bool
			 */
			bool
			readAscii (std::istream & file, Shape< vertex_data_t, index_data_t > & geometry) noexcept
			{
				/* Keep track of unique vertices to build indexed geometry */
				/* Simple approach: Linear search or Map. For speed/simplicity in this context,
				   we will just duplicate vertices for now or use a basic dedup if necessary.
				   Given ShapeBuilder usually handles some stuff, but here we populate vectors manually.
				   Let's use a builder approach similar to OBJ reference if possible,
				   but here we will just push vertices and triangles.
				*/

				/*
				   Shape builder helper is usually better.
				   However, `Shape` class has `build` method.
				   We will populate vectors and then let the `build` method do the job if we used the lambda approach.
				   Alternatively, we can manually populate.
				*/

				return geometry.build([&file] (std::vector< std::pair< index_data_t, index_data_t > > & /*groups*/, std::vector< ShapeVertex< vertex_data_t > > & vertices, std::vector< ShapeTriangle< vertex_data_t, index_data_t > > & triangles) {
					std::string currentLine;
					std::vector< ShapeVertex< vertex_data_t > > faceVertices;
					Math::Vector< 3, float > normal;
					size_t lineNumber = 0;

					while ( std::getline(file, currentLine) )
					{
						++lineNumber;

						/* Trim leading whitespace */
						const auto first = currentLine.find_first_not_of(" \t\r");

						if ( first == std::string::npos )
						{
							continue;
						}

						if ( currentLine.compare(first, 5, "solid") == 0 )
						{
							continue;
						}

						if ( currentLine.compare(first, 12, "facet normal") == 0 )
						{
							/* "facet normal ni nj nk"
							 * scan after "facet normal" */
							std::array< float, 3 > values{};

							if ( !parseThreeFloats(std::string_view{currentLine}.substr(first + 12), values) )
							{
								Logging::error("VertexFactory::FileFormatSTL", "readAscii(), line " + std::to_string(lineNumber) + ": a malformed or non-finite facet normal, the file is refused !");

								return false;
							}

							normal = {values[0], values[1], values[2]};
							faceVertices.clear();
						}
						else if ( currentLine.compare(first, 6, "vertex") == 0 )
						{
							std::array< float, 3 > values{};

							if ( !parseThreeFloats(std::string_view{currentLine}.substr(first + 6), values) )
							{
								Logging::error("VertexFactory::FileFormatSTL", "readAscii(), line " + std::to_string(lineNumber) + ": a malformed or non-finite vertex, the file is refused !");

								return false;
							}

							const auto x = values[0];
							const auto y = values[1];
							const auto z = values[2];

							ShapeVertex< vertex_data_t > v;
							v.setPosition(Math::Vector< 3, vertex_data_t >{static_cast< vertex_data_t >(x), static_cast< vertex_data_t >(y), static_cast< vertex_data_t >(z)});
							v.setNormal(Math::Vector< 3, vertex_data_t >{static_cast< vertex_data_t >(normal.x()), static_cast< vertex_data_t >(normal.y()), static_cast< vertex_data_t >(normal.z())});

							faceVertices.push_back(v);
						}
						else if ( currentLine.compare(first, 7, "endloop") == 0 )
						{
							/* End of a loop (face) */
							if ( faceVertices.size() == 3 )
							{
								/* Add vertices to global list and create triangle */
								/* Simple non-indexed assembly (optimizable later) */
								auto baseIndex = static_cast< index_data_t >(vertices.size());

								vertices.push_back(faceVertices[0]);
								vertices.push_back(faceVertices[1]);
								vertices.push_back(faceVertices[2]);

								ShapeTriangle< vertex_data_t, index_data_t > tri;
								tri.setVertexIndex(0, baseIndex);
								tri.setVertexIndex(1, baseIndex + 1);
								tri.setVertexIndex(2, baseIndex + 2);

								triangles.push_back(tri);
							}
						}
					}
					return true;
				}, false, false);
			}

			/**
			 * @brief Reads a Binary STL file.
			 * @param file A reference to the input stream.
			 * @param geometry A reference to the geometry to fill.
			 * @return bool
			 */
			bool
			readBinary (std::istream & file, Shape< vertex_data_t, index_data_t > & geometry) noexcept
			{
				/* Skip header */
				file.seekg(80);

				uint32_t triangleCount = 0;

				if ( !file.read(reinterpret_cast< char * >(&triangleCount), 4) )
				{
					return false;
				}

				/* Validate triangleCount against the remaining bytes (each binary triangle is
				 * exactly 50 bytes) BEFORE reserve(), so a hostile count cannot request a huge
				 * allocation. Division avoids integer overflow. */
				const auto dataStart = file.tellg();
				file.seekg(0, std::ios::end);
				const auto dataEnd = file.tellg();
				file.seekg(dataStart);

				if ( dataStart < 0 || dataEnd < dataStart )
				{
					Logging::error("VertexFactory::FileFormatSTL", "readBinary(), invalid stream state !");

					return false;
				}

				constexpr uint64_t triangleBytes = 50;
				const uint64_t remaining = static_cast< uint64_t >(dataEnd - dataStart);

				if ( triangleCount > remaining / triangleBytes )
				{
					Logging::error("VertexFactory::FileFormatSTL", "readBinary(), triangle count exceeds the stream size !");

					return false;
				}

				return geometry.build([&file, triangleCount] (std::vector< std::pair< index_data_t, index_data_t > > & /*groups*/, std::vector< ShapeVertex< vertex_data_t > > & vertices, std::vector< ShapeTriangle< vertex_data_t, index_data_t > > & triangles) {
					/* Optimize allocation */
					vertices.reserve(static_cast< std::size_t >(triangleCount) * 3);
					triangles.reserve(triangleCount);

					/* One record: the normal (3 floats), the 3 vertices (9 floats), the attribute byte count (2 bytes). */
					std::array< char, 50 > record{};

					for ( uint32_t i = 0; i < triangleCount; ++i )
					{
						file.read(record.data(), static_cast< std::streamsize >(record.size()));

						if ( !file.good() )
						{
							return false;
						}

						auto baseIndex = static_cast< index_data_t >(vertices.size());

						const Math::Vector< 3, vertex_data_t > normal = {
							static_cast< vertex_data_t >(readFloat(record, 0)),
							static_cast< vertex_data_t >(readFloat(record, 1)),
							static_cast< vertex_data_t >(readFloat(record, 2))
						};

						for ( size_t corner = 0; corner < 3; ++corner )
						{
							const auto first = 3 + (corner * 3);

							ShapeVertex< vertex_data_t > sv;
							sv.setPosition(Math::Vector< 3, vertex_data_t >{
								static_cast< vertex_data_t >(readFloat(record, first)),
								static_cast< vertex_data_t >(readFloat(record, first + 1)),
								static_cast< vertex_data_t >(readFloat(record, first + 2))
							});
							sv.setNormal(normal);
							vertices.push_back(sv);
						}

						ShapeTriangle< vertex_data_t, index_data_t > tri;
						tri.setVertexIndex(0, baseIndex);
						tri.setVertexIndex(1, baseIndex + 1);
						tri.setVertexIndex(2, baseIndex + 2);
						triangles.push_back(tri);
					}

					return true;
				}, false, false);
			}
	};
}
