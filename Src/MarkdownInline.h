#pragma once
#include "framework.h"
#include <dwrite.h>
#include <atomic>

// Reserves image geometry in a text layout. Images are drawn separately by the GPU.
class MarkdownInline final : public IDWriteInlineObject {
public:
    MarkdownInline(float width, float height);
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE Draw(void*, IDWriteTextRenderer*, FLOAT, FLOAT, BOOL, BOOL, IUnknown*) override;
    HRESULT STDMETHODCALLTYPE GetMetrics(DWRITE_INLINE_OBJECT_METRICS* metrics) override;
    HRESULT STDMETHODCALLTYPE GetOverhangMetrics(DWRITE_OVERHANG_METRICS* metrics) override;
    HRESULT STDMETHODCALLTYPE GetBreakConditions(DWRITE_BREAK_CONDITION* before, DWRITE_BREAK_CONDITION* after) override;
private:
    std::atomic<ULONG> references{1};
    float width, height;
};
