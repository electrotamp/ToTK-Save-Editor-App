#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace totk::ui {

struct WispProfile {
    std::string id;
    std::string name;

    bool enabled = true;
    float spawnWeight = 0.25f;
    bool crosses = true;

    int maxPerBand = 6;
    int maxFragments = 56;
    float spawnCooldownMin = 0.25f;
    float spawnCooldownMax = 0.75f;

    bool spawnTopBand = true;
    bool spawnBottomBand = true;
    bool topBandLeftToRight = true;
    bool bottomBandLeftToRight = false;
    float topBandEnd = 0.25f;
    float bottomBandStart = 0.75f;

    float spawnPadY = 0.12f;
    float bandSoftPad = 0.10f;
    float bandHardPad = 0.08f;
    float bandSpring = 22.0f;
    float speedMultiplier = 1.0f;
    float driftVerticalRatio = 0.10f;
    float exitMargin = 36.0f;
    float exitFadeMin = 2.8f;
    float exitFadeMax = 4.5f;

    int colorR = 0;
    int colorG = 255;
    int colorB = 187;

    float fadeIn = 0.75f;
    float tailAlphaMin = 0.18f;
    float tailAlphaPower = 1.8f;
    float tailWidthMin = 0.32f;
    int curveSubdiv = 8;
    float haloBlur = 0.55f;
    float haloAlpha = 0.26f;
    float coreAlpha = 0.90f;
    float whiteHighlight = 0.15f;
    float whiteFrom = 0.68f;
    float widthMin = 5.4f;
    float widthMax = 10.2f;
    float wobbleFreqMin = 1.6f;
    float wobbleFreqMax = 3.2f;
    float rippleFreqMin = 2.6f;
    float rippleFreqMax = 4.8f;
    float fragmentLifeMin = 0.9f;
    float fragmentLifeMax = 2.2f;
    float fragmentFadeIn = 0.4f;
    float spinMutateChance = 0.012f;
    float spinMutateAmt = 0.3f;

    int pointCount = 7;
    float tailLenMin = 16.0f;
    float tailLenMax = 32.0f;
    float speedMin = 100.0f;
    float speedMax = 112.0f;
    float speedClampMin = 98.0f;
    float speedClampMax = 114.0f;
    float speedJitter = 28.0f;
    float decayStartMin = 33.0f;
    float decayStartMax = 42.0f;
    float fadeDurMin = 5.4f;
    float fadeDurMax = 8.4f;
    float driftPush = 38.0f;
    float spinMin = -1.35f;
    float spinMax = 1.35f;
    float wobbleScale = 0.32f;
    float wobbleAmpA = 24.0f;
    float wobbleAmpB = 12.0f;
    float steerMax = 2.3f;
    float rippleHeading = 1.15f;
    float wobbleHeading = 0.8f;
    float chaosHeading = 1.15f;
    float headingKickChance = 0.032f;
    float headingKick = 1.5f;
    float piJumpChance = 0.006f;
    float piJumpMag = 0.18f;
    float segRipple = 14.0f;
    float segSway = 10.0f;
    float segScale = 2.1f;
    float segOrbit = 0.72f;
    float tailFollow = 2.8f;
    float smoothStrength = 0.17f;
    float fragmentChance = 0.11f;
    float breakChance = 0.018f;

    bool circleEnabled = false;
    float circleZoneStart = 0.333f;
    float circleZoneWidth = 0.333f;
    float circleApproach = 0.08f;
    float circleCenterX = 0.5f;
    float circleCenterY = 0.5f;
    bool circlePerfect = true;
    float circleRadiusX = 120.0f;
    float circleRadiusY = 120.0f;
    float circleAngularSpeed = 1.1f;
    float circlePull = 42.0f;
    float circleBlendTime = 1.4f;
};

class ZonaiWispSettings {
public:
    static ZonaiWispSettings& instance();

    bool loadFromRomfs();
    bool loadWispsDocument(const nlohmann::json& data);
    bool loaded() const { return loaded_; }

    const std::vector<WispProfile>& profiles() const { return profiles_; }
    uint32_t simSeed() const { return simSeed_; }

    const WispProfile* profileById(const std::string& id) const;
    size_t profileIndexFor(const std::string& id) const;

    static WispProfile defaultProfile();

private:
    bool loaded_ = false;
    uint32_t simSeed_ = 0xC0FFEE42;
    std::vector<WispProfile> profiles_;
    std::unordered_map<std::string, size_t> profileIndex_;
};

}  // namespace totk::ui
