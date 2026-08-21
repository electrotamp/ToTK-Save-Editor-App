#!/usr/bin/env bash
set -euo pipefail

# Set DEVKITPRO to your own devkitPro installation before building when it is
# not already provided by the devkitPro MSYS environment.
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export PATH="$DEVKITPRO/msys2/usr/bin:$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
if [[ "$PROJECT_DIR" == *" "* && -d "/e/totk-save-editor" ]]; then
  PROJECT_DIR="/e/totk-save-editor"
fi
BUILD_DIR="$PROJECT_DIR/build-switch"
GPP="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-g++.exe"
GCC="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-gcc.exe"

# Icon atlas fork: set TOTK_ICON_ATLAS=0 to build legacy per-PNG path on this branch.
TOTK_ICON_ATLAS="${TOTK_ICON_ATLAS:-1}"
TOTK_SAVE_EDITOR="${TOTK_SAVE_EDITOR:-0}"
TOTK_WEAPONS_ONLY_UI="${TOTK_WEAPONS_ONLY_UI:-0}"
TOTK_NXLINK_DEBUG="${TOTK_NXLINK_DEBUG:-0}"
# One combined sheet across every icon category (armors, materials, food, ...)
# instead of the weapons-only sheet. See docs/ATLAS_AND_GRID_REFERENCE.md.
TOTK_ATLAS_ALL="${TOTK_ATLAS_ALL:-0}"
TOTK_ATLAS_MAX_SHEET="${TOTK_ATLAS_MAX_SHEET:-6016}"
ATLAS_CXX_FLAG=""
WEAPONS_ONLY_FLAG=""
SAVE_EDITOR_CXX_FLAG=""
BUILD_OPT="-O2"
BUILD_NDEBUG="-DNDEBUG"
NXLINK_DEBUG_FLAG=""
DEBUG_THUMBS_FLAG=""
BUILD_ID_DEFINE=""
DEBUG_SYMBOLS_FLAG=""
NRO_TITLE="ToTK Save Editor"

if [[ "$TOTK_SAVE_EDITOR" == "1" ]]; then
  SAVE_EDITOR_CXX_FLAG="-DTOTK_SAVE_EDITOR=1"
  TOTK_ICON_ATLAS=1
  TOTK_WEAPONS_ONLY_UI=0
  TOTK_ATLAS_PREFIX=weapons/
  NRO_TITLE="ToTK Save Editor"
  echo "UI: TotK Save Editor (scratch picker + atlas weapons grid)"
fi

if [[ "$TOTK_ATLAS_ALL" == "1" ]]; then
  SAVE_EDITOR_CXX_FLAG="$SAVE_EDITOR_CXX_FLAG -DTOTK_ATLAS_ALL=1"
  echo "Atlas: ALL categories combined into one sheet, max ${TOTK_ATLAS_MAX_SHEET}px (TOTK_ATLAS_ALL=1)"
fi

if [[ "$TOTK_NXLINK_DEBUG" == "1" ]]; then
  BUILD_OPT="-O0"
  BUILD_NDEBUG="-DNDEBUG"
  DEBUG_SYMBOLS_FLAG="-g"
  NXLINK_DEBUG_FLAG="-DTOTK_NXLINK_DEBUG=1"
  TOTK_BUILD_ID="${TOTK_BUILD_ID:-nxlink-atlas-$(date +%Y%m%d-%H%M%S)}"
  BUILD_ID_DEFINE="-DTOTK_BUILD_ID=\"${TOTK_BUILD_ID}\""
  # Keep the Homebrew Menu title user-facing even for an NXLink debug build.
  NRO_TITLE="ToTK Save Editor"
  # Slot JPEG thumbs: enabled unless explicitly disabled.
  if [[ "${TOTK_DEBUG_NO_SLOT_THUMBS:-0}" == "1" ]]; then
    DEBUG_THUMBS_FLAG="-DTOTK_DEBUG_NO_SLOT_THUMBS=1"
    echo "NXLink debug: slot thumbnails DISABLED (set TOTK_DEBUG_NO_SLOT_THUMBS=0 to enable)"
  fi
  echo "NXLink debug profile: build_id=${TOTK_BUILD_ID} opt=${BUILD_OPT}"
fi
if [[ -n "${TOTK_BUILD_OPT:-}" ]]; then
  BUILD_OPT="$TOTK_BUILD_OPT"
  echo "Build opt override: ${BUILD_OPT}"
fi
if [[ "$TOTK_ICON_ATLAS" == "1" ]]; then
  ATLAS_CXX_FLAG="-DTOTK_ICON_ATLAS=1"
fi
if [[ "$TOTK_WEAPONS_ONLY_UI" == "1" ]]; then
  WEAPONS_ONLY_FLAG="-DTOTK_WEAPONS_ONLY_UI=1"
  echo "UI slice: weapons-only (save picker -> weapons grid)"
fi
if [[ "$TOTK_ICON_ATLAS" == "1" ]]; then
  echo "Building with icon atlas (TOTK_ICON_ATLAS=1)"
  PYTHON=""
  for candidate in python python3; do
    if command -v "$candidate" >/dev/null 2>&1; then
      PYTHON="$candidate"
      break
    fi
    if [[ -x "$candidate" ]]; then
      PYTHON="$candidate"
      break
    fi
  done
  if [[ -n "$PYTHON" ]]; then
    ATLAS_ARGS=()
    if [[ "$TOTK_ATLAS_ALL" == "1" ]]; then
      ATLAS_ARGS+=(--group-name atlas --max-sheet "$TOTK_ATLAS_MAX_SHEET")
      for category in armors arrows bows devices food horses key materials shields weapons; do
        ATLAS_ARGS+=(--include-prefix "$category/")
      done
    elif [[ -n "${TOTK_ATLAS_PREFIX:-}" ]]; then
      ATLAS_ARGS+=(--include-prefix "$TOTK_ATLAS_PREFIX")
    fi
    "$PYTHON" "$PROJECT_DIR/scripts/build_icon_atlas.py" "${ATLAS_ARGS[@]}"
  else
    echo "WARNING: python not found; using existing resources/assets/icon_atlas/*"
  fi
else
  echo "Building legacy per-PNG icons (TOTK_ICON_ATLAS=0)"
fi

# shellcheck source=backup_source_snapshot.sh
source "$SCRIPT_DIR/backup_source_snapshot.sh"
backup_source_snapshot "$PROJECT_DIR"

cd "$BUILD_DIR"

if [[ "$TOTK_ICON_ATLAS" == "1" ]]; then
  # "$BASH", not bare `bash` — see build_save_editor.sh's comment on the same
  # pattern: PATH here already has $DEVKITPRO/msys2/usr/bin ahead of the
  # runtime actually executing this script, so a plain `bash` lookup launches
  # a different MSYS2 bash.exe and env vars/TMP resolution get corrupted
  # crossing that boundary.
  "$BASH" "$SCRIPT_DIR/patch_borealis_image_atlas.sh"
fi

CXX_DEFINES="-D__GLFW__ -DUSE_LIBROMFS $ATLAS_CXX_FLAG $NXLINK_DEBUG_FLAG $DEBUG_THUMBS_FLAG $BUILD_ID_DEFINE"
CXX_INCLUDES="-I$PROJECT_DIR/include -I$PROJECT_DIR/source -I$PROJECT_DIR/lib/borealis/library/lib/extern/libromfs/lib/include -I$PROJECT_DIR/lib/borealis/library/lib/extern/switch-libpulsar/include -I$PROJECT_DIR/lib/borealis/library/include -I$PROJECT_DIR/lib/borealis/library/include/borealis/extern -I$PROJECT_DIR/lib/borealis/library/include/borealis/extern/nanovg -I$PROJECT_DIR/lib/borealis/library/lib/extern/fmt/include -I$PROJECT_DIR/lib/borealis/library/lib/extern/tweeny/include -I$PROJECT_DIR/lib/borealis/library/lib/extern/yoga/yoga/.. -I$PROJECT_DIR/lib/borealis/library/include/borealis/extern/tinyxml2 -isystem $DEVKITPRO/libnx/include"
CXX_FLAGS="-I$DEVKITPRO/libnx/include -I$DEVKITPRO/portlibs/switch/include $DEBUG_SYMBOLS_FLAG -O2 -DNDEBUG -std=gnu++17 -fPIE -ffunction-sections -fdata-sections -DBRLS_RESOURCES=\"romfs:/\" -DYG_ENABLE_EVENTS -Wno-volatile -DHAVE_LIBNX -DSWITCH -D__SWITCH__ -DSTBI_NO_THREAD_LOCALS -DBOREALIS_USE_OPENGL -DUSE_LIBROMFS"

compile() {
  echo "Compiling $1..."
  mkdir -p "$(dirname "$2")"
  "$GPP" \
    $CXX_DEFINES \
    -I"$PROJECT_DIR/include" \
    -I"$PROJECT_DIR/source" \
    -I"$PROJECT_DIR/lib/borealis/library/lib/extern/libromfs/lib/include" \
    -I"$PROJECT_DIR/lib/borealis/library/lib/extern/switch-libpulsar/include" \
    -I"$PROJECT_DIR/lib/borealis/library/include" \
    -I"$PROJECT_DIR/lib/borealis/library/include/borealis/extern" \
    -I"$PROJECT_DIR/lib/borealis/library/include/borealis/extern/nanovg" \
    -I"$PROJECT_DIR/lib/borealis/library/lib/extern/fmt/include" \
    -I"$PROJECT_DIR/lib/borealis/library/lib/extern/tweeny/include" \
    -I"$PROJECT_DIR/lib/borealis/library/lib/extern/yoga/yoga/.." \
    -I"$PROJECT_DIR/lib/borealis/library/include/borealis/extern/tinyxml2" \
    -isystem "$DEVKITPRO/libnx/include" \
    -I"$DEVKITPRO/libnx/include" \
    -I"$DEVKITPRO/portlibs/switch/include" \
    $DEBUG_SYMBOLS_FLAG $BUILD_OPT $BUILD_NDEBUG -std=gnu++17 -fPIE -ffunction-sections -fdata-sections \
    -DBRLS_RESOURCES=\"romfs:/\" \
    -DYG_ENABLE_EVENTS -Wno-volatile -DHAVE_LIBNX -DSWITCH -D__SWITCH__ \
    -DSTBI_NO_THREAD_LOCALS -DBOREALIS_USE_OPENGL -DUSE_LIBROMFS \
    $ATLAS_CXX_FLAG $NXLINK_DEBUG_FLAG $DEBUG_THUMBS_FLAG $WEAPONS_ONLY_FLAG $SAVE_EDITOR_CXX_FLAG $BUILD_ID_DEFINE \
    -c "$1" -o "$2"
}

compile_c() {
  echo "Compiling $1..."
  mkdir -p "$(dirname "$2")"
  "$GCC" \
    $CXX_DEFINES \
    -I"$PROJECT_DIR/include" \
    -I"$PROJECT_DIR/source" \
    -I"$DEVKITPRO/libnx/include" \
    -I"$DEVKITPRO/portlibs/switch/include" \
    -isystem "$DEVKITPRO/libnx/include" \
    $DEBUG_SYMBOLS_FLAG $BUILD_OPT $BUILD_NDEBUG \
    -ffunction-sections -fdata-sections \
    -DHAVE_LIBNX -DSWITCH -D__SWITCH__ \
    -c "$1" -o "$2"
}

if [[ "$TOTK_SAVE_EDITOR" == "1" ]]; then
  compile "$PROJECT_DIR/source/main.cpp" "CMakeFiles/totk_save_editor.dir/source/main.o"
  compile "$PROJECT_DIR/source/save_editor/save_editor_app.cpp" "CMakeFiles/totk_save_editor.dir/source/save_editor/save_editor_app.o"
  compile "$PROJECT_DIR/source/save_editor/tab_host_activity.cpp" "CMakeFiles/totk_save_editor.dir/source/save_editor/tab_host_activity.o"
  compile "$PROJECT_DIR/source/save_editor/glow_panel.cpp" "CMakeFiles/totk_save_editor.dir/source/save_editor/glow_panel.o"
  compile "$PROJECT_DIR/source/save_editor/heart_icon.cpp" "CMakeFiles/totk_save_editor.dir/source/save_editor/heart_icon.o"
  compile "$PROJECT_DIR/source/save_editor/save_editor_item_picker.cpp" "CMakeFiles/totk_save_editor.dir/source/save_editor/save_editor_item_picker.o"
  compile "$PROJECT_DIR/source/save_editor/save_editor_weapon_edit.cpp" "CMakeFiles/totk_save_editor.dir/source/save_editor/save_editor_weapon_edit.o"
  compile "$PROJECT_DIR/source/save_editor/save_editor_stats.cpp" "CMakeFiles/totk_save_editor.dir/source/save_editor/save_editor_stats.o"
  compile "$PROJECT_DIR/source/save_editor/save_editor_pouch.cpp" "CMakeFiles/totk_save_editor.dir/source/save_editor/save_editor_pouch.o"
  compile "$PROJECT_DIR/source/save_editor/tab_order.cpp" "CMakeFiles/totk_save_editor.dir/source/save_editor/tab_order.o"
  compile "$PROJECT_DIR/source/app/app_state.cpp" "CMakeFiles/totk_save_editor.dir/source/app/app_state.o"
  compile "$PROJECT_DIR/source/ui/icon_grid_metrics.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/icon_grid_metrics.o"
  compile "$PROJECT_DIR/source/ui/item_database.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/item_database.o"
  compile "$PROJECT_DIR/source/ui/item_editor_ui.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/item_editor_ui.o"
  compile "$PROJECT_DIR/source/ui/bounded_numeric_cell.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/bounded_numeric_cell.o"
  compile "$PROJECT_DIR/source/ui/pouch_item_display.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/pouch_item_display.o"
  compile "$PROJECT_DIR/source/save/save_editor.cpp" "CMakeFiles/totk_save_editor.dir/source/save/save_editor.o"
  compile "$PROJECT_DIR/source/save/marc_file.cpp" "CMakeFiles/totk_save_editor.dir/source/save/marc_file.o"
  compile "$PROJECT_DIR/source/save/variable_store.cpp" "CMakeFiles/totk_save_editor.dir/source/save/variable_store.o"
  compile "$PROJECT_DIR/source/save/murmur_hash.cpp" "CMakeFiles/totk_save_editor.dir/source/save/murmur_hash.o"
  compile "$PROJECT_DIR/source/save/caption_parser.cpp" "CMakeFiles/totk_save_editor.dir/source/save/caption_parser.o"
  compile "$PROJECT_DIR/source/save/location_names.cpp" "CMakeFiles/totk_save_editor.dir/source/save/location_names.o"
  compile "$PROJECT_DIR/source/save/game_limits.cpp" "CMakeFiles/totk_save_editor.dir/source/save/game_limits.o"
  compile "$PROJECT_DIR/source/ui/zonai_wisp_overlay.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_overlay.o"
  compile "$PROJECT_DIR/source/ui/zonai_wisp_session.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_session.o"
  compile "$PROJECT_DIR/source/ui/zonai_wisp_settings.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_settings.o"
  compile "$PROJECT_DIR/source/ui/zonai_wisp_simulator.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_simulator.o"
  compile "$PROJECT_DIR/source/ui/editor_theme.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/editor_theme.o"
  compile "$PROJECT_DIR/source/ui/theme_import_activity.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/theme_import_activity.o"
  compile "$PROJECT_DIR/source/save/armor_upgrades.cpp" "CMakeFiles/totk_save_editor.dir/source/save/armor_upgrades.o"
  compile "$PROJECT_DIR/source/save/horse_defaults.cpp" "CMakeFiles/totk_save_editor.dir/source/save/horse_defaults.o"
  compile "$PROJECT_DIR/source/save/horse_enums.cpp" "CMakeFiles/totk_save_editor.dir/source/save/horse_enums.o"
  compile "$PROJECT_DIR/source/save/item_enums.cpp" "CMakeFiles/totk_save_editor.dir/source/save/item_enums.o"
  compile "$PROJECT_DIR/source/save/completism_data.cpp" "CMakeFiles/totk_save_editor.dir/source/save/completism_data.o"
  compile "$PROJECT_DIR/source/save/autobuilder_cai.cpp" "CMakeFiles/totk_save_editor.dir/source/save/autobuilder_cai.o"
  compile "$PROJECT_DIR/source/net/http_client.cpp" "CMakeFiles/totk_save_editor.dir/source/net/http_client.o"
  compile "$PROJECT_DIR/source/net/hyruleworks_client.cpp" "CMakeFiles/totk_save_editor.dir/source/net/hyruleworks_client.o"
  compile "$PROJECT_DIR/source/ui/autobuild_slot_picker.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/autobuild_slot_picker.o"
  compile "$PROJECT_DIR/source/ui/autobuild_import_activity.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/autobuild_import_activity.o"
  compile "$PROJECT_DIR/source/ui/autobuild_catalog_activity.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/autobuild_catalog_activity.o"
  compile "$PROJECT_DIR/source/ui/autobuild_detail_activity.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/autobuild_detail_activity.o"
  compile "$PROJECT_DIR/source/ui/autobuild_icon_store.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/autobuild_icon_store.o"
  compile "$PROJECT_DIR/source/ui/autobuild_card.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/autobuild_card.o"
  compile "$PROJECT_DIR/source/util/icon_atlas.cpp" "CMakeFiles/totk_save_editor.dir/source/util/icon_atlas.o"
  compile "$PROJECT_DIR/source/util/icon_texture_cache.cpp" "CMakeFiles/totk_save_editor.dir/source/util/icon_texture_cache.o"
  compile "$PROJECT_DIR/source/util/image_loader.cpp" "CMakeFiles/totk_save_editor.dir/source/util/image_loader.o"
  compile "$PROJECT_DIR/source/util/resource_loader.cpp" "CMakeFiles/totk_save_editor.dir/source/util/resource_loader.o"
  compile "$PROJECT_DIR/source/util/file_text.cpp" "CMakeFiles/totk_save_editor.dir/source/util/file_text.o"
  compile "$PROJECT_DIR/source/util/totk_log.cpp" "CMakeFiles/totk_save_editor.dir/source/util/totk_log.o"
  compile "$PROJECT_DIR/source/util/perf_trace.cpp" "CMakeFiles/totk_save_editor.dir/source/util/perf_trace.o"
  compile "$PROJECT_DIR/source/util/debug_stage.cpp" "CMakeFiles/totk_save_editor.dir/source/util/debug_stage.o"
  compile "$PROJECT_DIR/source/platform/switch_save_mount.cpp" "CMakeFiles/totk_save_editor.dir/source/platform/switch_save_mount.o"
  # This wrapper is required in every Switch build: it mounts romfs/SD and
  # initializes the services the editor needs before main() runs. Its optional
  # NXLink console hookup remains controlled inside the wrapper by the debug flag.
  compile_c "$PROJECT_DIR/source/platform/switch_wrapper.c" "CMakeFiles/totk_save_editor.dir/source/platform/switch_wrapper.o"
  SAVE_EDITOR_WRAPPER_OBJ="CMakeFiles/totk_save_editor.dir/source/platform/switch_wrapper.o"

  echo "Linking editor ELF..."
  "$GPP" -I"$DEVKITPRO/libnx/include" -I"$DEVKITPRO/portlibs/switch/include" $DEBUG_SYMBOLS_FLAG $BUILD_OPT $BUILD_NDEBUG \
    -L"$DEVKITPRO/libnx/lib" -L"$DEVKITPRO/portlibs/switch/lib" -fPIE \
    -specs="$DEVKITPRO/libnx/switch.specs" \
    CMakeFiles/totk_save_editor.dir/source/main.o \
    CMakeFiles/totk_save_editor.dir/source/save_editor/save_editor_app.o \
    CMakeFiles/totk_save_editor.dir/source/save_editor/tab_host_activity.o \
    CMakeFiles/totk_save_editor.dir/source/save_editor/glow_panel.o \
    CMakeFiles/totk_save_editor.dir/source/save_editor/heart_icon.o \
    CMakeFiles/totk_save_editor.dir/source/save_editor/save_editor_item_picker.o \
    CMakeFiles/totk_save_editor.dir/source/save_editor/save_editor_weapon_edit.o \
    CMakeFiles/totk_save_editor.dir/source/save_editor/save_editor_stats.o \
    CMakeFiles/totk_save_editor.dir/source/save_editor/save_editor_pouch.o \
    CMakeFiles/totk_save_editor.dir/source/save_editor/tab_order.o \
    CMakeFiles/totk_save_editor.dir/source/app/app_state.o \
    CMakeFiles/totk_save_editor.dir/source/ui/icon_grid_metrics.o \
    CMakeFiles/totk_save_editor.dir/source/ui/item_database.o \
    CMakeFiles/totk_save_editor.dir/source/ui/item_editor_ui.o \
    CMakeFiles/totk_save_editor.dir/source/ui/bounded_numeric_cell.o \
    CMakeFiles/totk_save_editor.dir/source/ui/pouch_item_display.o \
    CMakeFiles/totk_save_editor.dir/source/save/save_editor.o \
    CMakeFiles/totk_save_editor.dir/source/save/marc_file.o \
    CMakeFiles/totk_save_editor.dir/source/save/variable_store.o \
    CMakeFiles/totk_save_editor.dir/source/save/murmur_hash.o \
    CMakeFiles/totk_save_editor.dir/source/save/caption_parser.o \
    CMakeFiles/totk_save_editor.dir/source/save/location_names.o \
    CMakeFiles/totk_save_editor.dir/source/save/game_limits.o \
    CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_overlay.o \
    CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_session.o \
    CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_settings.o \
    CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_simulator.o \
    CMakeFiles/totk_save_editor.dir/source/ui/editor_theme.o \
    CMakeFiles/totk_save_editor.dir/source/ui/theme_import_activity.o \
    CMakeFiles/totk_save_editor.dir/source/save/armor_upgrades.o \
    CMakeFiles/totk_save_editor.dir/source/save/horse_defaults.o \
    CMakeFiles/totk_save_editor.dir/source/save/horse_enums.o \
    CMakeFiles/totk_save_editor.dir/source/save/item_enums.o \
    CMakeFiles/totk_save_editor.dir/source/save/completism_data.o \
    CMakeFiles/totk_save_editor.dir/source/save/autobuilder_cai.o \
    CMakeFiles/totk_save_editor.dir/source/net/http_client.o \
    CMakeFiles/totk_save_editor.dir/source/net/hyruleworks_client.o \
    CMakeFiles/totk_save_editor.dir/source/ui/autobuild_slot_picker.o \
    CMakeFiles/totk_save_editor.dir/source/ui/autobuild_import_activity.o \
    CMakeFiles/totk_save_editor.dir/source/ui/autobuild_catalog_activity.o \
    CMakeFiles/totk_save_editor.dir/source/ui/autobuild_detail_activity.o \
    CMakeFiles/totk_save_editor.dir/source/ui/autobuild_icon_store.o \
    CMakeFiles/totk_save_editor.dir/source/ui/autobuild_card.o \
    CMakeFiles/totk_save_editor.dir/source/util/icon_atlas.o \
    CMakeFiles/totk_save_editor.dir/source/util/icon_texture_cache.o \
    CMakeFiles/totk_save_editor.dir/source/util/image_loader.o \
    CMakeFiles/totk_save_editor.dir/source/util/resource_loader.o \
    CMakeFiles/totk_save_editor.dir/source/util/file_text.o \
    CMakeFiles/totk_save_editor.dir/source/util/totk_log.o \
    CMakeFiles/totk_save_editor.dir/source/util/perf_trace.o \
    CMakeFiles/totk_save_editor.dir/source/util/debug_stage.o \
    CMakeFiles/totk_save_editor.dir/source/platform/switch_save_mount.o \
    $SAVE_EDITOR_WRAPPER_OBJ \
    -o totk_save_editor.elf \
    lib/borealis/library/libborealis.a -lglfw3 -lEGL -lglapi -ldrm_nouveau -lnx -lm \
    lib/borealis/library/lib/extern/fmt/libfmt.a \
    lib/borealis/library/lib/extern/yoga/yoga/libyogacore.a \
    lib/borealis/library/libtinyxml2.a \
    -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz -lnx -lm
else
compile "$PROJECT_DIR/source/ui/zonai_wisp_settings.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_settings.o"
compile "$PROJECT_DIR/source/ui/zonai_wisp_simulator.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_simulator.o"
compile "$PROJECT_DIR/source/ui/zonai_wisp_session.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_session.o"
compile "$PROJECT_DIR/source/ui/zonai_wisp_overlay.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/zonai_wisp_overlay.o"
compile "$PROJECT_DIR/source/ui/app_theme.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/app_theme.o"
compile "$PROJECT_DIR/source/ui/editor_theme.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/editor_theme.o"
compile "$PROJECT_DIR/source/ui/theme_import_activity.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/theme_import_activity.o"
compile "$PROJECT_DIR/source/util/file_text.cpp" "CMakeFiles/totk_save_editor.dir/source/util/file_text.o"
compile "$PROJECT_DIR/source/ui/editor_pager_frame.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/editor_pager_frame.o"
compile "$PROJECT_DIR/source/activity/activities.cpp" "CMakeFiles/totk_save_editor.dir/source/activity/activities.o"
compile "$PROJECT_DIR/source/app/app_state.cpp" "CMakeFiles/totk_save_editor.dir/source/app/app_state.o"
compile "$PROJECT_DIR/source/save/caption_parser.cpp" "CMakeFiles/totk_save_editor.dir/source/save/caption_parser.o"
compile "$PROJECT_DIR/source/save/location_names.cpp" "CMakeFiles/totk_save_editor.dir/source/save/location_names.o"
compile "$PROJECT_DIR/source/save/armor_upgrades.cpp" "CMakeFiles/totk_save_editor.dir/source/save/armor_upgrades.o"
compile "$PROJECT_DIR/source/save/item_enums.cpp" "CMakeFiles/totk_save_editor.dir/source/save/item_enums.o"
compile "$PROJECT_DIR/source/save/game_limits.cpp" "CMakeFiles/totk_save_editor.dir/source/save/game_limits.o"
compile "$PROJECT_DIR/source/save/save_editor.cpp" "CMakeFiles/totk_save_editor.dir/source/save/save_editor.o"
compile "$PROJECT_DIR/source/util/image_loader.cpp" "CMakeFiles/totk_save_editor.dir/source/util/image_loader.o"
compile "$PROJECT_DIR/source/util/icon_texture_cache.cpp" "CMakeFiles/totk_save_editor.dir/source/util/icon_texture_cache.o"
compile "$PROJECT_DIR/source/util/icon_atlas.cpp" "CMakeFiles/totk_save_editor.dir/source/util/icon_atlas.o"
compile "$PROJECT_DIR/source/util/perf_trace.cpp" "CMakeFiles/totk_save_editor.dir/source/util/perf_trace.o"
compile "$PROJECT_DIR/source/util/debug_stage.cpp" "CMakeFiles/totk_save_editor.dir/source/util/debug_stage.o"
compile "$PROJECT_DIR/source/util/totk_log.cpp" "CMakeFiles/totk_save_editor.dir/source/util/totk_log.o"
compile "$PROJECT_DIR/source/ui/focus_helpers.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/focus_helpers.o"
compile "$PROJECT_DIR/source/ui/item_list_tab.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/item_list_tab.o"
compile "$PROJECT_DIR/source/ui/item_database.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/item_database.o"
compile "$PROJECT_DIR/source/ui/item_picker_activity.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/item_picker_activity.o"
compile "$PROJECT_DIR/source/ui/picker_grid_cache.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/picker_grid_cache.o"
compile "$PROJECT_DIR/source/ui/autobuilder_tab.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/autobuilder_tab.o"
compile "$PROJECT_DIR/source/ui/item_icon_tile.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/item_icon_tile.o"
compile "$PROJECT_DIR/source/ui/item_detail_header.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/item_detail_header.o"
compile "$PROJECT_DIR/source/ui/item_edit_activity.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/item_edit_activity.o"
compile "$PROJECT_DIR/source/ui/item_editor_ui.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/item_editor_ui.o"
compile "$PROJECT_DIR/source/ui/pouch_item_display.cpp" "CMakeFiles/totk_save_editor.dir/source/ui/pouch_item_display.o"
compile "$PROJECT_DIR/lib/borealis/library/lib/core/theme.cpp" "CMakeFiles/totk_save_editor.dir/lib/borealis/library/lib/core/theme.o"
compile "$PROJECT_DIR/lib/borealis/library/lib/platforms/switch/switch_input.cpp" "CMakeFiles/totk_save_editor.dir/lib/borealis/library/lib/platforms/switch/switch_input.o"
compile "$PROJECT_DIR/source/main.cpp" "CMakeFiles/totk_save_editor.dir/source/main.o"

if [[ "$TOTK_NXLINK_DEBUG" == "1" ]]; then
  compile_c "$PROJECT_DIR/source/platform/switch_wrapper.c" "CMakeFiles/totk_save_editor.dir/source/platform/switch_wrapper.o"
fi

echo "Linking ELF..."
LINK_FILE="CMakeFiles/totk_save_editor.dir/link.txt"
if ! grep -q "debug_stage.o" "$LINK_FILE"; then
  sed -i 's|source/util/perf_trace.o|source/util/perf_trace.o CMakeFiles/totk_save_editor.dir/source/util/debug_stage.o|' "$LINK_FILE"
fi
if ! grep -q "location_names.o" "$LINK_FILE"; then
  sed -i 's|source/save/caption_parser.o|source/save/caption_parser.o CMakeFiles/totk_save_editor.dir/source/save/location_names.o|' "$LINK_FILE"
fi
eval "$(cat "$LINK_FILE")"
fi

echo "Staging romfs..."
rm -rf "$BUILD_DIR/resources"
mkdir -p "$BUILD_DIR/resources"
cp -a "$PROJECT_DIR/lib/borealis/resources/." "$BUILD_DIR/resources/"
cp -a "$PROJECT_DIR/resources/." "$BUILD_DIR/resources/"
# The shipped editor default is the full theme document maintained here.
cp "$PROJECT_DIR/wisp profiles/default.json" "$BUILD_DIR/resources/data/editor_theme.json"
rm -f "$BUILD_DIR/resources/assets.zip"
rm -rf "$BUILD_DIR/resources/font"
rm -rf "$BUILD_DIR/resources/assets/item_icons_preferred"
rm -rf "$BUILD_DIR/resources/assets/item_icons_legacy"
rm -rf "$BUILD_DIR/resources/assets/item_icons_orbs"
rm -rf "$BUILD_DIR/resources/assets/item_icons_curated"
rm -rf "$BUILD_DIR/resources/assets/icon_sources"
if [[ "$TOTK_ICON_ATLAS" == "1" ]]; then
  rm -rf "$BUILD_DIR/resources/assets/item_icons"
  echo "Atlas romfs: dropped duplicate item_icons PNG tree (icons load from sprite sheets)"
fi

PROJECT_ICON="$PROJECT_DIR/resources/img/icon.jpg"
if [[ ! -f "$PROJECT_ICON" ]]; then
  PROJECT_ICON="$DEVKITPRO/libnx/default_icon.jpg"
fi

echo "Creating NRO..."
nacptool.exe --create "$NRO_TITLE" "ToTK Save Editor" "1.0.0" totk_save_editor.nacp --titleid=0x0100000005134C00
elf2nro.exe totk_save_editor.elf totk_save_editor.nro \
  --icon="$PROJECT_ICON" \
  --nacp=totk_save_editor.nacp \
  --romfsdir="$BUILD_DIR/resources"

backup_source_snapshot_nro "$PROJECT_DIR" "$BUILD_DIR"

ls -la totk_save_editor.nro
if [[ "$TOTK_NXLINK_DEBUG" == "1" ]]; then
  mkdir -p "$PROJECT_DIR/nxlink"
  printf '%s\n' "$TOTK_BUILD_ID" >"$PROJECT_DIR/nxlink/last_build_id.txt"
  echo "NXLink debug build id: $TOTK_BUILD_ID"
  echo "Upload: nxlink/build_and_run.bat  (or run.bat after build)"
fi
echo "Wisp JSON bundled:"
ls -la "$BUILD_DIR/resources/data/zonai_wisps.json"
ls -la "$BUILD_DIR/resources/data/editor_theme.json"
