#include "save/murmur_hash.hpp"

#include <cstdio>

namespace totk {

uint32_t murmurHash3(const std::string& text) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(text.data());
    const int len = static_cast<int>(text.size());
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    const uint32_t seed = 0;
    uint32_t h1 = seed;
    int i = 0;

    while (i + 4 <= len) {
        uint32_t k1 = data[i] | (data[i + 1] << 8) | (data[i + 2] << 16) | (data[i + 3] << 24);
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
        i += 4;
    }

    uint32_t k1 = 0;
    switch (len - i) {
        case 3:
            k1 ^= data[i + 2] << 16;
            [[fallthrough]];
        case 2:
            k1 ^= data[i + 1] << 8;
            [[fallthrough]];
        case 1:
            k1 ^= data[i];
            k1 *= c1;
            k1 = (k1 << 15) | (k1 >> 17);
            k1 *= c2;
            h1 ^= k1;
            break;
        default:
            break;
    }

    h1 ^= static_cast<uint32_t>(len);
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    return h1;
}

std::string hashHex(uint32_t hash) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%08x", hash);
    return buffer;
}

}  // namespace totk
