/*
 * src/WaveFactory/DLSSoundBank.cpp
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

#include "DLSSoundBank.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Logging/Logging.hpp"

namespace EmEn::Base::WaveFactory::DLS
{
	namespace
	{
		constexpr auto TracerTag{"WaveFactory::DLS"};

		/** @brief The most sub-chunks one list may hold (a bound on a hostile file's walk). */
		constexpr size_t MaxChunksPerList{1U << 20U};

		/** @brief The most connections one articulation chunk may hold. */
		constexpr uint32_t MaxConnections{4096};

		/** @brief The most SoundFont sample headers (a WORD sample index). */
		constexpr size_t MaxSampleHeaders{65535};

		/** @brief The zero samples SoundFont 2.04 § 6.1 requires after every sample. */
		constexpr size_t SampleTailZeros{46};

		/* DLS connection destinations (DLS Level 1 and Level 2, Microsoft dls1.h / dls2.h). */
		constexpr uint16_t DestinationGain{0x0001};
		constexpr uint16_t DestinationPan{0x0004};
		constexpr uint16_t DestinationEG1Attack{0x0206};
		constexpr uint16_t DestinationEG1Decay{0x0207};
		constexpr uint16_t DestinationEG1Release{0x0209};
		constexpr uint16_t DestinationEG1Sustain{0x020A};
		constexpr uint16_t DestinationEG1Delay{0x020B};
		constexpr uint16_t DestinationEG1Hold{0x020C};

		/** @brief The DLS "drum instrument" flag of insh's bank. */
		constexpr uint32_t DrumInstrumentFlag{0x80000000U};

		/** @brief A DLS time value meaning "zero seconds" (the 32-bit minimum). */
		constexpr int32_t DLSZeroTime{std::numeric_limits< int32_t >::min()};

		/* SoundFont 2 generators (SoundFont 2.04 § 8.1.2). */
		constexpr uint16_t GenPan{17};
		constexpr uint16_t GenDelayVolEnv{33};
		constexpr uint16_t GenAttackVolEnv{34};
		constexpr uint16_t GenHoldVolEnv{35};
		constexpr uint16_t GenDecayVolEnv{36};
		constexpr uint16_t GenSustainVolEnv{37};
		constexpr uint16_t GenReleaseVolEnv{38};
		constexpr uint16_t GenInstrument{41};
		constexpr uint16_t GenKeyRange{43};
		constexpr uint16_t GenVelRange{44};
		constexpr uint16_t GenInitialAttenuation{48};
		constexpr uint16_t GenSampleID{53};
		constexpr uint16_t GenSampleModes{54};
		constexpr uint16_t GenExclusiveClass{57};

		/** @brief A 4-character code at an offset of a span. */
		[[nodiscard]]
		bool
		isTag (std::span< const std::byte > data, size_t offset, std::string_view tag) noexcept
		{
			if ( offset > data.size() || data.size() - offset < 4 )
			{
				return false;
			}

			for ( size_t index = 0; index < 4; ++index )
			{
				if ( static_cast< char >(data[offset + index]) != tag[index] )
				{
					return false;
				}
			}

			return true;
		}

		/** @brief A little-endian unsigned integer of `bytes` bytes at an offset; nothing past the end. */
		[[nodiscard]]
		std::optional< uint32_t >
		readUnsigned (std::span< const std::byte > data, size_t offset, size_t bytes) noexcept
		{
			if ( offset > data.size() || data.size() - offset < bytes )
			{
				return std::nullopt;
			}

			uint32_t value = 0;

			for ( size_t index = 0; index < bytes; ++index )
			{
				value |= static_cast< uint32_t >(std::to_integer< uint8_t >(data[offset + index])) << (8U * index);
			}

			return value;
		}

		[[nodiscard]]
		std::optional< uint16_t >
		readU16 (std::span< const std::byte > data, size_t offset) noexcept
		{
			const auto value = readUnsigned(data, offset, 2);

			return value.has_value() ? std::optional< uint16_t >{static_cast< uint16_t >(value.value())} : std::nullopt;
		}

		[[nodiscard]]
		std::optional< uint32_t >
		readU32 (std::span< const std::byte > data, size_t offset) noexcept
		{
			return readUnsigned(data, offset, 4);
		}

		[[nodiscard]]
		std::optional< int32_t >
		readS32 (std::span< const std::byte > data, size_t offset) noexcept
		{
			const auto value = readUnsigned(data, offset, 4);

			return value.has_value() ? std::optional< int32_t >{std::bit_cast< int32_t >(value.value())} : std::nullopt;
		}

		[[nodiscard]]
		std::optional< int16_t >
		readS16 (std::span< const std::byte > data, size_t offset) noexcept
		{
			const auto value = readU16(data, offset);

			return value.has_value() ? std::optional< int16_t >{std::bit_cast< int16_t >(value.value())} : std::nullopt;
		}

		/**
		 * @brief Walks the sub-chunks of a RIFF / LIST body, calling visit(chunk start, id offset, data) for each.
		 * @return bool False when a chunk runs past the body's end, the visitor refuses one, or there are too many.
		 */
		template< typename visitor_t >
		[[nodiscard]]
		bool
		forEachChunk (std::span< const std::byte > body, const visitor_t & visit) noexcept
		{
			size_t offset = 0;
			size_t count = 0;

			while ( offset < body.size() )
			{
				const auto size = readU32(body, offset + 4);

				if ( !size.has_value() || ++count > MaxChunksPerList || size.value() > body.size() - offset - 8 )
				{
					return false;
				}

				if ( !visit(offset, body.subspan(offset + 8, size.value())) )
				{
					return false;
				}

				offset += 8 + static_cast< size_t >(size.value()) + (size.value() & 1U);
			}

			return true;
		}

		/** @brief The envelope and pan of an articulation, in the units SoundFont stores (unset = its default). */
		struct Articulation
		{
			std::optional< int32_t > attack;
			std::optional< int32_t > decay;
			std::optional< int32_t > release;
			std::optional< int32_t > delay;
			std::optional< int32_t > hold;
			std::optional< int32_t > sustainCentibels;
			std::optional< int32_t > pan;
			int32_t gainCentibels{0};
		};

		/** @brief A wave sample header: the unity note, the fine tune, the gain and the first loop. */
		struct WaveSample
		{
			uint16_t unityNote{60};
			int16_t fineTune{0};
			int32_t gainCentibels{0};
			uint32_t loopStart{0};
			uint32_t loopLength{0};
			bool looped{false};
		};

		struct Region
		{
			uint8_t keyLow{0};
			uint8_t keyHigh{127};
			uint8_t velocityLow{0};
			uint8_t velocityHigh{127};
			uint16_t keyGroup{0};
			uint32_t tableIndex{0};
			std::optional< WaveSample > sample;
			std::optional< Articulation > articulation;
			bool linked{false};
		};

		struct Instrument
		{
			std::string name;
			std::vector< Region > regions;
			std::optional< Articulation > articulation;
			uint16_t bank{0};
			uint16_t program{0};
		};

		struct Wave
		{
			std::vector< int16_t > samples;
			std::optional< WaveSample > sample;
			uint32_t sampleRate{0};
			bool usable{false};
		};

		/** @brief A DLS time (timecents × 65536, the minimum = zero seconds) in SoundFont timecents. */
		[[nodiscard]]
		int32_t
		timecents (int32_t scale) noexcept
		{
			if ( scale == DLSZeroTime )
			{
				return -12000;
			}

			return std::clamp(static_cast< int32_t >(std::lround(static_cast< double >(scale) / 65536.0)), -12000, 8000);
		}

		/** @brief Reads one 'art1' / 'art2' chunk into an articulation (absolute connections only). */
		[[nodiscard]]
		bool
		parseArticulation (std::span< const std::byte > data, Articulation & articulation) noexcept
		{
			const auto headerSize = readU32(data, 0);
			const auto connections = readU32(data, 4);

			if ( !headerSize.has_value() || !connections.has_value() || headerSize.value() < 8 || connections.value() > MaxConnections )
			{
				return false;
			}

			for ( uint32_t index = 0; index < connections.value(); ++index )
			{
				const auto offset = static_cast< size_t >(headerSize.value()) + (static_cast< size_t >(index) * 12U);
				const auto source = readU16(data, offset);
				const auto control = readU16(data, offset + 2);
				const auto destination = readU16(data, offset + 4);
				const auto scale = readS32(data, offset + 8);

				if ( !source.has_value() || !control.has_value() || !destination.has_value() || !scale.has_value() )
				{
					return false;
				}

				/* Only the absolute values (no source, no control): the modulated connections are not mapped. */
				if ( source.value() != 0 || control.value() != 0 )
				{
					continue;
				}

				switch ( destination.value() )
				{
					case DestinationEG1Attack : articulation.attack = timecents(scale.value()); break;
					case DestinationEG1Decay : articulation.decay = timecents(scale.value()); break;
					case DestinationEG1Release : articulation.release = timecents(scale.value()); break;
					case DestinationEG1Delay : articulation.delay = timecents(scale.value()); break;
					case DestinationEG1Hold : articulation.hold = timecents(scale.value()); break;

					case DestinationEG1Sustain :
					{
						/* 0.1 % × 65536 → the SoundFont attenuation of the sustain below the peak, in centibels. */
						const auto level = std::clamp(static_cast< double >(scale.value()) / 65536.0, 0.0, 1000.0);

						articulation.sustainCentibels = level <= 0.0 ? 1440 : std::clamp(static_cast< int32_t >(std::lround(-200.0 * std::log10(level / 1000.0))), 0, 1440);
					}
						break;

					case DestinationPan :
						articulation.pan = std::clamp(static_cast< int32_t >(std::lround(static_cast< double >(scale.value()) / 65536.0)), -500, 500);
						break;

					case DestinationGain :
						articulation.gainCentibels = std::clamp(static_cast< int32_t >(std::lround(static_cast< double >(scale.value()) / 65536.0)), -1440, 1440);
						break;

					default :
						break;
				}
			}

			return true;
		}

		/** @brief Reads the articulation chunks of a 'lart' / 'lar2' list body (after its type). */
		[[nodiscard]]
		bool
		parseArticulationList (std::span< const std::byte > body, std::optional< Articulation > & articulation) noexcept
		{
			return forEachChunk(body, [&body, &articulation] (size_t offset, std::span< const std::byte > data) {
				if ( !isTag(body, offset, "art1") && !isTag(body, offset, "art2") )
				{
					return true;
				}

				if ( !articulation.has_value() )
				{
					articulation.emplace();
				}

				return parseArticulation(data, articulation.value());
			});
		}

		/** @brief Reads a 'wsmp' chunk. */
		[[nodiscard]]
		std::optional< WaveSample >
		parseWaveSample (std::span< const std::byte > data) noexcept
		{
			const auto headerSize = readU32(data, 0);
			const auto unityNote = readU16(data, 4);
			const auto fineTune = readS16(data, 6);
			const auto gain = readS32(data, 8);
			const auto loops = readU32(data, 16);

			if ( !headerSize.has_value() || !unityNote.has_value() || !fineTune.has_value() || !gain.has_value() || !loops.has_value() || headerSize.value() < 20 )
			{
				return std::nullopt;
			}

			WaveSample sample;
			sample.unityNote = std::min< uint16_t >(unityNote.value(), 127);
			sample.fineTune = std::clamp< int16_t >(fineTune.value(), -99, 99);
			/* A relative gain, 1/655360 dB per unit: 65536 per centibel, negative attenuates. */
			sample.gainCentibels = std::clamp(static_cast< int32_t >(std::lround(static_cast< double >(gain.value()) / 65536.0)), -1440, 1440);

			if ( loops.value() > 0 )
			{
				const auto loopStart = readU32(data, static_cast< size_t >(headerSize.value()) + 8);
				const auto loopLength = readU32(data, static_cast< size_t >(headerSize.value()) + 12);

				if ( !loopStart.has_value() || !loopLength.has_value() )
				{
					return std::nullopt;
				}

				sample.loopStart = loopStart.value();
				sample.loopLength = loopLength.value();
				sample.looped = loopLength.value() > 0;
			}

			return sample;
		}

		/** @brief Reads a region list body ('rgn ' / 'rgn2', after its type). */
		[[nodiscard]]
		std::optional< Region >
		parseRegion (std::span< const std::byte > body) noexcept
		{
			Region region;
			bool hasHeader = false;

			const auto parsed = forEachChunk(body, [&] (size_t offset, std::span< const std::byte > data) {
				if ( isTag(body, offset, "rgnh") )
				{
					const auto keyLow = readU16(data, 0);
					const auto keyHigh = readU16(data, 2);
					const auto velocityLow = readU16(data, 4);
					const auto velocityHigh = readU16(data, 6);
					const auto keyGroup = readU16(data, 10);

					if ( !keyLow.has_value() || !keyHigh.has_value() || !velocityLow.has_value() || !velocityHigh.has_value() || !keyGroup.has_value() )
					{
						return false;
					}

					region.keyLow = static_cast< uint8_t >(std::min< uint16_t >(keyLow.value(), 127));
					region.keyHigh = static_cast< uint8_t >(std::min< uint16_t >(keyHigh.value(), 127));

					/* DLS Level 1 does not use the velocity range: an empty or reversed one means all velocities. */
					if ( velocityHigh.value() == 0 || velocityHigh.value() < velocityLow.value() )
					{
						region.velocityLow = 0;
						region.velocityHigh = 127;
					}
					else
					{
						region.velocityLow = static_cast< uint8_t >(std::min< uint16_t >(velocityLow.value(), 127));
						region.velocityHigh = static_cast< uint8_t >(std::min< uint16_t >(velocityHigh.value(), 127));
					}

					region.keyGroup = keyGroup.value();
					hasHeader = true;

					return true;
				}

				if ( isTag(body, offset, "wsmp") )
				{
					region.sample = parseWaveSample(data);

					return region.sample.has_value();
				}

				if ( isTag(body, offset, "wlnk") )
				{
					const auto tableIndex = readU32(data, 8);

					if ( !tableIndex.has_value() )
					{
						return false;
					}

					region.tableIndex = tableIndex.value();
					region.linked = true;

					return true;
				}

				if ( isTag(body, offset, "LIST") && (isTag(data, 0, "lart") || isTag(data, 0, "lar2")) )
				{
					return parseArticulationList(data.subspan(4), region.articulation);
				}

				return true;
			});

			if ( !parsed || !hasHeader || !region.linked || region.keyLow > region.keyHigh )
			{
				return std::nullopt;
			}

			return region;
		}

		/** @brief Reads an 'insh' chunk: the bank and the program. */
		[[nodiscard]]
		bool
		parseInstrumentHeader (std::span< const std::byte > data, Instrument & instrument) noexcept
		{
			const auto bank = readU32(data, 4);
			const auto program = readU32(data, 8);

			if ( !bank.has_value() || !program.has_value() )
			{
				return false;
			}

			/* The drum flag → SoundFont bank 128 (the percussion bank TinySoundFont plays channel 10 with); else the bank
			 * select MSB (CC 0, bits 8-14). */
			instrument.bank = (bank.value() & DrumInstrumentFlag) != 0 ? uint16_t{128} : static_cast< uint16_t >((bank.value() >> 8U) & 0x7FU);
			instrument.program = static_cast< uint16_t >(program.value() & 0x7FU);

			return true;
		}

		/** @brief Reads an 'lrgn' list body (after its type); a region this converter cannot read is skipped, not the bank. */
		[[nodiscard]]
		bool
		parseRegionList (std::span< const std::byte > list, std::vector< Region > & regions) noexcept
		{
			return forEachChunk(list, [&list, &regions] (size_t offset, std::span< const std::byte > data) {
				if ( !isTag(list, offset, "LIST") || !(isTag(data, 0, "rgn ") || isTag(data, 0, "rgn2")) )
				{
					return true;
				}

				if ( regions.size() >= MaxRegionsPerInstrument )
				{
					return false;
				}

				if ( const auto region = parseRegion(data.subspan(4)); region.has_value() )
				{
					regions.push_back(region.value());
				}

				return true;
			});
		}

		/** @brief Reads an instrument's 'INFO' list body (after its type): its 'INAM' name, at most 19 characters. */
		[[nodiscard]]
		bool
		parseInstrumentName (std::span< const std::byte > list, std::string & name) noexcept
		{
			return forEachChunk(list, [&list, &name] (size_t offset, std::span< const std::byte > data) {
				if ( !isTag(list, offset, "INAM") )
				{
					return true;
				}

				name.clear();

				for ( const auto character : data.first(std::min< size_t >(data.size(), 19)) )
				{
					if ( character == std::byte{0} )
					{
						break;
					}

					name.push_back(static_cast< char >(character));
				}

				return true;
			});
		}

		/** @brief Reads an instrument list body ('ins ', after its type). */
		[[nodiscard]]
		std::optional< Instrument >
		parseInstrument (std::span< const std::byte > body) noexcept
		{
			Instrument instrument;
			bool hasHeader = false;

			const auto parsed = forEachChunk(body, [&body, &instrument, &hasHeader] (size_t offset, std::span< const std::byte > data) {
				if ( isTag(body, offset, "insh") )
				{
					hasHeader = parseInstrumentHeader(data, instrument);

					return hasHeader;
				}

				if ( !isTag(body, offset, "LIST") )
				{
					return true;
				}

				if ( isTag(data, 0, "lrgn") )
				{
					return parseRegionList(data.subspan(4), instrument.regions);
				}

				if ( isTag(data, 0, "lart") || isTag(data, 0, "lar2") )
				{
					return parseArticulationList(data.subspan(4), instrument.articulation);
				}

				if ( isTag(data, 0, "INFO") )
				{
					return parseInstrumentName(data.subspan(4), instrument.name);
				}

				return true;
			});

			if ( !parsed || !hasHeader )
			{
				return std::nullopt;
			}

			return instrument;
		}

		/** @brief Reads one wave list body ('wave', after its type): a PCM 8/16-bit mono wave, its optional 'wsmp'. */
		[[nodiscard]]
		std::optional< Wave >
		parseWave (std::span< const std::byte > body) noexcept
		{
			Wave wave;
			std::optional< uint16_t > format;
			std::optional< uint16_t > channels;
			std::optional< uint16_t > bitsPerSample;
			std::span< const std::byte > pcm;
			bool hasData = false;

			const auto parsed = forEachChunk(body, [&] (size_t offset, std::span< const std::byte > data) {
				if ( isTag(body, offset, "fmt ") )
				{
					format = readU16(data, 0);
					channels = readU16(data, 2);
					const auto rate = readU32(data, 4);
					bitsPerSample = readU16(data, 14);

					if ( !format.has_value() || !channels.has_value() || !rate.has_value() || !bitsPerSample.has_value() )
					{
						return false;
					}

					wave.sampleRate = rate.value();

					return true;
				}

				if ( isTag(body, offset, "data") )
				{
					pcm = data;
					hasData = true;

					return true;
				}

				if ( isTag(body, offset, "wsmp") )
				{
					wave.sample = parseWaveSample(data);

					return wave.sample.has_value();
				}

				return true;
			});

			if ( !parsed )
			{
				return std::nullopt;
			}

			/* PCM (format tag 1), mono, 8 or 16 bits, a usable rate: anything else stays in the pool, unusable. */
			if ( !hasData || format.value_or(0) != 1 || channels.value_or(0) != 1 || wave.sampleRate < 1000 || wave.sampleRate > 384000 )
			{
				return wave;
			}

			if ( bitsPerSample.value_or(0) == 16 )
			{
				wave.samples.resize(pcm.size() / 2);

				for ( size_t index = 0; index < wave.samples.size(); ++index )
				{
					wave.samples[index] = readS16(pcm, index * 2).value_or(0);
				}
			}
			else if ( bitsPerSample.value_or(0) == 8 )
			{
				/* 8-bit WAVE PCM is unsigned, centred on 128. */
				wave.samples.resize(pcm.size());

				for ( size_t index = 0; index < wave.samples.size(); ++index )
				{
					wave.samples[index] = static_cast< int16_t >((std::to_integer< int >(pcm[index]) - 128) * 256);
				}
			}

			wave.usable = !wave.samples.empty();

			return wave;
		}

		/** @brief The two chunks that carry a type before their sub-chunks. */
		enum class Container : uint8_t
		{
			RIFF,
			LIST
		};

		/** @brief Appends bytes to a growing file. */
		class Writer final
		{
			public:

				void
				u8 (uint8_t value) noexcept
				{
					m_bytes.push_back(std::byte{value});
				}

				void
				u16 (uint32_t value) noexcept
				{
					this->u8(static_cast< uint8_t >(value & 0xFFU));
					this->u8(static_cast< uint8_t >((value >> 8U) & 0xFFU));
				}

				void
				u32 (uint32_t value) noexcept
				{
					this->u16(value & 0xFFFFU);
					this->u16(value >> 16U);
				}

				void
				tag (std::string_view fourCC) noexcept
				{
					for ( const auto character : fourCC.substr(0, 4) )
					{
						this->u8(static_cast< uint8_t >(character));
					}
				}

				/** @brief A fixed 20-byte, zero-padded name. */
				void
				name20 (std::string_view name) noexcept
				{
					for ( size_t index = 0; index < 20; ++index )
					{
						this->u8(index < name.size() && index < 19 ? static_cast< uint8_t >(name[index]) : uint8_t{0});
					}
				}

				void
				append (const std::vector< std::byte > & bytes) noexcept
				{
					m_bytes.insert(m_bytes.end(), bytes.cbegin(), bytes.cend());
				}

				/** @brief Wraps the bytes written into a chunk: its id, its size, the bytes, a pad byte when odd. */
				[[nodiscard]]
				std::vector< std::byte >
				chunk (std::string_view id) const noexcept
				{
					Writer wrapped;

					wrapped.tag(id);
					wrapped.u32(static_cast< uint32_t >(m_bytes.size()));
					wrapped.append(m_bytes);

					if ( (m_bytes.size() & 1U) != 0 )
					{
						wrapped.u8(0);
					}

					return wrapped.m_bytes;
				}

				/** @brief Wraps the bytes written into a 'RIFF' or a 'LIST' chunk of a type. */
				[[nodiscard]]
				std::vector< std::byte >
				container (Container kind, std::string_view type) const noexcept
				{
					Writer typed;

					typed.tag(type);
					typed.append(m_bytes);

					return typed.chunk(kind == Container::RIFF ? "RIFF" : "LIST");
				}

			private:

				std::vector< std::byte > m_bytes;
		};

		/** @brief One generator of a zone. */
		struct Generator
		{
			uint16_t operation{0};
			uint16_t amount{0};
		};

		[[nodiscard]]
		Generator
		range (uint16_t operation, uint8_t low, uint8_t high) noexcept
		{
			return {.operation = operation, .amount = static_cast< uint16_t >(low | (static_cast< uint16_t >(high) << 8U))};
		}

		[[nodiscard]]
		Generator
		signedAmount (uint16_t operation, int32_t value) noexcept
		{
			return {.operation = operation, .amount = std::bit_cast< uint16_t >(static_cast< int16_t >(std::clamp(value, -32768, 32767)))};
		}

		/** @brief The region's articulation over the instrument's: the region's values win, the instrument's fill. */
		[[nodiscard]]
		Articulation
		mergedArticulation (const std::optional< Articulation > & instrument, const std::optional< Articulation > & region) noexcept
		{
			if ( !instrument.has_value() )
			{
				return region.value_or(Articulation{});
			}

			if ( !region.has_value() )
			{
				return instrument.value();
			}

			auto merged = region.value();
			const auto & fallback = instrument.value();

			merged.attack = merged.attack.has_value() ? merged.attack : fallback.attack;
			merged.decay = merged.decay.has_value() ? merged.decay : fallback.decay;
			merged.release = merged.release.has_value() ? merged.release : fallback.release;
			merged.delay = merged.delay.has_value() ? merged.delay : fallback.delay;
			merged.hold = merged.hold.has_value() ? merged.hold : fallback.hold;
			merged.sustainCentibels = merged.sustainCentibels.has_value() ? merged.sustainCentibels : fallback.sustainCentibels;
			merged.pan = merged.pan.has_value() ? merged.pan : fallback.pan;

			if ( merged.gainCentibels == 0 )
			{
				merged.gainCentibels = fallback.gainCentibels;
			}

			return merged;
		}

		/** @brief The parsed bank. */
		struct Bank
		{
			std::vector< Instrument > instruments;
			std::vector< Wave > waves;
		};

		/** @brief Reads a 'ptbl' chunk: the pool table's cues. */
		[[nodiscard]]
		bool
		parsePoolTable (std::span< const std::byte > data, std::vector< uint32_t > & cues) noexcept
		{
			const auto headerSize = readU32(data, 0);
			const auto count = readU32(data, 4);

			if ( !headerSize.has_value() || !count.has_value() || headerSize.value() < 8 || count.value() > MaxWaves )
			{
				return false;
			}

			cues.clear();
			cues.reserve(count.value());

			for ( uint32_t index = 0; index < count.value(); ++index )
			{
				const auto cue = readU32(data, static_cast< size_t >(headerSize.value()) + (static_cast< size_t >(index) * 4U));

				if ( !cue.has_value() )
				{
					return false;
				}

				cues.push_back(cue.value());
			}

			return true;
		}

		/** @brief Reads an 'lins' list body (after its type); an instrument it cannot read refuses the bank. */
		[[nodiscard]]
		bool
		parseInstrumentList (std::span< const std::byte > list, std::vector< Instrument > & instruments) noexcept
		{
			return forEachChunk(list, [&list, &instruments] (size_t offset, std::span< const std::byte > data) {
				if ( !isTag(list, offset, "LIST") || !isTag(data, 0, "ins ") )
				{
					return true;
				}

				if ( instruments.size() >= MaxInstruments )
				{
					return false;
				}

				auto instrument = parseInstrument(data.subspan(4));

				if ( !instrument.has_value() )
				{
					return false;
				}

				instruments.push_back(std::move(instrument.value()));

				return true;
			});
		}

		/**
		 * @brief The waves of the pool in pool-table order. Each cue is the offset of a wave's LIST chunk from the start of
		 * the pool's data (after 'wvpl'); a cue that points at no readable wave gives an unusable one, kept for the indices.
		 */
		[[nodiscard]]
		std::vector< Wave >
		readPoolWaves (std::span< const std::byte > pool, const std::vector< uint32_t > & cues) noexcept
		{
			std::vector< Wave > waves;
			waves.reserve(cues.size());

			for ( const auto cue : cues )
			{
				const auto size = readU32(pool, static_cast< size_t >(cue) + 4);

				/* The LIST tag first: it proves cue + 4 is inside the pool, and the size read proves cue + 8 is. */
				if ( !isTag(pool, cue, "LIST") || !size.has_value() || size.value() > pool.size() - cue - 8 || !isTag(pool, static_cast< size_t >(cue) + 8, "wave") )
				{
					waves.emplace_back();

					continue;
				}

				auto wave = parseWave(pool.subspan(static_cast< size_t >(cue) + 12, size.value() - 4));
				waves.push_back(wave.has_value() ? std::move(wave.value()) : Wave{});
			}

			return waves;
		}

		/** @brief Reads the bank: the instruments, the pool table, the wave pool (the waves in pool-table order). */
		[[nodiscard]]
		std::optional< Bank >
		parseBank (std::span< const std::byte > dlsBank) noexcept
		{
			if ( !isTag(dlsBank, 0, "RIFF") || !isTag(dlsBank, 8, "DLS ") )
			{
				return std::nullopt;
			}

			const auto declared = readU32(dlsBank, 4);

			if ( !declared.has_value() || declared.value() < 4 || declared.value() > dlsBank.size() - 8 )
			{
				return std::nullopt;
			}

			const auto body = dlsBank.subspan(12, declared.value() - 4);
			Bank bank;
			std::vector< uint32_t > cues;
			std::span< const std::byte > pool;
			bool hasPool = false;

			const auto parsed = forEachChunk(body, [&body, &bank, &cues, &pool, &hasPool] (size_t offset, std::span< const std::byte > data) {
				if ( isTag(body, offset, "ptbl") )
				{
					return parsePoolTable(data, cues);
				}

				if ( !isTag(body, offset, "LIST") )
				{
					return true;
				}

				if ( isTag(data, 0, "lins") )
				{
					return parseInstrumentList(data.subspan(4), bank.instruments);
				}

				if ( isTag(data, 0, "wvpl") )
				{
					pool = data.subspan(4);
					hasPool = true;
				}

				return true;
			});

			if ( !parsed || !hasPool || cues.empty() )
			{
				return std::nullopt;
			}

			bank.waves = readPoolWaves(pool, cues);

			return bank;
		}

		/** @brief One SoundFont sample header: a region's window on a wave. */
		struct SampleHeader
		{
			uint32_t start{0};
			uint32_t end{0};
			uint32_t loopStart{0};
			uint32_t loopEnd{0};
			uint32_t sampleRate{0};
			uint8_t originalPitch{60};
			int8_t pitchCorrection{0};
			bool looped{false};
		};

		/** @brief The sample header of a region on a wave starting at `start` in the sample pool. */
		[[nodiscard]]
		SampleHeader
		sampleHeader (const WaveSample & sample, const Wave & wave, uint32_t start) noexcept
		{
			const auto count = static_cast< uint32_t >(wave.samples.size());
			/* A loop outside the wave is dropped, not clamped: the region plays once. */
			const bool looped = sample.looped && sample.loopStart < count && sample.loopLength <= count - sample.loopStart;

			return {
				.start = start,
				.end = start + count,
				.loopStart = looped ? start + sample.loopStart : start,
				.loopEnd = looped ? start + sample.loopStart + sample.loopLength : start + count,
				.sampleRate = wave.sampleRate,
				.originalPitch = static_cast< uint8_t >(sample.unityNote),
				.pitchCorrection = static_cast< int8_t >(sample.fineTune),
				.looped = looped
			};
		}

		/** @brief Writes one 'shdr' record (SoundFont 2.04 § 7.10). */
		void
		writeSampleHeader (Writer & writer, size_t index, const SampleHeader & header) noexcept
		{
			writer.name20("dls" + std::to_string(index));
			writer.u32(header.start);
			writer.u32(header.end);
			writer.u32(header.loopStart);
			writer.u32(header.loopEnd);
			writer.u32(header.sampleRate);
			writer.u8(header.originalPitch);
			writer.u8(std::bit_cast< uint8_t >(header.pitchCorrection));
			writer.u16(0);
			writer.u16(1); /* Mono. */
		}

		/**
		 * @brief The generators of a region's zone, all but its sampleID (which the caller appends: it comes last).
		 * @note SoundFont 2.04 § 8.1.2: keyRange first, velRange second.
		 */
		[[nodiscard]]
		std::vector< Generator >
		regionZone (const Region & region, const Articulation & articulation, const WaveSample & sample, bool looped) noexcept
		{
			std::vector< Generator > zone;

			zone.push_back(range(GenKeyRange, region.keyLow, region.keyHigh));
			zone.push_back(range(GenVelRange, region.velocityLow, region.velocityHigh));

			if ( const auto attenuation = -(sample.gainCentibels + articulation.gainCentibels); attenuation > 0 )
			{
				zone.push_back(signedAmount(GenInitialAttenuation, std::min(attenuation, 1440)));
			}

			const std::array< std::pair< uint16_t, std::optional< int32_t > >, 7 > envelope{{
				{GenDelayVolEnv, articulation.delay}, {GenAttackVolEnv, articulation.attack}, {GenHoldVolEnv, articulation.hold},
				{GenDecayVolEnv, articulation.decay}, {GenSustainVolEnv, articulation.sustainCentibels},
				{GenReleaseVolEnv, articulation.release}, {GenPan, articulation.pan}
			}};

			for ( const auto & [operation, value] : envelope )
			{
				if ( value.has_value() )
				{
					zone.push_back(signedAmount(operation, value.value()));
				}
			}

			if ( region.keyGroup != 0 )
			{
				zone.push_back({.operation = GenExclusiveClass, .amount = region.keyGroup});
			}

			if ( looped )
			{
				zone.push_back({.operation = GenSampleModes, .amount = 1});
			}

			return zone;
		}

		/** @brief The sample pool: every usable wave once, each followed by its 46 zero samples, and where each starts. */
		struct SamplePool
		{
			Writer data;
			std::vector< std::optional< uint32_t > > waveStart;
		};

		/** @brief Lays the usable waves out in the sample pool; nothing when it would overflow the 32-bit sample indices. */
		[[nodiscard]]
		std::optional< SamplePool >
		layoutSamples (const std::vector< Wave > & waves) noexcept
		{
			SamplePool pool;
			pool.waveStart.resize(waves.size());
			uint64_t cursor = 0;

			for ( size_t index = 0; index < waves.size(); ++index )
			{
				const auto & wave = waves[index];

				if ( !wave.usable )
				{
					continue;
				}

				if ( cursor + wave.samples.size() + SampleTailZeros > std::numeric_limits< uint32_t >::max() )
				{
					return std::nullopt;
				}

				pool.waveStart[index] = static_cast< uint32_t >(cursor);

				for ( const auto sample : wave.samples )
				{
					pool.data.u16(std::bit_cast< uint16_t >(sample));
				}

				for ( size_t zero = 0; zero < SampleTailZeros; ++zero )
				{
					pool.data.u16(0);
				}

				cursor += wave.samples.size() + SampleTailZeros;
			}

			return pool;
		}

		/** @brief The SoundFont 'pdta' records being written: one preset per instrument, one instrument zone per region. */
		class Hydra final
		{
			public:

				/**
				 * @brief Adds an instrument: its playable regions (a wave in the pool) as zones, and its preset.
				 * @return bool False when the bank holds more regions than SoundFont sample headers can index.
				 */
				[[nodiscard]]
				bool
				addInstrument (const Instrument & instrument, const std::vector< Wave > & waves, const std::vector< std::optional< uint32_t > > & waveStart) noexcept
				{
					std::vector< std::vector< Generator > > zones;

					for ( const auto & region : instrument.regions )
					{
						if ( region.tableIndex >= waves.size() )
						{
							continue;
						}

						const auto start = waveStart[region.tableIndex];

						if ( !start.has_value() )
						{
							continue;
						}

						if ( m_sampleHeaderCount >= MaxSampleHeaders )
						{
							return false;
						}

						const auto & wave = waves[region.tableIndex];
						/* The region's own wave sample header, else the wave's, else unity note 60 with no loop. */
						const auto sample = region.sample.has_value() ? region.sample.value() : wave.sample.value_or(WaveSample{});
						const auto header = sampleHeader(sample, wave, start.value());

						writeSampleHeader(m_sampleHeaders, m_sampleHeaderCount, header);

						auto zone = regionZone(region, mergedArticulation(instrument.articulation, region.articulation), sample, header.looped);
						zone.push_back({.operation = GenSampleID, .amount = static_cast< uint16_t >(m_sampleHeaderCount)});
						zones.push_back(std::move(zone));
						++m_sampleHeaderCount;
					}

					if ( !zones.empty() )
					{
						this->writeInstrument(instrument, zones);
					}

					return true;
				}

				/** @brief The instruments written so far. */
				[[nodiscard]]
				uint32_t
				instrumentCount () const noexcept
				{
					return m_instrumentCount;
				}

				/** @brief Writes the terminal records (SoundFont 2.04 § 7.2 - 7.10) and returns the 'pdta' list. */
				[[nodiscard]]
				std::vector< std::byte >
				finish () noexcept
				{
					m_presetHeaders.name20("EOP");
					m_presetHeaders.u16(0);
					m_presetHeaders.u16(0);
					m_presetHeaders.u16(m_presetBagCount);
					m_presetHeaders.u32(0);
					m_presetHeaders.u32(0);
					m_presetHeaders.u32(0);
					m_presetBags.u16(m_presetGeneratorCount);
					m_presetBags.u16(0);
					m_presetGenerators.u32(0);
					m_instrumentHeaders.name20("EOI");
					m_instrumentHeaders.u16(m_instrumentBagCount);
					m_instrumentBags.u16(m_instrumentGeneratorCount);
					m_instrumentBags.u16(0);
					m_instrumentGenerators.u32(0);
					m_sampleHeaders.name20("EOS");

					for ( size_t zero = 0; zero < 26; ++zero )
					{
						m_sampleHeaders.u8(0);
					}

					/* No modulator: the terminal record alone. */
					Writer modulators;

					for ( size_t zero = 0; zero < 10; ++zero )
					{
						modulators.u8(0);
					}

					Writer hydra;
					hydra.append(m_presetHeaders.chunk("phdr"));
					hydra.append(m_presetBags.chunk("pbag"));
					hydra.append(modulators.chunk("pmod"));
					hydra.append(m_presetGenerators.chunk("pgen"));
					hydra.append(m_instrumentHeaders.chunk("inst"));
					hydra.append(m_instrumentBags.chunk("ibag"));
					hydra.append(modulators.chunk("imod"));
					hydra.append(m_instrumentGenerators.chunk("igen"));
					hydra.append(m_sampleHeaders.chunk("shdr"));

					return hydra.container(Container::LIST, "pdta");
				}

			private:

				/** @brief Writes an instrument with its zones, and one preset with one zone pointing at it. */
				void
				writeInstrument (const Instrument & instrument, const std::vector< std::vector< Generator > > & zones) noexcept
				{
					const auto name = instrument.name.empty() ? "dls" + std::to_string(m_instrumentCount) : instrument.name;

					m_instrumentHeaders.name20(name);
					m_instrumentHeaders.u16(m_instrumentBagCount);

					for ( const auto & zone : zones )
					{
						m_instrumentBags.u16(m_instrumentGeneratorCount);
						m_instrumentBags.u16(0);
						++m_instrumentBagCount;

						for ( const auto & generator : zone )
						{
							m_instrumentGenerators.u16(generator.operation);
							m_instrumentGenerators.u16(generator.amount);
							++m_instrumentGeneratorCount;
						}
					}

					m_presetHeaders.name20(name);
					m_presetHeaders.u16(instrument.program);
					m_presetHeaders.u16(instrument.bank);
					m_presetHeaders.u16(m_presetBagCount);
					m_presetHeaders.u32(0);
					m_presetHeaders.u32(0);
					m_presetHeaders.u32(0);

					m_presetBags.u16(m_presetGeneratorCount);
					m_presetBags.u16(0);
					++m_presetBagCount;

					m_presetGenerators.u16(GenInstrument);
					m_presetGenerators.u16(m_instrumentCount);
					++m_presetGeneratorCount;
					++m_instrumentCount;
				}

				Writer m_presetHeaders;
				Writer m_presetBags;
				Writer m_presetGenerators;
				Writer m_instrumentHeaders;
				Writer m_instrumentBags;
				Writer m_instrumentGenerators;
				Writer m_sampleHeaders;
				size_t m_sampleHeaderCount{0};
				uint32_t m_presetBagCount{0};
				uint32_t m_presetGeneratorCount{0};
				uint32_t m_instrumentCount{0};
				uint32_t m_instrumentBagCount{0};
				uint32_t m_instrumentGeneratorCount{0};
		};

		/** @brief Writes the SoundFont file of a parsed bank; nothing when it holds no playable region or overflows. */
		[[nodiscard]]
		std::optional< std::vector< std::byte > >
		writeSoundFont (const Bank & bank) noexcept
		{
			const auto pool = layoutSamples(bank.waves);

			if ( !pool.has_value() )
			{
				return std::nullopt;
			}

			Hydra hydra;

			for ( const auto & instrument : bank.instruments )
			{
				if ( !hydra.addInstrument(instrument, bank.waves, pool->waveStart) )
				{
					return std::nullopt;
				}
			}

			if ( hydra.instrumentCount() == 0 )
			{
				return std::nullopt;
			}

			Writer info;
			{
				Writer version;
				version.u16(2);
				version.u16(1);
				info.append(version.chunk("ifil"));

				Writer engine;
				engine.name20("EMU8000");
				info.append(engine.chunk("isng"));

				Writer bankName;
				bankName.name20("Embedded DLS");
				info.append(bankName.chunk("INAM"));
			}

			Writer sampleData;
			sampleData.append(pool->data.chunk("smpl"));

			Writer file;
			file.append(info.container(Container::LIST, "INFO"));
			file.append(sampleData.container(Container::LIST, "sdta"));
			file.append(hydra.finish());

			return file.container(Container::RIFF, "sfbk");
		}
	}

	std::optional< std::vector< std::byte > >
	convertToSoundFont (std::span< const std::byte > dlsBank) noexcept
	{
		const auto bank = parseBank(dlsBank);

		if ( !bank.has_value() )
		{
			Logging::warning(TracerTag, "convertToSoundFont(), the DLS bank is malformed !");

			return std::nullopt;
		}

		auto soundFont = writeSoundFont(bank.value());

		if ( !soundFont.has_value() )
		{
			Logging::warning(TracerTag, "convertToSoundFont(), the DLS bank holds no playable region !");
		}

		return soundFont;
	}
}
