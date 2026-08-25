#pragma once

namespace totk::ui {

// Checks the latest GitHub release against the running build. Dispatches its
// own blocking network call via brls::async internally — safe to call
// directly from the UI thread.
//
// If an update is available this always shows the Yes / No / Don't ask
// again dialog. `announceResult` only controls whether a toast is shown for
// the two silent outcomes (already up to date, or the check itself failed):
// false for the launch-time auto-check (shouldn't nag on every boot when
// there's nothing to say), true for the About tab's manual "Check for
// Updates" button (the user just asked, so they should get an answer either
// way).
void checkForUpdatesAsync(bool announceResult);

}  // namespace totk::ui
