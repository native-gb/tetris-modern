#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev}"
"$(dirname "$0")/build.sh" "$preset"
ctest --preset "$preset"
