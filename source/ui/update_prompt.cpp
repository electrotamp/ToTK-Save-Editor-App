#include "ui/update_prompt.hpp"

#include "net/update_client.hpp"
#include "util/totk_log.hpp"

#include <borealis.hpp>

namespace totk::ui {

namespace {

void beginDownloadAndInstall(const totk::net::UpdateInfo& info) {
    brls::Application::notify("Downloading update v" + info.version + "...");
    brls::async([info]() {
        std::string error;
        const bool ok = totk::net::downloadUpdate(info, error);
        brls::sync([ok, error, info]() {
            if (!ok) {
                TOTK_LOG("updater: download failed: %s", error.c_str());
                brls::Application::notify("Update failed: " + error);
                return;
            }
            if (!totk::net::relaunchIntoUpdatedApp()) {
                auto* dialog = new brls::Dialog(
                    "Update downloaded, but this Homebrew Menu can't auto-relaunch into it. On your SD card, in "
                    "switch/totk-save-editor/, delete totk_save_editor.nro and rename "
                    "totk_save_editor.nro.update to totk_save_editor.nro, then relaunch.");
                dialog->addButton("OK", []() {});
                dialog->open();
            }
        });
    });
}

void showUpdateAvailableDialog(const totk::net::UpdateInfo& info) {
    const std::string prompt = "A new version is available: v" + info.version + " (you have v" +
                                totk::net::currentVersionString() + "). Update now?";
    auto* dialog = new brls::Dialog(prompt);
    dialog->addButton("Yes", [info]() { beginDownloadAndInstall(info); });
    dialog->addButton("No", []() {});
    dialog->addButton("Don't ask again", []() {
        totk::net::setAutoCheckEnabled(false);
        brls::Application::notify("Won't check automatically anymore — you can still check from the About tab.");
    });
    dialog->open();
}

}  // namespace

void checkForUpdatesAsync(bool announceResult) {
    brls::async([announceResult]() {
        totk::net::UpdateInfo info;
        std::string error;
        const bool ok = totk::net::fetchLatestRelease(info, error);
        brls::sync([ok, error, info, announceResult]() {
            if (!ok) {
                TOTK_LOG("updater: check failed: %s", error.c_str());
                if (announceResult) brls::Application::notify("Could not check for updates: " + error);
                return;
            }
            if (!totk::net::isNewerVersion(info.version, totk::net::currentVersionString())) {
                TOTK_LOG("updater: up to date (current=%s latest=%s)", totk::net::currentVersionString().c_str(),
                          info.version.c_str());
                if (announceResult) {
                    brls::Application::notify("You're on the latest version (v" + totk::net::currentVersionString() +
                                               ").");
                }
                return;
            }
            showUpdateAvailableDialog(info);
        });
    });
}

}  // namespace totk::ui
