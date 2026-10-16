#pragma once
#include "framework.h"
#include <dwrite.h>
#include <wrl/client.h>

// Private DirectWrite collection, compatible with the Windows 7 API surface.
// Font outlines stay in app assets; no system-wide font registration.
class FontSet {
public:
    explicit FontSet(IDWriteFactory* factory);
    ~FontSet();
    FontSet(const FontSet&) = delete;
    FontSet& operator=(const FontSet&) = delete;
    IDWriteFontCollection* Collection() const;
private:
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    Microsoft::WRL::ComPtr<IDWriteFontCollectionLoader> loader;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
};
