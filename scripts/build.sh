#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
preset="${1:-dev}"
cd "$repo_root"
cmake --preset "$preset"
cmake --build --preset "$preset"
