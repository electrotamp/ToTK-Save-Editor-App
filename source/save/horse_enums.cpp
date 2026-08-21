#include "save/horse_enums.hpp"

#include "save/murmur_hash.hpp"

namespace totk {

namespace {

std::vector<ItemEnumOption> makeHashOptions(std::initializer_list<const char*> names) {
    std::vector<ItemEnumOption> options;
    options.reserve(names.size());
    for (const char* name : names) {
        options.push_back({murmurHash3(name), name});
    }
    return options;
}

std::vector<IntEnumOption> makeIntOptions(std::initializer_list<std::pair<int32_t, const char*>> entries) {
    std::vector<IntEnumOption> options;
    options.reserve(entries.size());
    for (const auto& [value, label] : entries) {
        options.push_back({value, label});
    }
    return options;
}

}  // namespace

const std::vector<ItemEnumOption>& HorseEnums::manes() {
    static const std::vector<ItemEnumOption> kOptions = makeHashOptions(
        {"None", "Horse_Link_Mane", "Horse_Link_Mane_01", "Horse_Link_Mane_02", "Horse_Link_Mane_03",
         "Horse_Link_Mane_04", "Horse_Link_Mane_05", "Horse_Link_Mane_06", "Horse_Link_Mane_07",
         "Horse_Link_Mane_08", "Horse_Link_Mane_09", "Horse_Link_Mane_10", "Horse_Link_Mane_11",
         "Horse_Link_Mane_12", "Horse_Link_Mane_00L", "Horse_Link_Mane_01L", "Horse_Link_Mane_00S"});
    return kOptions;
}

const std::vector<ItemEnumOption>& HorseEnums::saddles() {
    static const std::vector<ItemEnumOption> kOptions = makeHashOptions(
        {"None", "GameRomHorseSaddle_00", "GameRomHorseSaddle_01", "GameRomHorseSaddle_02",
         "GameRomHorseSaddle_03", "GameRomHorseSaddle_04", "GameRomHorseSaddle_05", "GameRomHorseSaddle_06",
         "GameRomHorseSaddle_07", "GameRomHorseSaddle_00L", "GameRomHorseSaddle_00S"});
    return kOptions;
}

const std::vector<ItemEnumOption>& HorseEnums::reins() {
    static const std::vector<ItemEnumOption> kOptions = makeHashOptions(
        {"None", "GameRomHorseReins_00", "GameRomHorseReins_01", "GameRomHorseReins_02", "GameRomHorseReins_03",
         "GameRomHorseReins_04", "GameRomHorseReins_05", "GameRomHorseReins_06", "GameRomHorseReins_00L",
         "GameRomHorseReins_00S"});
    return kOptions;
}

const std::vector<ItemEnumOption>& HorseEnums::patterns() {
    static const std::vector<ItemEnumOption> kOptions = makeHashOptions({"00", "01", "02", "03", "04", "05", "06"});
    return kOptions;
}

const std::vector<ItemEnumOption>& HorseEnums::eyeColors() {
    static const std::vector<ItemEnumOption> kOptions = makeHashOptions({"Black", "Blue"});
    return kOptions;
}

const std::vector<IntEnumOption>& HorseEnums::horseTypes() {
    static const std::vector<IntEnumOption> kOptions = makeIntOptions(
        {{1, "Normal"}, {2, "Giant Black"}, {3, "Royal White"}, {4, "Epona"}, {5, "Stalhorse"}, {8, "Spot"},
         {9, "Lord of Mountain"}, {12, "Gold"}, {13, "Giant White"}});
    return kOptions;
}

const std::vector<IntEnumOption>& HorseEnums::statSpeeds() {
    static const std::vector<IntEnumOption> kOptions =
        makeIntOptions({{1, "2 stars"}, {2, "3 stars"}, {3, "4 stars"}, {4, "5 stars"}});
    return kOptions;
}

const std::vector<IntEnumOption>& HorseEnums::statStamina() {
    static const std::vector<IntEnumOption> kOptions = makeIntOptions(
        {{0, "Infinite"}, {2, "2 stars"}, {3, "3 stars"}, {4, "4 stars"}, {5, "5 stars"}});
    return kOptions;
}

const std::vector<IntEnumOption>& HorseEnums::statPull() {
    static const std::vector<IntEnumOption> kOptions =
        makeIntOptions({{1, "2 stars"}, {2, "3 stars"}, {3, "4 stars"}, {4, "5 stars"}});
    return kOptions;
}

std::string HorseEnums::labelForValue(const std::vector<ItemEnumOption>& options, uint32_t value) {
    for (const auto& option : options) {
        if (option.value == value) return option.label;
    }
    return options.empty() ? std::string{} : options.front().label;
}

std::string HorseEnums::labelForValue(const std::vector<IntEnumOption>& options, int32_t value) {
    for (const auto& option : options) {
        if (option.value == value) return option.label;
    }
    return "Type " + std::to_string(value);
}

int HorseEnums::indexForValue(const std::vector<IntEnumOption>& options, int32_t value) {
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i].value == value) return static_cast<int>(i);
    }
    return 0;
}

std::vector<std::string> HorseEnums::labelsFor(const std::vector<IntEnumOption>& options) {
    std::vector<std::string> labels;
    labels.reserve(options.size());
    for (const auto& option : options) {
        labels.push_back(option.label);
    }
    return labels;
}

}  // namespace totk
