#!/usr/bin/env bash
# Build osheet_cpp in Release with LTO + profile-guided optimization.
set -euo pipefail

export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/.local/share/vcpkg}"
if [[ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo "VCPKG_ROOT not found at $VCPKG_ROOT" >&2
    exit 1
fi

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DVCPKG_ROOT="$VCPKG_ROOT"
cmake --build build
