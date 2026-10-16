#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace RenderPolicy {
// scRGB: 1.0 = 80 nits on HDR displays. System SDR white matching comes later.
inline float SrgbToLinear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
inline float LinearToSrgb(float c) { return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c,1.f/2.4f)-0.055f; }
inline float FromHalf(uint16_t bits) {
    int exponent=(bits>>10)&31, fraction=bits&1023;
    float value=exponent?std::ldexp(1.f+fraction/1024.f,exponent-15):std::ldexp(float(fraction),-24);
    return bits&0x8000?-value:value;
}
inline uint16_t Half(float value) {
    value = (std::max)(0.0f, (std::min)(value, 65504.0f));
    uint32_t bits; std::memcpy(&bits, &value, sizeof(bits));
    int exponent = int((bits >> 23) & 255) - 127 + 15;
    uint32_t fraction = bits & 0x7fffff;
    if (exponent <= 0) {
        if (exponent < -10) return 0;
        return uint16_t(((fraction | 0x800000) + (1u << (13 - exponent))) >> (14 - exponent));
    }
    return uint16_t((exponent << 10) + ((fraction + 0x1000) >> 13));
}
inline bool UseHdr(bool displayActive, bool floatSurface) { return displayActive && floatSurface; }
}
