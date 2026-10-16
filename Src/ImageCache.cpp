#include "ImageCache.h"
#include "ImageUpload.h"
#include "NotebookImage.h"
#include <d2d1_1.h>
#include <d2d1effects.h>
#include <wrl/client.h>
#include <vector>
#include <algorithm>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
namespace {
struct ImageFailure : std::runtime_error {
    HRESULT result;
    explicit ImageFailure(HRESULT hr):std::runtime_error("Cannot prepare GPU image"),result(hr){}
};
void CheckBitmap(HRESULT hr) {if(FAILED(hr))throw ImageFailure(hr);}
}
struct ImageCache::Impl {
    struct Entry {
        std::weak_ptr<const NotebookImage> source;
        ComPtr<ID2D1Bitmap> bitmap;
        ComPtr<ID2D1Effect> color,whiteLevel;
        uint64_t used=0;
        size_t bytes=0;
        bool hdr=false;
        bool unavailable=false;
    };
    std::shared_ptr<const ImageUpload> uploads;
    ComPtr<ID2D1RenderTarget> target;
    std::vector<Entry> entries;
    uint64_t clock=0;
    size_t bytes=0;
};
ImageCache::ImageCache():impl(std::make_unique<Impl>()){}
ImageCache::~ImageCache()=default;
bool ImageCache::Draw(ID2D1RenderTarget* target,ID2D1DeviceContext* context,
    const std::shared_ptr<const NotebookImage>& image,float x,float y,float width,float height,bool hdr,float white) {
    if(impl->target.Get()!=target) {impl->entries.clear();impl->bytes=0;impl->target=target;}
    auto found=std::find_if(impl->entries.begin(),impl->entries.end(),[&](const Impl::Entry& entry){return entry.source.lock()==image && entry.hdr==hdr;});
    if(found==impl->entries.end()) {
        const auto size=image->Pixels().size();
        while(!impl->entries.empty() && (impl->bytes+size>512*1024*1024 || impl->entries.size()>=128)) {
            auto oldest=std::min_element(impl->entries.begin(),impl->entries.end(),[](const Impl::Entry& a,const Impl::Entry& b){return a.used<b.used;});
            impl->bytes-=oldest->bytes;impl->entries.erase(oldest);
        }
        Impl::Entry entry;entry.source=image;entry.bytes=size;entry.hdr=hdr;
        try {
        auto props=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
        auto surface=impl->uploads && context?impl->uploads->Find(image):nullptr;
        if(surface) {
            ComPtr<ID2D1Bitmap1> bitmap;
            auto properties=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,props.pixelFormat,96,96);
            CheckBitmap(context->CreateBitmapFromDxgiSurface(surface,&properties,&bitmap));
            CheckBitmap(bitmap.As(&entry.bitmap));
            entry.bytes=0; // Original allocation is owned/accounted by ImageUpload.
        } else {
        CheckBitmap(target->CreateBitmap(D2D1::SizeU(image->Width(),image->Height()),image->Pixels().data(),image->Width()*4,props,&entry.bitmap));
        }
        if(hdr || !image->Profile().empty()) {
            if(!context)throw ImageFailure(E_NOINTERFACE);
            ComPtr<ID2D1ColorContext> source,destination;
            CheckBitmap(context->CreateColorContext(image->Profile().empty()?D2D1_COLOR_SPACE_SRGB:D2D1_COLOR_SPACE_CUSTOM,
                image->Profile().empty()?nullptr:image->Profile().data(),static_cast<UINT32>(image->Profile().size()),&source));
            CheckBitmap(context->CreateColorContext(hdr?D2D1_COLOR_SPACE_SCRGB:D2D1_COLOR_SPACE_SRGB,nullptr,0,&destination));
            CheckBitmap(context->CreateEffect(CLSID_D2D1ColorManagement,&entry.color));
            ComPtr<ID2D1Effect> dpi;
            CheckBitmap(context->CreateEffect(CLSID_D2D1DpiCompensation,&dpi));
            dpi->SetInput(0,entry.bitmap.Get());
            CheckBitmap(dpi->SetValue(D2D1_DPICOMPENSATION_PROP_INPUT_DPI,D2D1::Vector2F(96,96)));
            entry.color->SetInputEffect(0,dpi.Get());
            CheckBitmap(entry.color->SetValue(D2D1_COLORMANAGEMENT_PROP_SOURCE_COLOR_CONTEXT,source.Get()));
            CheckBitmap(entry.color->SetValue(D2D1_COLORMANAGEMENT_PROP_DESTINATION_COLOR_CONTEXT,destination.Get()));
            CheckBitmap(entry.color->SetValue(D2D1_COLORMANAGEMENT_PROP_ALPHA_MODE,D2D1_COLORMANAGEMENT_ALPHA_MODE_PREMULTIPLIED));
            CheckBitmap(entry.color->SetValue(D2D1_PROPERTY_PRECISION,D2D1_BUFFER_PRECISION_16BPC_FLOAT));
            CheckBitmap(context->CreateEffect(CLSID_D2D1ColorMatrix,&entry.whiteLevel));
            entry.whiteLevel->SetInputEffect(0,entry.color.Get());
            CheckBitmap(entry.whiteLevel->SetValue(D2D1_PROPERTY_PRECISION,D2D1_BUFFER_PRECISION_16BPC_FLOAT));
        }
        } catch(const ImageFailure& error) {
            if(error.result==D2DERR_RECREATE_TARGET || error.result==DXGI_ERROR_DEVICE_REMOVED || error.result==DXGI_ERROR_DEVICE_RESET)throw;
            entry.bitmap.Reset();entry.color.Reset();entry.whiteLevel.Reset();entry.bytes=0;entry.unavailable=true;
        }
        impl->bytes+=entry.bytes;impl->entries.push_back(std::move(entry));found=impl->entries.end()-1;
    }
    found->used=++impl->clock;
    if(found->unavailable)return false;
    if(found->color) {
        const float level=hdr?white:1;
        D2D1_MATRIX_5X4_F matrix={level,0,0,0, 0,level,0,0, 0,0,level,0, 0,0,0,1, 0,0,0,0};
        CheckBitmap(found->whiteLevel->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX,matrix));
        D2D1_MATRIX_3X2_F previous;target->GetTransform(&previous);
        target->SetTransform(D2D1::Matrix3x2F::Scale(width/image->Width(),height/image->Height())*D2D1::Matrix3x2F::Translation(x,y)*previous);
        ComPtr<ID2D1Image> rendered;found->whiteLevel->GetOutput(&rendered);
        const auto origin=D2D1::Point2F(0,0);
        context->DrawImage(rendered.Get(),&origin,nullptr,D2D1_INTERPOLATION_MODE_LINEAR);
        target->SetTransform(previous);
    } else target->DrawBitmap(found->bitmap.Get(),D2D1::RectF(x,y,x+width,y+height),1,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    return true;
}

void ImageCache::Clear() {impl->entries.clear();impl->bytes=0;impl->target.Reset();}

void ImageCache::SetUploads(std::shared_ptr<const ImageUpload> uploads) {
    if(impl->uploads==uploads)return;
    Clear();impl->uploads=std::move(uploads);
}
