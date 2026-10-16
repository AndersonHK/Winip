#pragma once
#include "framework.h"
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <functional>
#include <memory>
#include <unordered_map>
class Notebook;
class NotebookImage;

// Immutable full-resolution textures prepared by the document loader.
// Only the thread-safe D3D device is used here, never the drawing context.
class ImageUpload {
public:
    ImageUpload(ID3D11Device* device, const Notebook& notebook, const std::function<bool()>& cancelled);
    ~ImageUpload();
    ImageUpload(const ImageUpload&)=delete;
    ImageUpload& operator=(const ImageUpload&)=delete;
    IDXGISurface* Find(const std::shared_ptr<const NotebookImage>& image) const;
    size_t Bytes() const;
    size_t Count() const;
private:
    struct Entry {
        std::shared_ptr<const NotebookImage> image;
        Microsoft::WRL::ComPtr<IDXGISurface> surface;
    };
    std::unordered_map<const NotebookImage*,Entry> entries;
    size_t bytes=0;
};
