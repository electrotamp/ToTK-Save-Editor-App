#include "ui/zonai_wisp_settings.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

#include "util/resource_loader.hpp"
#include "util/totk_log.hpp"

namespace totk::ui {

namespace {

template <typename T>
void readField(const nlohmann::json& json, const char* key, T& out) {
    if (json.contains(key) && !json[key].is_null()) {
        out = json[key].get<T>();
    }
}

void readBoolField(const nlohmann::json& json, const char* key, bool& out) {
    if (!json.contains(key) || json[key].is_null()) return;
    if (json[key].is_boolean()) {
        out = json[key].get<bool>();
    } else {
        out = json[key].get<int>() != 0;
    }
}

void overlayJsonProfile(const nlohmann::json& json, WispProfile& profile) {
    readField(json, "id", profile.id);
    readField(json, "name", profile.name);
    readBoolField(json, "enabled", profile.enabled);
    readField(json, "spawnWeight", profile.spawnWeight);
    readBoolField(json, "crosses", profile.crosses);
    readField(json, "maxPerBand", profile.maxPerBand);
    readField(json, "maxFragments", profile.maxFragments);
    readField(json, "spawnCooldownMin", profile.spawnCooldownMin);
    readField(json, "spawnCooldownMax", profile.spawnCooldownMax);
    readBoolField(json, "spawnTopBand", profile.spawnTopBand);
    readBoolField(json, "spawnBottomBand", profile.spawnBottomBand);
    readBoolField(json, "topBandLeftToRight", profile.topBandLeftToRight);
    readBoolField(json, "bottomBandLeftToRight", profile.bottomBandLeftToRight);
    readField(json, "topBandEnd", profile.topBandEnd);
    readField(json, "bottomBandStart", profile.bottomBandStart);
    readField(json, "spawnPadY", profile.spawnPadY);
    readField(json, "bandSoftPad", profile.bandSoftPad);
    readField(json, "bandHardPad", profile.bandHardPad);
    readField(json, "bandSpring", profile.bandSpring);
    readField(json, "speedMultiplier", profile.speedMultiplier);
    readField(json, "driftVerticalRatio", profile.driftVerticalRatio);
    readField(json, "exitMargin", profile.exitMargin);
    readField(json, "exitFadeMin", profile.exitFadeMin);
    readField(json, "exitFadeMax", profile.exitFadeMax);
    readField(json, "colorR", profile.colorR);
    readField(json, "colorG", profile.colorG);
    readField(json, "colorB", profile.colorB);
    readField(json, "fadeIn", profile.fadeIn);
    readField(json, "tailAlphaMin", profile.tailAlphaMin);
    readField(json, "tailAlphaPower", profile.tailAlphaPower);
    readField(json, "tailWidthMin", profile.tailWidthMin);
    readField(json, "curveSubdiv", profile.curveSubdiv);
    readField(json, "haloBlur", profile.haloBlur);
    readField(json, "haloAlpha", profile.haloAlpha);
    readField(json, "coreAlpha", profile.coreAlpha);
    readField(json, "whiteHighlight", profile.whiteHighlight);
    readField(json, "whiteFrom", profile.whiteFrom);
    readField(json, "widthMin", profile.widthMin);
    readField(json, "widthMax", profile.widthMax);
    readField(json, "wobbleFreqMin", profile.wobbleFreqMin);
    readField(json, "wobbleFreqMax", profile.wobbleFreqMax);
    readField(json, "rippleFreqMin", profile.rippleFreqMin);
    readField(json, "rippleFreqMax", profile.rippleFreqMax);
    readField(json, "fragmentLifeMin", profile.fragmentLifeMin);
    readField(json, "fragmentLifeMax", profile.fragmentLifeMax);
    readField(json, "fragmentFadeIn", profile.fragmentFadeIn);
    readField(json, "spinMutateChance", profile.spinMutateChance);
    readField(json, "spinMutateAmt", profile.spinMutateAmt);
    readField(json, "pointCount", profile.pointCount);
    readField(json, "tailLenMin", profile.tailLenMin);
    readField(json, "tailLenMax", profile.tailLenMax);
    readField(json, "speedMin", profile.speedMin);
    readField(json, "speedMax", profile.speedMax);
    readField(json, "speedClampMin", profile.speedClampMin);
    readField(json, "speedClampMax", profile.speedClampMax);
    readField(json, "speedJitter", profile.speedJitter);
    readField(json, "decayStartMin", profile.decayStartMin);
    readField(json, "decayStartMax", profile.decayStartMax);
    readField(json, "fadeDurMin", profile.fadeDurMin);
    readField(json, "fadeDurMax", profile.fadeDurMax);
    readField(json, "driftPush", profile.driftPush);
    readField(json, "spinMin", profile.spinMin);
    readField(json, "spinMax", profile.spinMax);
    readField(json, "wobbleScale", profile.wobbleScale);
    readField(json, "wobbleAmpA", profile.wobbleAmpA);
    readField(json, "wobbleAmpB", profile.wobbleAmpB);
    readField(json, "steerMax", profile.steerMax);
    readField(json, "rippleHeading", profile.rippleHeading);
    readField(json, "wobbleHeading", profile.wobbleHeading);
    readField(json, "chaosHeading", profile.chaosHeading);
    readField(json, "headingKickChance", profile.headingKickChance);
    readField(json, "headingKick", profile.headingKick);
    readField(json, "piJumpChance", profile.piJumpChance);
    readField(json, "piJumpMag", profile.piJumpMag);
    readField(json, "segRipple", profile.segRipple);
    readField(json, "segSway", profile.segSway);
    readField(json, "segScale", profile.segScale);
    readField(json, "segOrbit", profile.segOrbit);
    readField(json, "tailFollow", profile.tailFollow);
    readField(json, "smoothStrength", profile.smoothStrength);
    readField(json, "fragmentChance", profile.fragmentChance);
    readField(json, "breakChance", profile.breakChance);
    readBoolField(json, "circleEnabled", profile.circleEnabled);
    readField(json, "circleZoneStart", profile.circleZoneStart);
    readField(json, "circleZoneWidth", profile.circleZoneWidth);
    readField(json, "circleApproach", profile.circleApproach);
    readField(json, "circleCenterX", profile.circleCenterX);
    readField(json, "circleCenterY", profile.circleCenterY);
    readBoolField(json, "circlePerfect", profile.circlePerfect);
    readField(json, "circleRadiusX", profile.circleRadiusX);
    readField(json, "circleRadiusY", profile.circleRadiusY);
    readField(json, "circleAngularSpeed", profile.circleAngularSpeed);
    readField(json, "circlePull", profile.circlePull);
    readField(json, "circleBlendTime", profile.circleBlendTime);

    if (json.contains("circleRadius") && !json.contains("circleRadiusX")) {
        const float radius = json["circleRadius"].get<float>();
        profile.circleRadiusX = radius;
        profile.circleRadiusY = radius;
    }
}

WispProfile finalizeProfile(const nlohmann::json& json) {
    WispProfile profile = ZonaiWispSettings::defaultProfile();
    overlayJsonProfile(json, profile);
    if (profile.id.empty()) profile.id = "profile";
    if (profile.name.empty()) profile.name = profile.id;
    profile.pointCount = std::max(3, profile.pointCount);
    if (profile.circlePerfect) {
        profile.circleRadiusY = profile.circleRadiusX;
    }
    return profile;
}

}  // namespace

ZonaiWispSettings& ZonaiWispSettings::instance() {
    static ZonaiWispSettings settings;
    return settings;
}

WispProfile ZonaiWispSettings::defaultProfile() {
    return WispProfile{};
}

const WispProfile* ZonaiWispSettings::profileById(const std::string& id) const {
    const auto it = profileIndex_.find(id);
    if (it == profileIndex_.end()) return nullptr;
    return &profiles_[it->second];
}

size_t ZonaiWispSettings::profileIndexFor(const std::string& id) const {
    const auto it = profileIndex_.find(id);
    return it == profileIndex_.end() ? 0 : it->second;
}

bool ZonaiWispSettings::loadFromRomfs() {
    try {
        const auto raw = totk::loadResourceText("data/zonai_wisps.json");
        return loadWispsDocument(nlohmann::json::parse(raw));
    } catch (const std::exception& ex) {
        TOTK_LOG("zonai wisps: load failed — %s", ex.what());
        return false;
    } catch (...) {
        TOTK_LOG("zonai wisps: load failed — unknown error");
        return false;
    }
}

bool ZonaiWispSettings::loadWispsDocument(const nlohmann::json& data) {
    profiles_.clear();
    profileIndex_.clear();
    loaded_ = false;

    if (!data.contains("profiles") || !data["profiles"].is_array() || data["profiles"].empty()) {
        TOTK_LOG("zonai wisps: invalid JSON — missing profiles[]");
        return false;
    }

    const int version = data.value("version", 1);
    const auto& legacyGlobal = data.contains("global") ? data["global"] : nlohmann::json::object();
    const auto& legacyRender = data.contains("render") ? data["render"] : nlohmann::json::object();
    const bool needsMigration = version < 2 || data.contains("render");

    if (data.contains("meta") && data["meta"].contains("simSeed")) {
        simSeed_ = data["meta"]["simSeed"].get<uint32_t>();
    }

    for (const auto& profileJson : data["profiles"]) {
        nlohmann::json merged = profileJson;
        if (needsMigration) {
            for (auto it = legacyGlobal.begin(); it != legacyGlobal.end(); ++it) {
                if (!merged.contains(it.key())) merged[it.key()] = it.value();
            }
            for (auto it = legacyRender.begin(); it != legacyRender.end(); ++it) {
                if (!merged.contains(it.key())) merged[it.key()] = it.value();
            }
            if (!merged.contains("maxPerBand") && legacyGlobal.contains("maxWispsPerBand")) {
                merged["maxPerBand"] = legacyGlobal["maxWispsPerBand"];
            }
            if (!merged.contains("maxFragments") && legacyGlobal.contains("maxFragments")) {
                merged["maxFragments"] = legacyGlobal["maxFragments"];
            }
            if (legacyRender.contains("boltSubdiv") && merged.value("id", "") == "bolt" &&
                !merged.contains("curveSubdiv")) {
                merged["curveSubdiv"] = legacyRender["boltSubdiv"];
            }
        }

        WispProfile profile = finalizeProfile(merged);
        profileIndex_[profile.id] = profiles_.size();
        profiles_.push_back(std::move(profile));
    }

    loaded_ = !profiles_.empty();
    TOTK_LOG("zonai wisps: loaded %zu profiles seed=0x%x", profiles_.size(), simSeed_);
    return loaded_;
}

}  // namespace totk::ui
