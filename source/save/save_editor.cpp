#include "save/caption_parser.hpp"
#include "save/completism_data.hpp"
#include "save/armor_upgrades.hpp"
#include "save/game_limits.hpp"
#include "save/horse_defaults.hpp"
#include "save/save_editor.hpp"

#include "save/murmur_hash.hpp"
#include "util/totk_log.hpp"
#include "util/perf_trace.hpp"
#include "util/debug_stage.hpp"

#if defined(__SWITCH__)
#include "platform/switch_save_mount.hpp"
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

bool pathExists(const std::string& path) {
#if defined(__SWITCH__)
    return access(path.c_str(), F_OK) == 0;
#else
    return fs::exists(path);
#endif
}

}  // namespace

namespace totk {

const std::vector<GameVersion> SaveEditor::kGameVersions = {
    {"v1.0", 2307552, 0x0046c3c8, 0x0003c050},
    {"v1.1.x/v1.2.x", 2307656, 0x0047e0f4, 0x0003c088},
    {"v1.4.x", 2307856, 0x0049e946, 0x0003c138},
};

const std::vector<SaveEditor::HashEntry> SaveEditor::kCoreHashes = {
    {0xfbe01da1, "PlayerStatus.MaxLife", false},
    {0xa77921d7, "PlayerStatus.CurrentRupee", false},
    {0xf9212c74, "PlayerStatus.MaxStamina", false},
    {0x15ec5858, "HorseInnMemberPoint", false},
    {0xe573f564, "Playtime", false},
    {0xafd01d68, "PlayerStatus.MaxEnergy", false},
    {0xc884818d, "PlayerStatus.SavePos", true},
    {0x1d6189da, "Sequence_CurrentBanc", true},
    {0xd7a3f6ba, "Pouch.Weapon.ValidNum", true},
    {0xc61785c2, "Pouch.Bow.ValidNum", true},
    {0x05271e7d, "Pouch.Shield.ValidNum", true},
    {0x14d7f4c4, "MapData.IconData.StampData.Type", true},
    {0xf24fc2e7, "MapData.IconData.StampData.Pos", true},
    {0xd2025694, "MapData.IconData.StampData.Layer", true},
    {0xd27f8651, "AutoBuilder.Draft.Content.Index", true},
    {0xa56722b6, "AutoBuilder.Draft.Content.CombinedActorInfo", true},
    {0xc5bf2815, "AutoBuilder.Draft.Content.CameraPos", true},
    {0xef74dca7, "AutoBuilder.Draft.Content.CameraAt", true},
    {0x67f4b46b, "AutoBuilder.Draft.Content.IsFavorite", true, /*optional=*/true},
};

SaveEditor::SaveEditor() = default;

std::string SaveEditor::defaultSaveRoot() {
#if defined(__SWITCH__)
    return {};
#else
    return "sdmc:/switch/totk-save-editor/saves";
#endif
}

bool SaveEditor::loadProgress(const std::string& path) {
    TOTK_PERF_SCOPE("save.loadProgress");
    try {
        {
            TOTK_PERF_SCOPE("save.loadFile");
            if (!file_.loadFromFile(path)) return false;
        }
        loadedPath_ = path;
        vars_ = std::make_unique<VariableStore>(file_);
        vars_->resetCache();
        {
            TOTK_PERF_SCOPE("save.validateAndIndex");
            if (!validateAndIndex()) return false;
        }
        {
            TOTK_PERF_SCOPE("save.reloadFromFile");
            reloadFromFile();
        }
        loaded_ = true;
        return true;
    } catch (const std::exception& ex) {
#if defined(__SWITCH__)
        std::fprintf(stderr, "SaveEditor::loadProgress failed: %s\n", ex.what());
#endif
        loaded_ = false;
        loadedPath_.clear();
        vars_.reset();
        return false;
    }
}

bool SaveEditor::saveProgress(const std::string& path) {
    if (!loaded_) return false;
    try {
        TOTK_LOG("saveProgress begin");
        applyStatsToFile();
        saveAllPouches();
        saveAutobuilder();
        const std::string outPath = path.empty() ? loadedPath_ : path;
        if (!file_.saveToFile(outPath)) {
            TOTK_LOG("saveProgress: write failed path=%s", outPath.c_str());
            return false;
        }
#if defined(__SWITCH__)
        if (outPath.rfind("totk:", 0) == 0) {
            if (!SwitchSaveMount::commit()) {
                TOTK_LOG("saveProgress: commit failed: %s", SwitchSaveMount::lastError().c_str());
                return false;
            }
        }
#endif
        TOTK_LOG("saveProgress ok path=%s rupees=%u", outPath.c_str(), stats_.rupees);
        return true;
    } catch (const std::exception& ex) {
        TOTK_LOG("saveProgress failed: %s", ex.what());
        return false;
    }
}

bool SaveEditor::validateAndIndex() {
    TOTK_STAGE("save.validate.magic");
    if (file_.readU32(0) != 0x01020304) return false;
    if (file_.size() < 2307552 || file_.size() >= 4194304) return false;

    TOTK_STAGE("save.validate.hashTableEnd");
    if (!vars_->findHashTableEnd()) return false;

    TOTK_STAGE("save.validate.resolveOffsets end=0x%zx", vars_->hashTableEnd());
    if (!resolveOffsets()) return false;

    TOTK_STAGE("save.validate.loadGuids guidsAt=0x%zx", guidsArrayOffset_);
    loadGuids();
    TOTK_STAGE("save.validate.guids=%zu", guids_.size());

    const uint32_t header = file_.readU32(4);
    const uint32_t meta = file_.readU32(8);
    gameMod_ = true;
    gameVersion_ = "*Game mod*";
    for (const auto& version : kGameVersions) {
        if (file_.size() == version.fileSize && header == version.header && meta == version.metaDataStart) {
            gameVersion_ = version.label;
            gameMod_ = false;
            break;
        }
    }
    TOTK_STAGE("save.validate.done version=%s", gameVersion_.c_str());
    return true;
}

bool SaveEditor::resolveOffsets() {
    offsets_.clear();
    for (size_t i = 0x28; i < vars_->hashTableEnd(); i += 8) {
        const uint32_t hash = file_.readU32(i);
        if (hash == VariableStore::kSaveTypeHash) {
            guidsArrayOffset_ = file_.readU32(i + 4);
            break;
        }
        for (const auto& entry : kCoreHashes) {
            if (entry.hash != hash) continue;
            offsets_[entry.name] = entry.isPointer ? file_.readU32(i + 4) : i + 4;
        }
    }
    for (const auto& entry : kCoreHashes) {
        if (entry.optional) continue;
        if (!offsets_.count(entry.name)) return false;
    }
    return true;
}

void SaveEditor::loadGuids() {
    guids_.clear();
    for (size_t i = guidsArrayOffset_; i + 8 <= file_.size(); i += 8) {
        const uint32_t lower = file_.readU32(i);
        const uint32_t upper = file_.readU32(i + 4);
        if (lower == 0 && upper == 0) break;
        guids_.push_back((static_cast<uint64_t>(upper) << 32) | lower);
    }
}

size_t SaveEditor::offsetFor(const std::string& key) const {
    return offsets_.at(key);
}

size_t SaveEditor::pointerFor(const std::string& key) const {
    return offsetFor(key);
}

static std::string formatPlaytime(uint32_t seconds) {
    const uint32_t hrs = seconds / 3600;
    const uint32_t mins = (seconds / 60) % 60;
    const uint32_t secs = seconds % 60;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hrs << ':'
        << std::setw(2) << mins << ':' << std::setw(2) << secs;
    return oss.str();
}

void SaveEditor::reloadFromFile() {
    TOTK_PERF_SCOPE("save.reloadFromFile.total");
    {
        TOTK_PERF_SCOPE("save.loadStats");
        loadStats();
    }
    {
        TOTK_PERF_SCOPE("save.loadPouches");
        loadPouches();
    }
    {
        TOTK_PERF_SCOPE("save.loadAutobuilder");
        loadAutobuilder();
    }
}

void SaveEditor::loadStats() {
    stats_.playtime = formatPlaytime(vars_->readUInt(offsetFor("Playtime")));
    stats_.rupees = vars_->readUInt(offsetFor("PlayerStatus.CurrentRupee"));
    stats_.maxHearts = vars_->readUInt(offsetFor("PlayerStatus.MaxLife"));
    stats_.maxStamina = vars_->readUInt(offsetFor("PlayerStatus.MaxStamina"));
    stats_.maxBattery = vars_->readFloat(offsetFor("PlayerStatus.MaxEnergy"));
    stats_.ponyPoints = vars_->readUInt(offsetFor("HorseInnMemberPoint"));
    stats_.pouchWeapons = vars_->readUIntArray(pointerFor("Pouch.Weapon.ValidNum")).front();
    stats_.pouchBows = vars_->readUIntArray(pointerFor("Pouch.Bow.ValidNum")).front();
    stats_.pouchShields = vars_->readUIntArray(pointerFor("Pouch.Shield.ValidNum")).front();
    stats_.savePos = vars_->readVector3(offsetFor("PlayerStatus.SavePos"));

    const auto pinTypes = vars_->readUIntArray(pointerFor("MapData.IconData.StampData.Type"));
    stats_.mapPinCount = static_cast<int>(std::count_if(pinTypes.begin(), pinTypes.end(), [](uint32_t v) { return v != 0; }));
    GameLimits::clampPlayerStats(stats_);
}

void SaveEditor::loadPouches() {
    equipment_["weapons"].clear();
    equipment_["bows"].clear();
    equipment_["shields"].clear();
    stackItems_["arrows"].clear();
    stackItems_["materials"].clear();
    stackItems_["food"].clear();
    stackItems_["devices"].clear();
    stackItems_["key"].clear();
    armors_.clear();
    horses_.clear();

    const auto weaponIds = vars_->readString64Array(vars_->resolveArrayPointer("Pouch.Weapon.Content.Name"));
    const auto weaponDur = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Weapon.Content.Life"));
    const auto weaponMod = vars_->readUIntArray(vars_->resolveArrayPointer("Pouch.Weapon.Content.Effect.Type"));
    const auto weaponModVal = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Weapon.Content.Effect.Value"));
    const auto weaponFuse = vars_->readString64Array(vars_->resolveArrayPointer("Pouch.Weapon.Content.Combined.Name"));
    for (size_t i = 0; i < weaponIds.size(); ++i) {
        if (weaponIds[i].empty()) break;
        EquipmentItem item;
        item.category = "weapons";
        item.id = weaponIds[i];
        item.durability = i < weaponDur.size() ? weaponDur[i] : 70;
        item.modifier = i < weaponMod.size() ? weaponMod[i] : 0;
        item.modifierValue = i < weaponModVal.size() ? weaponModVal[i] : 0;
        item.fuseId = i < weaponFuse.size() ? weaponFuse[i] : "";
        equipment_["weapons"].push_back(item);
    }

    const auto arrowIds = vars_->readString64Array(vars_->resolveArrayPointer("Pouch.Arrow.Content.Name"));
    const auto arrowCount = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Arrow.Content.StockNum"));
    for (size_t i = 0; i < arrowIds.size(); ++i) {
        if (arrowIds[i].empty()) break;
        StackItem item;
        item.category = "arrows";
        item.id = arrowIds[i];
        item.count = i < arrowCount.size() ? arrowCount[i] : 1;
        stackItems_["arrows"].push_back(item);
    }

    const auto bowIds = vars_->readString64Array(vars_->resolveArrayPointer("Pouch.Bow.Content.Name"));
    const auto bowDur = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Bow.Content.Life"));
    const auto bowMod = vars_->readUIntArray(vars_->resolveArrayPointer("Pouch.Bow.Content.Effect.Type"));
    const auto bowModVal = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Bow.Content.Effect.Value"));
    for (size_t i = 0; i < bowIds.size(); ++i) {
        if (bowIds[i].empty()) break;
        EquipmentItem item;
        item.category = "bows";
        item.id = bowIds[i];
        item.durability = i < bowDur.size() ? bowDur[i] : 70;
        item.modifier = i < bowMod.size() ? bowMod[i] : 0;
        item.modifierValue = i < bowModVal.size() ? bowModVal[i] : 0;
        equipment_["bows"].push_back(item);
    }

    const auto shieldIds = vars_->readString64Array(vars_->resolveArrayPointer("Pouch.Shield.Content.Name"));
    const auto shieldDur = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Shield.Content.Life"));
    const auto shieldMod = vars_->readUIntArray(vars_->resolveArrayPointer("Pouch.Shield.Content.Effect.Type"));
    const auto shieldModVal = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Shield.Content.Effect.Value"));
    const auto shieldFuse = vars_->readString64Array(vars_->resolveArrayPointer("Pouch.Shield.Content.Combined.Name"));
    for (size_t i = 0; i < shieldIds.size(); ++i) {
        if (shieldIds[i].empty()) break;
        EquipmentItem item;
        item.category = "shields";
        item.id = shieldIds[i];
        item.durability = i < shieldDur.size() ? shieldDur[i] : 70;
        item.modifier = i < shieldMod.size() ? shieldMod[i] : 0;
        item.modifierValue = i < shieldModVal.size() ? shieldModVal[i] : 0;
        item.fuseId = i < shieldFuse.size() ? shieldFuse[i] : "";
        equipment_["shields"].push_back(item);
    }

    const auto armorIds = vars_->readString64Array(vars_->resolveArrayPointer("Pouch.Armor.Content.Name"));
    const auto armorDye = vars_->readUIntArray(vars_->resolveArrayPointer("Pouch.Armor.Content.ColorVariation"));
    for (size_t i = 0; i < armorIds.size(); ++i) {
        if (armorIds[i].empty()) break;
        ArmorItem item;
        item.id = armorIds[i];
        item.dyeColor = i < armorDye.size() ? armorDye[i] : 0;
        armors_.push_back(item);
    }

    const auto matIds = vars_->readString64Array(vars_->resolveArrayPointer("Pouch.Material.Content.Name"));
    const auto matCount = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Material.Content.StockNum"));
    for (size_t i = 0; i < matIds.size(); ++i) {
        if (matIds[i].empty()) break;
        StackItem item;
        item.category = "materials";
        item.id = matIds[i];
        item.count = i < matCount.size() ? matCount[i] : 1;
        stackItems_["materials"].push_back(item);
    }

    const auto foodIds = vars_->readString64Array(vars_->resolveArrayPointer("Pouch.Food.Content.Name"));
    const auto foodCount = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Food.Content.StockNum"));
    const auto foodHearts = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Food.Content.LifeRecover"));
    const auto foodPrice = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Food.Content.Price"));
    const auto foodEffect = vars_->readUIntArray(vars_->resolveArrayPointer("Pouch.Food.Content.Effect.Type"));
    const auto foodEffectLevel = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Food.Content.Effect.Level"));
    const auto foodEffectTime = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.Food.Content.Effect.Time"));
    for (size_t i = 0; i < foodIds.size(); ++i) {
        if (foodIds[i].empty()) break;
        StackItem item;
        item.category = "food";
        item.id = foodIds[i];
        item.count = i < foodCount.size() ? foodCount[i] : 1;
        item.hearts = i < foodHearts.size() ? foodHearts[i] : 0;
        item.price = i < foodPrice.size() ? foodPrice[i] : 1;
        item.effect = i < foodEffect.size() ? foodEffect[i] : 0;
        item.effectLevel = i < foodEffectLevel.size() ? foodEffectLevel[i] : 0;
        item.effectDuration = i < foodEffectTime.size() ? foodEffectTime[i] : 0;
        stackItems_["food"].push_back(item);
    }

    const auto keyIds = vars_->readString64Array(vars_->resolveArrayPointer("Pouch.KeyItem.Content.Name"));
    const auto keyCount = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.KeyItem.Content.StockNum"));
    for (size_t i = 0; i < keyIds.size(); ++i) {
        if (keyIds[i].empty()) break;
        StackItem item;
        item.category = "key";
        item.id = keyIds[i];
        item.count = i < keyCount.size() ? keyCount[i] : 1;
        stackItems_["key"].push_back(item);
    }

    const auto deviceIds = vars_->readString64Array(vars_->resolveArrayPointer("Pouch.SpecialParts.Content.Name"));
    const auto deviceCount = vars_->readIntArray(vars_->resolveArrayPointer("Pouch.SpecialParts.Content.StockNum"));
    for (size_t i = 0; i < deviceIds.size(); ++i) {
        if (deviceIds[i].empty()) break;
        StackItem item;
        item.category = "devices";
        item.id = deviceIds[i];
        item.count = i < deviceCount.size() ? deviceCount[i] : 1;
        stackItems_["devices"].push_back(item);
    }

    try {
        const auto horseIds = vars_->readString64Array(vars_->resolveArrayPointer("OwnedHorseList.ActorName"));
        const auto horseNames = vars_->readWString16Array(vars_->resolveArrayPointer("OwnedHorseList.Name"));
        const auto horseBond = vars_->readFloatArray(vars_->resolveArrayPointer("OwnedHorseList.Familiarity"));
        const auto horseStrength = vars_->readIntArray(vars_->resolveArrayPointer("OwnedHorseList.Toughness"));
        const auto horseSpeed = vars_->readIntArray(vars_->resolveArrayPointer("OwnedHorseList.Speed"));
        const auto horseStamina = vars_->readIntArray(vars_->resolveArrayPointer("OwnedHorseList.ChargeNum"));
        const auto horsePull = vars_->readIntArray(vars_->resolveArrayPointer("OwnedHorseList.HorsePower"));
        const auto horseColor = vars_->readIntArray(vars_->resolveArrayPointer("OwnedHorseList.ColorType"));
        const auto horseFoot = vars_->readIntArray(vars_->resolveArrayPointer("OwnedHorseList.FootType"));
        const auto horseMane = vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Mane"));
        const auto horseSaddle = vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Saddle"));
        const auto horseReins = vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Rein"));
        const auto horseType = vars_->readIntArray(vars_->resolveArrayPointer("OwnedHorseList.HorseType"));
        const auto horseBondChecked =
            vars_->readBoolArray(vars_->resolveArrayPointer("OwnedHorseList.IsFamiliarityChecked"));
        const auto horsePattern = vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Body.Pattern"));
        const auto horseEyeColor = vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Body.EyeColor"));
        const auto horsePrimaryRed =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Body.PrimaryColor.Red"));
        const auto horsePrimaryGreen =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Body.PrimaryColor.Green"));
        const auto horsePrimaryBlue =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Body.PrimaryColor.Blue"));
        const auto horseSecondaryRed =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Body.SecondaryColor.Red"));
        const auto horseSecondaryGreen =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Body.SecondaryColor.Green"));
        const auto horseSecondaryBlue =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Body.SecondaryColor.Blue"));
        const auto horseNoseRed = vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Body.NoseColor.Red"));
        const auto horseNoseGreen =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Body.NoseColor.Green"));
        const auto horseNoseBlue = vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Body.NoseColor.Blue"));
        const auto horseHairPrimaryRed =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Hair.PrimaryColor.Red"));
        const auto horseHairPrimaryGreen =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Hair.PrimaryColor.Green"));
        const auto horseHairPrimaryBlue =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Hair.PrimaryColor.Blue"));
        const auto horseHairSecondaryRed =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Hair.SecondaryColor.Red"));
        const auto horseHairSecondaryGreen =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Hair.SecondaryColor.Green"));
        const auto horseHairSecondaryBlue =
            vars_->readUIntArray(vars_->resolveArrayPointer("OwnedHorseList.Hair.SecondaryColor.Blue"));
        const auto horseUid = vars_->readUInt64Array(vars_->resolveArrayPointer("OwnedHorseList.UidHash"));
        const auto horseRoom = vars_->readIntArray(vars_->resolveArrayPointer("OwnedHorseList.RoomID"));
        for (size_t i = 0; i < horseIds.size(); ++i) {
            if (horseIds[i].empty()) break;
            HorseItem item;
            item.id = horseIds[i];
            item.name = i < horseNames.size() ? horseNames[i] : "";
            item.bond = i < horseBond.size() ? horseBond[i] : 0.0f;
            item.bondChecked = i < horseBondChecked.size() ? horseBondChecked[i] : false;
            item.strength = i < horseStrength.size() ? horseStrength[i] : 100;
            item.speed = i < horseSpeed.size() ? horseSpeed[i] : 1;
            item.stamina = i < horseStamina.size() ? horseStamina[i] : 1;
            item.pull = i < horsePull.size() ? horsePull[i] : 1;
            item.colorType = i < horseColor.size() ? static_cast<uint32_t>(horseColor[i]) : 1u;
            item.footType = i < horseFoot.size() ? static_cast<uint32_t>(horseFoot[i]) : 0u;
            item.mane = i < horseMane.size() ? horseMane[i] : 0u;
            item.saddle = i < horseSaddle.size() ? horseSaddle[i] : 0u;
            item.reins = i < horseReins.size() ? horseReins[i] : 0u;
            item.horseType = i < horseType.size() ? static_cast<uint32_t>(horseType[i]) : 1u;
            item.iconPattern = i < horsePattern.size() ? horsePattern[i] : 0u;
            item.iconEyeColor = i < horseEyeColor.size() ? horseEyeColor[i] : 0u;
            item.iconPrimaryColorRed = i < horsePrimaryRed.size() ? horsePrimaryRed[i] : 0u;
            item.iconPrimaryColorGreen = i < horsePrimaryGreen.size() ? horsePrimaryGreen[i] : 0u;
            item.iconPrimaryColorBlue = i < horsePrimaryBlue.size() ? horsePrimaryBlue[i] : 0u;
            item.iconSecondaryColorRed = i < horseSecondaryRed.size() ? horseSecondaryRed[i] : 0u;
            item.iconSecondaryColorGreen = i < horseSecondaryGreen.size() ? horseSecondaryGreen[i] : 0u;
            item.iconSecondaryColorBlue = i < horseSecondaryBlue.size() ? horseSecondaryBlue[i] : 0u;
            item.iconNoseColorRed = i < horseNoseRed.size() ? horseNoseRed[i] : 0u;
            item.iconNoseColorGreen = i < horseNoseGreen.size() ? horseNoseGreen[i] : 0u;
            item.iconNoseColorBlue = i < horseNoseBlue.size() ? horseNoseBlue[i] : 0u;
            item.iconHairPrimaryColorRed = i < horseHairPrimaryRed.size() ? horseHairPrimaryRed[i] : 0u;
            item.iconHairPrimaryColorGreen = i < horseHairPrimaryGreen.size() ? horseHairPrimaryGreen[i] : 0u;
            item.iconHairPrimaryColorBlue = i < horseHairPrimaryBlue.size() ? horseHairPrimaryBlue[i] : 0u;
            item.iconHairSecondaryColorRed = i < horseHairSecondaryRed.size() ? horseHairSecondaryRed[i] : 0u;
            item.iconHairSecondaryColorGreen = i < horseHairSecondaryGreen.size() ? horseHairSecondaryGreen[i] : 0u;
            item.iconHairSecondaryColorBlue = i < horseHairSecondaryBlue.size() ? horseHairSecondaryBlue[i] : 0u;
            item.amiiboUid = i < horseUid.size() ? horseUid[i] : 0u;
            item.roomId = i < horseRoom.size() ? horseRoom[i] : 0;
            horses_.push_back(item);
        }
    } catch (const std::exception& ex) {
        TOTK_LOG("loadPouches horses skipped: %s", ex.what());
    }

    GameLimits::clampAllPouches(equipment_, stackItems_, armors_, horses_);
}

void SaveEditor::loadAutobuilder() {
    autobuilderSlots_.clear();
    const auto indices = vars_->readIntArray(pointerFor("AutoBuilder.Draft.Content.Index"));
    const auto isFavorite = hasHash("AutoBuilder.Draft.Content.IsFavorite")
                                 ? vars_->readBoolArray(pointerFor("AutoBuilder.Draft.Content.IsFavorite"))
                                 : std::vector<bool>{};
    const auto blueprints = vars_->readBinaryArray(pointerFor("AutoBuilder.Draft.Content.CombinedActorInfo"));
    const auto cameraPos = vars_->readVector3Array(pointerFor("AutoBuilder.Draft.Content.CameraPos"));
    const auto cameraAt = vars_->readVector3Array(pointerFor("AutoBuilder.Draft.Content.CameraAt"));
    // AutoBuilder.Draft.Content.IsFavorite (hash 0x67f4b46b, discovered
    // 2026-08-19 in resources/data/hashes.csv — never previously read or
    // written by this editor) is the real per-position favorite flag.
    // `Index` is NOT a favorite/unfavorite marker — it's the display-order
    // slot (0-29) a favorited position currently occupies in the in-game
    // Favorites grid, which is why it always reads back as a clean
    // permutation of 0-29 whenever every position happens to be favorited
    // (confirmed on real hardware 2026-08-19). Trust IsFavorite for
    // favorite status; Index is kept only for the display-order mapping in
    // autobuilderFavoriteSlots() and is meaningless for a position whose
    // IsFavorite bit is false.
    const size_t totalPositions =
        std::max({indices.size(), isFavorite.size(), blueprints.size(), cameraPos.size(), cameraAt.size()});
    TOTK_LOG("autobuild: array sizes index=%zu isFavorite=%zu blueprint=%zu cameraPos=%zu cameraAt=%zu",
             indices.size(), isFavorite.size(), blueprints.size(), cameraPos.size(), cameraAt.size());
    for (size_t i = 0; i < totalPositions; ++i) {
        AutoBuilderSlot slot;
        slot.index = static_cast<int>(i);
        slot.slotValue = i < indices.size() ? indices[i] : -1;
        if (i < blueprints.size()) slot.blueprint = blueprints[i];
        if (i < cameraPos.size()) slot.cameraPos = cameraPos[i];
        if (i < cameraAt.size()) slot.cameraAt = cameraAt[i];
        slot.favorite = i < isFavorite.size() ? isFavorite[i] : (slot.slotValue >= 0);
        autobuilderSlots_.push_back(slot);
    }
    for (size_t i = 0; i < autobuilderSlots_.size() && i < 60; ++i) {
        const auto& slot = autobuilderSlots_[i];
        TOTK_LOG("autobuild: pos[%zu] slotValue=%d favorite=%d blueprintSize=%zu", i, slot.slotValue,
                 slot.favorite ? 1 : 0, slot.blueprint.size());
    }
}

std::vector<AutobuilderFavoriteSlot> SaveEditor::autobuilderFavoriteSlots() const {
    std::vector<size_t> favorited;
    for (size_t i = 0; i < autobuilderSlots_.size(); ++i) {
        if (autobuilderSlots_[i].favorite) favorited.push_back(i);
    }
    std::stable_sort(favorited.begin(), favorited.end(), [this](size_t lhs, size_t rhs) {
        return autobuilderSlots_[lhs].slotValue < autobuilderSlots_[rhs].slotValue;
    });

    std::vector<AutobuilderFavoriteSlot> result;
    result.reserve(kMaxAutobuildFavorites);
    for (int favoriteIndex = 0; favoriteIndex < kMaxAutobuildFavorites; ++favoriteIndex) {
        AutobuilderFavoriteSlot slot;
        slot.favoriteIndex = favoriteIndex;
        if (static_cast<size_t>(favoriteIndex) < favorited.size()) {
            slot.occupied = true;
            slot.draftPosition = favorited[static_cast<size_t>(favoriteIndex)];
        }
        result.push_back(slot);
    }
    return result;
}

bool SaveEditor::insertAutobuilderNewest(const AutobuilderCai& blueprint, bool favorite, std::string& error,
                                         size_t* outDraftPosition) {
    if (blueprint.combinedActorInfo.size() != AutobuilderCai::kCaiSize) {
        error = "Blueprint data is the wrong size to import.";
        return false;
    }
    if (autobuilderSlots_.size() != 30) {
        error = "This save does not have the expected 30-entry Autobuild history ring.";
        return false;
    }

    // Favorites are protected from history eviction. Pick the oldest
    // unfavorited record (the greatest Index), even when one or more still-
    // older records are favorites. Removing age K and advancing only ages
    // 0..K-1 produces 1..K; protected ages K+1..29 remain unchanged. The new
    // record fills age 0, preserving a unique 0..29 ordering without asking
    // the user to alter their favorites first.
    long targetPosition = -1;
    int targetIndex = -1;
    for (size_t i = 0; i < autobuilderSlots_.size(); ++i) {
        if (autobuilderSlots_[i].slotValue < 0 || autobuilderSlots_[i].slotValue >= 30) {
            error = "Autobuild Index data is outside the expected 0-29 range.";
            return false;
        }
        if (!autobuilderSlots_[i].favorite && autobuilderSlots_[i].slotValue > targetIndex) {
            targetPosition = static_cast<long>(i);
            targetIndex = autobuilderSlots_[i].slotValue;
        }
    }
    if (targetPosition < 0) {
        error = "Every Autobuild entry is favorited; there is no History entry available to replace.";
        return false;
    }

    for (auto& slot : autobuilderSlots_) {
        if (slot.slotValue < targetIndex) ++slot.slotValue;
    }

    auto& target = autobuilderSlots_[static_cast<size_t>(targetPosition)];
    target.slotValue = 0;
    target.blueprint = blueprint.combinedActorInfo;
    target.cameraPos = blueprint.cameraPos;
    target.cameraAt = blueprint.cameraAt;
    target.favorite = favorite;

    if (outDraftPosition) *outDraftPosition = static_cast<size_t>(targetPosition);
    error.clear();
    TOTK_LOG(
        "insertAutobuilderNewest: draftPosition=%zu evictedIndex=%d favorite=%d blueprintSize=%zu cameraPos=(%.1f,%.1f,%.1f) "
        "cameraAt=(%.1f,%.1f,%.1f) index=0",
        static_cast<size_t>(targetPosition), targetIndex, favorite ? 1 : 0, target.blueprint.size(),
        target.cameraPos.x, target.cameraPos.y, target.cameraPos.z, target.cameraAt.x, target.cameraAt.y,
        target.cameraAt.z);
    return true;
}

bool SaveEditor::importAutobuilderFavorite(int favoriteIndex, const AutobuilderCai& blueprint, std::string& error,
                                            size_t* outDraftPosition) {
    if (favoriteIndex < 0 || favoriteIndex >= kMaxAutobuildFavorites) {
        error = "Favorite slot must be between 1 and " + std::to_string(kMaxAutobuildFavorites) + ".";
        return false;
    }
    const auto favoriteSlots = autobuilderFavoriteSlots();
    const auto& target = favoriteSlots[static_cast<size_t>(favoriteIndex)];
    if (target.occupied) {
        autobuilderSlots_[target.draftPosition].favorite = false;
    }
    const bool inserted = insertAutobuilderNewest(blueprint, true, error, outDraftPosition);
    if (!inserted && target.occupied) autobuilderSlots_[target.draftPosition].favorite = true;
    if (inserted) TOTK_LOG("importAutobuilderFavorite: favoriteIndex=%d inserted as newest", favoriteIndex);
    return inserted;
}

bool SaveEditor::importAutobuilderHistory(long historyDraftPosition, const AutobuilderCai& blueprint,
                                           std::string& error, size_t* outDraftPosition) {
    if (historyDraftPosition >= 0) {
        TOTK_LOG("importAutobuilderHistory: ignoring obsolete physical target=%ld; inserting newest",
                 historyDraftPosition);
    }
    return insertAutobuilderNewest(blueprint, false, error, outDraftPosition);
}

void SaveEditor::applyStatsToFile() {
    GameLimits::clampPlayerStats(stats_);
    vars_->writeUInt(offsetFor("PlayerStatus.CurrentRupee"), stats_.rupees);
    vars_->writeUInt(offsetFor("PlayerStatus.MaxLife"), stats_.maxHearts);
    vars_->writeUInt(offsetFor("PlayerStatus.MaxStamina"), stats_.maxStamina);
    vars_->writeFloat(offsetFor("PlayerStatus.MaxEnergy"), stats_.maxBattery);
    vars_->writeUInt(offsetFor("HorseInnMemberPoint"), stats_.ponyPoints);
    vars_->writeVector3(offsetFor("PlayerStatus.SavePos"), stats_.savePos);

    auto weaponSizes = vars_->readUIntArray(pointerFor("Pouch.Weapon.ValidNum"));
    if (!weaponSizes.empty()) {
        weaponSizes[0] = stats_.pouchWeapons;
        vars_->writeUIntArray(pointerFor("Pouch.Weapon.ValidNum"), weaponSizes);
    }
    auto bowSizes = vars_->readUIntArray(pointerFor("Pouch.Bow.ValidNum"));
    if (!bowSizes.empty()) {
        bowSizes[0] = stats_.pouchBows;
        vars_->writeUIntArray(pointerFor("Pouch.Bow.ValidNum"), bowSizes);
    }
    auto shieldSizes = vars_->readUIntArray(pointerFor("Pouch.Shield.ValidNum"));
    if (!shieldSizes.empty()) {
        shieldSizes[0] = stats_.pouchShields;
        vars_->writeUIntArray(pointerFor("Pouch.Shield.ValidNum"), shieldSizes);
    }
}

void SaveEditor::saveAllPouches() {
    GameLimits::clampAllPouches(equipment_, stackItems_, armors_, horses_);
    const auto arrayPtr = [this](const std::string& key) { return vars_->resolveArrayPointer(key); };

    const auto writeEquipment = [&](const std::string& prefix, const std::vector<EquipmentItem>& items, bool hasFuse) {
        const size_t maxSize = vars_->readString64Array(arrayPtr(prefix + ".Content.Name")).size();
        std::vector<std::string> ids(maxSize);
        std::vector<int32_t> durability(maxSize, 0);
        std::vector<uint32_t> modifier(maxSize, 0);
        std::vector<int32_t> modifierValue(maxSize, 0);
        std::vector<std::string> fuse(maxSize);
        for (size_t i = 0; i < items.size() && i < maxSize; ++i) {
            ids[i] = items[i].id;
            durability[i] = items[i].durability;
            modifier[i] = items[i].modifier;
            modifierValue[i] = items[i].modifierValue;
            if (hasFuse) fuse[i] = items[i].fuseId;
        }
        vars_->writeString64Array(arrayPtr(prefix + ".Content.Name"), ids);
        vars_->writeIntArray(arrayPtr(prefix + ".Content.Life"), durability);
        vars_->writeUIntArray(arrayPtr(prefix + ".Content.Effect.Type"), modifier);
        vars_->writeIntArray(arrayPtr(prefix + ".Content.Effect.Value"), modifierValue);
        if (hasFuse) {
            vars_->writeString64Array(arrayPtr(prefix + ".Content.Combined.Name"), fuse);
        }
    };

    writeEquipment("Pouch.Weapon", equipment_["weapons"], true);
    writeEquipment("Pouch.Bow", equipment_["bows"], false);
    writeEquipment("Pouch.Shield", equipment_["shields"], true);

    const size_t armorMax = vars_->readString64Array(arrayPtr("Pouch.Armor.Content.Name")).size();
    std::vector<std::string> armorIds(armorMax);
    std::vector<uint32_t> armorDye(armorMax, 0);
    for (size_t i = 0; i < armors_.size() && i < armorMax; ++i) {
        armorIds[i] = armors_[i].id;
        armorDye[i] = armors_[i].dyeColor;
    }
    vars_->writeString64Array(arrayPtr("Pouch.Armor.Content.Name"), armorIds);
    vars_->writeUIntArray(arrayPtr("Pouch.Armor.Content.ColorVariation"), armorDye);

    const auto writeStackItems = [&](const std::string& prefix, const std::vector<StackItem>& items, bool isFood = false) {
        const size_t maxSize = vars_->readString64Array(arrayPtr(prefix + ".Content.Name")).size();
        std::vector<std::string> ids(maxSize);
        std::vector<int32_t> counts(maxSize, 0);
        std::vector<int32_t> hearts(maxSize, 0);
        std::vector<int32_t> prices(maxSize, 1);
        std::vector<uint32_t> effectTypes(maxSize, 0);
        std::vector<int32_t> effectLevels(maxSize, 0);
        std::vector<int32_t> effectTimes(maxSize, 0);
        for (size_t i = 0; i < items.size() && i < maxSize; ++i) {
            ids[i] = items[i].id;
            counts[i] = items[i].count;
            if (isFood) {
                hearts[i] = items[i].hearts;
                prices[i] = items[i].price;
                effectTypes[i] = items[i].effect;
                effectLevels[i] = items[i].effectLevel;
                effectTimes[i] = items[i].effectDuration;
            }
        }
        vars_->writeString64Array(arrayPtr(prefix + ".Content.Name"), ids);
        vars_->writeIntArray(arrayPtr(prefix + ".Content.StockNum"), counts);
        if (isFood) {
            vars_->writeIntArray(arrayPtr(prefix + ".Content.LifeRecover"), hearts);
            vars_->writeIntArray(arrayPtr(prefix + ".Content.Price"), prices);
            vars_->writeUIntArray(arrayPtr(prefix + ".Content.Effect.Type"), effectTypes);
            vars_->writeIntArray(arrayPtr(prefix + ".Content.Effect.Level"), effectLevels);
            vars_->writeIntArray(arrayPtr(prefix + ".Content.Effect.Time"), effectTimes);
        }
    };

    writeStackItems("Pouch.Arrow", stackItems_["arrows"]);
    writeStackItems("Pouch.Material", stackItems_["materials"]);
    writeStackItems("Pouch.Food", stackItems_["food"], true);
    writeStackItems("Pouch.KeyItem", stackItems_["key"]);
    writeStackItems("Pouch.SpecialParts", stackItems_["devices"]);
    saveHorses();
}

void SaveEditor::saveHorses() {
    const auto arrayPtr = [this](const std::string& key) { return vars_->resolveArrayPointer(key); };
    const size_t maxSize = vars_->readString64Array(arrayPtr("OwnedHorseList.ActorName")).size();
    if (maxSize == 0) return;

    for (auto& horse : horses_) {
        HorseDefaults::finalizeForStableSave(horse, horses_, vars_.get());
    }

    std::vector<std::string> ids = vars_->readString64Array(arrayPtr("OwnedHorseList.ActorName"));
    std::vector<std::string> names = vars_->readWString16Array(arrayPtr("OwnedHorseList.Name"));
    std::vector<float> bonds = vars_->readFloatArray(arrayPtr("OwnedHorseList.Familiarity"));
    std::vector<int32_t> strengths = vars_->readIntArray(arrayPtr("OwnedHorseList.Toughness"));
    std::vector<int32_t> speeds = vars_->readIntArray(arrayPtr("OwnedHorseList.Speed"));
    std::vector<int32_t> staminas = vars_->readIntArray(arrayPtr("OwnedHorseList.ChargeNum"));
    std::vector<int32_t> pulls = vars_->readIntArray(arrayPtr("OwnedHorseList.HorsePower"));
    std::vector<int32_t> colors = vars_->readIntArray(arrayPtr("OwnedHorseList.ColorType"));
    std::vector<int32_t> feet = vars_->readIntArray(arrayPtr("OwnedHorseList.FootType"));
    std::vector<uint32_t> manes = vars_->readUIntArray(arrayPtr("OwnedHorseList.Mane"));
    std::vector<uint32_t> saddles = vars_->readUIntArray(arrayPtr("OwnedHorseList.Saddle"));
    std::vector<uint32_t> reins = vars_->readUIntArray(arrayPtr("OwnedHorseList.Rein"));
    std::vector<int32_t> types = vars_->readIntArray(arrayPtr("OwnedHorseList.HorseType"));
    std::vector<bool> bondChecked = vars_->readBoolArray(arrayPtr("OwnedHorseList.IsFamiliarityChecked"));
    std::vector<uint32_t> patterns = vars_->readUIntArray(arrayPtr("OwnedHorseList.Body.Pattern"));
    std::vector<uint32_t> eyeColors = vars_->readUIntArray(arrayPtr("OwnedHorseList.Body.EyeColor"));
    std::vector<uint32_t> primaryRed = vars_->readUIntArray(arrayPtr("OwnedHorseList.Body.PrimaryColor.Red"));
    std::vector<uint32_t> primaryGreen = vars_->readUIntArray(arrayPtr("OwnedHorseList.Body.PrimaryColor.Green"));
    std::vector<uint32_t> primaryBlue = vars_->readUIntArray(arrayPtr("OwnedHorseList.Body.PrimaryColor.Blue"));
    std::vector<uint32_t> secondaryRed = vars_->readUIntArray(arrayPtr("OwnedHorseList.Body.SecondaryColor.Red"));
    std::vector<uint32_t> secondaryGreen = vars_->readUIntArray(arrayPtr("OwnedHorseList.Body.SecondaryColor.Green"));
    std::vector<uint32_t> secondaryBlue = vars_->readUIntArray(arrayPtr("OwnedHorseList.Body.SecondaryColor.Blue"));
    std::vector<uint32_t> noseRed = vars_->readUIntArray(arrayPtr("OwnedHorseList.Body.NoseColor.Red"));
    std::vector<uint32_t> noseGreen = vars_->readUIntArray(arrayPtr("OwnedHorseList.Body.NoseColor.Green"));
    std::vector<uint32_t> noseBlue = vars_->readUIntArray(arrayPtr("OwnedHorseList.Body.NoseColor.Blue"));
    std::vector<uint32_t> hairPrimaryRed = vars_->readUIntArray(arrayPtr("OwnedHorseList.Hair.PrimaryColor.Red"));
    std::vector<uint32_t> hairPrimaryGreen = vars_->readUIntArray(arrayPtr("OwnedHorseList.Hair.PrimaryColor.Green"));
    std::vector<uint32_t> hairPrimaryBlue = vars_->readUIntArray(arrayPtr("OwnedHorseList.Hair.PrimaryColor.Blue"));
    std::vector<uint32_t> hairSecondaryRed = vars_->readUIntArray(arrayPtr("OwnedHorseList.Hair.SecondaryColor.Red"));
    std::vector<uint32_t> hairSecondaryGreen =
        vars_->readUIntArray(arrayPtr("OwnedHorseList.Hair.SecondaryColor.Green"));
    std::vector<uint32_t> hairSecondaryBlue = vars_->readUIntArray(arrayPtr("OwnedHorseList.Hair.SecondaryColor.Blue"));
    std::vector<uint64_t> uids = vars_->readUInt64Array(arrayPtr("OwnedHorseList.UidHash"));
    std::vector<int32_t> rooms = vars_->readIntArray(arrayPtr("OwnedHorseList.RoomID"));
    if (names.size() < maxSize) names.resize(maxSize);
    if (bonds.size() < maxSize) bonds.resize(maxSize);
    if (strengths.size() < maxSize) strengths.resize(maxSize);
    if (speeds.size() < maxSize) speeds.resize(maxSize);
    if (staminas.size() < maxSize) staminas.resize(maxSize);
    if (pulls.size() < maxSize) pulls.resize(maxSize);
    if (colors.size() < maxSize) colors.resize(maxSize, 1);
    if (feet.size() < maxSize) feet.resize(maxSize);
    if (manes.size() < maxSize) manes.resize(maxSize);
    if (saddles.size() < maxSize) saddles.resize(maxSize);
    if (reins.size() < maxSize) reins.resize(maxSize);
    if (types.size() < maxSize) types.resize(maxSize, 1);
    if (bondChecked.size() < maxSize) bondChecked.resize(maxSize);
    if (patterns.size() < maxSize) patterns.resize(maxSize);
    if (eyeColors.size() < maxSize) eyeColors.resize(maxSize);
    if (primaryRed.size() < maxSize) primaryRed.resize(maxSize);
    if (primaryGreen.size() < maxSize) primaryGreen.resize(maxSize);
    if (primaryBlue.size() < maxSize) primaryBlue.resize(maxSize);
    if (secondaryRed.size() < maxSize) secondaryRed.resize(maxSize);
    if (secondaryGreen.size() < maxSize) secondaryGreen.resize(maxSize);
    if (secondaryBlue.size() < maxSize) secondaryBlue.resize(maxSize);
    if (noseRed.size() < maxSize) noseRed.resize(maxSize);
    if (noseGreen.size() < maxSize) noseGreen.resize(maxSize);
    if (noseBlue.size() < maxSize) noseBlue.resize(maxSize);
    if (hairPrimaryRed.size() < maxSize) hairPrimaryRed.resize(maxSize);
    if (hairPrimaryGreen.size() < maxSize) hairPrimaryGreen.resize(maxSize);
    if (hairPrimaryBlue.size() < maxSize) hairPrimaryBlue.resize(maxSize);
    if (hairSecondaryRed.size() < maxSize) hairSecondaryRed.resize(maxSize);
    if (hairSecondaryGreen.size() < maxSize) hairSecondaryGreen.resize(maxSize);
    if (hairSecondaryBlue.size() < maxSize) hairSecondaryBlue.resize(maxSize);
    if (uids.size() < maxSize) uids.resize(maxSize);
    if (rooms.size() < maxSize) rooms.resize(maxSize);

    for (size_t i = 0; i < horses_.size() && i < maxSize; ++i) {
        GameLimits::clampHorseItem(horses_[i]);
        ids[i] = horses_[i].id;
        names[i] = horses_[i].name;
        bonds[i] = horses_[i].bond;
        bondChecked[i] = horses_[i].bondChecked;
        strengths[i] = horses_[i].strength;
        speeds[i] = horses_[i].speed;
        staminas[i] = horses_[i].stamina;
        pulls[i] = horses_[i].pull;
        colors[i] = static_cast<int32_t>(horses_[i].colorType);
        feet[i] = static_cast<int32_t>(horses_[i].footType);
        manes[i] = horses_[i].mane;
        saddles[i] = horses_[i].saddle;
        reins[i] = horses_[i].reins;
        types[i] = static_cast<int32_t>(horses_[i].horseType);
        patterns[i] = horses_[i].iconPattern;
        eyeColors[i] = horses_[i].iconEyeColor;
        primaryRed[i] = horses_[i].iconPrimaryColorRed;
        primaryGreen[i] = horses_[i].iconPrimaryColorGreen;
        primaryBlue[i] = horses_[i].iconPrimaryColorBlue;
        secondaryRed[i] = horses_[i].iconSecondaryColorRed;
        secondaryGreen[i] = horses_[i].iconSecondaryColorGreen;
        secondaryBlue[i] = horses_[i].iconSecondaryColorBlue;
        noseRed[i] = horses_[i].iconNoseColorRed;
        noseGreen[i] = horses_[i].iconNoseColorGreen;
        noseBlue[i] = horses_[i].iconNoseColorBlue;
        hairPrimaryRed[i] = horses_[i].iconHairPrimaryColorRed;
        hairPrimaryGreen[i] = horses_[i].iconHairPrimaryColorGreen;
        hairPrimaryBlue[i] = horses_[i].iconHairPrimaryColorBlue;
        hairSecondaryRed[i] = horses_[i].iconHairSecondaryColorRed;
        hairSecondaryGreen[i] = horses_[i].iconHairSecondaryColorGreen;
        hairSecondaryBlue[i] = horses_[i].iconHairSecondaryColorBlue;
        uids[i] = horses_[i].amiiboUid;
        rooms[i] = horses_[i].roomId;
    }

    const uint32_t noneHash = murmurHash3("None");
    for (size_t i = horses_.size(); i < maxSize; ++i) {
        ids[i].clear();
        names[i].clear();
        bonds[i] = 0.0f;
        bondChecked[i] = false;
        strengths[i] = 0;
        speeds[i] = 0;
        staminas[i] = 0;
        pulls[i] = 0;
        colors[i] = 0;
        feet[i] = 0;
        manes[i] = noneHash;
        saddles[i] = noneHash;
        reins[i] = noneHash;
        types[i] = 0;
        patterns[i] = 0;
        eyeColors[i] = 0;
        primaryRed[i] = 0;
        primaryGreen[i] = 0;
        primaryBlue[i] = 0;
        secondaryRed[i] = 0;
        secondaryGreen[i] = 0;
        secondaryBlue[i] = 0;
        noseRed[i] = 0;
        noseGreen[i] = 0;
        noseBlue[i] = 0;
        hairPrimaryRed[i] = 0;
        hairPrimaryGreen[i] = 0;
        hairPrimaryBlue[i] = 0;
        hairSecondaryRed[i] = 0;
        hairSecondaryGreen[i] = 0;
        hairSecondaryBlue[i] = 0;
        uids[i] = 0;
        rooms[i] = 0;
    }

    vars_->writeString64Array(arrayPtr("OwnedHorseList.ActorName"), ids);
    vars_->writeWString16Array(arrayPtr("OwnedHorseList.Name"), names);
    vars_->writeFloatArray(arrayPtr("OwnedHorseList.Familiarity"), bonds);
    vars_->writeBoolArray(arrayPtr("OwnedHorseList.IsFamiliarityChecked"), bondChecked);
    vars_->writeIntArray(arrayPtr("OwnedHorseList.Toughness"), strengths);
    vars_->writeIntArray(arrayPtr("OwnedHorseList.Speed"), speeds);
    vars_->writeIntArray(arrayPtr("OwnedHorseList.ChargeNum"), staminas);
    vars_->writeIntArray(arrayPtr("OwnedHorseList.HorsePower"), pulls);
    vars_->writeIntArray(arrayPtr("OwnedHorseList.ColorType"), colors);
    vars_->writeIntArray(arrayPtr("OwnedHorseList.FootType"), feet);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Mane"), manes);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Saddle"), saddles);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Rein"), reins);
    vars_->writeIntArray(arrayPtr("OwnedHorseList.HorseType"), types);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Body.Pattern"), patterns);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Body.EyeColor"), eyeColors);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Body.PrimaryColor.Red"), primaryRed);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Body.PrimaryColor.Green"), primaryGreen);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Body.PrimaryColor.Blue"), primaryBlue);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Body.SecondaryColor.Red"), secondaryRed);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Body.SecondaryColor.Green"), secondaryGreen);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Body.SecondaryColor.Blue"), secondaryBlue);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Body.NoseColor.Red"), noseRed);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Body.NoseColor.Green"), noseGreen);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Body.NoseColor.Blue"), noseBlue);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Hair.PrimaryColor.Red"), hairPrimaryRed);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Hair.PrimaryColor.Green"), hairPrimaryGreen);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Hair.PrimaryColor.Blue"), hairPrimaryBlue);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Hair.SecondaryColor.Red"), hairSecondaryRed);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Hair.SecondaryColor.Green"), hairSecondaryGreen);
    vars_->writeUIntArray(arrayPtr("OwnedHorseList.Hair.SecondaryColor.Blue"), hairSecondaryBlue);
    vars_->writeUInt64Array(arrayPtr("OwnedHorseList.UidHash"), uids);
    vars_->writeIntArray(arrayPtr("OwnedHorseList.RoomID"), rooms);

    HorseDefaults::syncUsedUidRegistry(vars_.get(), horses_);
}

void SaveEditor::saveAutobuilder() {
    const auto indexPtr = pointerFor("AutoBuilder.Draft.Content.Index");
    const bool hasIsFavorite = hasHash("AutoBuilder.Draft.Content.IsFavorite");
    const size_t isFavoritePtr = hasIsFavorite ? pointerFor("AutoBuilder.Draft.Content.IsFavorite") : 0;
    const auto blueprintPtr = pointerFor("AutoBuilder.Draft.Content.CombinedActorInfo");
    const auto cameraPosPtr = pointerFor("AutoBuilder.Draft.Content.CameraPos");
    const auto cameraAtPtr = pointerFor("AutoBuilder.Draft.Content.CameraAt");

    const auto existingIndices = vars_->readIntArray(indexPtr);
    const auto existingIsFavorite = hasIsFavorite ? vars_->readBoolArray(isFavoritePtr) : std::vector<bool>{};
    const auto existingBlueprints = vars_->readBinaryArray(blueprintPtr);
    const auto existingCameraPos = vars_->readVector3Array(cameraPosPtr);
    const auto existingCameraAt = vars_->readVector3Array(cameraAtPtr);

    // IsFavorite (hash 0x67f4b46b, discovered 2026-08-19) is the real
    // per-position favorite flag and must be written back alongside Index —
    // previously this function only ever wrote Index/blueprint/camera data,
    // so an imported build's blueprint could land correctly while the game
    // still didn't consider that position an actual favorite. Index (favorite
    // display-order slot 0-29, or -1) stays a materially SHORTER on-disk
    // array than CombinedActorInfo/CameraPos/CameraAt (confirmed on real
    // hardware 2026-08-19). Each array keeps its OWN existing on-disk length
    // here; writeXArray() below writes in place at each array's own fixed
    // stride and cannot grow/shrink it (see VariableStore::writeBinaryArray).
    const size_t indexSize = existingIndices.size();
    const size_t isFavoriteSize = existingIsFavorite.size();
    const size_t positionSize =
        std::max({existingBlueprints.size(), existingCameraPos.size(), existingCameraAt.size()});
    if (indexSize == 0 && isFavoriteSize == 0 && positionSize == 0) return;

    // writeBinaryArray() writes each element in place at the file's existing
    // length-prefixed layout (see VariableStore::writeBinaryArray) — it
    // cannot grow/shrink an element without corrupting everything after it.
    // Validate every element we're about to touch is still exactly
    // kCaiSize before writing anything, rather than partially writing then
    // discovering a mismatch.
    for (size_t i = 0; i < autobuilderSlots_.size() && i < positionSize; ++i) {
        if (autobuilderSlots_[i].blueprint.empty()) continue;  // untouched slot, nothing to write
        const size_t existingSize = i < existingBlueprints.size() ? existingBlueprints[i].size() : 0;
        if (autobuilderSlots_[i].blueprint.size() != existingSize) {
            TOTK_LOG("saveAutobuilder: refusing write, slot %zu blueprint size %zu != on-disk %zu", i,
                      autobuilderSlots_[i].blueprint.size(), existingSize);
            return;
        }
    }

    std::vector<int32_t> indices(indexSize, -1);
    std::vector<bool> isFavoriteOut(isFavoriteSize, false);
    std::vector<std::vector<uint8_t>> blueprints = existingBlueprints;
    blueprints.resize(positionSize);
    std::vector<Vec3> cameraPos = existingCameraPos;
    std::vector<Vec3> cameraAt = existingCameraAt;
    cameraPos.resize(positionSize);
    cameraAt.resize(positionSize);

    for (size_t i = 0; i < autobuilderSlots_.size() && i < indexSize; ++i) {
        indices[i] = autobuilderSlots_[i].slotValue;
    }
    for (size_t i = 0; i < autobuilderSlots_.size() && i < isFavoriteSize; ++i) {
        isFavoriteOut[i] = autobuilderSlots_[i].favorite;
    }
    for (size_t i = 0; i < autobuilderSlots_.size() && i < positionSize; ++i) {
        if (!autobuilderSlots_[i].blueprint.empty()) {
            blueprints[i] = autobuilderSlots_[i].blueprint;
            cameraPos[i] = autobuilderSlots_[i].cameraPos;
            cameraAt[i] = autobuilderSlots_[i].cameraAt;
        }
    }

    vars_->writeIntArray(indexPtr, indices);
    if (hasIsFavorite) vars_->writeBoolArray(isFavoritePtr, isFavoriteOut);
    vars_->writeBinaryArray(blueprintPtr, blueprints);
    vars_->writeVector3Array(cameraPosPtr, cameraPos);
    vars_->writeVector3Array(cameraAtPtr, cameraAt);
}

bool SaveEditor::restoreAllWeaponDecay() {
    bool changed = false;
    for (auto& item : equipment_["weapons"]) {
        if (item.id.find("(decayed)") != std::string::npos) {
            const auto pos = item.id.find(" (decayed)");
            if (pos != std::string::npos) item.id = item.id.substr(0, pos);
            changed = true;
        }
    }
    return changed;
}

bool SaveEditor::restoreAllDurability(const std::string& category) {
    if (!equipment_.count(category)) return false;
    for (auto& item : equipment_[category]) item.durability = 999;
    return true;
}

bool SaveEditor::removeAllMapPins() {
    const size_t pointer = pointerFor("MapData.IconData.StampData.Type");
    const auto types = vars_->readUIntArray(pointer);
    std::vector<uint32_t> cleared(types.size(), 0);
    vars_->writeUIntArray(pointer, cleared);
    stats_.mapPinCount = 0;
    return true;
}

size_t SaveEditor::equipmentCapacity(const std::string& category) const {
    if (category == "weapons") return stats_.pouchWeapons;
    if (category == "bows") return stats_.pouchBows;
    if (category == "shields") return stats_.pouchShields;
    return 0;
}

size_t SaveEditor::stackSlotCapacity(const std::string& category) const {
    if (!loaded_ || !vars_) return 0;
    std::string prefix;
    if (category == "arrows") {
        prefix = "Pouch.Arrow";
    } else if (category == "materials") {
        prefix = "Pouch.Material";
    } else if (category == "food") {
        prefix = "Pouch.Food";
    } else if (category == "key") {
        prefix = "Pouch.KeyItem";
    } else if (category == "devices") {
        prefix = "Pouch.SpecialParts";
    } else {
        return 0;
    }
    try {
        return vars_->readString64Array(vars_->resolveArrayPointer(prefix + ".Content.Name")).size();
    } catch (...) {
        return 0;
    }
}

size_t SaveEditor::armorCapacity() const {
    if (!loaded_ || !vars_) return 0;
    try {
        return vars_->readString64Array(vars_->resolveArrayPointer("Pouch.Armor.Content.Name")).size();
    } catch (...) {
        return 0;
    }
}

size_t SaveEditor::horseCapacity() const {
    if (!loaded_ || !vars_) return 0;
    try {
        return vars_->readString64Array(vars_->resolveArrayPointer("OwnedHorseList.ActorName")).size();
    } catch (...) {
        return 0;
    }
}

bool SaveEditor::addEquipmentItem(const std::string& category, const std::string& id) {
    if (id.empty() || !equipment_.count(category)) return false;
    if (equipment_[category].size() >= equipmentCapacity(category)) return false;

    EquipmentItem item;
    item.category = category;
    item.id = id;
    item.durability = 70;
    GameLimits::clampEquipmentItem(item);
    equipment_[category].push_back(item);
    return true;
}

bool SaveEditor::hasArrows() const {
    const auto it = stackItems_.find("arrows");
    return it != stackItems_.end() && !it->second.empty();
}

bool SaveEditor::addArrows() {
    if (hasArrows()) return false;
    if (!addStackItem("arrows", "NormalArrow")) return false;

    try {
        const size_t equipPtr = vars_->resolveArrayPointer("Pouch.Arrow.EquipIndex");
        auto equipIndex = vars_->readIntArray(equipPtr);
        if (!equipIndex.empty() && equipIndex[0] == -1) {
            equipIndex[0] = 0;
            vars_->writeIntArray(equipPtr, equipIndex);
        }
    } catch (...) {
    }

    return true;
}

bool SaveEditor::addStackItem(const std::string& category, const std::string& id) {
    if (id.empty() || !stackItems_.count(category)) return false;

    for (auto& item : stackItems_[category]) {
        if (item.id == id) {
            item.count += 1;
            GameLimits::clampStackItem(item);
            return true;
        }
    }

    if (stackItems_[category].size() >= stackSlotCapacity(category)) return false;

    StackItem item;
    item.category = category;
    item.id = id;
    item.count = category == "food" ? 1 : 1;
    if (category == "food") {
        item.hearts = 4;
        item.price = 1;
    }
    GameLimits::clampStackItem(item);
    stackItems_[category].push_back(item);
    return true;
}

bool SaveEditor::addArmorItem(const std::string& id) {
    if (id.empty() || !loaded_ || !vars_) return false;
    const size_t maxSize =
        vars_->readString64Array(vars_->resolveArrayPointer("Pouch.Armor.Content.Name")).size();
    if (armors_.size() >= maxSize) return false;

    ArmorItem item;
    item.id = ArmorUpgrades::baseId(id);
    GameLimits::clampArmorItem(item);
    armors_.push_back(item);
    return true;
}

bool SaveEditor::addHorseItem(const std::string& id) {
    if (id.empty() || !loaded_ || !vars_) return false;
    if (horses_.size() >= horseCapacity()) return false;

    HorseItem item = HorseDefaults::createForId(id, horses_, vars_.get());
    HorseDefaults::finalizeForStableSave(item, horses_, vars_.get());
    horses_.push_back(item);
    return true;
}

bool SaveEditor::removeEquipmentItem(const std::string& category, size_t index) {
    if (!equipment_.count(category)) return false;
    auto& items = equipment_[category];
    if (index >= items.size()) return false;
    items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool SaveEditor::removeStackItem(const std::string& category, size_t index) {
    if (!stackItems_.count(category)) return false;
    auto& items = stackItems_[category];
    if (index >= items.size()) return false;
    items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool SaveEditor::removeArmorItem(size_t index) {
    if (index >= armors_.size()) return false;
    armors_.erase(armors_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool SaveEditor::removeHorseItem(size_t index) {
    if (index >= horses_.size()) return false;
    horses_.erase(horses_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

int SaveEditor::countCompletedHashes(const std::string& category) const {
    vars_->indexAllHashes();
    const auto& hashes = CompletismData::instance().hashesFor(category);
    int count = 0;
    for (const uint32_t hash : hashes) {
        const auto offset = vars_->tryGetHashOffset(hash);
        if (offset && vars_->readUInt(*offset) != 0) ++count;
    }
    return count;
}

bool SaveEditor::unlockAllCompletion(const std::string& category) {
    const auto& hashes = CompletismData::instance().hashesFor(category);
    if (hashes.empty()) return false;
    bool changed = false;
    for (const uint32_t hash : hashes) {
        const auto offset = vars_->tryGetHashOffset(hash);
        if (!offset) continue;
        if (vars_->readUInt(*offset) == 0) {
            vars_->writeUInt(*offset, 1);
            changed = true;
        }
    }
    return changed;
}

bool SaveEditor::pinMissingLocations(const std::string& category, int limit) {
    const auto& data = CompletismData::instance();
    const std::vector<uint32_t>* hashes = nullptr;
    const auto& foundHashes = data.hashesFor(category + "_found");
    const auto& defeatedHashes = data.hashesFor(category + "_defeated");
    const auto& directHashes = data.hashesFor(category);
    if (!foundHashes.empty()) {
        hashes = &foundHashes;
    } else if (!defeatedHashes.empty()) {
        hashes = &defeatedHashes;
    } else if (!directHashes.empty()) {
        hashes = &directHashes;
    } else {
        return false;
    }

    const auto& coords = data.coordinatesFor(category);
    if (coords.empty()) return false;

    const size_t typePtr = pointerFor("MapData.IconData.StampData.Type");
    const size_t posPtr = pointerFor("MapData.IconData.StampData.Pos");
    auto types = vars_->readUIntArray(typePtr);
    auto positions = vars_->readVector3Array(posPtr);
    int added = 0;

    for (size_t i = 0; i < hashes->size() && i < coords.size() && added < limit; ++i) {
        const auto offset = vars_->tryGetHashOffset((*hashes)[i]);
        if (!offset || vars_->readUInt(*offset) != 0) continue;

        for (size_t pin = 0; pin < types.size(); ++pin) {
            if (types[pin] != 0) continue;
            types[pin] = murmurHash3("MapPinType_Crystal");
            if (pin < positions.size()) {
                positions[pin].x = coords[i][0];
                positions[pin].y = coords[i][2];
                positions[pin].z = coords[i][1];
            }
            added++;
            break;
        }
    }

    if (added > 0) {
        vars_->writeUIntArray(typePtr, types);
        vars_->writeVector3Array(posPtr, positions);
        stats_.mapPinCount += added;
    }
    return added > 0;
}

std::vector<SaveSlotInfo> SaveEditor::scanSaveDirectory(const std::string& basePath) {
    std::vector<SaveSlotInfo> slots;
    if (basePath.empty()) return slots;
    for (int i = 0; i < 6; ++i) {
        SaveSlotInfo info;
        info.slotIndex = i;
        const std::string slotDir = basePath + "/slot_0" + std::to_string(i);
        info.progressPath = slotDir + "/progress.sav";
        info.captionPath = slotDir + "/caption.sav";
        if (!pathExists(info.progressPath)) continue;

        info.dateLabel = "Slot " + std::to_string(i);
        CaptionMetadata caption;
        if (pathExists(info.captionPath) && CaptionParser::loadFromFile(info.captionPath, caption, false) &&
            caption.valid) {
            info.hasCaption = true;
            info.formattedDate = caption.formattedDate;
            info.sortKey = caption.sortKey;
            info.autosave = caption.autosave;
            info.locationName = CaptionParser::displayLocationName(caption);
        } else {
            info.formattedDate = "Unknown save time";
            info.sortKey = static_cast<int64_t>(i);
        }
        slots.push_back(std::move(info));
    }

    std::sort(slots.begin(), slots.end(), [](const SaveSlotInfo& a, const SaveSlotInfo& b) {
        if (a.sortKey != b.sortKey) return a.sortKey > b.sortKey;
        if (a.autosave != b.autosave) return a.autosave < b.autosave;
        return a.slotIndex < b.slotIndex;
    });
    return slots;
}

}  // namespace totk
