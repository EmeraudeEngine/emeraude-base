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
#include <cstdint>
#include <cstring>
#include <istream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

/* Local inclusions for inheritances. */
#include "FileFormatInterface.hpp"

/* Local inclusions for usages. */
#include "emeraude_platform.hpp"
#include "IO/ByteStream.hpp"
#include "IO/ByteStreamBuffer.hpp"
#include "IO/MemoryStreamBuffer.hpp"
#include "Logging/Logging.hpp"
#include "VertexFactory/ShapeLoadResult.hpp"

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
			readStream (IO::ByteStream & stream, ShapeLoadResult< vertex_data_t, index_data_t > & result, const ReadOptions & readOptions) noexcept override
			{
				auto & geometry = result.shape;

				if ( !stream.isOpen() )
				{
					Logging::error("VertexFactory::FileFormatSTL", "readStream(), stream is not open !");
					return false;
				}

				/* NOTE: A stream that seeks is read through a window, never held whole (a 390 MB binary STL peaked at 3274
				 * MiB, the file twice, before 2026-11-26). One that cannot seek is read whole first, then in place (see
				 * IO::ByteStreamBuffer and IO::MemoryStreamBuffer). */
				IO::ByteStreamBuffer window{stream};
				std::string buffer;
				std::optional< IO::MemoryStreamBuffer > memory;
				std::streambuf * source = &window;

				if ( !window.isUsable() )
				{
					const auto dataSize = stream.size();
					buffer.resize(dataSize);

					if ( !stream.read(buffer.data(), dataSize) )
					{
						Logging::error("VertexFactory::FileFormatSTL", "readStream(), failed to read stream data !");

						return false;
					}

					source = &memory.emplace(buffer);
				}

				std::istream input{source};

				const bool read = FileFormatSTL::isAscii(input) ? FileFormatSTL::readAscii(input, geometry, readOptions.weldVertices) : FileFormatSTL::readBinary(input, geometry, readOptions.weldVertices);

				if ( !read )
				{
					return false;
				}

				/* A welded vertex has no normal of its own (the facet normals of its triangles differ): smooth ones on
				 * request. Unwelded, each corner keeps its facet's normal, as read. */
				if ( readOptions.weldVertices && readOptions.requestNormal )
				{
					return geometry.computeTriangleNormal() && geometry.computeVertexNormal();
				}

				return true;
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
			 * @brief Welds the corners sharing a position into one vertex (ReadOptions::weldVertices).
			 * @note An open-addressing table of vertex indexes, at most half full, the positions read back from the vertex
			 * list itself: 4 bytes a slot, about 32 MiB for 3.9 M positions where a std::unordered_map takes several times
			 * that. Two positions weld when their components are bit-identical, -0 and +0 being one; nothing is welded
			 * under a tolerance (a file's corners are written from the same floats).
			 */
			class PositionWelder final
			{
				public:

					/**
					 * @brief Returns the vertex at a position, added to the list when it is not there yet.
					 * @param vertices A reference to the vertex list being built (only this welder adds to it).
					 * @param position A reference to the position.
					 * @return index_data_t
					 */
					[[nodiscard]]
					index_data_t
					weld (std::vector< ShapeVertex< vertex_data_t > > & vertices, const Math::Vector< 3, vertex_data_t > & position) noexcept
					{
						if ( (vertices.size() + 1) * 2 > m_slots.size() )
						{
							this->grow(vertices);
						}

						const auto mask = m_slots.size() - 1;

						for ( auto slot = PositionWelder::hashOf(position) & mask; ; slot = (slot + 1) & mask )
						{
							const auto index = m_slots[slot];

							if ( index == Empty )
							{
								const auto added = static_cast< index_data_t >(vertices.size());

								vertices.emplace_back(position);
								m_slots[slot] = added;

								return added;
							}

							if ( PositionWelder::same(vertices[index].position(), position) )
							{
								return index;
							}
						}
					}

				private:

					/** @brief A free slot. */
					static constexpr index_data_t Empty{std::numeric_limits< index_data_t >::max()};

					/** @brief The first table size. */
					static constexpr size_t InitialSlots{1024};

					/** @brief The integer holding the bits of a vertex_data_t. */
					using bits_t = std::conditional_t< sizeof(vertex_data_t) == sizeof(uint32_t), uint32_t, uint64_t >;

					/**
					 * @brief Returns the bits of a component, -0 as +0.
					 * @param value The component.
					 * @return uint64_t
					 */
					[[nodiscard]]
					static
					uint64_t
					bitsOf (vertex_data_t value) noexcept
					{
						return std::bit_cast< bits_t >(value + vertex_data_t{0});
					}

					/**
					 * @brief Returns whether two positions are bit-identical (-0 and +0 being one).
					 * @param positionA A reference to a position.
					 * @param positionB A reference to a position.
					 * @return bool
					 */
					[[nodiscard]]
					static
					bool
					same (const Math::Vector< 3, vertex_data_t > & positionA, const Math::Vector< 3, vertex_data_t > & positionB) noexcept
					{
						return bitsOf(positionA[Math::X]) == bitsOf(positionB[Math::X]) && bitsOf(positionA[Math::Y]) == bitsOf(positionB[Math::Y]) && bitsOf(positionA[Math::Z]) == bitsOf(positionB[Math::Z]);
					}

					/**
					 * @brief Hashes a position.
					 * @param position A reference to the position.
					 * @return size_t
					 */
					[[nodiscard]]
					static
					size_t
					hashOf (const Math::Vector< 3, vertex_data_t > & position) noexcept
					{
						constexpr uint64_t Multiplier{0x9E3779B97F4A7C15ULL};

						const uint64_t withY = (bitsOf(position[Math::X]) * Multiplier) ^ bitsOf(position[Math::Y]);
						const uint64_t withZ = (withY * Multiplier) ^ bitsOf(position[Math::Z]);
						const uint64_t mixed = (withZ ^ (withZ >> 32U)) * 0xD6E8FEB86659FD93ULL;

						return static_cast< size_t >(mixed ^ (mixed >> 32U));
					}

					/**
					 * @brief Doubles the table (or creates it) and places every vertex again.
					 * @param vertices A reference to the vertex list, every position in it unique.
					 */
					void
					grow (const std::vector< ShapeVertex< vertex_data_t > > & vertices) noexcept
					{
						auto slotCount = std::max(InitialSlots, m_slots.size() * 2);

						while ( (vertices.size() + 1) * 2 > slotCount )
						{
							slotCount *= 2;
						}

						m_slots.assign(slotCount, Empty);

						const auto mask = slotCount - 1;

						for ( size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex )
						{
							auto slot = PositionWelder::hashOf(vertices[vertexIndex].position()) & mask;

							while ( m_slots[slot] != Empty )
							{
								slot = (slot + 1) & mask;
							}

							m_slots[slot] = static_cast< index_data_t >(vertexIndex);
						}
					}

					std::vector< index_data_t > m_slots;
			};

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
			 * @param weldVertices Welds the corners sharing a position (ReadOptions::weldVertices).
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			readAscii (std::istream & file, Shape< vertex_data_t, index_data_t > & geometry, bool weldVertices) noexcept
			{
				/* NOTE: Each corner its own vertex, or one vertex per position when welded (ReadOptions::weldVertices). */
				return geometry.build([&file, weldVertices] (std::vector< std::pair< index_data_t, index_data_t > > & /*groups*/, std::vector< ShapeVertex< vertex_data_t > > & vertices, std::vector< ShapeTriangle< vertex_data_t, index_data_t > > & triangles) {
					PositionWelder welder;
					AsciiFacet facet;
					std::string currentLine;
					size_t lineNumber = 0;

					while ( std::getline(file, currentLine) )
					{
						++lineNumber;

						if ( !FileFormatSTL::readAsciiLine(currentLine, lineNumber, facet, weldVertices ? &welder : nullptr, vertices, triangles) )
						{
							return false;
						}
					}

					return true;
				}, false, false);
			}

			/** @brief The facet an ASCII file is reading: its normal and the corners met so far. */
			struct AsciiFacet
			{
				Math::Vector< 3, float > normal;
				std::vector< ShapeVertex< vertex_data_t > > corners;
			};

			/**
			 * @brief Reads one line of an ASCII file: a facet normal starts a facet, a vertex adds a corner, an endloop adds
			 * the triangle of a facet of three corners; any other line is skipped.
			 * @param line A reference to the line.
			 * @param lineNumber The line number, for the errors.
			 * @param facet A reference to the facet being read.
			 * @param welder A pointer to the welder, nullptr when the corners are not welded.
			 * @param vertices A reference to the vertex list being built.
			 * @param triangles A reference to the triangle list being built.
			 * @return bool False when the line refuses the file.
			 */
			[[nodiscard]]
			static
			bool
			readAsciiLine (const std::string & line, size_t lineNumber, AsciiFacet & facet, PositionWelder * welder, std::vector< ShapeVertex< vertex_data_t > > & vertices, std::vector< ShapeTriangle< vertex_data_t, index_data_t > > & triangles) noexcept
			{
				/* Trim leading whitespace */
				const auto first = line.find_first_not_of(" \t\r");

				if ( first == std::string::npos || line.compare(first, 5, "solid") == 0 )
				{
					return true;
				}

				std::array< float, 3 > values{};

				if ( line.compare(first, 12, "facet normal") == 0 )
				{
					/* "facet normal ni nj nk", scanned after "facet normal". */
					if ( !FileFormatSTL::readAsciiTriplet(std::string_view{line}.substr(first + 12), lineNumber, "facet normal", values) )
					{
						return false;
					}

					facet.normal = {values[0], values[1], values[2]};
					facet.corners.clear();

					return true;
				}

				if ( line.compare(first, 6, "vertex") == 0 )
				{
					if ( !FileFormatSTL::readAsciiTriplet(std::string_view{line}.substr(first + 6), lineNumber, "vertex", values) )
					{
						return false;
					}

					ShapeVertex< vertex_data_t > vertex;
					vertex.setPosition(Math::Vector< 3, vertex_data_t >{static_cast< vertex_data_t >(values[0]), static_cast< vertex_data_t >(values[1]), static_cast< vertex_data_t >(values[2])});
					vertex.setNormal(Math::Vector< 3, vertex_data_t >{static_cast< vertex_data_t >(facet.normal.x()), static_cast< vertex_data_t >(facet.normal.y()), static_cast< vertex_data_t >(facet.normal.z())});

					facet.corners.push_back(vertex);

					return true;
				}

				/* End of a loop (face). */
				if ( line.compare(first, 7, "endloop") == 0 && facet.corners.size() == 3 )
				{
					FileFormatSTL::addFacet(facet.corners, welder, vertices, triangles);
				}

				return true;
			}

			/**
			 * @brief Reads the three floats of an ASCII line, the file refused with an error otherwise.
			 * @param text The text after the keyword.
			 * @param lineNumber The line number, for the error.
			 * @param what What the line holds, for the error ("vertex", "facet normal").
			 * @param values A reference to the three values.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			readAsciiTriplet (std::string_view text, size_t lineNumber, const char * what, std::array< float, 3 > & values) noexcept
			{
				if ( parseThreeFloats(text, values) )
				{
					return true;
				}

				Logging::error("VertexFactory::FileFormatSTL", "readAscii(), line " + std::to_string(lineNumber) + ": a malformed or non-finite " + what + ", the file is refused !");

				return false;
			}

			/**
			 * @brief Adds the triangle of an ASCII facet.
			 * @param faceVertices A reference to the facet's three corners, its normal in each.
			 * @param welder A pointer to the welder, nullptr when the corners are not welded (each its own vertex, with
			 * its facet's normal).
			 * @param vertices A reference to the vertex list being built.
			 * @param triangles A reference to the triangle list being built.
			 */
			static
			void
			addFacet (const std::vector< ShapeVertex< vertex_data_t > > & faceVertices, PositionWelder * welder, std::vector< ShapeVertex< vertex_data_t > > & vertices, std::vector< ShapeTriangle< vertex_data_t, index_data_t > > & triangles) noexcept
			{
				if ( welder != nullptr )
				{
					triangles.emplace_back(welder->weld(vertices, faceVertices[0].position()), welder->weld(vertices, faceVertices[1].position()), welder->weld(vertices, faceVertices[2].position()));

					return;
				}

				const auto baseIndex = static_cast< index_data_t >(vertices.size());

				vertices.push_back(faceVertices[0]);
				vertices.push_back(faceVertices[1]);
				vertices.push_back(faceVertices[2]);

				triangles.emplace_back(baseIndex, baseIndex + 1, baseIndex + 2);
			}

			/**
			 * @brief Reads a Binary STL file.
			 * @param file A reference to the input stream.
			 * @param geometry A reference to the geometry to fill.
			 * @param weldVertices Welds the corners sharing a position (ReadOptions::weldVertices).
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			readBinary (std::istream & file, Shape< vertex_data_t, index_data_t > & geometry, bool weldVertices) noexcept
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

				return geometry.build([&file, triangleCount, weldVertices] (std::vector< std::pair< index_data_t, index_data_t > > & /*groups*/, std::vector< ShapeVertex< vertex_data_t > > & vertices, std::vector< ShapeTriangle< vertex_data_t, index_data_t > > & triangles) {
					/* NOTE: A closed mesh has half as many positions as triangles, plus two per body (measured: 3 896 890 for
					 * 7 793 740 triangles); 1/16 more covers small boundaries and seams, without which the list doubles
					 * (+223 MiB on that file). Unwelded, three vertices a triangle. */
					const auto count = static_cast< std::size_t >(triangleCount);
					vertices.reserve(weldVertices ? (count / 2) + (count / 16) + 64 : count * 3);
					triangles.reserve(triangleCount);

					PositionWelder welder;

					/* One record: the normal (3 floats), the 3 vertices (9 floats), the attribute byte count (2 bytes). */
					std::array< char, 50 > record{};

					for ( uint32_t i = 0; i < triangleCount; ++i )
					{
						file.read(record.data(), static_cast< std::streamsize >(record.size()));

						if ( !file.good() )
						{
							return false;
						}

						const auto cornerPosition = [&record] (size_t corner) noexcept {
							const auto first = 3 + (corner * 3);

							return Math::Vector< 3, vertex_data_t >{
								static_cast< vertex_data_t >(readFloat(record, first)),
								static_cast< vertex_data_t >(readFloat(record, first + 1)),
								static_cast< vertex_data_t >(readFloat(record, first + 2))
							};
						};

						if ( weldVertices )
						{
							triangles.emplace_back(welder.weld(vertices, cornerPosition(0)), welder.weld(vertices, cornerPosition(1)), welder.weld(vertices, cornerPosition(2)));

							continue;
						}

						/* Every corner its own vertex, with its facet's normal. */
						const auto baseIndex = static_cast< index_data_t >(vertices.size());

						const Math::Vector< 3, vertex_data_t > normal = {
							static_cast< vertex_data_t >(readFloat(record, 0)),
							static_cast< vertex_data_t >(readFloat(record, 1)),
							static_cast< vertex_data_t >(readFloat(record, 2))
						};

						for ( size_t corner = 0; corner < 3; ++corner )
						{
							ShapeVertex< vertex_data_t > vertex{cornerPosition(corner)};
							vertex.setNormal(normal);
							vertices.push_back(vertex);
						}

						triangles.emplace_back(baseIndex, baseIndex + 1, baseIndex + 2);
					}

					return true;
				}, false, false);
			}
	};
}
