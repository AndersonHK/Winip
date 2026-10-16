#include "../Src/framework.h"
#include "TextRasterCacheTests.h"
#include "../Src/TextRasterCache.h"
#include "../Src/TextLayoutCache.h"
#include "../Src/RenderMetrics.h"
#include "../Src/FontSet.h"
#include "../Src/Notebook.h"
#include "../Src/CellOutput.h"
#include "../Src/ImageCache.h"
#include "../Src/ImageUpload.h"
#include "../Src/RenderPolicy.h"
#include "../ThirdParty/nlohmann/json.hpp"
#include <d3d11.h>
#include <d2d1_1.h>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <fstream>

using Microsoft::WRL::ComPtr;
namespace {
void Check(HRESULT result){if(FAILED(result))throw std::runtime_error("GPU cache fidelity test API failure");}
}
void RunTextRasterCacheTests() {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> gpu;
    Check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&gpu));
    ComPtr<IDXGIDevice> dxgi;Check(device.As(&dxgi));
    ComPtr<ID2D1Factory1> factory;Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.GetAddressOf()));
    ComPtr<ID2D1Device> d2d;Check(factory->CreateDevice(dxgi.Get(),&d2d));
    ComPtr<ID2D1DeviceContext> target;Check(d2d->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&target));
    ComPtr<IDWriteFactory> write;Check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write.GetAddressOf())));
    FontSet fonts(write.Get());RenderMetrics diagnostics;TextLayoutCache layouts(write.Get(),fonts.Collection(),diagnostics);
    ComPtr<IDWriteRenderingParams> defaults,parameters;Check(write->CreateRenderingParams(&defaults));
    Check(write->CreateCustomRenderingParams(defaults->GetGamma(),defaults->GetEnhancedContrast(),defaults->GetClearTypeLevel(),defaults->GetPixelGeometry(),DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,&parameters));
    target->SetTextRenderingParams(parameters.Get());target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    auto notebook=Notebook::Load(L"Tests/Fixtures/images.ipynb");
    auto image=notebook.Cells()[0].output->Blocks()[0].image;
    auto profiled=Notebook::Load(L"Tests/Fixtures/images-icc.ipynb");
    auto profileImage=profiled.Cells()[0].output->Blocks()[0].image;
    auto wide=Notebook::Load(L"Tests/Fixtures/images-wide.ipynb");
    auto wideImage=wide.Cells()[0].output->Blocks()[0].image;
    if(!wideImage || wideImage->Width()!=4097)throw std::runtime_error("Large image was rejected or reduced");
    if(profileImage->Profile().empty())throw std::runtime_error("ICC profile was discarded");
    auto uploaded=std::make_shared<const ImageUpload>(device.Get(),notebook,std::function<bool()>{});
    auto uploadedProfile=std::make_shared<const ImageUpload>(device.Get(),profiled,std::function<bool()>{});
    auto uploadedWide=std::make_shared<const ImageUpload>(device.Get(),wide,std::function<bool()>{});
    if(uploaded->Count()!=1 || uploadedProfile->Count()!=1 || uploadedWide->Count()!=1)throw std::runtime_error("Eager GPU images were not prepared");
    bool cancelled=false;
    try {ImageUpload rejected(device.Get(),notebook,[]{return true;});}catch(const std::runtime_error&){cancelled=true;}
    if(!cancelled)throw std::runtime_error("Image upload ignored cancelled open");
    ImageCache images;auto imageResults=nlohmann::json::array();
    for(bool eager:{false,true})for(const auto& sourceImage:{image,profileImage,wideImage})for(bool hdr:{false,true})for(float scale:{1.f,1.25f,2.f})for(float sizing:{1.f,.5f})for(int repeat=0;repeat<2;++repeat) {
        images.SetUploads(eager?(sourceImage==image?uploaded:sourceImage==profileImage?uploadedProfile:uploadedWide):nullptr);
        const float imageWidth=float(sourceImage->Width()), imageHeight=float(sourceImage->Height());
        const float white=hdr?3.5f:1.f;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=unsigned(std::ceil(imageWidth*scale*sizing));desc.Height=unsigned(std::ceil(imageHeight*scale*sizing));
        desc.MipLevels=desc.ArraySize=1;desc.Format=hdr?DXGI_FORMAT_R16G16B16A16_FLOAT:DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        ComPtr<ID3D11Texture2D> texture;Check(device->CreateTexture2D(&desc,nullptr,&texture));
        ComPtr<IDXGISurface> surface;Check(texture.As(&surface));
        D2D1_BITMAP_PROPERTIES1 props{};props.pixelFormat={desc.Format,D2D1_ALPHA_MODE_IGNORE};props.dpiX=props.dpiY=96;
        props.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        ComPtr<ID2D1Bitmap1> bitmap;Check(target->CreateBitmapFromDxgiSurface(surface.Get(),&props,&bitmap));
        target->SetTarget(bitmap.Get());target->SetDpi(96*scale,96*scale);
        target->BeginDraw();target->Clear(D2D1::ColorF(white,white,white));
        if(!images.Draw(target.Get(),target.Get(),sourceImage,0,0,imageWidth*sizing,imageHeight*sizing,hdr,white))throw std::runtime_error("Valid image unavailable");
        Check(target->EndDraw());target->SetTarget(nullptr);
        desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;Check(device->CreateTexture2D(&desc,nullptr,&staging));gpu->CopyResource(staging.Get(),texture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};Check(gpu->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
        float maximum=0;
        const auto points=sourceImage==wideImage
            ? std::vector<std::pair<int,int>>{{100,8},{4000,8},{4000,24},{100,24}}
            : std::vector<std::pair<int,int>>{{100,200},{400,220},{400,64},{100,20}};
        for(auto point:points) {
            const auto source=&sourceImage->Pixels()[(point.second*sourceImage->Width()+point.first)*4];float alpha=source[3]/255.f;
            auto row=static_cast<const unsigned char*>(mapped.pData)+int(point.second*scale*sizing)*mapped.RowPitch;
            int col=int(point.first*scale*sizing);
            for(int ch=0;ch<3;++ch) {
                float expected=hdr?(alpha?RenderPolicy::SrgbToLinear(source[ch]/255.f/alpha)*alpha:0)+(1-alpha):source[ch]/255.f+(1-alpha);
                float actual=hdr?RenderPolicy::FromHalf(reinterpret_cast<const uint16_t*>(row)[col*4+2-ch])/white:row[col*4+ch]/255.f;
                if(std::abs(actual-expected)>2.f/255)std::cerr<<"image sample scale="<<scale<<" sizing="<<sizing<<" repeat="<<repeat<<" point="<<point.first<<","<<point.second<<" channel="<<ch<<" expected="<<expected<<" actual="<<actual<<" alpha="<<alpha<<"\n";
                maximum=std::max(maximum,std::abs(actual-expected));
            }
        }
        gpu->Unmap(staging.Get(),0);
        if(maximum>2.f/255)throw std::runtime_error("Image alpha/color fidelity failed hdr="+std::to_string(hdr)+" delta="+std::to_string(maximum));
        imageResults.push_back({{"eager_gpu",eager},{"width",sourceImage->Width()},{"icc",!sourceImage->Profile().empty()},{"hdr",hdr},{"dpi_scale",scale},{"display_scale",sizing},{"repeat",repeat},{"max_linear_or_sdr_delta",maximum}});
    }
    images.Clear();
    for(int repeat=0;repeat<2;++repeat)
        if(images.Draw(target.Get(),nullptr,profileImage,0,0,512,256,false,1))throw std::runtime_error("Missing color-management capability accepted");
    images.Clear();
    std::ofstream("Artifacts/image-fidelity.json")<<imageResults.dump(2);
    std::cout<<"144 GPU image color/alpha/DPI/downscale/cache comparisons passed (direct and eager uploads)\n";
    size_t comparisons=0;
    auto results=nlohmann::json::array();
    for(bool hdr:{false,true}) {
        TextRasterCache cache(1024*1024);
        for(float scale:{1.f,1.25f,1.5f,2.f,1.f}) for(float phase:{0.f,0.25f}) for(int variant=0;variant<4;++variant) {
            auto text=variant==0?L"ASCII: ffi → Ångström gyp\nSecond line":variant==1?L"日本語 😀 العربية עברית":variant==2?L"Changed text after resize":L"Overflowing long line: ";
            std::wstring content=text;if(variant==3)content+=std::wstring(4096,L'W');
            auto layout=layouts.Get(content,180,17,variant==0,false,variant==2);
            auto color=[&](float value){return hdr?RenderPolicy::SrgbToLinear(value)*3.5f:value;};
            D2D1_COLOR_F ink=D2D1::ColorF(color(0.1f),color(variant==2?0.5f:0.1f),color(0.2f));
            auto draw=[&](bool cached) {
                D3D11_TEXTURE2D_DESC desc{};desc.Width=768;desc.Height=240;desc.MipLevels=desc.ArraySize=1;
                desc.Format=hdr?DXGI_FORMAT_R16G16B16A16_FLOAT:DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
                ComPtr<ID3D11Texture2D> texture;Check(device->CreateTexture2D(&desc,nullptr,&texture));
                ComPtr<IDXGISurface> surface;Check(texture.As(&surface));
                D2D1_BITMAP_PROPERTIES1 properties{};properties.pixelFormat={desc.Format,D2D1_ALPHA_MODE_IGNORE};properties.dpiX=properties.dpiY=96;
                properties.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
                ComPtr<ID2D1Bitmap1> bitmap;Check(target->CreateBitmapFromDxgiSurface(surface.Get(),&properties,&bitmap));
                target->SetTarget(bitmap.Get());target->SetDpi(96*scale,96*scale);
                ComPtr<ID2D1SolidColorBrush> brush;Check(target->CreateSolidColorBrush(ink,&brush));
                target->BeginDraw();target->Clear(D2D1::ColorF(color(0.97f),color(0.97f),color(0.97f)));
                target->PushAxisAlignedClip(D2D1::RectF(5,5,290,110),D2D1_ANTIALIAS_MODE_ALIASED);
                float x=12+phase,y=13+phase;
                if(!cached || !cache.Draw(target.Get(),layout.Get(),ink,x,y,scale))target->DrawTextLayout(D2D1::Point2F(x,y),layout.Get(),brush.Get());
                target->PopAxisAlignedClip();Check(target->EndDraw());target->SetTarget(nullptr);
                desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
                ComPtr<ID3D11Texture2D> staging;Check(device->CreateTexture2D(&desc,nullptr,&staging));gpu->CopyResource(staging.Get(),texture.Get());
                D3D11_MAPPED_SUBRESOURCE mapped{};Check(gpu->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
                std::vector<int> pixels;pixels.reserve(768*240*3);
                for(unsigned row=0;row<240;++row)for(unsigned col=0;col<768;++col)for(unsigned ch=0;ch<3;++ch) {
                    auto data=static_cast<const uint8_t*>(mapped.pData)+row*mapped.RowPitch;
                    int value=hdr?(int)std::lround(RenderPolicy::LinearToSrgb(RenderPolicy::FromHalf(reinterpret_cast<const uint16_t*>(data)[col*4+ch])/3.5f)*255):data[col*4+ch];
                    pixels.push_back(value);
                }
                gpu->Unmap(staging.Get(),0);return pixels;
            };
            auto reference=draw(false);
            for(int repeat=0;repeat<2;++repeat) {
                auto cached=draw(true);int maximum=0;
                for(size_t i=0;i<cached.size();++i)maximum=std::max(maximum,std::abs(cached[i]-reference[i]));
                if(maximum>1)throw std::runtime_error("Text cache pixels differ: hdr="+std::to_string(hdr)+" scale="+std::to_string(scale)+" phase="+std::to_string(phase)+" variant="+std::to_string(variant)+" max="+std::to_string(maximum));
                results.push_back({{"hdr",hdr},{"scale",scale},{"fractional_origin",phase},{"variant",variant},{"repeat",repeat},{"max_channel_delta",maximum},{"cache_bytes",cache.Bytes()}});
                ++comparisons;
            }
            if(cache.Bytes()>1024*1024)throw std::runtime_error("GPU text cache exceeded byte budget");
        }
        if(!cache.Hits() || !cache.Misses())throw std::runtime_error("Fidelity test did not exercise cold and warm caches");
        if(!cache.Evictions())throw std::runtime_error("Fidelity test did not exercise cache eviction");
        cache.Clear();if(cache.Bytes())throw std::runtime_error("GPU text cache clear retained tracked allocations");
    }
    std::cout<<comparisons<<" GPU cache fidelity comparisons passed (SDR/HDR, DPI, fractional origins, mutation, overflow, eviction)\n";
    std::ofstream("Artifacts/text-cache-fidelity.json")<<nlohmann::json{{"comparisons",comparisons},{"maximum_allowed_channel_delta",1},{"results",results}}.dump(2);
}
