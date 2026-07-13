#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 /path/to/sdlpal [version]" >&2
  exit 2
fi

sdlpal_root="$1"
version="${2:-0.1.0}"
repo_root="$(cd "$(dirname "$0")" && pwd)"
dist_dir="$repo_root/dist"
bundle_name="sdlpal-rix-player-${version}-source-with-sdlpal"
work_dir="$dist_dir/$bundle_name"
archive="$dist_dir/${bundle_name}.tar.gz"

if ! git -C "$repo_root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "The player directory must be a Git repository." >&2
  exit 1
fi
if ! git -C "$sdlpal_root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "SDLPAL_ROOT must point to a Git checkout." >&2
  exit 1
fi

rm -rf "$work_dir"
mkdir -p "$work_dir/player" "$work_dir/third_party/sdlpal"

# Export only tracked files. This avoids build trees, local settings, and game
# data that should never be part of a source release.
git -C "$repo_root" archive --format=tar HEAD | tar -xf - -C "$work_dir/player"
git -C "$sdlpal_root" archive --format=tar HEAD | tar -xf - -C "$work_dir/third_party/sdlpal"

git -C "$repo_root" rev-parse HEAD > "$work_dir/PLAYER_COMMIT.txt"
git -C "$sdlpal_root" rev-parse HEAD > "$work_dir/SDLPAL_COMMIT.txt"

mkdir -p "$dist_dir"
tar -czf "$archive" -C "$dist_dir" "$bundle_name"
rm -rf "$work_dir"

printf 'Created: %s\n' "$archive"
