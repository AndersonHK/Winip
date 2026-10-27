#pragma once
#include "framework.h"
#include <dxgi.h>
#include "RenderMetrics.h"
#include "CellEditor.h"
#include <memory>
#include <string>
#include <cstdint>
class Notebook;
class NotebookImage;
class ImageUpload;
struct ID3D11Device;
bool CompositionAvailable();
// Font shaping/metrics run on the document loader, independently of GPU lifetime.
int PrepareNotebookLayout(Notebook& notebook);

// The render worker owns GPU resources. Native geometry transactions await its complete frame.
class Renderer {
public:
    explicit Renderer(HWND window, bool forceSdr = false, bool diagnostics = false);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void Resize(unsigned width, unsigned height, bool refreshDisplay);
    void Begin(float scale);
    void SetHdrEnabled(bool enabled);
    bool Present(const std::wstring& diagnosticSnapshot = {});
    void RoundedBox(float x, float y, float w, float h, float radius, COLORREF color, float opacity=1);
    ID3D11Device* UploadDevice() const;
    void SetImageUploads(std::shared_ptr<const ImageUpload> uploads);
    void Image(const std::shared_ptr<const NotebookImage>& image,float x,float y,float width,float height);
    void Box(float x, float y, float w, float h, COLORREF color);
    float MeasureText(const std::wstring& text, float width, float size, bool bold, bool extraBold=false);
    float WrappedTextHeight(const std::wstring& text, float width, float size);
    void WrappedText(const std::wstring& text, float x, float y, float width, float size, COLORREF color);
    float Text(const std::wstring& text, float x, float y, float w, float h,
              float size, COLORREF color, bool mono, bool bold, bool ellipsis = false, bool extraBold = false);
    void PushClip(float left, float top, float right, float bottom);
    void PopClip();
    void TitleInput(const std::wstring& text, size_t caret, size_t anchor, float x, float y, float width);
    void CellInput(const CellEditView& view, float x, float y);
    void PreparedText(IDWriteTextLayout* layout, float x, float y);
    const RenderMetrics& Metrics() const;
    size_t TextCacheBytes() const;
    uint64_t TextCacheHits() const;
    uint64_t TextCacheMisses() const;
    bool IsHdr() const;
    float WhiteScale() const;
    unsigned RefreshHz() const;
    unsigned AllocationWidth() const;
    unsigned AllocationHeight() const;
    // Raw DXGI display evidence; sparse event-driven frames are not dropped frames.
    HRESULT FrameStatistics(DXGI_FRAME_STATISTICS& statistics, unsigned& lastPresent) const;
    unsigned Width() const;
    unsigned Height() const;
    std::wstring AdapterName() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
