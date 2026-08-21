#!/usr/bin/env bash
# Timestamped source (+ optional NRO) snapshots for every build.
# Called from build_switch.sh and build_wisp_incremental.sh.

_totk_backup_root() {
  local project_dir="$1"
  echo "${TOTK_BACKUP_DIR:-$project_dir/releases/build-backups}"
}

_totk_backup_stamp() {
  date +%Y%m%d-%H%M%S
}

# Copy source/include/xml before compiling. Safe to call multiple times per build.
backup_source_snapshot() {
  local project_dir="$1"
  local backup_root
  backup_root="$(_totk_backup_root "$project_dir")"
  local stamp="${TOTK_BACKUP_STAMP:-$(_totk_backup_stamp)}"
  local dest="$backup_root/$stamp"

  if [[ -n "${TOTK_BACKUP_STAMP:-}" && -d "$dest" ]]; then
    echo "Source backup already exists for this build: $dest"
    return 0
  fi

  mkdir -p "$dest/source" "$dest/include" "$dest/resources/xml/activity"
  cp -a "$project_dir/source/." "$dest/source/"
  cp -a "$project_dir/include/." "$dest/include/"
  cp -a "$project_dir/resources/xml/activity/." "$dest/resources/xml/activity/"
  if [[ -f "$project_dir/CMakeLists.txt" ]]; then
    cp -a "$project_dir/CMakeLists.txt" "$dest/"
  fi

  {
    echo "TotK Save Editor - build source snapshot"
    echo "Stamp: $stamp"
    echo "Project: $project_dir"
    echo "Created: $(date '+%Y-%m-%d %H:%M:%S %z' 2>/dev/null || date)"
    echo ""
    echo "Contents: source/, include/, resources/xml/activity/, CMakeLists.txt"
    echo "NRO copied here after a successful build (totk_save_editor.nro)."
  } >"$dest/README.txt"

  echo "$stamp" >"$backup_root/.latest-stamp"
  export TOTK_BACKUP_STAMP="$stamp"
  echo "Source backup: $dest"

  _totk_prune_old_backups "$backup_root"
}

# Attach the built NRO to the snapshot for this build (call after link succeeds).
backup_source_snapshot_nro() {
  local project_dir="$1"
  local build_dir="${2:-$project_dir/build-switch}"
  local nro="$build_dir/totk_save_editor.nro"
  local backup_root
  backup_root="$(_totk_backup_root "$project_dir")"
  local stamp="${TOTK_BACKUP_STAMP:-$(cat "$backup_root/.latest-stamp" 2>/dev/null || true)}"

  if [[ -z "$stamp" ]]; then
    echo "backup_source_snapshot_nro: no active stamp (source backup missing?)" >&2
    return 0
  fi

  local dest="$backup_root/$stamp"
  if [[ ! -d "$dest" ]]; then
    echo "backup_source_snapshot_nro: snapshot dir missing: $dest" >&2
    return 0
  fi

  if [[ ! -f "$nro" ]]; then
    echo "backup_source_snapshot_nro: NRO not found: $nro" >&2
    return 0
  fi

  cp -a "$nro" "$dest/totk_save_editor.nro"
  {
    echo "NRO: totk_save_editor.nro"
    echo "NRO size: $(wc -c <"$nro" | tr -d ' ') bytes"
    echo "NRO time: $(date -r "$nro" '+%Y-%m-%d %H:%M:%S' 2>/dev/null || stat -c '%y' "$nro" 2>/dev/null || echo unknown)"
  } >>"$dest/README.txt"
  echo "NRO backup: $dest/totk_save_editor.nro"
}

_totk_prune_old_backups() {
  local backup_root="$1"
  local keep="${TOTK_BACKUP_KEEP:-30}"
  local -a dirs=()
  local d count

  shopt -s nullglob
  for d in "$backup_root"/[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]-[0-9][0-9][0-9][0-9][0-9][0-9]; do
    [[ -d "$d" ]] && dirs+=("$d")
  done
  shopt -u nullglob

  count="${#dirs[@]}"
  if (( count <= keep )); then
    return 0
  fi

  # local, not a bare `IFS=... dirs=(...)` prefix assignment: with no command
  # word following, bash treats consecutive `name=value` pairs as ordinary
  # assignments in the current shell, not scoped to this line — that leaked
  # IFS=$'\n' into every caller of this sourced script (build_wisp_incremental.sh),
  # silently breaking word-splitting on every unquoted multi-flag variable
  # (e.g. -D flags got merged into one token instead of separate compiler args).
  local IFS=$'\n'
  dirs=($(printf '%s\n' "${dirs[@]}" | sort))
  local remove=$((count - keep))
  local i
  for ((i = 0; i < remove; ++i)); do
    rm -rf "${dirs[$i]}"
    echo "Pruned old backup: ${dirs[$i]}"
  done
}
