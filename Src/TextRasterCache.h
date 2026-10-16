#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <list>
#include <cstdint>

// Render-thread owner of full-resolution GPU text images. No CPU pixel copies.
class TextRasterCache {
public:
    explicit TextRasterCache(size_t byteBudget=32*1024*1024);
    ~TextRasterCache();
    TextRasterCache(const TextRasterCache&) = delete;
    TextRasterCache& operator=(const TextRasterCache&) = delete;
    bool Draw(ID2D1RenderTarget* target, IDWriteTextLayout* layout,
              D2D1_COLOR_F color, float x, float y, float scale);
    void Clear();
    size_t Bytes() const;
    uint64_t Hits() const;
    uint64_t Misses() const;
    uint64_t Evictions() const;
private:
    struct Entry {
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
        D2D1_COLOR_F color{};
        float scale=1,phaseX=0,phaseY=0,left=0,top=0;
        size_t bytes=0;
    };
    size_t budget, bytes=0;
    uint64_t hits=0, misses=0;
    uint64_t evictions=0;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> domain;
    std::list<Entry> entries;
};
