#include "MarkdownInline.h"
MarkdownInline::MarkdownInline(float w, float h) : width(w), height(h) {}
HRESULT MarkdownInline::QueryInterface(REFIID iid, void** result) {
    if (!result) return E_POINTER;
    *result = nullptr;
    if (iid != __uuidof(IUnknown) && iid != __uuidof(IDWriteInlineObject)) return E_NOINTERFACE;
    *result = static_cast<IDWriteInlineObject*>(this); AddRef(); return S_OK;
}
ULONG MarkdownInline::AddRef() { return ++references; }
ULONG MarkdownInline::Release() { const auto count = --references; if (!count) delete this; return count; }
HRESULT MarkdownInline::Draw(void*, IDWriteTextRenderer*, FLOAT, FLOAT, BOOL, BOOL, IUnknown*) { return S_OK; }
HRESULT MarkdownInline::GetMetrics(DWRITE_INLINE_OBJECT_METRICS* value) {
    if (!value) return E_POINTER;
    *value = {width, height, height, FALSE}; return S_OK;
}
HRESULT MarkdownInline::GetOverhangMetrics(DWRITE_OVERHANG_METRICS* value) {
    if (!value) return E_POINTER;
    *value = {}; return S_OK;
}
HRESULT MarkdownInline::GetBreakConditions(DWRITE_BREAK_CONDITION* before, DWRITE_BREAK_CONDITION* after) {
    if (!before || !after) return E_POINTER;
    *before = DWRITE_BREAK_CONDITION_MUST_BREAK;
    // The parser supplies paragraph separators after images. A forced break at
    // end of text creates an empty line that inherits the inline image height.
    *after = DWRITE_BREAK_CONDITION_CAN_BREAK;
    return S_OK;
}
