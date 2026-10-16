#include "framework.h"
#include "TextRasterCache.h"
#include <algorithm>
#include <cmath>

using Microsoft::WRL::ComPtr;
TextRasterCache::TextRasterCache(size_t byteBudget):budget(byteBudget){}
TextRasterCache::~TextRasterCache()=default;
void TextRasterCache::Clear(){entries.clear();bytes=0;domain.Reset();}
size_t TextRasterCache::Bytes()const{return bytes;}
uint64_t TextRasterCache::Hits()const{return hits;}
uint64_t TextRasterCache::Misses()const{return misses;}
uint64_t TextRasterCache::Evictions()const{return evictions;}
bool TextRasterCache::Draw(ID2D1RenderTarget* target,IDWriteTextLayout* layout,D2D1_COLOR_F color,float x,float y,float scale) {
    if(!target || !layout || !std::isfinite(scale) || scale<=0 || !std::isfinite(x) || !std::isfinite(y))return false;
    D2D1_MATRIX_3X2_F transform{};target->GetTransform(&transform);
    float dpiX=0,dpiY=0;target->GetDpi(&dpiX,&dpiY);
    // Future transforms must use direct drawing until their full pixel footprint
    // is represented in a cache key. Never silently magnify an identity raster.
    if(transform._11!=1 || transform._22!=1 || transform._12!=0 || transform._21!=0 ||
        transform._31!=0 || transform._32!=0 || dpiX!=96*scale || dpiY!=96*scale)return false;
    if(domain.Get()!=target){Clear();domain=target;}
    float pixelX=x*scale,pixelY=y*scale;
    float phaseX=pixelX-std::floor(pixelX),phaseY=pixelY-std::floor(pixelY);
    auto found=std::find_if(entries.begin(),entries.end(),[&](const Entry& entry){
        return entry.layout.Get()==layout && entry.scale==scale && entry.phaseX==phaseX && entry.phaseY==phaseY &&
            entry.color.r==color.r && entry.color.g==color.g && entry.color.b==color.b && entry.color.a==color.a;
    });
    if(found!=entries.end()) {++hits;entries.splice(entries.begin(),entries,found);}
    else {
        ++misses;
        DWRITE_TEXT_METRICS metrics{};DWRITE_OVERHANG_METRICS overhang{};
        if(FAILED(layout->GetMetrics(&metrics)) || FAILED(layout->GetOverhangMetrics(&overhang)))return false;
        // Include ink overhang and antialiasing fringe, preserving fractional origin.
        float left=std::floor(std::min(0.f,-overhang.left)*scale)-2;
        float top=std::floor(std::min(0.f,-overhang.top)*scale)-2;
        float right=std::ceil((metrics.left+metrics.widthIncludingTrailingWhitespace+std::max(0.f,overhang.right))*scale+phaseX)+2;
        float bottom=std::ceil((metrics.top+metrics.height+std::max(0.f,overhang.bottom))*scale+phaseY)+2;
        double width=right-left,height=bottom-top;
        auto limit=target->GetMaximumBitmapSize();
        if(width<=0 || height<=0 || width>limit || height>limit || width*height*8>double(budget)/2)return false;
        size_t cost=static_cast<size_t>(width)*static_cast<size_t>(height)*8;
        while(!entries.empty() && (bytes+cost>budget || entries.size()>=512)){bytes-=entries.back().bytes;entries.pop_back();++evictions;}
        ComPtr<ID2D1BitmapRenderTarget> raster;
        auto pixels=D2D1::SizeU((UINT32)width,(UINT32)height);
        auto size=D2D1::SizeF(pixels.width/scale,pixels.height/scale);
        auto format=target->GetPixelFormat();format.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;
        if(FAILED(target->CreateCompatibleRenderTarget(&size,&pixels,&format,D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,&raster)))return false;
        raster->SetDpi(96*scale,96*scale);
        raster->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        ComPtr<IDWriteRenderingParams> parameters;target->GetTextRenderingParams(&parameters);
        raster->SetTextRenderingParams(parameters.Get());
        ComPtr<ID2D1SolidColorBrush> brush;
        if(FAILED(raster->CreateSolidColorBrush(color,&brush)))return false;
        raster->BeginDraw();raster->Clear(D2D1::ColorF(0,0,0,0));
        raster->DrawTextLayout(D2D1::Point2F((phaseX-left)/scale,(phaseY-top)/scale),layout,brush.Get());
        if(FAILED(raster->EndDraw()))return false;
        Entry entry;entry.layout=layout;entry.color=color;entry.scale=scale;entry.phaseX=phaseX;entry.phaseY=phaseY;
        entry.left=left;entry.top=top;entry.bytes=cost;
        if(FAILED(raster->GetBitmap(&entry.bitmap)))return false;
        entries.push_front(std::move(entry));bytes+=cost;
    }
    const auto& entry=entries.front();auto pixels=entry.bitmap->GetPixelSize();
    float left=(std::floor(pixelX)+entry.left)/scale,top=(std::floor(pixelY)+entry.top)/scale;
    // One source pixel to one destination physical pixel, including fractional DPI.
    target->DrawBitmap(entry.bitmap.Get(),D2D1::RectF(left,top,left+pixels.width/scale,top+pixels.height/scale),1,
        D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    return true;
}
