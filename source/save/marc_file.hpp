#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

class MarcFile {
public:
    MarcFile() = default;
    explicit MarcFile(size_t size);

    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path) const;

    size_t size() const { return data_.size(); }
    uint8_t* data() { return data_.data(); }
    const uint8_t* data() const { return data_.data(); }

    uint8_t readU8(size_t offset) const;
    uint16_t readU16(size_t offset) const;
    uint32_t readU32(size_t offset) const;
    int32_t readS32(size_t offset) const;
    float readF32(size_t offset) const;
    std::vector<uint8_t> readBytes(size_t offset, size_t length) const;
    std::string readString(size_t offset, size_t maxLength) const;
    std::string readWString16(size_t offset, size_t maxChars) const;

    void writeU8(size_t offset, uint8_t value);
    void writeU16(size_t offset, uint16_t value);
    void writeU32(size_t offset, uint32_t value);
    void writeS32(size_t offset, int32_t value);
    void writeF32(size_t offset, float value);
    void writeBytes(size_t offset, const std::vector<uint8_t>& bytes);
    void writeString(size_t offset, const std::string& value, size_t maxLength);

private:
    std::vector<uint8_t> data_;
};
