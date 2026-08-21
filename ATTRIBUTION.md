# Attribution

This Switch homebrew port is based on the community TotK save editor work. Please keep these credits visible in the app and in documentation.

## Primary reference

- **Marc Robledo** — [savegame-editors / zelda-totk](https://github.com/marcrobledo/savegame-editors/tree/master/zelda-totk)
  - Save parsing/writing logic
  - Web UI design reference
  - Item icon PNGs and TotK UI SVG assets
  - `zelda-totk.hashes.csv` master variable database

## Research & data contributors (from the reference editor)

- Echocolat, Exincracci, HylianLZ, Karlos007, ApacheThunder
- MacSpazzy (credited as "SuperSpazzy" / "McSpazzy" in different reference-editor source
  comments — hash crack, research, and the original Autobuilder blueprint viewer/editor),
  MrCheeze, Phil, savage13
- xiyuesaves (filterable item dropdown in web editor)
- JonJaded, Ozymandias07 (horse data, alongside Karlos007)

## Community build database

- **HyruleWorks** — [hyruleworks.com](https://www.hyruleworks.com/) — community database of
  Autobuild blueprints; the reference web editor links out to it, and this app's Autobuild
  browser imports directly from its catalog.

## Location name data

- **zeldamods** — [objmap-totk](https://github.com/zeldamods/objmap-totk) / [objmap-totk.zeldamods.org](https://objmap-totk.zeldamods.org)
  - `resources/data/location_names.json` is built from that project's extracted
    `text/StaticMsg/LocationMarker.json` and `text/StaticMsg/Dungeon.json` game
    message-archive dumps (the game's own internal id -> display name text),
    used to show real in-game location names on the save-slot picker.

## Third-party libraries

- **Borealis** — [xfangfang/borealis](https://github.com/xfangfang/borealis) / [natinusala/borealis](https://github.com/natinusala/borealis)
- **devkitPro / libnx** — Nintendo Switch homebrew toolchain

## Asset usage

Item icons in `resources/assets/item_icons/` are copied from the reference editor for visual parity. Do not redistribute those assets separately without respecting the original license in `reference/savegame-editors/`.
