#!/usr/bin/env bash
set -euo pipefail

args=(
  -S .
  -B build
  -G Ninja
  -DCMAKE_BUILD_TYPE=Release
  -DRIX_PLAYER_STATIC=ON
)

if [[ $# -ge 1 && -n "$1" ]]; then
  if command -v cygpath >/dev/null 2>&1; then
    root="$(cygpath -m "$1")"
  else
    root="$1"
  fi
  args+=("-DSDLPAL_ROOT=$root")
fi

cmake "${args[@]}"
cmake --build build
printf '\nBuilt: %s\n' "$(pwd)/build/sdlpal-rix-player.exe"
