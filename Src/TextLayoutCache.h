#pragma once
#include <dwrite.h>
#include <wrl/client.h>
#include <map>
#include <string>
#include <tuple>
#include <cstdint>
class RenderMetrics;

// Shaped DIP geometry survives viewport and DPI changes; clipping stays in the view.
class TextLayoutCache {
public:
    TextLayoutCache(IDWriteFactory* factory,IDWriteFontCollection* fonts,RenderMetrics& metrics);
    ~TextLayoutCache();
    TextLayoutCache(const TextLayoutCache&) = delete;
    TextLayoutCache& operator=(const TextLayoutCache&) = delete;
    // Shared layouts are immutable after publication. Changed inputs get a new layout.
    Microsoft::WRL::ComPtr<IDWriteTextLayout> Get(const std::wstring& text,float width,float size,bool mono,bool bold,bool ellipsis, bool extraBold=false,bool wrap=false);
private:
    using Key=std::tuple<std::wstring,float,float,bool,bool,bool,bool,bool>;
    struct Entry {Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;uint64_t used=0;size_t cost=0;};
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fonts;
    RenderMetrics& metrics;
    std::map<Key,Entry> entries;
    size_t bytes=0;
    uint64_t serial=0;
};
