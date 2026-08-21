#pragma once

#include <borealis.hpp>

namespace totk {
struct SaveSlotInfo;
}

namespace totk::ui {

class BootSplashActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/boot_splash.xml");
    void onContentAvailable() override;
    void willDisappear(bool resetState = false) override;
    void finishDismiss();

private:
    brls::Timer dismissTimer_;
    brls::Box* focusSink_ = nullptr;
    static constexpr brls::Time kDisplayDurationMs = 2000;
};

class SlotPickerActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/slot_picker.xml");
    void onContentAvailable() override;
    void onResume() override;

    void prepareSlots();
    brls::View* firstFocusTarget();

private:
    bool slotsPopulated_ = false;
};

class EditorActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/editor.xml");
    void onContentAvailable() override;
    void willAppear(bool resetState = false) override;
    void scheduleHomeFocus();
};

class WeaponsEditorActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/editor_weapons_only.xml");
    void onContentAvailable() override;
    void willAppear(bool resetState = false) override;
};

}  // namespace totk::ui
