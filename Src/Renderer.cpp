#include "Renderer.h"
#include "LoadMetrics.h"
#include "CellOutput.h"
#include "ImageCache.h"
#include "RenderPolicy.h"
#include "Notebook.h"
#include "Markdown.h"
#include "FontSet.h"
#include "RasterContract.h"
#include "TextRasterCache.h"
#include "TextLayoutCache.h"
#include <d3d11.h>
#include <d2d1_1.h>
// Only COM declarations; the entry point is loaded dynamically on Windows 8+.
#pragma push_macro("NTDDI_VERSION")
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN8
#include <dcomp.h>
#pragma pop_macro("NTDDI_VERSION")
#include <dwrite.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <tuple>
#include <vector>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
namespace {
using CreateComposition=HRESULT(WINAPI*)(IDXGIDevice*,REFIID,void**);
CreateComposition CompositionEntry() {
    // Dynamic system-directory load keeps the Windows 7 loader path viable.
    static HMODULE module=[] {wchar_t directory[MAX_PATH]{};if(!GetSystemDirectoryW(directory,MAX_PATH))return (HMODULE)nullptr;
        return LoadLibraryW((std::wstring(directory)+L"\\dcomp.dll").c_str());}();
    return module?reinterpret_cast<CreateComposition>(GetProcAddress(module,"DCompositionCreateDevice")):nullptr;
}
void Check(HRESULT hr, const char* operation) {
    if(FAILED(hr)) throw std::runtime_error(std::string(operation)+" failed (HRESULT "+std::to_string((unsigned long)hr)+")");
}
float SystemWhite(HWND window) {
    MONITORINFOEXW monitor{}; monitor.cbSize=sizeof(monitor);
    if(!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)) return 1;
    // These entry points exist on Windows 7; unsupported newer query types fail safely.
    for(int attempt=0;attempt<3;++attempt) {
        UINT32 pathCount=0, modeCount=0;
        if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&pathCount,&modeCount)!=ERROR_SUCCESS) break;
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        LONG result=QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&pathCount,paths.data(),&modeCount,modes.data(),nullptr);
        if(result==ERROR_INSUFFICIENT_BUFFER) continue;
        if(result!=ERROR_SUCCESS) break;
        for(UINT32 i=0;i<pathCount;++i) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header={DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,sizeof(source),paths[i].sourceInfo.adapterId,paths[i].sourceInfo.id};
            if(DisplayConfigGetDeviceInfo(&source.header)!=ERROR_SUCCESS || wcscmp(source.viewGdiDeviceName,monitor.szDevice)) continue;
            DISPLAYCONFIG_SDR_WHITE_LEVEL white{};
            white.header={DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL,sizeof(white),paths[i].targetInfo.adapterId,paths[i].targetInfo.id};
            if(DisplayConfigGetDeviceInfo(&white.header)==ERROR_SUCCESS && white.SDRWhiteLevel)
                return std::clamp(white.SDRWhiteLevel/1000.0f,1.0f,12.5f);
        }
        break;
    }
    return 1;
}
bool DisplayHdr(HWND window, IDXGIFactory1* factory) {
    auto monitor=MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST);
    ComPtr<IDXGIAdapter1> adapter;
    for(UINT a=0;factory->EnumAdapters1(a,&adapter)==S_OK;++a,adapter.Reset()) {
        ComPtr<IDXGIOutput> output;
        for(UINT o=0;adapter->EnumOutputs(o,&output)==S_OK;++o,output.Reset()) {
            DXGI_OUTPUT_DESC desc{}; output->GetDesc(&desc);
            if(desc.Monitor!=monitor) continue;
            ComPtr<IDXGIOutput6> newer; DXGI_OUTPUT_DESC1 info{};
            return SUCCEEDED(output.As(&newer)) && SUCCEEDED(newer->GetDesc1(&info)) &&
                info.ColorSpace==DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
        }
    }
    return false;
}
}
bool CompositionAvailable(){return CompositionEntry()!=nullptr;}
int PrepareNotebookLayout(Notebook& notebook) {
    LoadMetrics::Scope timing(LoadMetrics::NotebookLayout);
    ComPtr<IDWriteFactory> factory;
    Check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.GetAddressOf())),"Notebook measurement factory");
    FontSet fonts(factory.Get());
    std::vector<CellLayout> layouts;
    layouts.reserve(notebook.Cells().size());
    for(const auto& cell:notebook.Cells()) {
        CellLayout bounds;
        float cellWidth=0;
        bool heading=cell.text->kind==L"markdown" && cell.text->source.rfind(L"# ",0)==0 && cell.text->source.find(L'\n')==std::wstring::npos;
        auto measure=[&](const std::wstring& text,float size,bool mono,bool bold) {
            ComPtr<IDWriteTextFormat> format;
            Check(factory->CreateTextFormat(mono?L"Noto Sans Mono":L"Noto Sans",fonts.Collection(),bold?DWRITE_FONT_WEIGHT_BOLD:DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&format),"Notebook measurement format");
            Check(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP),"Notebook measurement wrapping");
            ComPtr<IDWriteTextLayout> layout;
            Check(factory->CreateTextLayout(text.data(),(UINT32)text.size(),format.Get(),1,1,&layout),"Notebook measurement layout");
            DWRITE_TEXT_METRICS metrics{};Check(layout->GetMetrics(&metrics),"Notebook text width");
            cellWidth=std::max(cellWidth,metrics.widthIncludingTrailingWhitespace);
            return (int)std::ceil(metrics.height);
        };
        if(cell.text->kind==L"markdown" && !cell.sourceMode) {
            bounds.markdown=cell.markdown && !cell.markdown->NeedsImages()?cell.markdown:Markdown::Prepare(factory.Get(),fonts.Collection(),cell.text->source,cell.original->contains("attachments")?&cell.original->at("attachments"):nullptr,notebook.Directory());
            bounds.sourceHeight=bounds.markdown->Height();cellWidth=static_cast<float>(bounds.markdown->Width()-4);
        } else bounds.sourceHeight=measure(heading?cell.text->source.substr(2):cell.text->source,heading?27.f:cell.text->kind==L"code"?17.f:18.f,cell.text->kind==L"code" || cell.text->kind==L"raw",heading);
        bounds.sourceWidth=int(std::ceil(cellWidth))+4;
        bounds.outputHeight=cell.text->output.empty()?0:measure(cell.text->output,17,true,false);
        if(cell.output) {bounds.outputHeight=cell.output->Height();cellWidth=std::max(cellWidth,float(cell.output->Width()));}
        bounds.contentWidth=(int)std::ceil(cellWidth)+4;
        layouts.push_back(bounds);
    }
    notebook = notebook.WithLayout(layouts);
    return notebook.ContentWidth();
}

struct Renderer::Impl {
    HWND window;
    RenderMetrics metrics;
    float scale=1;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> gpu;
    ComPtr<IDXGIFactory1> factory;
    ComPtr<IDXGISwapChain> chain;
    ComPtr<IDCompositionDevice> composition;
    ComPtr<IDCompositionTarget> compositionTarget;
    ComPtr<IDCompositionVisual> visual;
    ComPtr<ID2D1Factory> d2d;
    ComPtr<ID2D1DeviceContext> context;
    ComPtr<ID2D1RenderTarget> target;
    ComPtr<ID2D1SolidColorBrush> brush;
    ComPtr<IDWriteFactory> write;
    ComPtr<IDWriteRenderingParams> rendering;
    std::unique_ptr<FontSet> fonts;
    unsigned width=0,height=0,bufferWidth=0,bufferHeight=0,refreshHz=0;
    bool hdr=false, modern=false, recover=false, forceSdr=false, diagnostics=false;
    float white=1;
    std::wstring adapterName;
    std::unique_ptr<TextLayoutCache> layouts;
    TextRasterCache textRasters;
    ImageCache images;
    explicit Impl(HWND hwnd,bool sdr,bool diagnostics):window(hwnd),metrics(diagnostics),forceSdr(sdr),diagnostics(diagnostics) {
        // No WARP/reference/software fallback. A real GPU is a product requirement.
        D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
        Check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels,3,D3D11_SDK_VERSION,&device,nullptr,&gpu),"Hardware D3D11 device (feature level 10.0 or newer required)");
        ComPtr<IDXGIDevice> dxgi; Check(device.As(&dxgi),"DXGI device");
        Check(dxgi->SetGPUThreadPriority(0),"GPU scheduling priority");
        ComPtr<IDXGIDevice1> dxgi1;
        if(SUCCEEDED(dxgi.As(&dxgi1))) Check(dxgi1->SetMaximumFrameLatency(1),"Frame latency");
        ComPtr<IDXGIAdapter> adapter; Check(dxgi->GetAdapter(&adapter),"GPU adapter");
        DXGI_ADAPTER_DESC desc{}; Check(adapter->GetDesc(&desc),"GPU description"); adapterName=desc.Description;
        Check(adapter->GetParent(IID_PPV_ARGS(&factory)),"DXGI factory");
        Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,d2d.GetAddressOf()),"Direct2D factory");
        ComPtr<ID2D1Factory1> newer;
        if(SUCCEEDED(d2d.As(&newer))) {
            ComPtr<ID2D1Device> d2dDevice;
            if(auto create=CompositionEntry()) {
                Check(create(dxgi.Get(),__uuidof(IDCompositionDevice),reinterpret_cast<void**>(composition.GetAddressOf())),"DirectComposition device");
                Check(composition->CreateTargetForHwnd(window,TRUE,&compositionTarget),"Composition window target");
                Check(composition->CreateVisual(&visual),"Composition visual");
                Check(compositionTarget->SetRoot(visual.Get()),"Composition root");
            }
            Check(newer->CreateDevice(dxgi.Get(),&d2dDevice),"GPU Direct2D device");
            Check(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&context),"GPU drawing context");
        }
        Check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write.GetAddressOf())),"DirectWrite");
        fonts=std::make_unique<FontSet>(write.Get());
        layouts=std::make_unique<TextLayoutCache>(write.Get(),fonts->Collection(),metrics);
    }
    void ReleaseTarget() {
        if(!context)images.Clear();
        brush.Reset(); target.Reset(); if(context) context->SetTarget(nullptr);
        gpu->ClearState(); gpu->Flush();
    }
    void Resize(unsigned w,unsigned h,bool refresh) {
        if(!w || !h) return;
        if(chain && width==w && height==h && !refresh) return;
        RenderMetrics::Scope resizeTiming(metrics,RenderMetrics::Stage::Resize);
        bool wantHdr;float nextWhite;
        {
        RenderMetrics::Scope displayTiming(metrics,RenderMetrics::Stage::Display);
        if(!factory->IsCurrent()) {
            factory.Reset();
            Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"Refresh display capabilities");
        }
        MONITORINFOEXW monitor{};monitor.cbSize=sizeof(monitor);DEVMODEW mode{};mode.dmSize=sizeof(mode);
        if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor) && EnumDisplaySettingsExW(monitor.szDevice,ENUM_CURRENT_SETTINGS,&mode,0))refreshHz=mode.dmDisplayFrequency;
        wantHdr=!forceSdr && context && DisplayHdr(window,factory.Get());
        nextWhite=wantHdr?SystemWhite(window):1;
        }
        if(refresh || !rendering) {
            textRasters.Clear();
            ComPtr<IDWriteRenderingParams> defaults;
            Check(write->CreateRenderingParams(&defaults),"Text rendering defaults");
            rendering.Reset();
            Check(write->CreateCustomRenderingParams(defaults->GetGamma(),defaults->GetEnhancedContrast(),defaults->GetClearTypeLevel(),
                defaults->GetPixelGeometry(),DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,&rendering),"High-DPI text rendering");
            if(target)target->SetTextRenderingParams(rendering.Get());
        }
        if(chain && width==w && height==h && hdr==wantHdr){white=nextWhite;return;}
        unsigned allocatedWidth=w,allocatedHeight=h;
        if(!context || wantHdr!=hdr || nextWhite!=white)textRasters.Clear();
        ReleaseTarget();
        DXGI_FORMAT format=wantHdr?DXGI_FORMAT_R16G16B16A16_FLOAT:DXGI_FORMAT_B8G8R8A8_UNORM;
        if(chain && wantHdr==hdr) {
            Check(chain->ResizeBuffers(0,allocatedWidth,allocatedHeight,format,0),"Resize GPU back buffers");
        } else {
            chain.Reset(); gpu->Flush();
            ComPtr<IDXGIFactory2> factory2;
            modern=SUCCEEDED(factory.As(&factory2));
            if(modern) {
                DXGI_SWAP_CHAIN_DESC1 desc{}; desc.Width=allocatedWidth; desc.Height=allocatedHeight; desc.Format=format;
                desc.SampleDesc.Count=1; desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
                desc.BufferCount=2; desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; desc.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
                // Composition requires STRETCH in the descriptor, but the visual
                // has an identity transform and an exact physical-pixel surface.
                // No HWND auto-scaling or scaled cached frame is used.
                desc.Scaling=composition?DXGI_SCALING_STRETCH:DXGI_SCALING_NONE;
                ComPtr<IDXGISwapChain1> created;
                if(composition)Check(factory2->CreateSwapChainForComposition(device.Get(),&desc,nullptr,&created),"Create composition presentation");
                else Check(factory2->CreateSwapChainForHwnd(device.Get(),window,&desc,nullptr,nullptr,&created),"Create flip presentation");
                Check(created.As(&chain),"Swap chain");
            } else {
                wantHdr=false; format=DXGI_FORMAT_B8G8R8A8_UNORM;
                DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferDesc.Width=allocatedWidth; desc.BufferDesc.Height=allocatedHeight; desc.BufferDesc.Format=format;
                desc.SampleDesc.Count=1; desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount=2;
                desc.OutputWindow=window; desc.Windowed=TRUE; desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
                Check(factory->CreateSwapChain(device.Get(),&desc,&chain),"Create Windows 7 presentation");
            }
            Check(factory->MakeWindowAssociation(window,DXGI_MWA_NO_WINDOW_CHANGES|DXGI_MWA_NO_ALT_ENTER),"Windowed presentation");
        }
        if(wantHdr) {
            ComPtr<IDXGISwapChain3> colorChain;
            UINT support=0;
            if(FAILED(chain.As(&colorChain)) || FAILED(colorChain->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,&support)) ||
                !(support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
                // Recreate as SDR when FP16 exists but scRGB presentation does not.
                colorChain.Reset();chain.Reset();gpu->Flush();
                hdr=false; wantHdr=false; format=DXGI_FORMAT_B8G8R8A8_UNORM;
                ComPtr<IDXGIFactory2> f2; Check(factory.As(&f2),"SDR fallback factory");
                DXGI_SWAP_CHAIN_DESC1 desc{}; desc.Width=allocatedWidth; desc.Height=allocatedHeight; desc.Format=format; desc.SampleDesc.Count=1;
                desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount=2; desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
                desc.Scaling=composition?DXGI_SCALING_STRETCH:DXGI_SCALING_NONE;
                ComPtr<IDXGISwapChain1> created;
                if(composition)Check(f2->CreateSwapChainForComposition(device.Get(),&desc,nullptr,&created),"Composition SDR fallback");
                else Check(f2->CreateSwapChainForHwnd(device.Get(),window,&desc,nullptr,nullptr,&created),"SDR fallback");
                Check(created.As(&chain),"SDR fallback chain");
            } else Check(colorChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709),"scRGB color space");
        }
        if(composition) {
            Check(visual->SetContent(chain.Get()),"Composition content");
            D2D_RECT_F clip{0,0,(float)w,(float)h};Check(visual->SetClip(clip),"Composition client clip");
        }
        hdr=wantHdr; white=hdr?nextWhite:1;
        ComPtr<IDXGISurface> surface; Check(chain->GetBuffer(0,IID_PPV_ARGS(&surface)),"GPU drawing surface");
        if(context) {
            D2D1_BITMAP_PROPERTIES1 props{}; props.pixelFormat={format,D2D1_ALPHA_MODE_IGNORE};
            props.dpiX=props.dpiY=96; props.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            ComPtr<ID2D1Bitmap1> bitmap;
            Check(context->CreateBitmapFromDxgiSurface(surface.Get(),&props,&bitmap),"GPU target bitmap");
            context->SetTarget(bitmap.Get()); Check(context.As(&target),"Drawing target");
        } else {
            auto props=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_HARDWARE,D2D1::PixelFormat(format,D2D1_ALPHA_MODE_IGNORE));
            Check(d2d->CreateDxgiSurfaceRenderTarget(surface.Get(),&props,&target),"Windows 7 hardware drawing target");
        }
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        target->SetTextRenderingParams(rendering.Get());
        Check(target->CreateSolidColorBrush(D2D1::ColorF(0,0,0),&brush),"GPU brush");
        width=w; height=h;bufferWidth=allocatedWidth;bufferHeight=allocatedHeight;
    }
    D2D1_COLOR_F Color(COLORREF value) const {
        float r=GetRValue(value)/255.f,g=GetGValue(value)/255.f,b=GetBValue(value)/255.f;
        if(hdr) {r=RenderPolicy::SrgbToLinear(r)*white;g=RenderPolicy::SrgbToLinear(g)*white;b=RenderPolicy::SrgbToLinear(b)*white;}
        return D2D1::ColorF(r,g,b,1);
    }
    void Snapshot(const std::wstring& path) {
        if(path.empty()) return;
        // Explicit test artifact: GPU back-buffer readback only when requested.
        ComPtr<ID3D11Texture2D> back,staging;
        Check(chain->GetBuffer(0,IID_PPV_ARGS(&back)),"Diagnostic back buffer");
        D3D11_TEXTURE2D_DESC desc{}; back->GetDesc(&desc);
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
        Check(device->CreateTexture2D(&desc,nullptr,&staging),"Diagnostic staging buffer");
        gpu->CopyResource(staging.Get(),back.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{}; Check(gpu->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"Diagnostic readback");
        std::vector<uint32_t> pixels((size_t)width*height);
        for(unsigned y=0;y<height;++y) {
            auto row=static_cast<const uint8_t*>(mapped.pData)+(size_t)y*mapped.RowPitch;
            if(!hdr) std::memcpy(pixels.data()+(size_t)y*width,row,(size_t)width*4);
            else for(unsigned x=0;x<width;++x) {
                auto p=reinterpret_cast<const uint16_t*>(row)+x*4;
                auto channel=[&](uint16_t bits) {
                    float v=RenderPolicy::FromHalf(bits)/white;
                    v=RenderPolicy::LinearToSrgb(v);
                    return (uint32_t)std::lround(std::clamp(v,0.f,1.f)*255);
                };
                pixels[(size_t)y*width+x]=channel(p[2])|(channel(p[1])<<8)|(channel(p[0])<<16)|0xff000000;
            }
        }
        gpu->Unmap(staging.Get(),0);
        BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+width*height*4;
        BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=width;info.biHeight=-(LONG)height;info.biPlanes=1;info.biBitCount=32;
        std::ofstream stream(std::filesystem::path(path),std::ios::binary);
        stream.write(reinterpret_cast<const char*>(&file),sizeof(file));stream.write(reinterpret_cast<const char*>(&info),sizeof(info));
        stream.write(reinterpret_cast<const char*>(pixels.data()),(std::streamsize)pixels.size()*4);
    }
};
Renderer::Renderer(HWND window,bool forceSdr,bool diagnostics):impl(std::make_unique<Impl>(window,forceSdr,diagnostics)){}
Renderer::~Renderer()=default;
void Renderer::Resize(unsigned w,unsigned h,bool refresh) {
    if(impl->recover) impl=std::make_unique<Impl>(impl->window,impl->forceSdr,impl->diagnostics);
    impl->Resize(w,h,refresh);
}
void Renderer::SetHdrEnabled(bool enabled) {impl->forceSdr=!enabled;}
void Renderer::Begin(float scale) {
    if(!std::isfinite(scale) || scale<=0)throw std::invalid_argument("Invalid display scale");
    // Text layouts retain geometry in DIPs, not raster pixels. Direct2D rasterizes
    // outlines anew at this target DPI; we never enlarge a cached window image.
    impl->scale=scale;
    impl->target->SetDpi(96*scale,96*scale); impl->target->BeginDraw();
    impl->target->Clear(impl->Color(RGB(255,255,255)));
}
bool Renderer::Present(const std::wstring& snapshot) {
    HRESULT draw;
    {RenderMetrics::Scope timing(impl->metrics,RenderMetrics::Stage::EndDraw);draw=impl->target->EndDraw();}
    if(draw==D2DERR_RECREATE_TARGET || draw==DXGI_ERROR_DEVICE_REMOVED || draw==DXGI_ERROR_DEVICE_RESET) {impl->recover=true;return false;}
    Check(draw,"GPU drawing");
    impl->Snapshot(snapshot);
    HRESULT hr;
    {RenderMetrics::Scope timing(impl->metrics,RenderMetrics::Stage::Present);hr=impl->chain->Present(1,0);} // Vsync, render worker only, no tearing flag.
    if(hr==DXGI_ERROR_DEVICE_REMOVED || hr==DXGI_ERROR_DEVICE_RESET) {impl->recover=true;return false;}
    Check(hr,"Vsync presentation");
    if(impl->composition){RenderMetrics::Scope timing(impl->metrics,RenderMetrics::Stage::Commit);Check(impl->composition->Commit(),"Commit complete composition frame");}
    return hr!=DXGI_STATUS_OCCLUDED;
}
void Renderer::RoundedBox(float x,float y,float w,float h,float radius,COLORREF color,float opacity) {
    if(w<=0 || h<=0 || opacity<=0)return;
    auto ink=impl->Color(color);ink.a=std::clamp(opacity,0.f,1.f);impl->brush->SetColor(ink);
    impl->target->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,y,x+w,y+h),radius,radius),impl->brush.Get());
}
void Renderer::Box(float x,float y,float w,float h,COLORREF color) {
    if(w<=0 || h<=0) return;
    impl->brush->SetColor(impl->Color(color));
    impl->target->FillRectangle(D2D1::RectF(x,y,x+w,y+h),impl->brush.Get());
}
float Renderer::WrappedTextHeight(const std::wstring& text,float width,float size) {
    if(text.empty() || width<=0)return 0;
    auto layout=impl->layouts->Get(text,width,size,false,false,false,false,true);
    DWRITE_TEXT_METRICS metrics{};layout->GetMetrics(&metrics);return metrics.height;
}
void Renderer::WrappedText(const std::wstring& text,float x,float y,float width,float size,COLORREF color) {
    if(text.empty() || width<=0)return;
    auto layout=impl->layouts->Get(text,width,size,false,false,false,false,true);
    impl->brush->SetColor(impl->Color(color));
    if(!impl->textRasters.Draw(impl->target.Get(),layout.Get(),impl->Color(color),x,y,impl->scale))
        impl->target->DrawTextLayout(D2D1::Point2F(x,y),layout.Get(),impl->brush.Get());
}
float Renderer::MeasureText(const std::wstring& text,float width,float size,bool bold,bool extraBold) {
    auto layout=impl->layouts->Get(text,width,size,false,bold,true,extraBold);
    DWRITE_TEXT_METRICS metrics{};layout->GetMetrics(&metrics);return std::min(width,metrics.widthIncludingTrailingWhitespace);
}
float Renderer::Text(const std::wstring& text,float x,float y,float w,float h,float size,COLORREF color,bool mono,bool bold,bool ellipsis,bool extraBold) {
    if(text.empty() || w<=0 || h<=0) return 0;
    auto layout=impl->layouts->Get(text,w,size,mono,bold,ellipsis,extraBold);
    impl->brush->SetColor(impl->Color(color));
    impl->target->PushAxisAlignedClip(D2D1::RectF(x,y,x+w,y+h),D2D1_ANTIALIAS_MODE_ALIASED);
    if(!impl->textRasters.Draw(impl->target.Get(),layout.Get(),impl->Color(color),x,y,impl->scale))
        impl->target->DrawTextLayout(D2D1::Point2F(x,y),layout.Get(),impl->brush.Get());
    impl->target->PopAxisAlignedClip();
    DWRITE_TEXT_METRICS metrics{};layout->GetMetrics(&metrics);return std::min(w,metrics.widthIncludingTrailingWhitespace);
}
void Renderer::PushClip(float l,float t,float r,float b){impl->target->PushAxisAlignedClip(D2D1::RectF(l,t,r,b),D2D1_ANTIALIAS_MODE_ALIASED);}
void Renderer::PopClip(){impl->target->PopAxisAlignedClip();}
void Renderer::PreparedText(IDWriteTextLayout* layout,float x,float y) {
    auto color=impl->Color(RGB(25,25,25));impl->brush->SetColor(color);
    if(!impl->textRasters.Draw(impl->target.Get(),layout,color,x,y,impl->scale))
        impl->target->DrawTextLayout(D2D1::Point2F(x,y),layout,impl->brush.Get());
}
void Renderer::CellInput(const CellEditView& view, float x, float y) {
    if (!view.layout) return;
    if (view.caret!=view.anchor) {
        UINT32 count=0;
        const auto first=static_cast<UINT32>(std::min(view.caret,view.anchor));
        const auto length=static_cast<UINT32>(std::max(view.caret,view.anchor)-first);
        view.layout->HitTestTextRange(first,length,x,y,nullptr,0,&count);
        std::vector<DWRITE_HIT_TEST_METRICS> ranges(count);
        Check(view.layout->HitTestTextRange(first,length,x,y,ranges.data(),count,&count),"Cell selection");
        for (const auto& range:ranges) Box(range.left,range.top,std::max(2.f,range.width),range.height,RGB(194,225,250));
    }
    auto color=impl->Color(RGB(25,25,25));
    impl->brush->SetColor(color);
    if(!impl->textRasters.Draw(impl->target.Get(),view.layout.Get(),color,x,y,impl->scale))
        impl->target->DrawTextLayout(D2D1::Point2F(x,y),view.layout.Get(),impl->brush.Get());
    FLOAT caretX=0,caretY=0; DWRITE_HIT_TEST_METRICS hit{};
    Check(view.layout->HitTestTextPosition(static_cast<UINT32>(view.caret),FALSE,&caretX,&caretY,&hit),"Cell caret");
    Box(x+caretX,y+caretY,1,std::max(20.f,hit.height),RGB(25,25,25));
}
void Renderer::TitleInput(const std::wstring& text,size_t caret,size_t anchor,float x,float y,float width) {
    ComPtr<IDWriteTextFormat> format;
    Check(impl->write->CreateTextFormat(L"Noto Sans",impl->fonts->Collection(),DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,24,L"en-us",&format),"Title font");
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    ComPtr<IDWriteTextLayout> layout;
    Check(impl->write->CreateTextLayout(text.data(),(UINT32)text.size(),format.Get(),16384,40,&layout),"Title layout");
    FLOAT caretX=0,caretY=0;DWRITE_HIT_TEST_METRICS hit{};
    Check(layout->HitTestTextPosition((UINT32)caret,FALSE,&caretX,&caretY,&hit),"Title caret");
    float offset=std::max(0.f,caretX-width+18);
    Box(x-5,y-3,width+10,42,RGB(60,150,220));Box(x-4,y-2,width+8,40,RGB(255,255,255));
    PushClip(x,y,x+width,y+38);
    if(caret!=anchor) {
        DWRITE_HIT_TEST_METRICS ranges[256]{};UINT32 count=0;
        Check(layout->HitTestTextRange((UINT32)std::min(caret,anchor),(UINT32)(std::max(caret,anchor)-std::min(caret,anchor)),x-offset,y,ranges,256,&count),"Title selection");
        for(UINT32 i=0;i<count;++i)Box(ranges[i].left,ranges[i].top,ranges[i].width,ranges[i].height,RGB(194,225,250));
    }
    impl->brush->SetColor(impl->Color(RGB(35,35,35)));
    impl->target->DrawTextLayout(D2D1::Point2F(x-offset,y),layout.Get(),impl->brush.Get());
    // Steady caret: no idle animation timer or partially updated surface.
    Box(x+caretX-offset,y+3,1,30,RGB(35,35,35));PopClip();
}
bool Renderer::IsHdr()const{return impl->hdr;}
float Renderer::WhiteScale()const{return impl->white;}
unsigned Renderer::Width()const{return impl->width;}
unsigned Renderer::Height()const{return impl->height;}
std::wstring Renderer::AdapterName()const{return impl->adapterName;}

unsigned Renderer::RefreshHz()const{return impl->refreshHz;}
HRESULT Renderer::FrameStatistics(DXGI_FRAME_STATISTICS& statistics,unsigned& lastPresent)const {
    impl->chain->GetLastPresentCount(&lastPresent);return impl->chain->GetFrameStatistics(&statistics);
}

unsigned Renderer::AllocationWidth()const{return impl->bufferWidth;}
unsigned Renderer::AllocationHeight()const{return impl->bufferHeight;}

const RenderMetrics& Renderer::Metrics()const{return impl->metrics;}

size_t Renderer::TextCacheBytes()const{return impl->textRasters.Bytes();}
uint64_t Renderer::TextCacheHits()const{return impl->textRasters.Hits();}
uint64_t Renderer::TextCacheMisses()const{return impl->textRasters.Misses();}

ID3D11Device* Renderer::UploadDevice() const {return impl->context?impl->device.Get():nullptr;}
void Renderer::SetImageUploads(std::shared_ptr<const ImageUpload> uploads) {impl->images.SetUploads(std::move(uploads));}
void Renderer::Image(const std::shared_ptr<const NotebookImage>& image,float x,float y,float width,float height) {
    if(!impl->images.Draw(impl->target.Get(),impl->context.Get(),image,x,y,width,height,impl->hdr,impl->white))
        Text(L"[Image unavailable on this graphics device]",x,y,width,std::min(height,60.f),14,RGB(110,70,70),false,false);
}
