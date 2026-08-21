#pragma once

#include <functional>
#include <string>

#include <borealis.hpp>

namespace totk::ui {

class ItemFocusHeader;
class ItemPickerActivity;

class ItemEditActivity : public brls::Activity {
public:
    ItemEditActivity(std::string category, int index, std::function<void(int deletedIndex)> onDeleted = nullptr,
                     std::function<void()> onClosed = nullptr);

    brls::View* createContentView() override;
    ~ItemEditActivity() override;

private:
    std::string category_;
    int index_;
    std::function<void(int deletedIndex)> onDeleted_;
    std::function<void()> onClosed_;

    ItemFocusHeader* header_ = nullptr;
    brls::DetailCell* fuseCell_ = nullptr;

    void confirmDelete();
    void refreshEquipmentDisplay();
    void refreshHorseDisplay();
    void refreshArmorDisplay();
    void openFusePicker();
};

}  // namespace totk::ui
