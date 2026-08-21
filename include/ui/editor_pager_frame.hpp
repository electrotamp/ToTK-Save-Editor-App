#pragma once

#include <borealis/views/tab_frame.hpp>
#include <functional>
#include <string>
#include <vector>

namespace totk::ui {

class ZonaiWispOverlay;

// Full-width editor shell: one page visible, L/R to switch (no sidebar).
class EditorPagerFrame : public brls::Box {
public:
    EditorPagerFrame();

    void handleXMLElement(tinyxml2::XMLElement* element) override;

    void showPage(int index);
    int activePage() const { return activePage_; }
    void redirectFocusSink();
    void focusActivePage();
    void refreshChrome();

    brls::View* getDefaultFocus() override;
    void willAppear(bool resetState = false) override;

    static brls::View* create();

private:
    struct Page {
        std::string label;
        brls::View* view = nullptr;
    };

    std::string appTitle_ = "TotK Save Editor";
    brls::Box* focusSink_ = nullptr;
    brls::Box* contentHost_ = nullptr;
    ZonaiWispOverlay* wispOverlay_ = nullptr;
    std::vector<Page> pages_;
    int activePage_ = 0;
    bool appeared_ = false;
    bool initialPageShown_ = false;

    void addPage(std::string label, brls::TabViewCreator creator);
    void updateHeaderTitle();
    void updatePageHints();
    void focusPageContent(brls::View* page);
    void releaseFocusSink();
};

}  // namespace totk::ui
