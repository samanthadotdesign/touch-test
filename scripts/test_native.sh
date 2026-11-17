#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
native_test_dir="${TMPDIR:-/tmp}/poetry-native-tests"
mkdir -p "$native_test_dir"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
  -Itest/native_stubs -Isrc -I'.pio/libdeps/esp32s3/GFX Library for Arduino/src' \
  test/poetry_native.cpp src/app.cpp src/letters.cpp src/atmosphere.cpp \
  -o "$native_test_dir/poetry"
"$native_test_dir/poetry"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
  -Itest/native_stubs -Isrc test/atmosphere_native.cpp src/atmosphere.cpp \
  -o "$native_test_dir/atmosphere"
"$native_test_dir/atmosphere"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
  -Itest/audio_stubs -Isrc test/audio_input_native.cpp src/audio_input.cpp \
  -o "$native_test_dir/audio"
"$native_test_dir/audio"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
  -Itest/display_stubs -Isrc test/display_native.cpp src/display.cpp \
  -o "$native_test_dir/display"
"$native_test_dir/display" canvas
"$native_test_dir/display" fallback
