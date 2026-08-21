#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>

namespace totk::ui {

class BoundedNumericCell : public brls::DetailCell {
public:
    BoundedNumericCell();

    void init(std::string title, long value, long minValue, long maxValue, std::function<long(long)> clampFn,
              brls::Event<long>::Callback callback, std::string hint = "", bool allowNegative = false);

    void setValue(long value, bool silent = false);
    long getValue() const { return value_; }

  private:
    void updateUI();
    int maxInputLengthForRange() const;

    long value_ = 0;
    long minValue_ = 0;
    long maxValue_ = 0;
    std::string hint_;
    bool allowNegative_ = false;
    std::function<long(long)> clampFn_;
    brls::Event<long> event_;
};

}  // namespace totk::ui
