#pragma once
#include <stdint.h>
#include <cstring>
#include <bit>

// Source groups start at a four-byte-aligned PSRAM address. Four input pixels
// expand to five LCD pixels, preserving the existing 4:5 nearest-neighbour map.
template<bool packed>
inline void c64_convert_four(const uint8_t *in, uint16_t *out, const uint16_t *palette) {
    if constexpr(packed) {
        static_assert(std::endian::native == std::endian::little);
        uint32_t indices;
        // memcpy avoids type-aliasing UB; the alignment lets Arm emit one LDR.
        std::memcpy(&indices, __builtin_assume_aligned(in,4), sizeof indices);
        out[0]=out[1]=palette[indices&15];
        out[2]=palette[(indices>>8)&15];
        out[3]=palette[(indices>>16)&15];
        out[4]=palette[(indices>>24)&15];
    } else {
        out[0]=out[1]=palette[in[0]&15];out[2]=palette[in[1]&15];
        out[3]=palette[in[2]&15];out[4]=palette[in[3]&15];
    }
}
