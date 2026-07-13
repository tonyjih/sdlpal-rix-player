#!/usr/bin/env sh
set -eu

if [ "$#" -ge 1 ] && [ -n "$1" ]; then
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDLPAL_ROOT="$1"
else
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build
printf '\nBuilt: %s\n' "$(pwd)/build/sdlpal-rix-player"
