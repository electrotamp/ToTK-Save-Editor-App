#include <borealis.hpp>

#include <exception>
#include <cstdlib>

#include "glad/glad.h"

#if !defined(TOTK_SAVE_EDITOR)
#include "activity/activities.hpp"
#endif
#include "app/app_state.hpp"
#include "save/completism_data.hpp"
#include "ui/item_database.hpp"
#if !defined(TOTK_SAVE_EDITOR)
#include "ui/item_list_tab.hpp"
#endif
#include "util/totk_log.hpp"
#include "util/perf_trace.hpp"
#include "util/debug_stage.hpp"
#include "util/icon_texture_cache.hpp"
#ifdef TOTK_ICON_ATLAS
#include "util/icon_atlas.hpp"
#endif
#ifdef TOTK_SAVE_EDITOR
#include "save_editor/save_editor_app.hpp"
#include "net/http_client.hpp"
#endif
#if !defined(TOTK_WEAPONS_ONLY_UI)
#include "ui/editor_theme.hpp"
#include "ui/zonai_wisp_session.hpp"
#endif
#if !defined(TOTK_WEAPONS_ONLY_UI) && !defined(TOTK_SAVE_EDITOR)
#include "ui/editor_pager_frame.hpp"
#include "ui/status_tab.hpp"
#endif
#if defined(__SWITCH__)
#include "platform/switch_save_mount.hpp"
#endif

namespace {

[[noreturn]] void totkTerminateHandler() {
    try {
        if (auto current = std::current_exception()) {
            std::rethrow_exception(current);
        }
        TOTK_LOG("terminate: no active exception");
    } catch (const std::exception& ex) {
        TOTK_LOG("terminate: %s", ex.what());
    } catch (...) {
        TOTK_LOG("terminate: unknown exception");
    }
    std::abort();
}

}  // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::set_terminate(totkTerminateHandler);
#ifdef TOTK_NXLINK_DEBUG
    totk::resetStageLog();
    TOTK_STAGE("main.start build=%s", TOTK_BUILD_ID);
#endif
#ifdef TOTK_SAVE_EDITOR
    TOTK_LOG("main start build=%s ui=save-editor atlas=weapons-only", TOTK_BUILD_ID);
#elif defined(TOTK_WEAPONS_ONLY_UI)
    TOTK_LOG("main start build=%s ui=weapons-only icon-atlas=1 nxlink-debug=%d", TOTK_BUILD_ID,
#ifdef TOTK_NXLINK_DEBUG
             1
#else
             0
#endif
    );
#else
    TOTK_LOG("main start build=%s ui=minimal-atlas tabs=stats+weapons layout=fullscreen-pager"
#ifdef TOTK_ICON_ATLAS
             " icon-atlas=1"
#endif
#ifdef TOTK_NXLINK_DEBUG
             " nxlink-debug=1"
#endif
             ,
             TOTK_BUILD_ID);
#endif

    if (!brls::Application::init()) {
        TOTK_LOG("Borealis init failed");
        brls::Logger::error("Failed to initialize Borealis");
        return EXIT_FAILURE;
    }

#ifdef TOTK_NXLINK_DEBUG
    TOTK_LOG("nxlink debug: logs → stderr + nxlink/logs/run-*.log");
#endif

#ifdef TOTK_ICON_ATLAS
    totk::IconTextureCache::ensureCapacity(
#ifdef TOTK_SAVE_EDITOR
        32
#elif defined(TOTK_WEAPONS_ONLY_UI)
        8
#else
        16
#endif
    );
#else
    totk::IconTextureCache::ensureCapacity(512);
#endif

    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    brls::Application::setGlobalQuit(false);

    {
        TOTK_PERF_SCOPE("startup.itemDatabase");
        totk::ItemDatabase::instance().loadFromRomfs();
    }

    // Never previously called anywhere — koroks_hidden/shrines_status/etc.
    // hash groups would silently stay empty without this, and everything
    // reading from CompletismData (completion counts, pin-missing actions)
    // would quietly no-op.
    {
        TOTK_PERF_SCOPE("startup.completismData");
        const bool completismOk = totk::CompletismData::instance().loadFromRomfs();
        TOTK_LOG("editor: completism loaded=%d shrines_status=%zu lightroots_status=%zu koroks_hidden=%zu "
                 "koroks_carry=%zu",
                 completismOk ? 1 : 0, totk::CompletismData::instance().hashesFor("shrines_status").size(),
                 totk::CompletismData::instance().hashesFor("lightroots_status").size(),
                 totk::CompletismData::instance().hashesFor("koroks_hidden").size(),
                 totk::CompletismData::instance().hashesFor("koroks_carry").size());
    }

#if defined(TOTK_SAVE_EDITOR)
    totk::net::HttpClient::initialize();
    totk::ui::EditorTheme::instance().setWispReloadHandler([]() {
        totk::ui::ZonaiWispSession::instance().reloadFromSettings();
    });
    totk::ui::EditorTheme::instance().loadStartup();
    brls::Application::createWindow("ToTK Save Editor");
    brls::Application::pushActivity(new totk::save_editor::PickerActivity(), brls::TransitionAnimation::NONE);
#elif defined(TOTK_WEAPONS_ONLY_UI)
    brls::Application::registerXMLView("ItemListTabWeapons", totk::ui::ItemListTab::createWeapons);
    brls::Application::createWindow("ToTK Save Editor");
    brls::Application::pushActivity(new totk::ui::SlotPickerActivity(), brls::TransitionAnimation::NONE);
#else
    totk::ui::EditorTheme::instance().loadStartup();
    totk::ui::EditorTheme::instance().setWispReloadHandler([]() {
        totk::ui::ZonaiWispSession::instance().reloadFromSettings();
    });
    brls::Application::registerXMLView("EditorPagerFrame", totk::ui::EditorPagerFrame::create);
    brls::Application::registerXMLView("StatusTab", totk::ui::StatusTab::create);
    brls::Application::registerXMLView("ItemListTabWeapons", totk::ui::ItemListTab::createWeapons);
    brls::Application::createWindow("ToTK Save Editor");
    brls::Application::pushActivity(new totk::ui::SlotPickerActivity(), brls::TransitionAnimation::NONE);
#endif

    {
        GLint maxTexSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
        TOTK_LOG("gl max_texture_size=%d", maxTexSize);
    }

    while (true) {
        totk::FrameMonitor::tick();
#if !defined(TOTK_WEAPONS_ONLY_UI)
        totk::ui::ZonaiWispSession::instance().advanceFrameClock();
#endif
        if (!brls::Application::mainLoop()) break;
    }

#if defined(__SWITCH__)
    totk::SwitchSaveMount::shutdown();
#endif
#if defined(TOTK_SAVE_EDITOR)
    totk::net::HttpClient::shutdown();
#endif

    return EXIT_SUCCESS;
}

#ifdef __WINRT__
#include <borealis/core/main.hpp>
#endif
