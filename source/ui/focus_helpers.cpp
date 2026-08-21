#include "ui/focus_helpers.hpp"

#include <string>

#include "activity/activities.hpp"
#include "util/totk_log.hpp"

namespace totk::ui::focus {

bool isDescendantOf(brls::View* view, brls::View* ancestor) {
    if (!view || !ancestor) return false;
    for (brls::View* node = view; node; node = node->getParent()) {
        if (node == ancestor) return true;
    }
    return false;
}

bool isEffectivelyVisible(brls::View* view) {
    for (brls::View* node = view; node; node = node->getParent()) {
        if (node->getVisibility() != brls::Visibility::VISIBLE) {
            return false;
        }
    }
    return view != nullptr;
}

bool hasUsableFrame(brls::View* view) {
    if (!view) return false;
    const brls::Rect frame = view->getFrame();
    return frame.getWidth() > 1.0f && frame.getHeight() > 1.0f;
}

bool focusBelongsTo(brls::View* view, brls::View* ancestor) {
    return isDescendantOf(view, ancestor);
}

bool hasEditorOverlayActivity() {
    const auto stack = brls::Application::getActivitiesStack();
    if (stack.size() <= 1) return false;

    // True only when a modal sits above the main editor shell (not when SlotPicker is underneath).
    for (int i = static_cast<int>(stack.size()) - 1; i >= 0; --i) {
        brls::Activity* act = stack[static_cast<size_t>(i)];
        if (dynamic_cast<EditorActivity*>(act) || dynamic_cast<WeaponsEditorActivity*>(act)) {
            return i < static_cast<int>(stack.size()) - 1;
        }
    }
    return false;
}

std::string viewLabel(brls::View* view) {
    if (!view) return "(null)";
    return view->describe();
}

void trace(const char* event) {
    brls::View* current = brls::Application::getCurrentFocus();
    TOTK_LOG("focus-trace: %s current=%s focusable=%d", event, viewLabel(current).c_str(),
             current && current->isFocusable() ? 1 : 0);
}

void traceAttempt(const char* event, brls::View* target) {
    brls::View* current = brls::Application::getCurrentFocus();
    brls::View* effective = target ? target->getDefaultFocus() : nullptr;
    if (!effective) effective = target;
    TOTK_LOG(
        "focus-trace: %s target=%s visible=%d frame=%d current=%s", event,
        viewLabel(effective).c_str(), effective && isEffectivelyVisible(effective) ? 1 : 0,
        effective && hasUsableFrame(effective) ? 1 : 0, viewLabel(current).c_str());
}

void focusView(brls::View* view) {
    if (!view) {
        trace("focusView.skip null");
        return;
    }

    brls::View* target = view->getDefaultFocus();
    if (!target) target = view;
    if (!isEffectivelyVisible(target)) {
        traceAttempt("focusView.skip invisible", view);
        return;
    }

    brls::View* previous = brls::Application::getCurrentFocus();
    (void)previous;
    traceAttempt("focusView.before", view);
    brls::Application::giveFocus(target);
    brls::View* now = brls::Application::getCurrentFocus();
    TOTK_LOG("focus-trace: focusView.after current=%s moved=%d", viewLabel(now).c_str(), now == target ? 1 : 0);
}

void scheduleVisibleFocus(brls::View* view, int attemptsLeft, std::function<void(bool landed)> onComplete) {
    brls::sync([view, attemptsLeft, onComplete = std::move(onComplete)]() mutable {
        brls::View* target = view ? view->getDefaultFocus() : nullptr;
        if (!target) target = view;

        if (target && isEffectivelyVisible(target) && hasUsableFrame(target)) {
            focusView(target);
            const brls::View* current = brls::Application::getCurrentFocus();
            if (current == target || current == target->getDefaultFocus()) {
                TOTK_LOG("focus: landed on %s", target->describe().c_str());
                if (onComplete) onComplete(true);
                return;
            }
        }

        if (target && !isEffectivelyVisible(target)) {
            if (onComplete) onComplete(false);
            return;
        }

        if (attemptsLeft > 0) {
            scheduleVisibleFocus(view, attemptsLeft - 1, std::move(onComplete));
            return;
        }

        TOTK_LOG("focus: failed to land after retries target=%s", target ? target->describe().c_str() : "(null)");
        if (onComplete) onComplete(false);
    });
}

}  // namespace totk::ui::focus
