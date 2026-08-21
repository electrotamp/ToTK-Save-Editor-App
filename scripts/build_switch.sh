#!/usr/bin/env bash
set -euo pipefail

# Uses the active devkitPro installation. Set DEVKITPRO explicitly if it is
# not already set by the devkitPro MSYS environment.
if [[ -z "${DEVKITPRO:-}" || ! -f "${DEVKITPRO}/cmake/Switch.cmake" ]]; then
  export DEVKITPRO="/opt/devkitpro"
fi
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
export PATH="$DEVKITPRO/msys2/usr/bin:$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
# GNU Make breaks on spaces in paths; use junction if available.
if [[ "$PROJECT_DIR" == *" "* && -d "/e/totk-save-editor" ]]; then
  PROJECT_DIR="/e/totk-save-editor"
fi
BUILD_DIR="$PROJECT_DIR/build-switch"

# shellcheck source=backup_source_snapshot.sh
source "$SCRIPT_DIR/backup_source_snapshot.sh"
backup_source_snapshot "$PROJECT_DIR"

if [[ ! -f "$DEVKITPRO/cmake/Switch.cmake" ]]; then
  echo "devkitPro not found at $DEVKITPRO" >&2
  exit 1
fi

echo "Using DEVKITPRO=$DEVKITPRO"
echo "Project: $PROJECT_DIR"

if [[ ! -f "$PROJECT_DIR/resources/data/items.json" ]]; then
  echo "Generating assets (first build)..."
  powershell.exe -ExecutionPolicy Bypass -File "$PROJECT_DIR/scripts/setup_assets.ps1"
fi

mkdir -p "$BUILD_DIR"
# Do not rm -rf build-switch on Windows (deep romfs tree fails under MSYS).
cmake -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
  -DPLATFORM_SWITCH=ON \
  -DCMAKE_BUILD_TYPE=Release \
  "$PROJECT_DIR"

cmake --build "$BUILD_DIR" --target totk_save_editor.nro -j

backup_source_snapshot_nro "$PROJECT_DIR" "$BUILD_DIR"

echo "Built: $BUILD_DIR/totk_save_editor.nro"
