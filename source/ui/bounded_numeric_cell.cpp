#include "ui/bounded_numeric_cell.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <borealis/core/touch/tap_gesture.hpp>

namespace totk::ui {

namespace {

long digitCount(long value) {
    value = std::llabs(value);
    if (value == 0) return 1;
    long digits = 0;
    while (value > 0) {
        ++digits;
        value /= 10;
    }
    return digits;
}

}  // namespace

BoundedNumericCell::BoundedNumericCell() {
    detail->setTextColor(brls::Application::getTheme()["brls/list/listItem_value_color"]);
    registerClickAction([this](brls::View*) {
        const int maxLen = maxInputLengthForRange();
        const std::string leftButton = allowNegative_ ? "-" : "";
        brls::Application::getImeManager()->openForNumber(
            [this](long number) {
                const long clamped = clampFn_ ? clampFn_(number) : std::clamp(number, minValue_, maxValue_);
                setValue(clamped);
            },
            title->getFullText(), hint_, maxLen, std::to_string(value_), leftButton, "",
            brls::KeyboardKeyDisableBitmask::KEYBOARD_DISABLE_NONE);
        return true;
    });
    // DetailCell (our base) registers its click action for gamepad A only —
    // touch taps need an explicit gesture recognizer to reach it (this is how
    // brls::Button wires itself up too; plain cells don't get one for free).
    this->addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

void BoundedNumericCell::init(std::string titleText, long value, long minValue, long maxValue,
                              std::function<long(long)> clampFn, brls::Event<long>::Callback callback,
                              std::string hint, bool allowNegative) {
    title->setText(titleText);
    minValue_ = minValue;
    maxValue_ = maxValue;
    hint_ = std::move(hint);
    allowNegative_ = allowNegative;
    clampFn_ = std::move(clampFn);
    event_.subscribe(std::move(callback));
    setValue(value, true);
}

int BoundedNumericCell::maxInputLengthForRange() const {
    const long digits = std::max(digitCount(minValue_), digitCount(maxValue_));
    return static_cast<int>(digits + (allowNegative_ ? 1 : 0));
}

void BoundedNumericCell::setValue(long value, bool silent) {
    const long clamped = clampFn_ ? clampFn_(value) : std::clamp(value, minValue_, maxValue_);
    value_ = clamped;
    if (!silent) {
        event_.fire(clamped);
    }
    updateUI();
}

void BoundedNumericCell::updateUI() {
    detail->setText(std::to_string(value_));
    detail->setTextColor(brls::Application::getTheme()["brls/list/listItem_value_color"]);
}

}  // namespace totk::ui
