#!/usr/bin/env bash
#
# src/Fuzzing/build-fuzzers.sh — build the emeraude-base libFuzzer targets (Ave robustus! A.3).
#
# libFuzzer needs clang + the clang_rt runtimes (Debian: libclang-rt-<ver>-dev). The base
# library itself stays a normal g++ build; each fuzz target is a standalone clang executable
# that links the g++-built static lib for compiled symbols (Logging, Processor, FastJSON, ...)
# and instruments the (header-only) parser code it pulls in. The parsers are exercised with
# the SAME flags base uses (-fno-exceptions), plus ASan + UBSan + libFuzzer coverage.
#
# Prerequisites: a configured emeraude-base build dir providing the generated config header
# and the static library. Defaults to .claude-build-debug; override with BASE_BUILD_DIR.
#
# Usage:   src/Fuzzing/build-fuzzers.sh
#          BASE_BUILD_DIR=.claude-build-release src/Fuzzing/build-fuzzers.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${ROOT}"

CXX="${CXX:-clang++}"
BASE_BUILD_DIR="${BASE_BUILD_DIR:-.claude-build-debug}"
OUT_DIR="${OUT_DIR:-src/Fuzzing/build}"

# Resolve the ext-deps root and the static library produced by the base build.
EXT_LIBS_PATH="$(sed -n 's/^EMERAUDE_EXT_LIBS_PATH:PATH=//p' "${BASE_BUILD_DIR}/CMakeCache.txt")"
STATIC_LIB="$(find "${BASE_BUILD_DIR}" -name 'libEmeraudeBase.a' -print -quit)"

if [ -z "${EXT_LIBS_PATH}" ] || [ -z "${STATIC_LIB}" ]; then
	echo "error: configure/build ${BASE_BUILD_DIR} first (need libEmeraudeBase.a + CMakeCache)." >&2
	exit 1
fi

INCLUDES=(
	-I src
	-I "${BASE_BUILD_DIR}/include"
	-I "${EXT_LIBS_PATH}/include"
	-I dependencies/tinysoundfont
)

# Audio/codec chain + image codecs + compression + jsoncpp, ordered dependents-before-dependencies
# for the static linker (e.g. libpng before libz). Over-linking is harmless: unreferenced archive
# members are dropped, so a single shared link line serves every target.
EXT_LINK=(
	-L "${EXT_LIBS_PATH}/lib"
	-lsamplerate -lsndfile -lFLAC -lvorbisenc -lvorbis -lopus -lmpg123 -lmp3lame -logg
	-lpng -ljpeg -llzma
	-ljsoncpp -lz -lm
)

FLAGS=(-std=c++20 -fno-exceptions -g -O1 -fsanitize=fuzzer,address,undefined)

TARGETS=(
	fuzz_midi fuzz_obj fuzz_wav fuzz_json_sfx
	fuzz_png fuzz_jpeg fuzz_targa fuzz_hdr
	fuzz_native fuzz_stl fuzz_mdx
	fuzz_compression fuzz_ini
	fuzz_http_response
)

# A parser implemented in a .cpp sits in the g++-built static library, uninstrumented: libFuzzer gets no coverage of
# it and ASan / UBSan see none of its accesses (fuzz_http_response ran 520 M blind executions at 59 edges, 2026-10-17).
# Such a target compiles its parser's own sources, instrumented; the archive's copies are then never pulled in.
declare -A EXTRA_SOURCES=(
	[fuzz_compression]="src/Compression/LZMA.cpp src/Compression/LZMA/Decompressor.cpp src/Compression/ZLIB.cpp"
	[fuzz_http_response]="src/Network/HTTPResponseParser.cpp src/Network/HTTPResponse.cpp src/Network/HTTPHeaders.cpp"
	[fuzz_ini]="src/INIParser.cpp"
	[fuzz_jpeg]="src/PixelFactory/FileFormatJpeg.cpp"
	[fuzz_png]="src/PixelFactory/FileFormatPNG.cpp"
	[fuzz_wav]="src/WaveFactory/FileFormatSNDFile.cpp"
)

mkdir -p "${OUT_DIR}"

for target in "${TARGETS[@]}"; do
	echo "==> building ${target}"
	read -r -a extraSources <<< "${EXTRA_SOURCES[${target}]:-}"
	"${CXX}" "${FLAGS[@]}" "${INCLUDES[@]}" \
		"src/Fuzzing/${target}.cpp" \
		"${extraSources[@]}" \
		"${STATIC_LIB}" \
		"${EXT_LINK[@]}" \
		-o "${OUT_DIR}/${target}"
	mkdir -p "${OUT_DIR}/${target}.corpus"
done

echo
echo "built: ${TARGETS[*]/#/${OUT_DIR}/}"
echo "run e.g.: ASAN_OPTIONS=detect_leaks=0 ${OUT_DIR}/fuzz_midi ${OUT_DIR}/fuzz_midi.corpus -max_total_time=60"