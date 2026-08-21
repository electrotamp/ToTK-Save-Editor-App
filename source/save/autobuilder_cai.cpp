#include "save/autobuilder_cai.hpp"

#include <cstring>

namespace totk {

namespace {

bool hasHeader(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < AutobuilderCai::kHeaderSize) return false;
    return std::memcmp(bytes.data(), AutobuilderCai::kHeader, AutobuilderCai::kHeaderSize) == 0;
}

float readF32(const std::vector<uint8_t>& bytes, size_t offset) {
    float value = 0.0f;
    std::memcpy(&value, bytes.data() + offset, sizeof(float));
    return value;
}

void writeF32(std::vector<uint8_t>& bytes, size_t offset, float value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(float));
}

}  // namespace

bool parseAutobuilderCai(const std::vector<uint8_t>& bytes, AutobuilderCai& out, std::string& error) {
    if (!hasHeader(bytes)) {
        error = "Not a valid Autobuild blueprint (missing \"CmbAct\" header).";
        return false;
    }

    if (bytes.size() != AutobuilderCai::kCaiSize && bytes.size() != AutobuilderCai::kDraftSize &&
        bytes.size() != AutobuilderCai::kEditorSize) {
        error = "Unexpected .cai file size (" + std::to_string(bytes.size()) + " bytes).";
        return false;
    }

    out.combinedActorInfo.assign(bytes.begin(), bytes.begin() + static_cast<long>(AutobuilderCai::kCaiSize));
    out.hasCamera = false;
    out.cameraPos = Vec3{};
    out.cameraAt = Vec3{};

    if (bytes.size() == AutobuilderCai::kEditorSize) {
        out.cameraPos.x = readF32(bytes, AutobuilderCai::kCaiSize + 0);
        out.cameraPos.y = readF32(bytes, AutobuilderCai::kCaiSize + 4);
        out.cameraPos.z = readF32(bytes, AutobuilderCai::kCaiSize + 8);
        out.cameraAt.x = readF32(bytes, AutobuilderCai::kCaiSize + 12);
        out.cameraAt.y = readF32(bytes, AutobuilderCai::kCaiSize + 16);
        out.cameraAt.z = readF32(bytes, AutobuilderCai::kCaiSize + 20);
        out.hasCamera = true;
    }

    error.clear();
    return true;
}

std::vector<uint8_t> exportAutobuilderCai(const AutobuilderCai& blueprint) {
    std::vector<uint8_t> bytes(AutobuilderCai::kEditorSize, 0);
    const size_t copySize = std::min(blueprint.combinedActorInfo.size(), AutobuilderCai::kCaiSize);
    std::memcpy(bytes.data(), blueprint.combinedActorInfo.data(), copySize);

    writeF32(bytes, AutobuilderCai::kCaiSize + 0, blueprint.cameraPos.x);
    writeF32(bytes, AutobuilderCai::kCaiSize + 4, blueprint.cameraPos.y);
    writeF32(bytes, AutobuilderCai::kCaiSize + 8, blueprint.cameraPos.z);
    writeF32(bytes, AutobuilderCai::kCaiSize + 12, blueprint.cameraAt.x);
    writeF32(bytes, AutobuilderCai::kCaiSize + 16, blueprint.cameraAt.y);
    writeF32(bytes, AutobuilderCai::kCaiSize + 20, blueprint.cameraAt.z);

    return bytes;
}

}  // namespace totk
