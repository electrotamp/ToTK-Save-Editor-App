#!/usr/bin/env bash
set -euo pipefail

export DEVKITPRO="${DEVKITPRO:-/e/devkitpro}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
if [[ "$PROJECT_DIR" == *" "* && -d "/e/totk-save-editor" ]]; then
  PROJECT_DIR="/e/totk-save-editor"
fi
BUILD_DIR="$PROJECT_DIR/build-switch"
GPP="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-g++.exe"
AR="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-ar.exe"
BOREALIS_LIB="$BUILD_DIR/lib/borealis/library/libborealis.a"
OUT_OBJ="$BUILD_DIR/borealis_image_atlas.o"

SRC="$PROJECT_DIR/lib/borealis/library/lib/views/image.cpp"
if [[ -f "$OUT_OBJ" && "$OUT_OBJ" -nt "$SRC" && "$OUT_OBJ" -nt "$0" ]]; then
  echo "Reusing cached $OUT_OBJ (image.cpp unchanged)"
  "$AR" d "$BOREALIS_LIB" image.o
  "$AR" r "$BOREALIS_LIB" "$OUT_OBJ"
  echo "Patched $BOREALIS_LIB"
  exit 0
fi

echo "Rebuilding borealis image.o with atlas support..."
# Retried with growing delays: immediately after the icon-atlas step's burst
# of large PNG writes (build_icon_atlas.py, just before this script runs) and
# the source-backup snapshot's file copying, the very next compiler
# invocation has intermittently failed here with a temporary-file permission
# error — transient contention right after
# heavy disk I/O, not a real toolchain problem (the identical command always
# succeeds run standalone moments later). Observed reproducibly during the
# 2026-08-17 Autobuild networking work. The mtime-based cache above is the
# real fix (this step becomes a no-op on every build after the first where
# image.cpp hasn't changed); the retry loop is a fallback for the first-ever
# build in a fresh build-switch directory.
compile_ok=0
for attempt in 1 2 3 4 5; do
  if "$GPP" -D__GLFW__ \
    -I"$PROJECT_DIR/lib/borealis/library/include" \
    -I"$PROJECT_DIR/lib/borealis/library/include/borealis/extern" \
    -I"$PROJECT_DIR/lib/borealis/library/include/borealis/extern/nanovg" \
    -I"$PROJECT_DIR/lib/borealis/library/lib/extern/fmt/include" \
    -I"$PROJECT_DIR/lib/borealis/library/lib/extern/tweeny/include" \
    -I"$PROJECT_DIR/lib/borealis/library/lib/extern/yoga/yoga/.." \
    -I"$PROJECT_DIR/lib/borealis/library/lib/extern/libromfs/lib/include" \
    -I"$PROJECT_DIR/lib/borealis/library/include/borealis/extern/tinyxml2" \
    -isystem "$DEVKITPRO/libnx/include" \
    -g -O2 -DNDEBUG -std=gnu++17 -fPIE -ffunction-sections -fdata-sections \
    -DBRLS_RESOURCES=\"romfs:/\" -DYG_ENABLE_EVENTS -Wno-volatile -DHAVE_LIBNX -DSWITCH -D__SWITCH__ \
    -DSTBI_NO_THREAD_LOCALS -DBOREALIS_USE_OPENGL \
    -c "$PROJECT_DIR/lib/borealis/library/lib/views/image.cpp" -o "$OUT_OBJ"; then
    compile_ok=1
    break
  fi
  echo "  retrying image.cpp compile (attempt $attempt failed, likely transient) ..."
  sleep $((attempt * 5))
done
if [[ "$compile_ok" != "1" ]]; then
  echo "patch_borealis_image_atlas.sh: image.cpp compile failed after 5 attempts" >&2
  exit 1
fi

"$AR" d "$BOREALIS_LIB" image.o
"$AR" r "$BOREALIS_LIB" "$OUT_OBJ"
echo "Patched $BOREALIS_LIB"
