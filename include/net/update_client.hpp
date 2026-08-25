#pragma once

#include <string>

namespace totk::net {

struct UpdateInfo {
    std::string version;      // e.g. "1.1.0" — leading 'v' stripped from the release tag
    std::string downloadUrl;  // browser_download_url for totk_save_editor.nro
    std::string releaseUrl;   // release's own html_url, for logging/reference
};

// Current running build's version, from the APP_VERSION_STRING compile
// definition (set in CMakeLists.txt from VERSION_MAJOR/MINOR/ALTER).
std::string currentVersionString();

// True if `latest` is a strictly newer major.minor.patch than `current`.
// Missing/non-numeric components are treated as 0 — not a full semver
// parser (no pre-release/build metadata), but this project only ever
// publishes plain X.Y.Z tags.
bool isNewerVersion(const std::string& latest, const std::string& current);

// GET https://api.github.com/repos/electrotamp/ToTK-Save-Editor-App/releases/latest
// Blocking network call — run off the UI thread via brls::async, same as
// the rest of this app's networking (see net/http_client.hpp).
bool fetchLatestRelease(UpdateInfo& outInfo, std::string& error);

// Downloads the .nro asset from `info.downloadUrl` and installs it over this
// app's own on-SD install path (sdmc:/switch/totk-save-editor/totk_save_editor.nro)
// via a temp-file-then-rename. Blocking — run via brls::async. Switch-only;
// always fails with an explanatory error off-Switch.
bool downloadAndInstallUpdate(const UpdateInfo& info, std::string& error);

// Chain-loads the just-installed NRO via libnx envSetNextLoad() and requests
// a clean app exit (brls::Application::quit()) so main()'s normal shutdown
// path still runs before hbloader picks up the next load. Returns false
// (no-op) off-Switch, or when the current launch environment doesn't
// support NextLoad — callers should fall back to telling the user to
// relaunch manually in that case.
bool relaunchIntoUpdatedApp();

// Persisted "check for updates automatically on launch" preference —
// sdmc:/switch/totk-save-editor/update_settings.json. Defaults to true.
// Only gates the launch-time auto-check; a manual check (About tab) always
// runs regardless of this setting.
bool autoCheckEnabled();
void setAutoCheckEnabled(bool enabled);

}  // namespace totk::net
