#include "ui/editor_pager_frame.hpp"

#include "ui/focus_helpers.hpp"
#include "ui/item_list_tab.hpp"
#include "ui/status_tab.hpp"
#include "ui/zonai_wisp_overlay.hpp"
#include "ui/zonai_wisp_settings.hpp"
#include "util/totk_log.hpp"
#include "util/perf_trace.hpp"

#include <tinyxml2.h>
#include <borealis/core/application.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/core/box.hpp>

using namespace brls::literals;

namespace totk::ui {

namespace {

brls::View* rejectHiddenPageFocus(brls::View* next, brls::Box* host) {
    if (!next || !host) return next;

    for (brls::View* child : host->getChildren()) {
        if (child->getVisibility() == brls::Visibility::VISIBLE) continue;
        if (focus::isDescendantOf(next, child) || next == child) {
            return nullptr;
        }
    }

    return next;
}

class PagerContentHost : public brls::Box {
public:
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override {
        return rejectHiddenPageFocus(brls::Box::getNextFocus(direction, currentView), this);
    }
};

brls::View* findFirstFocusable(brls::View* root) {
    if (!root || root->getVisibility() != brls::Visibility::VISIBLE) return nullptr;

    if (auto* box = dynamic_cast<brls::Box*>(root)) {
        for (brls::View* child : box->getChildren()) {
            if (brls::View* focusable = findFirstFocusable(child)) {
                return focusable;
            }
        }
    }

    if (root->isFocusable()) return root;
    return nullptr;
}

brls::AppletFrame* findAppletFrame(brls::View* view) {
    for (brls::View* node = view; node; node = node->getParent()) {
        if (auto* frame = dynamic_cast<brls::AppletFrame*>(node)) {
            return frame;
        }
    }
    return nullptr;
}

}  // namespace

EditorPagerFrame::EditorPagerFrame() {
    this->setAxis(brls::Axis::COLUMN);
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);
    this->setFocusable(false);

    focusSink_ = new brls::Box();
    focusSink_->setPositionType(brls::PositionType::ABSOLUTE);
    focusSink_->setPositionTop(0);
    focusSink_->setPositionLeft(0);
    focusSink_->setVisibility(brls::Visibility::VISIBLE);
    focusSink_->setAlpha(0.0f);
    focusSink_->setHideHighlightBackground(true);
    focusSink_->setWidth(1);
    focusSink_->setHeight(1);
    focusSink_->setShrink(0);
    focusSink_->setFocusable(true);
    focusSink_->setCustomNavigationRoute(brls::FocusDirection::UP, focusSink_);
    focusSink_->setCustomNavigationRoute(brls::FocusDirection::DOWN, focusSink_);
    focusSink_->setCustomNavigationRoute(brls::FocusDirection::LEFT, focusSink_);
    focusSink_->setCustomNavigationRoute(brls::FocusDirection::RIGHT, focusSink_);
    focusSink_->setFocusable(false);

#if !defined(TOTK_NXLINK_DEBUG)
    if (ZonaiWispSettings::instance().loaded()) {
        wispOverlay_ = attachWispOverlay(this);
    }
#endif

    contentHost_ = new PagerContentHost();
    contentHost_->setAxis(brls::Axis::COLUMN);
    contentHost_->setWidthPercentage(100);
    contentHost_->setHeightPercentage(100);
    contentHost_->setGrow(1.0f);
    contentHost_->setFocusable(false);
    this->addView(contentHost_);

    this->addView(focusSink_);

    this->registerStringXMLAttribute("title", [this](const std::string& value) {
        appTitle_ = value;
        updateHeaderTitle();
    });

    this->registerAction(
        "Previous page", brls::BUTTON_LB,
        [this](brls::View*) {
            if (pages_.empty()) return false;
            showPage(activePage_ - 1);
            return true;
        },
        false, true);

    this->registerAction(
        "Next page", brls::BUTTON_RB,
        [this](brls::View*) {
            if (pages_.empty()) return false;
            showPage(activePage_ + 1);
            return true;
        },
        false, true);
}

void EditorPagerFrame::addPage(std::string label, brls::TabViewCreator creator) {
    if (!contentHost_) return;

    brls::View* pageView = creator();
    if (!pageView) return;

    pageView->setGrow(1.0f);
    pageView->setWidthPercentage(100);
    pageView->setHeightPercentage(100);
    pageView->setVisibility(brls::Visibility::GONE);
    contentHost_->addView(pageView);

    pages_.push_back(Page{std::move(label), pageView});
    if (pages_.size() == 1) {
        activePage_ = 0;
        pageView->setVisibility(brls::Visibility::VISIBLE);
    }

    brls::sync([this]() { refreshChrome(); });
}

void EditorPagerFrame::refreshChrome() {
    updateHeaderTitle();
    updatePageHints();
}

void EditorPagerFrame::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
    appeared_ = true;
    brls::sync([this]() {
        if (!initialPageShown_) {
            showPage(activePage_);
        } else {
            refreshChrome();
            focusActivePage();
        }
    });
}

void EditorPagerFrame::showPage(int index) {
    if (pages_.empty()) return;

    if (focus::hasEditorOverlayActivity()) {
        TOTK_LOG("pager.showPage blocked overlay active requested=%d active=%d", index, activePage_);
        return;
    }

    if (index < 0) index = 0;
    if (index >= static_cast<int>(pages_.size())) {
        index = static_cast<int>(pages_.size()) - 1;
    }

    const std::string label = pages_[static_cast<size_t>(index)].label;
    TOTK_PERF("pager.showPage index=%d label=%s from=%d", index, label.c_str(), activePage_);
    TOTK_LOG("focus-trace: pager.showPage begin index=%d label=%s from=%d current=%s", index, label.c_str(),
             activePage_, focus::viewLabel(brls::Application::getCurrentFocus()).c_str());

    TOTK_PERF_SCOPE("pager.showPage");

    activePage_ = index;
    for (size_t i = 0; i < pages_.size(); ++i) {
        pages_[i].view->setVisibility(i == static_cast<size_t>(activePage_) ? brls::Visibility::VISIBLE
                                                                              : brls::Visibility::GONE);
    }

    brls::View* page = pages_[static_cast<size_t>(activePage_)].view;
    if (auto* listTab = dynamic_cast<ItemListTab*>(page)) {
        listTab->refresh();
    }

    updateHeaderTitle();
    updatePageHints();

    if (appeared_) {
        focusPageContent(pages_[static_cast<size_t>(activePage_)].view);
        focus::trace("pager.showPage.after_focus_content");
    }
    initialPageShown_ = true;
}

void EditorPagerFrame::focusActivePage() {
    if (pages_.empty()) return;
    releaseFocusSink();

    brls::View* page = pages_[static_cast<size_t>(activePage_)].view;
    if (auto* statusTab = dynamic_cast<StatusTab*>(page)) {
        statusTab->activatePageFocus();
        releaseFocusSink();
        return;
    }

    focusPageContent(page);
}

void EditorPagerFrame::updateHeaderTitle() {
    if (pages_.empty()) return;

    const std::string title = appTitle_ + "  ·  " + pages_[static_cast<size_t>(activePage_)].label;
    getAppletFrameItem()->title = title;

    if (brls::AppletFrame* frame = findAppletFrame(this)) {
        frame->setTitle(title);
    }
}

void EditorPagerFrame::updatePageHints() {
    const bool hasPrev = activePage_ > 0;
    const bool hasNext = activePage_ + 1 < static_cast<int>(pages_.size());

    this->updateActionHint(brls::BUTTON_LB, hasPrev ? pages_[static_cast<size_t>(activePage_ - 1)].label : "");
    this->updateActionHint(brls::BUTTON_RB, hasNext ? pages_[static_cast<size_t>(activePage_ + 1)].label : "");
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void EditorPagerFrame::redirectFocusSink() {
    if (!focusSink_) return;
    focusSink_->setFocusable(true);
    brls::Application::giveFocus(focusSink_);
    focus::trace("pager.redirectFocusSink");
}

void EditorPagerFrame::releaseFocusSink() {
    if (focusSink_) {
        focus::trace("pager.releaseFocusSink");
        focusSink_->setFocusable(false);
    }
}

brls::View* EditorPagerFrame::getDefaultFocus() {
    if (!pages_.empty()) {
        brls::View* page = pages_[static_cast<size_t>(activePage_)].view;
        if (auto* statusTab = dynamic_cast<StatusTab*>(page)) {
            if (brls::View* focus = statusTab->getDefaultFocus()) {
                return focus;
            }
        }
        if (brls::View* focus = findFirstFocusable(page)) {
            return focus;
        }
    }

    return nullptr;
}

void EditorPagerFrame::focusPageContent(brls::View* page) {
    if (auto* listTab = dynamic_cast<ItemListTab*>(page)) {
        focus::trace("pager.focusPageContent.list_tab");
        listTab->activatePageFocus();
        releaseFocusSink();
        return;
    }

    if (auto* statusTab = dynamic_cast<StatusTab*>(page)) {
        focus::trace("pager.focusPageContent.status_tab");
        statusTab->activatePageFocus();
        releaseFocusSink();
        return;
    }

    focus::trace("pager.focusPageContent.fallback");
    focus::focusView(findFirstFocusable(page));
    releaseFocusSink();
}

void EditorPagerFrame::handleXMLElement(tinyxml2::XMLElement* element) {
    const std::string name = element->Name();

    if (name == "brls:Tab") {
        const tinyxml2::XMLAttribute* labelAttribute = element->FindAttribute("label");
        if (!labelAttribute) {
            brls::fatal("\"label\" attribute missing from \"" + name + "\" tab");
        }

        const std::string label = brls::View::getStringXMLAttributeValue(labelAttribute->Value());
        tinyxml2::XMLElement* viewElement = element->FirstChildElement();

        if (viewElement) {
            addPage(label, [viewElement] { return brls::View::createFromXMLElement(viewElement); });
            if (viewElement->NextSiblingElement()) {
                brls::fatal("\"brls:Tab\" can only contain one child element");
            }
        } else {
            addPage(label, [] { return nullptr; });
        }
        return;
    }

    brls::Box::handleXMLElement(element);
}

brls::View* EditorPagerFrame::create() {
    return new EditorPagerFrame();
}

}  // namespace totk::ui
