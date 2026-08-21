#!/usr/bin/env bash
# Ground-up editor: save picker -> load -> weapons grid (atlas). Does not use fork UI code.
set -euo pipefail

# Set DEVKITPRO to your own devkitPro installation before building when it is
# not already provided by the devkitPro MSYS environment.
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export PATH="$DEVKITPRO/msys2/usr/bin:$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

export TOTK_SAVE_EDITOR=1
export TOTK_ICON_ATLAS=1
# Standard editor builds are release builds. Set either value to 1 manually
# only while investigating an issue on hardware.
export TOTK_NXLINK_DEBUG=0
export TOTK_DEBUG_NO_SLOT_THUMBS=0
unset TOTK_BUILD_ID
export TOTK_BUILD_OPT=-O2
export TOTK_ATLAS_PREFIX=weapons/
# Default to the combined all-categories atlas (armors, arrows, bows, devices,
# food, horses, key, materials, shields, weapons -> single atlas_0.png). This
# matches every editor build produced through 2026-08-11 (~36.5MB NRO); set
# TOTK_ATLAS_ALL=0 to fall back to the smaller weapons-only atlas instead.
export TOTK_ATLAS_ALL="${TOTK_ATLAS_ALL:-1}"
export TOTK_BUILD_ID="${TOTK_BUILD_ID:-editor-$(date +%Y%m%d-%H%M%S)}"

echo "=== TotK Save Editor (scratch shell + weapons atlas) ==="
echo "  build_id: ${TOTK_BUILD_ID}"
echo ""

# "$BASH" (bash's own path to itself, fixed at this shell's startup), not a
# bare `bash` lookup: this script prepends $DEVKITPRO/msys2/usr/bin to PATH
# above, which shadows the currently-running bash with devkitPro's own
# bundled MSYS2 bash.exe. Invoking a nested script through that DIFFERENT
# MSYS2 runtime silently corrupted exported env vars crossing the boundary
# (confirmed 2026-08-17: TOTK_SAVE_EDITOR=1 exported here, read back as
# unset/0 inside build_wisp_incremental.sh) and, further downstream, broke
# TMP-directory resolution for compiler subprocesses. "$BASH" keeps every
# nested invocation in
# the same runtime that started this script, regardless of later PATH edits.
"$BASH" "$SCRIPT_DIR/build_wisp_incremental.sh"
