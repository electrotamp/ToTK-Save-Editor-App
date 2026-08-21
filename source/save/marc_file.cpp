#include "save/marc_file.hpp"

#include <fstream>

MarcFile::MarcFile(size_t size) : data_(size, 0) {}

bool MarcFile::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);
    data_.resize(fileSize);
    file.read(reinterpret_cast<char*>(data_.data()), static_cast<std::streamsize>(fileSize));
    return file.good();
}

bool MarcFile::saveToFile(const std::string& path) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(data_.data()), static_cast<std::streamsize>(data_.size()));
    return file.good();
}

uint8_t MarcFile::readU8(size_t offset) const {
    return data_.at(offset);
}

uint16_t MarcFile::readU16(size_t offset) const {
    return static_cast<uint16_t>(data_.at(offset) | (data_.at(offset + 1) << 8));
}

uint32_t MarcFile::readU32(size_t offset) const {
    return static_cast<uint32_t>(
        data_.at(offset) | (data_.at(offset + 1) << 8) | (data_.at(offset + 2) << 16) |
        (data_.at(offset + 3) << 24));
}

int32_t MarcFile::readS32(size_t offset) const {
    return static_cast<int32_t>(readU32(offset));
}

float MarcFile::readF32(size_t offset) const {
    const uint32_t bits = readU32(offset);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::vector<uint8_t> MarcFile::readBytes(size_t offset, size_t length) const {
    return std::vector<uint8_t>(data_.begin() + offset, data_.begin() + offset + length);
}

std::string MarcFile::readString(size_t offset, size_t maxLength) const {
    std::string value;
    value.reserve(maxLength);
    for (size_t i = 0; i < maxLength && offset + i < data_.size(); ++i) {
        const char c = static_cast<char>(data_.at(offset + i));
        if (c == '\0') break;
        value.push_back(c);
    }
    return value;
}

std::string MarcFile::readWString16(size_t offset, size_t maxChars) const {
    std::string value;
    for (size_t i = 0; i < maxChars; ++i) {
        const uint16_t code = readU16(offset + i * 2);
        if (code == 0) break;
        if (code < 0x80) value.push_back(static_cast<char>(code));
        else value.push_back('?');
    }
    return value;
}

void MarcFile::writeU8(size_t offset, uint8_t value) {
    data_.at(offset) = value;
}

void MarcFile::writeU16(size_t offset, uint16_t value) {
    data_.at(offset) = static_cast<uint8_t>(value & 0xFF);
    data_.at(offset + 1) = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void MarcFile::writeU32(size_t offset, uint32_t value) {
    data_.at(offset) = static_cast<uint8_t>(value & 0xFF);
    data_.at(offset + 1) = static_cast<uint8_t>((value >> 8) & 0xFF);
    data_.at(offset + 2) = static_cast<uint8_t>((value >> 16) & 0xFF);
    data_.at(offset + 3) = static_cast<uint8_t>((value >> 24) & 0xFF);
}

void MarcFile::writeS32(size_t offset, int32_t value) {
    writeU32(offset, static_cast<uint32_t>(value));
}

void MarcFile::writeF32(size_t offset, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeU32(offset, bits);
}

void MarcFile::writeBytes(size_t offset, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) data_.at(offset + i) = bytes[i];
}

void MarcFile::writeString(size_t offset, const std::string& value, size_t maxLength) {
    for (size_t i = 0; i < maxLength; ++i) {
        data_.at(offset + i) = i < value.size() ? static_cast<uint8_t>(value[i]) : 0;
    }
}
