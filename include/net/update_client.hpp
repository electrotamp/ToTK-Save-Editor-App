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

// Downloads the .nro asset from `info.downloadUrl` to a temp path next to
// this app's own on-SD install path (sdmc:/switch/totk-save-editor/) —
// totk_save_editor.nro.update. Does NOT touch the install path itself: on
// real hardware, overwriting/renaming over the .nro that's currently the
// running process can fail with a low-level I/O error (the file is still
// "in use" from the loader's point of view), so installation is deferred —
// see relaunchIntoUpdatedApp() / finalizePendingUpdate(). Blocking — run via
// brls::async. Switch-only; always fails with an explanatory error off-Switch.
bool downloadUpdate(const UpdateInfo& info, std::string& error);

// Chain-loads the just-downloaded temp NRO (not the install path — see
// downloadUpdate()) via libnx envSetNextLoad() and requests a clean app exit
// (brls::Application::quit()) so main()'s normal shutdown path still runs
// before hbloader picks up the next load. The temp file becomes the running
// process on relaunch, which is what lets finalizePendingUpdate() safely
// overwrite the (now idle) install path on that next startup. Returns false
// (no-op) off-Switch, or when the current launch environment doesn't support
// NextLoad — callers should fall back to telling the user to relaunch
// manually in that case.
bool relaunchIntoUpdatedApp();

// Call once, early in main(), before anything else touches the install
// path. If a downloaded-but-not-yet-installed update is sitting at the temp
// path (see downloadUpdate()), moves it into place over the install path.
// Safe to call unconditionally on every launch: a no-op when there's no
// pending temp file, and if the move fails (e.g. this happens to be a normal
// launch of the install-path NRO itself, which is still "in use"), it just
// logs and leaves the temp file for the next opportunity rather than erroring.
void finalizePendingUpdate();

// Persisted "check for updates automatically on launch" preference —
// sdmc:/switch/totk-save-editor/update_settings.json. Defaults to true.
// Only gates the launch-time auto-check; a manual check (About tab) always
// runs regardless of this setting.
bool autoCheckEnabled();
void setAutoCheckEnabled(bool enabled);

}  // namespace totk::net
