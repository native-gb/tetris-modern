#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
work_root="$(mktemp -d "${TMPDIR:-/tmp}/native-gb-tetris-clean.XXXXXX")"
checkout="$work_root/source"
build="$work_root/build"
trap 'rm -rf "$work_root"' EXIT

mkdir -p "$checkout"
git -C "$repo_root" archive HEAD | tar -x -C "$checkout"

if git -C "$repo_root" ls-files | grep -E '\.(gb|gbc|gba|sav|ss[0-9]|png|gif|wav|flac|ogg)$' >/dev/null; then
    echo "Tracked ROM, capture, save-state, or extracted asset found." >&2
    exit 1
fi

cmake -S "$checkout" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DTETRIS_STRICT=ON \
    -DTETRIS_WARN_AS_ERROR=ON \
    -DTETRIS_GUBSY_DIR="$work_root/missing-gubsy" \
    -DTETRIS_GB_AUDIO_DIR="$work_root/missing-gb-audio" \
    "${@}"
cmake --build "$build" --parallel
ctest --test-dir "$build" --output-on-failure

if strings "$build/native-gb-tetris-modern" | \
        grep -E 'native-gb-tetris-re|GBRE|mGBA|gbre_live' >/dev/null; then
    echo "Private reverse-engineering runtime string found in the clean executable." >&2
    exit 1
fi

echo "Clean no-ROM checkout passed."
