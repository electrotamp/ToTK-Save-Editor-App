#include "ui/zonai_wisp_simulator.hpp"

#include <algorithm>
#include <cmath>

namespace totk::ui {

namespace {

constexpr float kPi = 3.14159265358979323846f;

float smoothstep(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}

bool flowExitsRight(float flowHeading) {
    return std::abs(flowHeading) < 0.01f;
}

float bandFlowHeading(const WispProfile& profile, const std::string& bandId) {
    const bool leftToRight = bandId == "top" ? profile.topBandLeftToRight : profile.bottomBandLeftToRight;
    return leftToRight ? 0.0f : kPi;
}

}  // namespace

void WispSimulator::reset(const ZonaiWispSettings& settings) {
    settings_ = &settings;
    rng_ = settings.simSeed();
    wisps_.clear();
    fragments_.clear();
    profileSpawnTimers_.clear();
    initProfileTimers();
}

void WispSimulator::clear() {
    wisps_.clear();
    fragments_.clear();
    if (settings_) initProfileTimers();
}

const WispProfile* WispSimulator::profileById(const std::string& id) const {
    return settings_ ? settings_->profileById(id) : nullptr;
}

float WispSimulator::rngUnit() const {
    rng_ = static_cast<uint32_t>(static_cast<uint32_t>(rng_ * 1664525u) + 1013904223u);
    return static_cast<float>(rng_ & 0xffffffu) / static_cast<float>(0x1000000);
}

float WispSimulator::rngRange(float a, float b) const {
    return a + (b - a) * rngUnit();
}

void WispSimulator::initProfileTimers() {
    profileSpawnTimers_.clear();
    if (!settings_) return;
    for (const WispProfile& profile : settings_->profiles()) {
        profileSpawnTimers_[profile.id] = rngRange(0.0f, profileSpawnDelay(profile));
    }
}

float WispSimulator::profileSpawnDelay(const WispProfile& profile) const {
    const float weight = std::max(0.05f, profile.spawnWeight);
    return rngRange(profile.spawnCooldownMin, profile.spawnCooldownMax) / weight;
}

std::vector<WispBandGenerator> WispSimulator::bandGenerators(const WispProfile& profile) const {
    std::vector<WispBandGenerator> gens;
    if (profile.spawnTopBand) {
        gens.push_back(WispBandGenerator{
            "top",
            profile.id,
            0.0f,
            kSimHeight * profile.topBandEnd,
            bandFlowHeading(profile, "top"),
        });
    }
    if (profile.spawnBottomBand) {
        gens.push_back(WispBandGenerator{
            "bottom",
            profile.id,
            kSimHeight * profile.bottomBandStart,
            kSimHeight,
            bandFlowHeading(profile, "bottom"),
        });
    }
    return gens;
}

int WispSimulator::countWispsInBand(const std::string& profileId, const std::string& genId) const {
    int count = 0;
    for (const WispEntity& wisp : wisps_) {
        if (wisp.alive && wisp.profileId == profileId && wisp.gen.id == genId) ++count;
    }
    return count;
}

int WispSimulator::countFragmentsForProfile(const std::string& profileId) const {
    int count = 0;
    for (const WispFragment& fragment : fragments_) {
        if (fragment.profileId == profileId) ++count;
    }
    return count;
}

std::optional<WispBandGenerator> WispSimulator::pickGeneratorForProfile(const WispProfile& profile) const {
    const auto gens = bandGenerators(profile);
    std::vector<WispBandGenerator> available;
    available.reserve(gens.size());
    for (const WispBandGenerator& gen : gens) {
        if (countWispsInBand(profile.id, gen.id) < profile.maxPerBand) available.push_back(gen);
    }
    if (available.empty()) return std::nullopt;
    const int index = static_cast<int>(rngUnit() * available.size());
    return available[static_cast<size_t>(std::min(index, static_cast<int>(available.size()) - 1))];
}

float WispSimulator::wispAlpha(const WispEntity& wisp, const WispProfile& profile) const {
    float alpha = 1.0f;
    if (wisp.age < profile.fadeIn) alpha = smoothstep(wisp.age / profile.fadeIn);
    if (wisp.age >= wisp.decayStart) {
        const float t = (wisp.age - wisp.decayStart) / wisp.fadeDuration;
        alpha = std::min(alpha, 1.0f - smoothstep(t));
    }
    if (wisp.exitFadeStart.has_value()) {
        const float t = (wisp.age - *wisp.exitFadeStart) / wisp.exitFadeDuration;
        alpha = std::min(alpha, 1.0f - smoothstep(t));
    }
    return alpha;
}

float WispSimulator::fragmentAlpha(const WispFragment& fragment, const WispProfile& profile) const {
    if (fragment.age < profile.fragmentFadeIn) return smoothstep(fragment.age / profile.fragmentFadeIn);
    const float fadeStart = fragment.lifetime * 0.45f;
    if (fragment.age < fadeStart) return 1.0f;
    return 1.0f - smoothstep((fragment.age - fadeStart) / (fragment.lifetime - fadeStart));
}

void WispSimulator::beginExitFade(WispEntity& wisp, const WispProfile& profile) {
    if (wisp.exitFadeStart.has_value()) return;
    wisp.exitFadeStart = wisp.age;
    wisp.exitFadeDuration = rngRange(profile.exitFadeMin, profile.exitFadeMax);
}

void WispSimulator::checkExitFade(WispEntity& wisp, const WispProfile& profile) {
    const float margin = profile.exitMargin;
    if (flowExitsRight(wisp.gen.flowHeading) && wisp.x[0] > kSimWidth + margin) beginExitFade(wisp, profile);
    else if (!flowExitsRight(wisp.gen.flowHeading) && wisp.x[0] < -margin) beginExitFade(wisp, profile);
}

void WispSimulator::applyDriftBias(WispEntity& wisp, const WispProfile& profile) const {
    const float f = wisp.gen.flowHeading;
    wisp.vx += std::cos(f) * profile.driftPush * profile.speedMultiplier;
    wisp.vy += std::sin(f) * profile.driftPush * profile.driftVerticalRatio * profile.speedMultiplier;
}

void WispSimulator::applyBandConstraint(WispEntity& wisp, const WispProfile& profile) const {
    const WispBandGenerator& gen = wisp.gen;
    const float bandPad = (gen.bandMax - gen.bandMin) * profile.bandSoftPad;
    const float softMin = gen.bandMin + bandPad;
    const float softMax = gen.bandMax - bandPad;
    if (wisp.y[0] < softMin) wisp.vy += (softMin - wisp.y[0]) * profile.bandSpring * kTick;
    else if (wisp.y[0] > softMax) wisp.vy -= (wisp.y[0] - softMax) * profile.bandSpring * kTick;
}

void WispSimulator::clampHeadToBand(WispEntity& wisp, const WispProfile& profile) const {
    if (wisp.circleMix > 0.35f) return;
    const WispBandGenerator& gen = wisp.gen;
    const float pad = (gen.bandMax - gen.bandMin) * profile.bandHardPad;
    wisp.y[0] = std::max(gen.bandMin + pad, std::min(gen.bandMax - pad, wisp.y[0]));
}

void WispSimulator::applyCircleOrbit(WispEntity& wisp, const WispProfile& profile, float /*effSpeed*/) {
    if (!profile.circleEnabled) {
        wisp.circleEngaged = false;
        wisp.circleMix = 0.0f;
        return;
    }

    const float cx = kSimWidth * profile.circleCenterX;
    const float cy = kSimHeight * profile.circleCenterY;
    const float rx = std::max(8.0f, profile.circleRadiusX);
    const float ry = std::max(8.0f, profile.circleRadiusY);
    const float zoneStart = kSimWidth * profile.circleZoneStart;
    const float zoneEnd = zoneStart + kSimWidth * profile.circleZoneWidth;
    const float pad = kSimWidth * profile.circleApproach;
    const float x = wisp.x[0];

    if (!wisp.circleEngaged) {
        const bool inCore = x >= zoneStart && x <= zoneEnd;
        const bool fromLeft = x >= zoneStart - pad && x < zoneStart;
        const bool fromRight = x > zoneEnd && x <= zoneEnd + pad;
        if (inCore || fromLeft || fromRight) {
            wisp.circleEngaged = true;
            wisp.circleAngle = std::atan2(wisp.y[0] - cy, wisp.x[0] - cx);
            wisp.circleDir = flowExitsRight(wisp.gen.flowHeading) ? 1 : -1;
        }
    }

    const float blendStep = kTick / std::max(0.1f, profile.circleBlendTime);
    if (wisp.circleEngaged) wisp.circleMix = std::min(1.0f, wisp.circleMix + blendStep);
    else wisp.circleMix = std::max(0.0f, wisp.circleMix - blendStep);

    const float mix = wisp.circleMix;
    if (mix <= 0.002f) return;

    if (!wisp.circleAngle.has_value()) {
        wisp.circleAngle = std::atan2(wisp.y[0] - cy, wisp.x[0] - cx);
        wisp.circleDir = flowExitsRight(wisp.gen.flowHeading) ? 1 : -1;
    }

    const int dir = wisp.circleDir;
    const float angSpeed = profile.circleAngularSpeed * static_cast<float>(dir);
    *wisp.circleAngle += angSpeed * kTick;

    const float angle = *wisp.circleAngle;
    const float tx = cx + std::cos(angle) * rx;
    const float ty = cy + std::sin(angle) * ry;
    const float tanVx = -std::sin(angle) * angSpeed * rx;
    const float tanVy = std::cos(angle) * angSpeed * ry;
    const float pull = profile.circlePull * mix;
    const float radialX = (tx - wisp.x[0]) * pull;
    const float radialY = (ty - wisp.y[0]) * pull;
    const float orbitVx = tanVx + radialX;
    const float orbitVy = tanVy + radialY;
    const float keep = 1.0f - mix;

    wisp.vx = wisp.vx * keep + orbitVx * mix;
    wisp.vy = wisp.vy * keep + orbitVy * mix;
    wisp.heading = std::atan2(wisp.vy, wisp.vx);
    wisp.speed = std::max(profile.speedClampMin,
                          std::min(profile.speedClampMax,
                                   std::hypot(wisp.vx, wisp.vy) / profile.speedMultiplier));
}

void WispSimulator::smoothWispPoints(WispEntity& wisp, float strength) {
    if (!wisp.hasSmooth) {
        wisp.sx = wisp.x;
        wisp.sy = wisp.y;
        wisp.hasSmooth = true;
    }
    for (int i = 0; i < wisp.pointCount; ++i) {
        wisp.sx[static_cast<size_t>(i)] += (wisp.x[static_cast<size_t>(i)] - wisp.sx[static_cast<size_t>(i)]) * strength;
        wisp.sy[static_cast<size_t>(i)] += (wisp.y[static_cast<size_t>(i)] - wisp.sy[static_cast<size_t>(i)]) * strength;
    }
}

void WispSimulator::emitFragment(const WispEntity& wisp, int i, const WispProfile& profile) {
    if (countFragmentsForProfile(wisp.profileId) >= profile.maxFragments) return;
    const float px = wisp.x[static_cast<size_t>(i)];
    const float py = wisp.y[static_cast<size_t>(i)];
    float dx = i > 0 ? wisp.x[static_cast<size_t>(i - 1)] - px : wisp.vx * 0.02f;
    float dy = i > 0 ? wisp.y[static_cast<size_t>(i - 1)] - py : std::sin(wisp.age * wisp.wobble) * 0.5f;
    const float len = std::hypot(dx, dy);
    if (len > 0.0001f) {
        dx /= len;
        dy /= len;
    } else {
        dx = 1.0f;
        dy = 0.0f;
    }
    const float nx = -dy;
    const float ny = dx;
    const float peel = rngRange(0.25f, 0.7f);
    const float flowPush = std::cos(wisp.gen.flowHeading) * rngRange(18.0f, 42.0f);
    fragments_.push_back(WispFragment{
        wisp.profileId,
        wisp.profileIndex,
        px,
        py,
        wisp.gen.flowHeading,
        (dx)*rngRange(-12.0f, 18.0f) + flowPush + nx * rngRange(-28.0f, 28.0f) * peel,
        (dy)*rngRange(-12.0f, 18.0f) + rngRange(-16.0f, 16.0f) + ny * rngRange(-28.0f, 28.0f) * peel,
        0.0f,
        rngRange(profile.fragmentLifeMin, profile.fragmentLifeMax),
        rngRange(1.0f, 2.8f),
        rngRange(2.5f, 5.5f),
    });
}

void WispSimulator::spawnWisp(const WispProfile& profile, const WispBandGenerator& gen) {
    const float padY = (gen.bandMax - gen.bandMin) * profile.spawnPadY;
    const float spawnY = rngRange(gen.bandMin + padY, gen.bandMax - padY);
    float spawnX;
    if (flowExitsRight(gen.flowHeading)) spawnX = rngRange(-120.0f, -24.0f);
    else spawnX = rngRange(kSimWidth + 24.0f, kSimWidth + 120.0f);

    const float heading = rngRange(0.0f, kPi * 2.0f);
    const float speed = rngRange(profile.speedMin, profile.speedMax) * profile.speedMultiplier;
    const float decayStart = rngRange(profile.decayStartMin, profile.decayStartMax);
    const float fadeDuration = rngRange(profile.fadeDurMin, profile.fadeDurMax);
    const int pointCount = std::max(3, profile.pointCount);
    const float spinRate = rngRange(profile.spinMin, profile.spinMax);

    WispEntity wisp;
    wisp.gen = gen;
    wisp.profileId = profile.id;
    if (settings_) wisp.profileIndex = settings_->profileIndexFor(profile.id);
    wisp.pointCount = pointCount;
    wisp.spinRate = spinRate;
    wisp.decayStart = decayStart;
    wisp.fadeDuration = fadeDuration;
    wisp.heading = heading;
    wisp.speed = speed;
    wisp.phase = rngRange(0.0f, kPi * 2.0f);
    wisp.steerNoise = rngRange(-1.3f, 1.3f);
    wisp.steerTimer = rngRange(0.12f, 0.55f);
    wisp.vx = std::cos(heading) * speed;
    wisp.vy = std::sin(heading) * speed;
    wisp.wobble = rngRange(profile.wobbleFreqMin, profile.wobbleFreqMax);
    wisp.ripple = rngRange(profile.rippleFreqMin, profile.rippleFreqMax);
    wisp.width = rngRange(profile.widthMin, profile.widthMax);
    wisp.x.assign(static_cast<size_t>(pointCount), 0.0f);
    wisp.y.assign(static_cast<size_t>(pointCount), 0.0f);

    const float tailSpan = rngRange(profile.tailLenMin, profile.tailLenMax);
    for (int i = 0; i < pointCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(pointCount - 1);
        wisp.x[static_cast<size_t>(i)] = spawnX - std::cos(heading) * t * tailSpan;
        wisp.y[static_cast<size_t>(i)] = spawnY - std::sin(heading) * t * tailSpan;
    }

    wisps_.push_back(std::move(wisp));
}

bool WispSimulator::trySpawnProfile(const WispProfile& profile) {
    if (!profile.enabled || profile.spawnWeight <= 0.0f) return false;
    const auto gen = pickGeneratorForProfile(profile);
    if (!gen.has_value()) return false;
    spawnWisp(profile, *gen);
    return true;
}

void WispSimulator::tick(float /*dt*/) {
    if (!settings_) return;

    for (const WispProfile& profile : settings_->profiles()) {
        float& timer = profileSpawnTimers_[profile.id];
        timer -= kTick;
        if (timer > 0.0f) continue;
        trySpawnProfile(profile);
        timer = profileSpawnDelay(profile);
    }

    for (WispEntity& wisp : wisps_) {
        if (!wisp.alive) continue;
        wisp.age += kTick;
        const WispProfile* profile = profileById(wisp.profileId);
        if (!profile) {
            wisp.alive = false;
            continue;
        }

        const float alpha = wispAlpha(wisp, *profile);
        const bool fadingOut = wisp.age >= wisp.decayStart || wisp.exitFadeStart.has_value();
        if (fadingOut && alpha <= 0.002f) {
            wisp.alive = false;
            continue;
        }

        wisp.steerTimer -= kTick;
        if (wisp.steerTimer <= 0.0f) {
            wisp.steerNoise += rngRange(-0.75f, 0.75f);
            wisp.steerNoise = std::max(-profile->steerMax, std::min(profile->steerMax, wisp.steerNoise));
            wisp.steerTimer = rngRange(0.2f, 0.65f);
        }

        wisp.heading += wisp.steerNoise * kTick;
        wisp.heading += wisp.spinRate * kTick;
        wisp.heading += std::sin(wisp.age * wisp.ripple + wisp.phase) * profile->rippleHeading * kTick;
        wisp.heading += std::cos(wisp.age * wisp.wobble * 1.7f + wisp.phase * 0.6f) * profile->wobbleHeading * kTick;
        wisp.heading += std::sin(wisp.age * wisp.wobble * 2.8f + wisp.phase * 1.4f) * profile->chaosHeading * kTick;
        if (rngUnit() < profile->headingKickChance) wisp.heading += rngRange(-profile->headingKick, profile->headingKick);
        if (profile->piJumpChance > 0.0f && rngUnit() < profile->piJumpChance) {
            wisp.heading += rngRange(-profile->piJumpMag, profile->piJumpMag) * kPi;
        }

        if (rngUnit() < profile->spinMutateChance) wisp.spinRate += rngRange(-profile->spinMutateAmt, profile->spinMutateAmt);
        wisp.spinRate = std::max(profile->spinMin, std::min(profile->spinMax, wisp.spinRate));

        wisp.speed += rngRange(-profile->speedJitter, profile->speedJitter) * kTick;
        wisp.speed = std::max(profile->speedClampMin, std::min(profile->speedClampMax, wisp.speed));

        const float perp = wisp.heading + kPi / 2.0f;
        const float effSpeed = wisp.speed * profile->speedMultiplier;
        const float wobbleMag = (std::sin(wisp.age * wisp.wobble * kPi * 2.0f) * profile->wobbleAmpA +
                                 std::sin(wisp.age * wisp.ripple * 1.3f + 1.1f) * profile->wobbleAmpB) *
                                profile->wobbleScale;
        wisp.vx = std::cos(wisp.heading) * effSpeed + std::cos(perp) * wobbleMag;
        wisp.vy = std::sin(wisp.heading) * effSpeed + std::sin(perp) * wobbleMag;

        applyCircleOrbit(wisp, *profile, effSpeed);
        const float circleMix = wisp.circleMix;

        if (circleMix < 0.5f) applyDriftBias(wisp, *profile);
        if (circleMix < 0.35f) applyBandConstraint(wisp, *profile);
        wisp.x[0] += wisp.vx * kTick;
        wisp.y[0] += wisp.vy * kTick;
        if (circleMix < 0.35f) clampHeadToBand(wisp, *profile);
        if (circleMix < 0.5f) checkExitFade(wisp, *profile);
        if (wisp.age > wisp.decayStart + wisp.fadeDuration + wisp.exitFadeDuration + 10.0f) {
            wisp.alive = false;
            continue;
        }

        for (int i = 1; i < wisp.pointCount; ++i) {
            wisp.x[static_cast<size_t>(i)] +=
                (wisp.x[static_cast<size_t>(i - 1)] - wisp.x[static_cast<size_t>(i)]) * profile->tailFollow * kTick;
            wisp.y[static_cast<size_t>(i)] +=
                (wisp.y[static_cast<size_t>(i - 1)] - wisp.y[static_cast<size_t>(i)]) * profile->tailFollow * kTick;
            const float phase = wisp.age * wisp.ripple + static_cast<float>(i) * 2.05f + wisp.phase;
            const float orbitR =
                std::sin(phase) * profile->segRipple + std::cos(phase * 1.37f) * profile->segSway;
            const float orbitA = wisp.heading + static_cast<float>(i) * profile->segOrbit + std::sin(phase * 0.8f) * 0.6f;
            wisp.x[static_cast<size_t>(i)] += std::cos(orbitA) * orbitR * kTick * profile->segScale;
            wisp.y[static_cast<size_t>(i)] += std::sin(orbitA) * orbitR * kTick * profile->segScale;
        }
        smoothWispPoints(wisp, profile->smoothStrength);

        if (wisp.age > 0.5f && wisp.age < wisp.decayStart * 0.92f) {
            if (rngUnit() < profile->fragmentChance) {
                emitFragment(wisp, static_cast<int>(rngRange(1.0f, static_cast<float>(wisp.pointCount - 1))), *profile);
            }
            if (rngUnit() < profile->breakChance) {
                const int idx = static_cast<int>(rngRange(2.0f, static_cast<float>(wisp.pointCount - 1)));
                emitFragment(wisp, idx, *profile);
                emitFragment(wisp, std::max(1, idx - 1), *profile);
            }
        }
    }

    for (int f = static_cast<int>(fragments_.size()) - 1; f >= 0; --f) {
        WispFragment& fragment = fragments_[static_cast<size_t>(f)];
        const WispProfile* profile = profileById(fragment.profileId);
        if (!profile) {
            fragments_.erase(fragments_.begin() + f);
            continue;
        }
        fragment.age += kTick;
        if (fragmentAlpha(fragment, *profile) <= 0.002f) {
            fragments_.erase(fragments_.begin() + f);
            continue;
        }
        fragment.x += fragment.vx * kTick;
        fragment.y += fragment.vy * kTick;
        fragment.vx += rngRange(-22.0f, 22.0f) * kTick;
        fragment.vy += rngRange(-22.0f, 22.0f) * kTick;
        fragment.vx += std::cos(fragment.flowHeading) * rngRange(8.0f, 18.0f) * kTick;
        fragment.vx *= 0.988f;
        fragment.vy *= 0.988f;
        fragment.x += std::sin(fragment.age * fragment.wiggle * kPi * 2.0f) * 12.0f * kTick;
        fragment.y += std::cos(fragment.age * fragment.wiggle * 1.4f) * 10.0f * kTick;
    }

    if (++compactCounter_ >= 30) {
        compactCounter_ = 0;
        compactDead();
    }
}

void WispSimulator::compactDead() {
    wisps_.erase(std::remove_if(wisps_.begin(), wisps_.end(),
                                [](const WispEntity& wisp) { return !wisp.alive; }),
                 wisps_.end());
}

}  // namespace totk::ui
