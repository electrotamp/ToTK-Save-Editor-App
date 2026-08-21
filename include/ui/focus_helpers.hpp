#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>

namespace totk::ui::focus {

bool isDescendantOf(brls::View* view, brls::View* ancestor);
bool isEffectivelyVisible(brls::View* view);
bool hasUsableFrame(brls::View* view);
bool focusBelongsTo(brls::View* view, brls::View* ancestor);

// True when a modal activity (item editor, picker overlay, etc.) sits above the editor shell.
bool hasEditorOverlayActivity();

void focusView(brls::View* view);
void scheduleVisibleFocus(brls::View* view, int attemptsLeft = 48,
                          std::function<void(bool landed)> onComplete = nullptr);

std::string viewLabel(brls::View* view);
void trace(const char* event);
void traceAttempt(const char* event, brls::View* target);

}  // namespace totk::ui::focus
