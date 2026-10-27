#include "KernelFooter.h"
#include <algorithm>
#include <cwctype>

namespace {
bool Inactive(const std::wstring& status) {
    return status==L"Kernel not started" || status==L"Kernel stopped" || status==L"Kernel disconnected";
}
}

std::wstring KernelFooter::Label(const std::wstring& status,const std::wstring& version,bool connected) {
    auto text=status.find(L"Kernel error:")==0?std::wstring(L"Kernel failed"):status;
    if(text.find(L"Kernel ")==0 && text.size()>7)text[7]=static_cast<wchar_t>(std::towupper(text[7]));
    if(connected && !Inactive(status) && !version.empty())text+=L" — Python "+version;
    return text;
}
bool KernelFooter::CanConnect(const std::wstring& status,bool connected,bool busy) {
    return !connected && !busy && Inactive(status);
}
bool KernelFooter::HitConnect(const KernelFooterRegion& region,int x,int y) {
    return region.connectable && x>=region.left && x<region.right && y>=region.top && y<region.bottom;
}

KernelFooterColumns KernelFooter::Allocate(int available, int kernel, int document, int renderer) {
    KernelFooterColumns result;
    result.kernel = std::min(std::max(0, available), kernel);
    int remaining = available - result.kernel;
    if (remaining >= document) { result.document = document; remaining -= document; }
    if (result.document && remaining >= renderer) result.renderer = renderer;
    return result;
}
