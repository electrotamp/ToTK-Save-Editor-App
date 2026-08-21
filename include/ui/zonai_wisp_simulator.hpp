#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ui/zonai_wisp_settings.hpp"

namespace totk::ui {

struct WispPoint2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct WispBandGenerator {
    std::string id;
    std::string profileId;
    float bandMin = 0.0f;
    float bandMax = 0.0f;
    float flowHeading = 0.0f;
};

struct WispEntity {
    bool alive = true;
    float age = 0.0f;
    WispBandGenerator gen;
    std::string profileId;
    size_t profileIndex = 0;
    int pointCount = 0;
    float spinRate = 0.0f;
    float decayStart = 0.0f;
    float fadeDuration = 0.0f;
    std::optional<float> exitFadeStart;
    float exitFadeDuration = 3.2f;
    float heading = 0.0f;
    float speed = 0.0f;
    float phase = 0.0f;
    float steerNoise = 0.0f;
    float steerTimer = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float wobble = 0.0f;
    float ripple = 0.0f;
    float width = 0.0f;
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> sx;
    std::vector<float> sy;
    bool hasSmooth = false;
    bool circleEngaged = false;
    float circleMix = 0.0f;
    std::optional<float> circleAngle;
    int circleDir = 1;
};

struct WispFragment {
    std::string profileId;
    size_t profileIndex = 0;
    float x = 0.0f;
    float y = 0.0f;
    float flowHeading = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float age = 0.0f;
    float lifetime = 0.0f;
    float size = 0.0f;
    float wiggle = 0.0f;
};

class WispSimulator {
public:
    static constexpr float kSimWidth = 1280.0f;
    static constexpr float kSimHeight = 720.0f;
    static constexpr float kTick = 1.0f / 60.0f;

    void reset(const ZonaiWispSettings& settings);

    void tick(float dt);
    void clear();

    const std::vector<WispEntity>& wisps() const { return wisps_; }
    const std::vector<WispFragment>& fragments() const { return fragments_; }

    float wispAlpha(const WispEntity& wisp, const WispProfile& profile) const;
    float fragmentAlpha(const WispFragment& fragment, const WispProfile& profile) const;

private:
    const WispProfile* profileById(const std::string& id) const;

    float rngUnit() const;
    float rngRange(float a, float b) const;

    void initProfileTimers();
    float profileSpawnDelay(const WispProfile& profile) const;
    bool trySpawnProfile(const WispProfile& profile);
    void spawnWisp(const WispProfile& profile, const WispBandGenerator& gen);

    std::vector<WispBandGenerator> bandGenerators(const WispProfile& profile) const;
    int countWispsInBand(const std::string& profileId, const std::string& genId) const;
    int countFragmentsForProfile(const std::string& profileId) const;
    std::optional<WispBandGenerator> pickGeneratorForProfile(const WispProfile& profile) const;

    void applyCircleOrbit(WispEntity& wisp, const WispProfile& profile, float effSpeed);
    void applyDriftBias(WispEntity& wisp, const WispProfile& profile) const;
    void applyBandConstraint(WispEntity& wisp, const WispProfile& profile) const;
    void clampHeadToBand(WispEntity& wisp, const WispProfile& profile) const;
    void checkExitFade(WispEntity& wisp, const WispProfile& profile);
    void beginExitFade(WispEntity& wisp, const WispProfile& profile);
    void smoothWispPoints(WispEntity& wisp, float strength);
    void emitFragment(const WispEntity& wisp, int index, const WispProfile& profile);
    void compactDead();

    const ZonaiWispSettings* settings_ = nullptr;
    mutable uint32_t rng_ = 0;
    int compactCounter_ = 0;
    std::unordered_map<std::string, float> profileSpawnTimers_;
    std::vector<WispEntity> wisps_;
    std::vector<WispFragment> fragments_;
};

}  // namespace totk::ui
