#include "NotebookImage.h"
#include "LoadMetrics.h"
#include <windows.h>
#include <wincrypt.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <stdexcept>
#include <fstream>
#include <filesystem>
using Microsoft::WRL::ComPtr;
namespace {
void CheckImage(HRESULT hr) {if(FAILED(hr))throw std::runtime_error("Cannot decode image");}
}
NotebookImage::NotebookImage(std::string_view base64) {
    LoadMetrics::Scope timing(LoadMetrics::ImageTotal);
    if(base64.size()>MAXDWORD)throw std::runtime_error("A single encoded image exceeds the Windows decoder range");
    // Upper bound includes whitespace/padding. Validate and decode in one pass.
    DWORD count=static_cast<DWORD>((base64.size()/4+1)*3);
    std::vector<unsigned char> bytes(count);
    if(base64.empty() || !CryptStringToBinaryA(base64.data(),static_cast<DWORD>(base64.size()),CRYPT_STRING_BASE64|CRYPT_STRING_STRICT,bytes.data(),&count,nullptr,nullptr))
        throw std::runtime_error("Invalid base64 image");
    bytes.resize(count);
    Decode(bytes);
}
NotebookImage::NotebookImage(std::vector<unsigned char> bytes) { Decode(bytes); }
std::shared_ptr<const NotebookImage> NotebookImage::Load(const std::wstring& path) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open image file");
    const auto size=file.tellg();
    if(size<0 || static_cast<uint64_t>(size)>MAXDWORD)throw std::runtime_error("Image exceeds the Windows decoder range");
    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot read image file");
    return std::shared_ptr<const NotebookImage>(new NotebookImage(std::move(bytes)));
}
void NotebookImage::Decode(std::vector<unsigned char>& bytes) {
    LoadMetrics::Scope timing(LoadMetrics::ImageDecode);
    if (bytes.size()>MAXDWORD) throw std::runtime_error("Image exceeds the Windows decoder range");
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    struct Apartment {HRESULT hr;~Apartment(){if(SUCCEEDED(hr))CoUninitialize();}} apartment{initialized};
    if(FAILED(initialized) && initialized!=RPC_E_CHANGED_MODE)CheckImage(initialized);
    ComPtr<IWICImagingFactory> factory;
    CheckImage(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream;CheckImage(factory->CreateStream(&stream));
    CheckImage(stream->InitializeFromMemory(bytes.data(),static_cast<DWORD>(bytes.size())));
    ComPtr<IWICBitmapDecoder> decoder;CheckImage(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnDemand,&decoder));
    GUID format{};CheckImage(decoder->GetContainerFormat(&format));
    if(format!=GUID_ContainerFormatPng && format!=GUID_ContainerFormatJpeg)throw std::runtime_error("Only PNG/JPEG images are supported");
    if(format==GUID_ContainerFormatPng && bytes.size()>24 && bytes[24]>8)throw std::runtime_error("PNG depths above 8 bits per channel are not supported yet");
    UINT frames=0;CheckImage(decoder->GetFrameCount(&frames));
    if(frames!=1)throw std::runtime_error("Animated images are not supported");
    ComPtr<IWICBitmapFrameDecode> frame;CheckImage(decoder->GetFrame(0,&frame));
    CheckImage(frame->GetSize(&width,&height));
    if(!width || !height || width>16384 || height>16384)throw std::runtime_error("Image exceeds the 16384-pixel Direct3D texture range");
    ComPtr<IWICColorContext> color;CheckImage(factory->CreateColorContext(&color));
    IWICColorContext* colors[]={color.Get()};UINT colorCount=0;
    if(SUCCEEDED(frame->GetColorContexts(1,colors,&colorCount)) && colorCount) {
        WICColorContextType kind=WICColorContextUninitialized;CheckImage(color->GetType(&kind));
        if(kind==WICColorContextProfile) {
            UINT size=0;CheckImage(color->GetProfileBytes(0,nullptr,&size));
            if(size>1024*1024)throw std::runtime_error("Image color profile exceeds 1 MiB");
            profile.resize(size);CheckImage(color->GetProfileBytes(size,profile.data(),&size));
        } else if(kind==WICColorContextExifColorSpace) {
            UINT space=0;CheckImage(color->GetExifColorSpace(&space));
            if(space!=1)throw std::runtime_error("Unsupported image EXIF color space");
        }
    }
    ComPtr<IWICFormatConverter> converter;CheckImage(factory->CreateFormatConverter(&converter));
    CheckImage(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    pixels.resize(static_cast<size_t>(width)*height*4);
    CheckImage(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data()));
}
unsigned NotebookImage::Width() const{return width;}
unsigned NotebookImage::Height() const{return height;}
const std::vector<unsigned char>& NotebookImage::Pixels() const{return pixels;}
const std::vector<unsigned char>& NotebookImage::Profile() const{return profile;}
