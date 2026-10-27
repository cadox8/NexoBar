#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=${1:-"$project_dir/cmake-build-release"}
output_dir="$project_dir/dist/nexobar"

if [ ! -f "$build_dir/cmake_install.cmake" ]; then
    echo "ERROR: Release directory not found or not configured: $build_dir" >&2
    echo "Usage: $0 [release-build-directory]" >&2
    exit 1
fi

cmake_executable=$(sed -n 's/^CMAKE_COMMAND:INTERNAL=//p' "$build_dir/CMakeCache.txt" | head -n 1)
if [ -z "$cmake_executable" ]; then
    cmake_executable=$(command -v cmake || true)
fi
if [ -z "$cmake_executable" ] || [ ! -x "$cmake_executable" ]; then
    echo "ERROR: CMake could not be found." >&2
    exit 1
fi

"$cmake_executable" -E remove_directory "$output_dir"
"$cmake_executable" --install "$build_dir" --config Release --component NexoBarRuntime --prefix "$output_dir"

if [ ! -x "$output_dir/NexoBar" ]; then
    echo "ERROR: NexoBar was not copied to the package." >&2
    exit 1
fi
empty_resource=$(find "$output_dir/resources" -type f -size 0 -print -quit)
if [ -n "$empty_resource" ]; then
    echo "ERROR: Empty resource file: $empty_resource" >&2
    exit 1
fi

echo "NexoBar package created in $output_dir"
