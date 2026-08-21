#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>
#include <vector>

namespace totk::ui {

// One catalogue card. `loadIcon`, if set, is called with the card's newly
// created (still-empty) thumbnail Image right after layout so the caller can
// populate it from whatever source it likes (SD icon store, console Cache
// archive, HyruleWorks network fetch, ...) — same pattern the Autobuild tab
// and HyruleWorks catalog already used before this shared component existed.
// `onSelect`, if set, makes the card focusable/selectable (BUTTON_A); leave
// it empty for a purely informational, non-interactive card.
struct AutobuildCardSpec {
    std::string title;
    std::string statusText;
    NVGcolor statusColor{};
    std::function<bool(brls::Image*)> loadIcon;
    std::function<void()> onSelect;
};

// Renders `cards` into `content` as a wrapped grid, `columns` per row, of
// catalogue-style cards (big thumbnail on top, title + status text below) —
// the shared visual used by the Autobuild tab, the slot picker, and the
// HyruleWorks catalog browser so all three read as the same catalogue
// rather than three different list styles. If `outThumbs` is non-null, the
// created thumbnail Image for each card is appended to it in order (even
// when that card had no `loadIcon`, so indices always line up with `cards`)
// — for callers whose icon source isn't ready synchronously (e.g. a network
// fetch still in flight) and needs to populate the Image after the grid is
// built, same pattern AutobuildCatalogActivity already used before this
// shared component existed.
void buildAutobuildCardGrid(brls::Box* content, int columns, const std::vector<AutobuildCardSpec>& cards,
                             std::vector<brls::Image*>* outThumbs = nullptr);

// Applies `loaders[i]` to `thumbs[i]`, a few at a time across multiple
// frames (chained via brls::sync) rather than all in one synchronous burst.
// Decoding/uploading many full-size icon textures in a single frame was
// confirmed to risk a hard crash on real hardware (2026-08-19, switching
// into the Autobuild tab) — this is the fix, used everywhere this app loads
// a batch of Autobuild-style icons (the tab, the slot picker, the
// HyruleWorks catalog). `stillValid` is checked before every chunk and
// stops the whole batch (silently) the moment it returns false — callers
// whose view tree can be torn down/rebuilt before a batch finishes (a tab
// switch, backing out of a picker) must pass a real check, e.g. comparing a
// captured generation counter against the caller's current one; a
// same-Activity, dies-with-nothing case can pass `[]{ return true; }`.
// `perFrame` defaults to a small number deliberately — this trades a few
// extra frames of "icons still popping in" for not spiking texture
// allocations.
void applyAutobuildIconsStaggered(std::vector<brls::Image*> thumbs,
                                   std::vector<std::function<bool(brls::Image*)>> loaders,
                                   std::function<bool()> stillValid, int perFrame = 4);

}  // namespace totk::ui
