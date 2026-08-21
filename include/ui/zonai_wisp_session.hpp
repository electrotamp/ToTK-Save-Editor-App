#pragma once

#include "ui/zonai_wisp_simulator.hpp"

namespace totk::ui {

// One wisp simulation for the entire app session (no reset on activity navigation).
class ZonaiWispSession {
public:
    static ZonaiWispSession& instance();

    WispSimulator& simulator() { return simulator_; }
    const WispSimulator& simulator() const { return simulator_; }

    void ensureRunning();
    void reloadFromSettings();
    void advanceTime(float dtSeconds);
    void advanceFrameClock();
    bool isRunning() const { return running_; }

private:
    WispSimulator simulator_;
    float timeAccumulator_ = 0.0f;
    bool running_ = false;
};

}  // namespace totk::ui
