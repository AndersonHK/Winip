#include "ImageUpload.h"
#include "Notebook.h"
#include "Markdown.h"
#include "CellOutput.h"
#include <stdexcept>

ImageUpload::ImageUpload(ID3D11Device* device, const Notebook& notebook, const std::function<bool()>& cancelled) {
    if(!device)return;
    // Bound retained texture storage independently from the fallback/LRU cache.
    constexpr size_t budget=size_t(1024)*1024*1024;
    auto prepare=[&](const std::shared_ptr<const NotebookImage>& image) {
        if(cancelled && cancelled())throw std::runtime_error("Notebook loading was cancelled.");
        if(!image || entries.count(image.get()) || image->Pixels().size()>budget-bytes)return;
        D3D11_TEXTURE2D_DESC description{};
        description.Width=image->Width();description.Height=image->Height();
        description.MipLevels=1;description.ArraySize=1;
        description.Format=DXGI_FORMAT_B8G8R8A8_UNORM;description.SampleDesc.Count=1;
        description.Usage=D3D11_USAGE_IMMUTABLE;description.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA source{image->Pixels().data(),image->Width()*4,0};
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        const auto result=device->CreateTexture2D(&description,&source,&texture);
        // Preserve the existing drawing path if this device cannot retain the texture.
        if(FAILED(result))return;
        Entry entry;entry.image=image;
        if(FAILED(texture.As(&entry.surface)))return;
        bytes+=image->Pixels().size();entries.emplace(image.get(),std::move(entry));
    };
    for(const auto& cell:notebook.Cells()) {
        if(cell.markdown)for(const auto& image:cell.markdown->Images())prepare(image.image);
        if(cell.output)for(const auto& block:cell.output->Blocks())prepare(block.image);
    }
}
ImageUpload::~ImageUpload()=default;
IDXGISurface* ImageUpload::Find(const std::shared_ptr<const NotebookImage>& image) const {
    const auto found=entries.find(image.get());
    return found==entries.end()?nullptr:found->second.surface.Get();
}
size_t ImageUpload::Bytes() const {return bytes;}
size_t ImageUpload::Count() const {return entries.size();}
