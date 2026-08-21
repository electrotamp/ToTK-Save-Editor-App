#include "save/item_enums.hpp"

#include "save/murmur_hash.hpp"

namespace totk {

namespace {

std::vector<ItemEnumOption> makeOptions(std::initializer_list<const char*> names) {
    std::vector<ItemEnumOption> options;
    options.reserve(names.size());
    for (const char* name : names) {
        options.push_back({murmurHash3(name), name});
    }
    return options;
}

}  // namespace

const std::vector<ItemEnumOption>& ItemEnums::weaponModifiers() {
    static const std::vector<ItemEnumOption> kOptions = makeOptions(
        {"None", "AttackUp", "AttackUpPlus", "DurabilityUp", "DurabilityUpPlus", "FinishBlow", "LongThrow"});
    return kOptions;
}

const std::vector<ItemEnumOption>& ItemEnums::bowModifiers() {
    static const std::vector<ItemEnumOption> kOptions =
        makeOptions({"None", "AttackUp", "AttackUpPlus", "DurabilityUp", "DurabilityUpPlus", "RapidFire", "FiveWay"});
    return kOptions;
}

const std::vector<ItemEnumOption>& ItemEnums::shieldModifiers() {
    static const std::vector<ItemEnumOption> kOptions =
        makeOptions({"None", "DurabilityUp", "DurabilityUpPlus", "GuardUp", "GuardUpPlus"});
    return kOptions;
}

const std::vector<ItemEnumOption>& ItemEnums::armorDyes() {
    static const std::vector<ItemEnumOption> kOptions = makeOptions(
        {"None", "Blue", "Red", "Yellow", "White", "Black", "Purple", "Green", "LightBlue", "Navy", "Orange", "Pink",
         "Crimson", "LightYellow", "Brown", "Gray"});
    return kOptions;
}

const std::vector<ItemEnumOption>& ItemEnums::foodEffects() {
    static const std::vector<ItemEnumOption> kOptions = makeOptions(
        {"None", "ResistHot", "ResistBurn", "ResistCold", "ResistElectric", "ResitLightning", "ResistFreeze",
         "ResistAncient", "SwimSpeedUp", "DecreaseSwimStamina", "SpinAttack", "ClimbWaterfall", "ClimbSpeedUp",
         "ClimbSpeedUpOnlyHorizontaly", "AttackUp", "AttackUpCold", "AttackUpHot", "AttackUpThunderstorm",
         "AttackUpDark", "AttackUpBone", "QuietnessUp", "SandMoveUp", "SnowMoveUp", "WakeWind", "TwiceJump",
         "EmergencyAvoid", "DefenseUp", "AllSpeed", "MiasmaGuard", "MaskBokoblin", "MaskMoriblin", "MaskLizalfos",
         "MaskLynel", "YigaDisguise", "StalDisguise", "LifeRecover", "LifeMaxUp", "StaminaRecover", "ExStaminaMaxUp",
         "LifeRepair", "DivingMobilityUp", "NotSlippy", "Moisturizing", "LightEmission", "RupeeGuard", "FallResist",
         "SwordBeamUp", "VisualizeLife", "NightMoveSpeedUp", "NightGlow", "DecreaseWallJumpStamina",
         "DecreaseChargeAttackStamina", "EmitTerror", "NoBurning", "NoFallDamage", "NoSlip", "RupeeGuardRate",
         "MaskAll", "DecreaseZonauEnergy", "ZonauEnergyHealUp", "MaskHorablin", "MiasmaDefenseUp", "ChargePowerUpCold",
         "ChargePowerUpHot", "ChargePowerUpThunderstorm", "LightFootprint", "SoulPowerUpLightning", "SoulPowerUpWater",
         "SoulPowerUpWind", "SoulPowerUpFire", "SoulPowerUpSpirit", "EnableUseSwordBeam"});
    return kOptions;
}

int ItemEnums::indexForValue(const std::vector<ItemEnumOption>& options, uint32_t value) {
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i].value == value) return static_cast<int>(i);
    }
    return 0;
}

std::string ItemEnums::labelForValue(const std::vector<ItemEnumOption>& options, uint32_t value) {
    for (const auto& option : options) {
        if (option.value == value) return option.label;
    }
    return {};
}

std::vector<std::string> ItemEnums::labelsFor(const std::vector<ItemEnumOption>& options) {
    std::vector<std::string> labels;
    labels.reserve(options.size());
    for (const auto& option : options) {
        labels.push_back(option.label);
    }
    return labels;
}

}  // namespace totk
