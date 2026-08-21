#pragma once

#include <borealis/views/tab_frame.hpp>
#include <tinyxml2.h>
#include <functional>

namespace totk::ui {

// TabFrame with app title in the sidebar column and no reliance on AppletFrame header.
// Revert: switch editor.xml back to brls:TabFrame + remove headerHidden.
class EditorTabFrame : public brls::Box {
public:
    EditorTabFrame();

    void handleXMLElement(tinyxml2::XMLElement* element) override;

    void addTab(std::string label, brls::TabViewCreator creator);
    void focusTab(int position);

    brls::Sidebar* sidebar() const { return sidebar_; }

    static brls::View* create();

private:
    brls::Label* titleLabel_ = nullptr;
    brls::Sidebar* sidebar_ = nullptr;
    brls::View* activeTab_ = nullptr;
};

}  // namespace totk::ui
