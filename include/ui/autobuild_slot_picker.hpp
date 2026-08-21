#pragma once

#include <functional>

namespace totk::ui {

// Which draft position an import should target, chosen by the user in
// pushAutobuildSlotPicker. Exactly one of the two target fields is
// meaningful, selected by `isFavorite`.
struct AutobuildImportTarget {
    bool isFavorite = false;
    int favoriteIndex = -1;         // valid when isFavorite: 0..SaveEditor::kMaxAutobuildFavorites-1
    long historyDraftPosition = -1;  // valid when !isFavorite: an existing draft position to overwrite
};

// Pushes a catalogue-style picker split into a Favorites section (up to
// SaveEditor::kMaxAutobuildFavorites real in-game Favorites buttons) and a
// History section (every currently unfavorited draft position). Picking one
// pops this activity, then invokes onPicked(target) with the caller's own
// activity back on top of the stack — safe to do further work from inside
// onPicked, including popping that activity itself.
void pushAutobuildSlotPicker(std::function<void(AutobuildImportTarget)> onPicked);

}  // namespace totk::ui
