#include "ui/editor_tab_frame.hpp"

#include <tinyxml2.h>
#include <borealis/core/application.hpp>
#include <borealis/core/i18n.hpp>

using namespace brls::literals;

namespace totk::ui {

namespace {

const char* kEditorTabFrameXML = R"xml(
<brls:Box
    width="auto"
    height="auto"
    axis="row">

    <brls:Box
        id="totk/editor_tab_frame/sidebar_column"
        width="@style/brls/tab_frame/sidebar_width"
        height="auto"
        axis="column"
        shrink="0">

        <brls:Label
            id="totk/editor_tab_frame/title"
            width="auto"
            height="auto"
            fontSize="@style/brls/applet_frame/header_title_font_size"
            marginTop="18"
            marginBottom="8"
            marginLeft="36"
            marginRight="16"
            singleLine="true" />

        <brls:Sidebar
            id="totk/editor_tab_frame/sidebar"
            width="auto"
            height="auto"
            grow="1.0" />
    </brls:Box>

</brls:Box>
)xml";

}  // namespace

EditorTabFrame::EditorTabFrame() {
    this->inflateFromXMLString(kEditorTabFrameXML);
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);

    titleLabel_ = dynamic_cast<brls::Label*>(this->getView("totk/editor_tab_frame/title"));
    sidebar_ = dynamic_cast<brls::Sidebar*>(this->getView("totk/editor_tab_frame/sidebar"));

    this->registerStringXMLAttribute("title", [this](const std::string& value) {
        if (titleLabel_) {
            titleLabel_->setText(value);
        }
    });
}

void EditorTabFrame::addTab(std::string label, brls::TabViewCreator creator) {
    if (!sidebar_) return;

    sidebar_->addItem(std::move(label), [this, creator](brls::View* view) {
        if (!view->isFocused()) return;

        if (activeTab_) {
            this->removeView(activeTab_);
            activeTab_ = nullptr;
        }

        brls::View* newContent = creator();
        if (!newContent) return;

        newContent->setGrow(1.0f);
        this->addView(newContent);
        activeTab_ = newContent;

        newContent->registerAction(
            "hints/back"_i18n, brls::BUTTON_B, [this](brls::View*) {
                if (brls::Application::getInputType() == brls::InputType::TOUCH) {
                    this->dismiss();
                } else if (sidebar_) {
                    brls::Application::giveFocus(sidebar_);
                }
                return true;
            },
            false, false, brls::SOUND_BACK);
    });
}

void EditorTabFrame::focusTab(int position) {
    if (sidebar_) {
        brls::Application::giveFocus(sidebar_->getItem(position));
    }
}

void EditorTabFrame::handleXMLElement(tinyxml2::XMLElement* element) {
    const std::string name = element->Name();

    if (name == "brls:Tab") {
        const tinyxml2::XMLAttribute* labelAttribute = element->FindAttribute("label");
        if (!labelAttribute) {
            brls::fatal("\"label\" attribute missing from \"" + name + "\" tab");
        }

        const std::string label = brls::View::getStringXMLAttributeValue(labelAttribute->Value());
        tinyxml2::XMLElement* viewElement = element->FirstChildElement();

        if (viewElement) {
            addTab(label, [viewElement] { return brls::View::createFromXMLElement(viewElement); });
            if (viewElement->NextSiblingElement()) {
                brls::fatal("\"brls:Tab\" can only contain one child element");
            }
        } else {
            addTab(label, [] { return nullptr; });
        }
        return;
    }

    if (name == "brls:Separator") {
        if (sidebar_) sidebar_->addSeparator();
        return;
    }

    brls::Box::handleXMLElement(element);
}

brls::View* EditorTabFrame::create() {
    return new EditorTabFrame();
}

}  // namespace totk::ui
