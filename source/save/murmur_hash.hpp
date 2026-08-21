#pragma once

#include <cstdint>
#include <string>

namespace totk {

uint32_t murmurHash3(const std::string& text);
std::string hashHex(uint32_t hash);

}  // namespace totk
