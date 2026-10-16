#pragma once
#include <memory>
struct ID2D1RenderTarget;
struct ID2D1DeviceContext;
class NotebookImage;
class ImageUpload;
class ImageCache {
public:
    ImageCache();
    ~ImageCache();
    void Clear();
    void SetUploads(std::shared_ptr<const ImageUpload> uploads);
    // False means this image cannot be prepared on the current device; caller draws a diagnostic.
    bool Draw(ID2D1RenderTarget* target,ID2D1DeviceContext* context,
        const std::shared_ptr<const NotebookImage>& image,float x,float y,float width,float height,bool hdr,float white);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
