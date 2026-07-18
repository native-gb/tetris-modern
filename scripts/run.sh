#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
preset="${NATIVE_TETRIS_PRESET:-dev}"
case "$preset" in
    dev) build_dir="build-debug" ;;
    asan) build_dir="build-asan" ;;
    release) build_dir="build" ;;
    *)
        echo "Unknown preset: $preset" >&2
        exit 2
        ;;
esac

"$repo_root/scripts/build.sh" "$preset"
exec "$repo_root/$build_dir/native-gb-tetris-modern" \
    --rom "$repo_root/roms/Tetris (JUE) (V1.1) [!].gb" "$@"
