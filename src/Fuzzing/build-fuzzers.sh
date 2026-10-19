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
#
# Portable to macOS's bash 3.2 (no associative array, no empty-array expansion under 'set -u'). On macOS, AppleClang
# ships no libFuzzer runtime: the default compiler is Homebrew's LLVM (brew install llvm); CXX overrides it.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${ROOT}"

if [ -z "${CXX:-}" ]; then
	if [ "$(uname -s)" = "Darwin" ]; then
		CXX="$(brew --prefix llvm 2>/dev/null)/bin/clang++"

		if [ ! -x "${CXX}" ]; then
			echo "error: AppleClang has no libFuzzer runtime; install Homebrew's LLVM (brew install llvm) or set CXX." >&2
			exit 1
		fi
	else
		CXX="clang++"
	fi
fi

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
# NOTE: A 'case', not an associative array: bash 3.2 (macOS) has none.
extra_sources ()
{
	case "$1" in
		fuzz_compression) echo "src/Compression/LZMA.cpp src/Compression/LZMA/Decompressor.cpp src/Compression/ZLIB.cpp" ;;
		fuzz_http_response) echo "src/Network/HTTPResponseParser.cpp src/Network/HTTPResponse.cpp src/Network/HTTPHeaders.cpp" ;;
		fuzz_ini) echo "src/INIParser.cpp" ;;
		fuzz_jpeg) echo "src/PixelFactory/FileFormatJpeg.cpp" ;;
		fuzz_png) echo "src/PixelFactory/FileFormatPNG.cpp" ;;
		fuzz_wav) echo "src/WaveFactory/FileFormatSNDFile.cpp" ;;
		*) echo "" ;;
	esac
}

mkdir -p "${OUT_DIR}"

for target in "${TARGETS[@]}"; do
	echo "==> building ${target}"
	read -r -a extraSources <<< "$(extra_sources "${target}")"
	# NOTE: ${array[@]+"${array[@]}"}: before bash 4.4, an empty array expanded under 'set -u' is an unbound variable.
	"${CXX}" "${FLAGS[@]}" "${INCLUDES[@]}" \
		"src/Fuzzing/${target}.cpp" \
		${extraSources[@]+"${extraSources[@]}"} \
		"${STATIC_LIB}" \
		"${EXT_LINK[@]}" \
		-o "${OUT_DIR}/${target}"
	mkdir -p "${OUT_DIR}/${target}.corpus"
done

echo
echo "built: ${TARGETS[*]/#/${OUT_DIR}/}"
echo "run e.g.: ASAN_OPTIONS=detect_leaks=0 ${OUT_DIR}/fuzz_midi ${OUT_DIR}/fuzz_midi.corpus -max_total_time=60"