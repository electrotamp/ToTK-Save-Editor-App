#include "ui/zonai_wisp_session.hpp"

#include <algorithm>

#include <borealis/core/time.hpp>

#include "ui/zonai_wisp_settings.hpp"

namespace totk::ui {

namespace {

constexpr float kMaxFrameDeltaSeconds = 0.5f;
constexpr int kMaxStepsPerFrame = 30;

}  // namespace

ZonaiWispSession& ZonaiWispSession::instance() {
    static ZonaiWispSession session;
    return session;
}

void ZonaiWispSession::ensureRunning() {
    if (!ZonaiWispSettings::instance().loaded()) {
        running_ = false;
        timeAccumulator_ = 0.0f;
        return;
    }
    if (running_) return;

    simulator_.reset(ZonaiWispSettings::instance());
    timeAccumulator_ = 0.0f;
    running_ = true;
}

void ZonaiWispSession::reloadFromSettings() {
    if (!ZonaiWispSettings::instance().loaded()) {
        simulator_.clear();
        running_ = false;
        timeAccumulator_ = 0.0f;
        return;
    }

    simulator_.reset(ZonaiWispSettings::instance());
    timeAccumulator_ = 0.0f;
    running_ = true;
}

void ZonaiWispSession::advanceTime(float dtSeconds) {
    if (!running_ || dtSeconds <= 0.0f) return;

    timeAccumulator_ += std::min(dtSeconds, kMaxFrameDeltaSeconds);

    constexpr float kTick = WispSimulator::kTick;
    int steps = 0;
    while (timeAccumulator_ >= kTick && steps < kMaxStepsPerFrame) {
        simulator_.tick(kTick);
        timeAccumulator_ -= kTick;
        ++steps;
    }
}

void ZonaiWispSession::advanceFrameClock() {
    static brls::Time lastUsec = 0;
    const brls::Time now = brls::getCPUTimeUsec();
    if (lastUsec == 0) {
        lastUsec = now;
        return;
    }

    const float dt = static_cast<float>(now - lastUsec) / 1000000.0f;
    lastUsec = now;
    advanceTime(dt);
}

}  // namespace totk::ui
