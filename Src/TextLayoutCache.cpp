#include "framework.h"
#include "TextLayoutCache.h"
#include "RenderMetrics.h"
#include <algorithm>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
namespace {
void Check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("Could not prepare text layout");}
}
TextLayoutCache::TextLayoutCache(IDWriteFactory* write,IDWriteFontCollection* collection,RenderMetrics& diagnostics)
    :factory(write),fonts(collection),metrics(diagnostics){}
TextLayoutCache::~TextLayoutCache()=default;
ComPtr<IDWriteTextLayout> TextLayoutCache::Get(const std::wstring& text,float width,float size,bool mono,bool bold,bool ellipsis,bool extraBold) {
    // No wrapping or paragraph centering: only ellipsis geometry depends on width.
    constexpr float unbounded=16777216.f;
    Key key{text,ellipsis?width:unbounded,size,mono,bold,ellipsis,extraBold};
    auto found=entries.find(key);
    if(found!=entries.end()){found->second.used=++serial;return found->second.layout;}
    RenderMetrics::Scope timing(metrics,RenderMetrics::Stage::Layout);
    ComPtr<IDWriteTextFormat> format;
    Check(factory->CreateTextFormat(mono?L"Noto Sans Mono":L"Noto Sans",fonts.Get(),extraBold?DWRITE_FONT_WEIGHT_EXTRA_BOLD:bold?DWRITE_FONT_WEIGHT_BOLD:DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&format));
    Check(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    if(ellipsis) {
        ComPtr<IDWriteInlineObject> sign;Check(factory->CreateEllipsisTrimmingSign(format.Get(),&sign));
        DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};Check(format->SetTrimming(&trim,sign.Get()));
    }
    ComPtr<IDWriteTextLayout> layout;
    Check(factory->CreateTextLayout(text.data(),(UINT32)text.size(),format.Get(),ellipsis?width:unbounded,unbounded,&layout));
    // Estimate shaped storage conservatively; COM allocation sizes are opaque.
    size_t cost=1024+text.size()*64;
    constexpr size_t budget=8*1024*1024;
    if(cost>budget)return layout;
    while(!entries.empty() && (entries.size()>=512 || bytes+cost>budget)) {
        auto oldest=std::min_element(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
        bytes-=oldest->second.cost;entries.erase(oldest);
    }
    entries.emplace(std::move(key),Entry{layout,++serial,cost});bytes+=cost;
    return layout;
}
