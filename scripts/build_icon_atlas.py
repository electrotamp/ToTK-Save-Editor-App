#!/usr/bin/env python3
"""Pack item_icons PNGs into GPU atlases + UV manifest for the icon-atlas fork.

Sheets are capped to fit Switch (max 2048px edge, 256 icons per sheet at 128px).
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit("Pillow required: pip install pillow") from exc

ROOT = Path(__file__).resolve().parents[1]
ICON_ROOT = ROOT / "resources" / "assets" / "item_icons"
OUT_DIR = ROOT / "resources" / "assets" / "icon_atlas"
CELL = 128
# Switch-safe: 2048px ~= 16MB RGBA decode; 4096px sheets OOM/crash on startup.
MAX_SHEET_PX = 2048


def is_armor_path(rel: str) -> bool:
    return rel.replace("\\", "/").startswith("armors/")


def collect_icons(include_prefixes: list[str] | None = None) -> tuple[list[Path], list[Path]]:
    prefixes = [p.replace("\\", "/") for p in (include_prefixes or []) if p]
    main_files: list[Path] = []
    armor_files: list[Path] = []
    for png in sorted(ICON_ROOT.rglob("*.png")):
        rel = png.relative_to(ICON_ROOT).as_posix()
        if prefixes and not any(rel.startswith(prefix) for prefix in prefixes):
            continue
        if is_armor_path(rel):
            armor_files.append(png)
        else:
            main_files.append(png)
    return main_files, armor_files


def icons_per_sheet(cell: int, max_px: int) -> int:
    grid = max(1, max_px // cell)
    return grid * grid


def pack_group(group: str, files: list[Path], cell: int, max_px: int) -> tuple[dict, dict]:
    atlas_meta: dict[str, dict] = {}
    all_entries: dict[str, dict] = {}

    if not files:
        return atlas_meta, all_entries

    chunk_size = icons_per_sheet(cell, max_px)
    cols = max(1, max_px // cell)

    for sheet_index, start in enumerate(range(0, len(files), chunk_size)):
        chunk = files[start : start + chunk_size]
        sheet_id = f"{group}_{sheet_index}"
        sheet_name = f"{sheet_id}.png"
        rows = math.ceil(len(chunk) / cols)
        width = cols * cell
        height = rows * cell
        sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))

        entries: dict[str, dict] = {}
        for index, png_path in enumerate(chunk):
            col = index % cols
            row = index // cols
            x = col * cell
            y = row * cell

            icon = Image.open(png_path).convert("RGBA")
            iw, ih = icon.size
            ox = x + max(0, (cell - iw) // 2)
            oy = y + max(0, (cell - ih) // 2)
            sheet.paste(icon, (ox, oy), icon)

            rel_key = "assets/item_icons/" + png_path.relative_to(ICON_ROOT).as_posix()
            entries[rel_key] = {"atlas": sheet_id, "x": x, "y": y, "w": cell, "h": cell}

        out_path = OUT_DIR / sheet_name
        sheet.save(out_path, optimize=True)
        rel_path = f"assets/icon_atlas/{sheet_name}"
        print(f"{sheet_id}: {len(chunk)} icons -> {width}x{height} -> {out_path}")

        atlas_meta[sheet_id] = {
            "path": rel_path,
            "width": width,
            "height": height,
            "count": len(chunk),
        }
        all_entries.update(entries)

    return atlas_meta, all_entries


def main() -> None:
    parser = argparse.ArgumentParser(description="Build TotK editor icon atlases")
    parser.add_argument("--cell", type=int, default=CELL, help="Atlas cell size in pixels")
    parser.add_argument("--max-sheet", type=int, default=MAX_SHEET_PX, help="Max atlas width/height")
    parser.add_argument(
        "--include-prefix",
        action="append",
        default=[],
        help="Only pack icons whose romfs path starts with this prefix (e.g. weapons/). Repeatable.",
    )
    parser.add_argument(
        "--group-name",
        default="weapons",
        help="Sheet name prefix for a filtered build, e.g. 'atlas' -> atlas_0.png (default: weapons, "
        "kept for backward compatibility with the weapons-only editor build)",
    )
    args = parser.parse_args()
    cell = max(32, args.cell)
    max_px = max(cell * 4, args.max_sheet)

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    # Remove stale monolithic sheets from earlier builds.
    for stale in OUT_DIR.glob("*.png"):
        stale.unlink()

    include_prefixes = [p for p in args.include_prefix if p]
    main_files, armor_files = collect_icons(include_prefixes or None)

    if include_prefixes:
        filtered = sorted(main_files + armor_files)
        print(
            f"icons: filtered={len(filtered)} prefixes={include_prefixes} cell={cell} max_sheet={max_px}"
        )
        group_atlases, group_entries = pack_group(args.group_name, filtered, cell, max_px)
        manifest = {
            "version": 2,
            "cell": cell,
            "max_sheet_px": max_px,
            "atlases": group_atlases,
            "entries": group_entries,
        }
        manifest_path = OUT_DIR / "manifest.json"
        manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
        print(
            f"manifest: {len(manifest['entries'])} entries, {len(manifest['atlases'])} sheets -> {manifest_path}"
        )
        return

    print(
        f"icons: main={len(main_files)} armor={len(armor_files)} "
        f"total={len(main_files) + len(armor_files)} cell={cell} max_sheet={max_px}"
    )

    main_atlases, main_entries = pack_group("main", main_files, cell, max_px)
    armor_atlases, armor_entries = pack_group("armor", armor_files, cell, max_px)

    manifest = {
        "version": 2,
        "cell": cell,
        "max_sheet_px": max_px,
        "atlases": {**main_atlases, **armor_atlases},
        "entries": {**main_entries, **armor_entries},
    }

    manifest_path = OUT_DIR / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"manifest: {len(manifest['entries'])} entries, {len(manifest['atlases'])} sheets -> {manifest_path}")


if __name__ == "__main__":
    main()
