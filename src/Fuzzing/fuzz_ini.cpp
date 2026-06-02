/*
 * fuzzing/fuzz_ini.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the INIParser (Ave robustus! — A.3).
 * INIParser::read() consumes a file path (std::ifstream + getline line classification), so the
 * fuzzer materialises each input into a per-process temp file and parses it under ASan/UBSan.
 * Exercises getLineType / parseSectionTitle / the section+variable population loop.
 */

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

/* POSIX. */
#include <unistd.h>

/* Local inclusions. */
#include "INIParser.hpp"

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
	using namespace EmEn::Base;

	/* One stable scratch path per fuzzer process (libFuzzer is single-threaded per process). */
	static const std::filesystem::path path =
		std::filesystem::temp_directory_path() / ("fuzz_ini_" + std::to_string(::getpid()) + ".ini");

	{
		std::ofstream out{path, std::ios::binary | std::ios::trunc};
		out.write(reinterpret_cast< const char * >(data), static_cast< std::streamsize >(size));
	}

	INIParser parser;
	(void)parser.read(path);

	return 0;
}