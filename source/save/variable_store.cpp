#include "save/variable_store.hpp"

#include "save/murmur_hash.hpp"

#include <stdexcept>

namespace totk {

VariableStore::VariableStore(MarcFile& file) : file_(file) {}

void VariableStore::resetCache() {
    cachedOffsets_.clear();
}

size_t VariableStore::findHashTableEnd() {
    for (size_t i = 0x28; i + 4 < file_.size(); i += 8) {
        if (file_.readU32(i) == kSaveTypeHash) {
            hashTableEnd_ = i + 4;
            return hashTableEnd_;
        }
    }
    return 0;
}

size_t VariableStore::getHashOffset(uint32_t hash) {
    if (const auto it = cachedOffsets_.find(hash); it != cachedOffsets_.end()) return it->second;

    for (size_t i = 0x28; i < hashTableEnd_; i += 8) {
        if (file_.readU32(i) == hash) {
            const size_t offset = i + 4;
            cachedOffsets_[hash] = offset;
            return offset;
        }
    }
    throw std::runtime_error("Hash not found: " + hashHex(hash));
}

size_t VariableStore::getHashOffset(const std::string& hashText) {
    return getHashOffset(murmurHash3(hashText));
}

size_t VariableStore::resolveArrayPointer(uint32_t hash) {
    return readUInt(getHashOffset(hash));
}

size_t VariableStore::resolveArrayPointer(const std::string& hashText) {
    return resolveArrayPointer(murmurHash3(hashText));
}

std::optional<size_t> VariableStore::tryGetHashOffset(uint32_t hash) {
    try {
        return getHashOffset(hash);
    } catch (...) {
        return std::nullopt;
    }
}

void VariableStore::indexAllHashes() {
    if (fullyIndexed_) return;
    fullyIndexed_ = true;
    for (size_t i = 0x28; i < hashTableEnd_; i += 8) {
        const uint32_t hash = file_.readU32(i);
        if (hash == 0) continue;
        cachedOffsets_.emplace(hash, i + 4);
    }
}

bool VariableStore::readBool(size_t offset) const {
    return file_.readU32(offset) != 0;
}

int32_t VariableStore::readInt(size_t offset) const {
    return file_.readS32(offset);
}

uint32_t VariableStore::readUInt(size_t offset) const {
    return file_.readU32(offset);
}

float VariableStore::readFloat(size_t offset) const {
    return file_.readF32(offset);
}

Vec2 VariableStore::readVector2(size_t offset) const {
    return {file_.readF32(offset), file_.readF32(offset + 4)};
}

Vec3 VariableStore::readVector3(size_t offset) const {
    if (kRawCoordinateTransform) {
        return {file_.readF32(offset), -file_.readF32(offset + 8), file_.readF32(offset + 4) - 105.0f};
    }
    return {file_.readF32(offset), file_.readF32(offset + 4), file_.readF32(offset + 8)};
}

uint64_t VariableStore::readUInt64(size_t offset) const {
    const uint32_t lower = file_.readU32(offset);
    const uint32_t upper = file_.readU32(offset + 4);
    return (static_cast<uint64_t>(upper) << 32) | lower;
}

std::string VariableStore::readString64(size_t offset) const {
    return file_.readString(offset, 0x40);
}

std::string VariableStore::readWString16(size_t offset) const {
    return file_.readWString16(offset, 0x10);
}

void VariableStore::writeBool(size_t offset, bool value) {
    file_.writeU32(offset, value ? 1u : 0u);
}

void VariableStore::writeInt(size_t offset, int32_t value) {
    file_.writeS32(offset, value);
}

void VariableStore::writeUInt(size_t offset, uint32_t value) {
    file_.writeU32(offset, value);
}

void VariableStore::writeFloat(size_t offset, float value) {
    file_.writeF32(offset, value);
}

void VariableStore::writeVector2(size_t offset, const Vec2& value) {
    file_.writeF32(offset, value.x);
    file_.writeF32(offset + 4, value.y);
}

void VariableStore::writeVector3(size_t offset, const Vec3& value) {
    if (kRawCoordinateTransform) {
        file_.writeF32(offset, value.x);
        file_.writeF32(offset + 4, value.z + 105.0f);
        file_.writeF32(offset + 8, -value.y);
    } else {
        file_.writeF32(offset, value.x);
        file_.writeF32(offset + 4, value.y);
        file_.writeF32(offset + 8, value.z);
    }
}

void VariableStore::writeUInt64(size_t offset, uint64_t value) {
    file_.writeU32(offset, static_cast<uint32_t>(value & 0xFFFFFFFF));
    file_.writeU32(offset + 4, static_cast<uint32_t>(value >> 32));
}

void VariableStore::writeString64(size_t offset, const std::string& value) {
    file_.writeString(offset, value, 0x40);
}

void VariableStore::writeWString16(size_t offset, const std::string& value) {
    std::vector<uint8_t> bytes(0x20, 0);
    for (size_t i = 0; i < value.size() && i < 0x10; ++i) {
        const uint16_t code = static_cast<uint8_t>(value[i]);
        bytes[i * 2] = static_cast<uint8_t>(code & 0xFF);
        bytes[i * 2 + 1] = static_cast<uint8_t>((code >> 8) & 0xFF);
    }
    file_.writeBytes(offset, bytes);
}

bool VariableStore::readBoolArrayBit(size_t arrayOffset, size_t index) const {
    const size_t byteOffset = arrayOffset + (index / 8);
    const uint8_t byte = file_.readU8(byteOffset);
    return (byte >> (index % 8)) & 0x01;
}

void VariableStore::writeBoolArrayBit(size_t arrayOffset, size_t index, bool value) {
    const size_t byteOffset = arrayOffset + (index / 8);
    uint8_t byte = file_.readU8(byteOffset);
    const uint8_t bitMask = static_cast<uint8_t>(1 << (index % 8));
    if (value) byte |= bitMask;
    else byte &= static_cast<uint8_t>(~bitMask);
    file_.writeU8(byteOffset, byte);
}

uint32_t VariableStore::readArrayLength(size_t pointerOffset) const {
    return file_.readU32(pointerOffset);
}

std::vector<bool> VariableStore::readBoolArray(size_t pointerOffset) const {
    const uint32_t length = readArrayLength(pointerOffset);
    std::vector<bool> values(length);
    for (uint32_t i = 0; i < length; ++i) values[i] = readBoolArrayBit(pointerOffset + 4, i);
    return values;
}

std::vector<uint32_t> VariableStore::readUIntArray(size_t pointerOffset) const {
    const uint32_t length = readArrayLength(pointerOffset);
    std::vector<uint32_t> values(length);
    size_t offset = pointerOffset + 4;
    for (uint32_t i = 0; i < length; ++i, offset += 4) values[i] = readUInt(offset);
    return values;
}

std::vector<int32_t> VariableStore::readIntArray(size_t pointerOffset) const {
    const uint32_t length = readArrayLength(pointerOffset);
    std::vector<int32_t> values(length);
    size_t offset = pointerOffset + 4;
    for (uint32_t i = 0; i < length; ++i, offset += 4) values[i] = readInt(offset);
    return values;
}

std::vector<float> VariableStore::readFloatArray(size_t pointerOffset) const {
    const uint32_t length = readArrayLength(pointerOffset);
    std::vector<float> values(length);
    size_t offset = pointerOffset + 4;
    for (uint32_t i = 0; i < length; ++i, offset += 4) values[i] = readFloat(offset);
    return values;
}

std::vector<Vec3> VariableStore::readVector3Array(size_t pointerOffset) const {
    const uint32_t length = readArrayLength(pointerOffset);
    std::vector<Vec3> values(length);
    size_t offset = pointerOffset + 4;
    for (uint32_t i = 0; i < length; ++i, offset += 12) values[i] = readVector3(offset);
    return values;
}

std::vector<std::string> VariableStore::readString64Array(size_t pointerOffset) const {
    const uint32_t length = readArrayLength(pointerOffset);
    std::vector<std::string> values(length);
    size_t offset = pointerOffset + 4;
    for (uint32_t i = 0; i < length; ++i, offset += 0x40) values[i] = readString64(offset);
    return values;
}

std::vector<std::string> VariableStore::readWString16Array(size_t pointerOffset) const {
    const uint32_t length = readArrayLength(pointerOffset);
    std::vector<std::string> values(length);
    size_t offset = pointerOffset + 4;
    for (uint32_t i = 0; i < length; ++i, offset += 0x20) values[i] = readWString16(offset);
    return values;
}

std::vector<uint64_t> VariableStore::readUInt64Array(size_t pointerOffset) const {
    const uint32_t length = readArrayLength(pointerOffset);
    std::vector<uint64_t> values(length);
    size_t offset = pointerOffset + 4;
    for (uint32_t i = 0; i < length; ++i, offset += 8) values[i] = readUInt64(offset);
    return values;
}

std::vector<std::vector<uint8_t>> VariableStore::readBinaryArray(size_t pointerOffset) const {
    const uint32_t length = readArrayLength(pointerOffset);
    std::vector<std::vector<uint8_t>> values;
    values.reserve(length);
    size_t offset = pointerOffset + 4;
    for (uint32_t i = 0; i < length; ++i) {
        const uint32_t len = file_.readU32(offset);
        offset += 4;
        values.push_back(file_.readBytes(offset, len));
        offset += len;
    }
    return values;
}

void VariableStore::writeBoolArray(size_t pointerOffset, const std::vector<bool>& values) {
    for (size_t i = 0; i < values.size(); ++i) writeBoolArrayBit(pointerOffset + 4, i, values[i]);
}

void VariableStore::writeUIntArray(size_t pointerOffset, const std::vector<uint32_t>& values) {
    size_t offset = pointerOffset + 4;
    for (const auto value : values) {
        writeUInt(offset, value);
        offset += 4;
    }
}

void VariableStore::writeIntArray(size_t pointerOffset, const std::vector<int32_t>& values) {
    size_t offset = pointerOffset + 4;
    for (const auto value : values) {
        writeInt(offset, value);
        offset += 4;
    }
}

void VariableStore::writeFloatArray(size_t pointerOffset, const std::vector<float>& values) {
    size_t offset = pointerOffset + 4;
    for (const auto value : values) {
        writeFloat(offset, value);
        offset += 4;
    }
}

void VariableStore::writeVector3Array(size_t pointerOffset, const std::vector<Vec3>& values) {
    size_t offset = pointerOffset + 4;
    for (const auto& value : values) {
        writeVector3(offset, value);
        offset += 12;
    }
}

void VariableStore::writeString64Array(size_t pointerOffset, const std::vector<std::string>& values) {
    size_t offset = pointerOffset + 4;
    for (const auto& value : values) {
        writeString64(offset, value);
        offset += 0x40;
    }
}

void VariableStore::writeWString16Array(size_t pointerOffset, const std::vector<std::string>& values) {
    size_t offset = pointerOffset + 4;
    for (const auto& value : values) {
        writeWString16(offset, value);
        offset += 0x20;
    }
}

void VariableStore::writeUInt64Array(size_t pointerOffset, const std::vector<uint64_t>& values) {
    size_t offset = pointerOffset + 4;
    for (const auto value : values) {
        writeUInt64(offset, value);
        offset += 8;
    }
}

void VariableStore::writeBinaryArray(size_t pointerOffset, const std::vector<std::vector<uint8_t>>& values) {
    size_t offset = pointerOffset + 4;
    for (const auto& data : values) {
        file_.writeU32(offset, static_cast<uint32_t>(data.size()));
        offset += 4;
        file_.writeBytes(offset, data);
        offset += data.size();
    }
}

}  // namespace totk
