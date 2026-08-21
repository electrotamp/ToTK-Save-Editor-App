#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "save/marc_file.hpp"

namespace totk {

struct Vec2 {
    float x = 0;
    float y = 0;
};

struct Vec3 {
    float x = 0;
    float y = 0;
    float z = 0;
};

enum class VarType {
    Bool,
    Int,
    UInt,
    Float,
    Enum,
    Vector2,
    Vector3,
    UInt64,
    String64,
    WString16,
    BoolArray,
    IntArray,
    UIntArray,
    FloatArray,
    Vector3Array,
    String64Array,
};

class VariableStore {
public:
    static constexpr uint32_t kSaveTypeHash = 0xa3db7114;
    static constexpr bool kRawCoordinateTransform = true;

    explicit VariableStore(MarcFile& file);

    void resetCache();
    size_t findHashTableEnd();
    size_t hashTableEnd() const { return hashTableEnd_; }

    size_t getHashOffset(uint32_t hash);
    size_t getHashOffset(const std::string& hashText);
    size_t resolveArrayPointer(uint32_t hash);
    size_t resolveArrayPointer(const std::string& hashText);
    std::optional<size_t> tryGetHashOffset(uint32_t hash);

    // One-time full linear pass populating cachedOffsets_ for every entry in
    // the hash table, instead of the one-at-a-time cache-on-miss behavior
    // tryGetHashOffset normally relies on. Worth calling before doing many
    // (tens to hundreds+) lookups against a freshly-constructed instance —
    // e.g. counting completion flags — where each individual miss would
    // otherwise re-scan the whole table. No-op after the first call.
    void indexAllHashes();

    bool readBool(size_t offset) const;
    int32_t readInt(size_t offset) const;
    uint32_t readUInt(size_t offset) const;
    float readFloat(size_t offset) const;
    Vec2 readVector2(size_t offset) const;
    Vec3 readVector3(size_t offset) const;
    uint64_t readUInt64(size_t offset) const;
    std::string readString64(size_t offset) const;
    std::string readWString16(size_t offset) const;

    void writeBool(size_t offset, bool value);
    void writeInt(size_t offset, int32_t value);
    void writeUInt(size_t offset, uint32_t value);
    void writeFloat(size_t offset, float value);
    void writeVector2(size_t offset, const Vec2& value);
    void writeVector3(size_t offset, const Vec3& value);
    void writeUInt64(size_t offset, uint64_t value);
    void writeString64(size_t offset, const std::string& value);
    void writeWString16(size_t offset, const std::string& value);

    bool readBoolArrayBit(size_t arrayOffset, size_t index) const;
    void writeBoolArrayBit(size_t arrayOffset, size_t index, bool value);

    uint32_t readArrayLength(size_t pointerOffset) const;
    std::vector<bool> readBoolArray(size_t pointerOffset) const;
    std::vector<uint32_t> readUIntArray(size_t pointerOffset) const;
    std::vector<int32_t> readIntArray(size_t pointerOffset) const;
    std::vector<float> readFloatArray(size_t pointerOffset) const;
    std::vector<Vec3> readVector3Array(size_t pointerOffset) const;
    std::vector<std::string> readString64Array(size_t pointerOffset) const;
    std::vector<std::string> readWString16Array(size_t pointerOffset) const;
    std::vector<uint64_t> readUInt64Array(size_t pointerOffset) const;
    std::vector<std::vector<uint8_t>> readBinaryArray(size_t pointerOffset) const;

    void writeBoolArray(size_t pointerOffset, const std::vector<bool>& values);
    void writeUIntArray(size_t pointerOffset, const std::vector<uint32_t>& values);
    void writeIntArray(size_t pointerOffset, const std::vector<int32_t>& values);
    void writeFloatArray(size_t pointerOffset, const std::vector<float>& values);
    void writeVector3Array(size_t pointerOffset, const std::vector<Vec3>& values);
    void writeString64Array(size_t pointerOffset, const std::vector<std::string>& values);
    void writeWString16Array(size_t pointerOffset, const std::vector<std::string>& values);
    void writeUInt64Array(size_t pointerOffset, const std::vector<uint64_t>& values);
    void writeBinaryArray(size_t pointerOffset, const std::vector<std::vector<uint8_t>>& values);

    MarcFile& file() { return file_; }
    const MarcFile& file() const { return file_; }

private:
    MarcFile& file_;
    size_t hashTableEnd_ = 0x03c800;
    std::unordered_map<uint32_t, size_t> cachedOffsets_;
    bool fullyIndexed_ = false;
};

}  // namespace totk
