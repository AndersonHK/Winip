#include "FontSet.h"
#include "framework.h"
#include <filesystem>
#include <vector>
#include <string>
#include <stdexcept>
#include <new>

using Microsoft::WRL::ComPtr;
namespace {
class Enumerator final : public IDWriteFontFileEnumerator {
    LONG refs=1;
    ComPtr<IDWriteFactory> factory;
    std::vector<std::wstring> paths;
    size_t next=0;
    ComPtr<IDWriteFontFile> current;
public:
    Enumerator(IDWriteFactory* f,const std::vector<std::wstring>& p):factory(f),paths(p){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** result) override {
        if(!result) return E_POINTER;
        *result=nullptr;
        if(id==__uuidof(IUnknown) || id==__uuidof(IDWriteFontFileEnumerator)) {
            *result=static_cast<IDWriteFontFileEnumerator*>(this);AddRef();return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release() override{ULONG value=InterlockedDecrement(&refs);if(!value)delete this;return value;}
    HRESULT STDMETHODCALLTYPE MoveNext(BOOL* hasCurrent) override {
        if(!hasCurrent)return E_POINTER;
        current.Reset();*hasCurrent=FALSE;
        if(next==paths.size())return S_OK;
        HRESULT hr=factory->CreateFontFileReference(paths[next++].c_str(),nullptr,&current);
        if(SUCCEEDED(hr))*hasCurrent=TRUE;
        return hr;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentFontFile(IDWriteFontFile** file) override {
        if(!file)return E_POINTER;
        *file=nullptr;if(!current)return E_FAIL;
        return current.CopyTo(file);
    }
};
class Loader final : public IDWriteFontCollectionLoader {
    LONG refs=1;
    std::vector<std::wstring> paths;
public:
    explicit Loader(std::vector<std::wstring> p):paths(std::move(p)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** result) override {
        if(!result)return E_POINTER;
        *result=nullptr;
        if(id==__uuidof(IUnknown) || id==__uuidof(IDWriteFontCollectionLoader)) {
            *result=static_cast<IDWriteFontCollectionLoader*>(this);AddRef();return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release() override{ULONG value=InterlockedDecrement(&refs);if(!value)delete this;return value;}
    HRESULT STDMETHODCALLTYPE CreateEnumeratorFromKey(IDWriteFactory* factory,const void*,UINT32,IDWriteFontFileEnumerator** enumerator) override {
        if(!enumerator)return E_POINTER;
        *enumerator=nullptr;
        try {*enumerator=new Enumerator(factory,paths);return S_OK;}catch(...){return E_OUTOFMEMORY;}
    }
};
}
FontSet::FontSet(IDWriteFactory* value):factory(value) {
    wchar_t executable[32768];DWORD size=GetModuleFileNameW(nullptr,executable,32768);
    if(!size || size==32768)throw std::runtime_error("Cannot locate bundled fonts");
    auto directory=std::filesystem::path(executable).parent_path()/L"fonts";
    std::vector<std::wstring> paths;
    for(auto name:{L"NotoSans-Regular.ttf",L"NotoSans-Bold.ttf",L"NotoSans-ExtraBold.ttf",L"NotoSansMono-Regular.ttf"}) {
        auto path=directory/name;
        if(!std::filesystem::is_regular_file(path))throw std::runtime_error("Bundled fonts are missing. Keep the fonts folder next to Winip.exe.");
        paths.push_back(path.wstring());
    }
    loader.Attach(new Loader(std::move(paths)));
    if(FAILED(factory->RegisterFontCollectionLoader(loader.Get())))throw std::runtime_error("Cannot register private font collection");
    const unsigned key=1;
    HRESULT hr=factory->CreateCustomFontCollection(loader.Get(),&key,sizeof(key),&collection);
    if(FAILED(hr)) {factory->UnregisterFontCollectionLoader(loader.Get());throw std::runtime_error("Cannot load bundled fonts");}
    for(auto name:{L"Noto Sans",L"Noto Sans Mono"}) {
        UINT32 index;BOOL found;
        if(FAILED(collection->FindFamilyName(name,&index,&found)) || !found) {
            collection.Reset();factory->UnregisterFontCollectionLoader(loader.Get());throw std::runtime_error("Bundled font family was not found");
        }
    }
}
FontSet::~FontSet(){collection.Reset();factory->UnregisterFontCollectionLoader(loader.Get());}

IDWriteFontCollection* FontSet::Collection() const { return collection.Get(); }
