#pragma once
#include <cmath>
#include <stdexcept>

namespace RasterContract {
// Layout units are DIPs. Raster dimensions are physical pixels, rounded up so
// fractional DPI never causes an undersized cache entry to be enlarged.
inline unsigned RequiredPixels(float logicalSize,float dpi) {
    if(!std::isfinite(logicalSize) || !std::isfinite(dpi) || logicalSize<0 || dpi<=0)
        throw std::invalid_argument("Invalid raster dimensions");
    double pixels=std::ceil(double(logicalSize)*double(dpi)/96.0);
    if(pixels>16384)throw std::invalid_argument("Raster exceeds supported GPU texture dimension");
    return (unsigned)pixels;
}
// This gate is for reusable raster resources, not immutable source images.
inline bool CacheIsSufficient(unsigned cachedWidth,unsigned cachedHeight,unsigned targetWidth,unsigned targetHeight) {
    return cachedWidth>=targetWidth && cachedHeight>=targetHeight;
}
}
