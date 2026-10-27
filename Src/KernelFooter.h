#pragma once
#include <string>

struct KernelFooterRegion {
    int left=0,top=0,right=0,bottom=0;
    bool connectable=false;
};

struct KernelFooterColumns {
    int kernel = 0, document = 0, renderer = 0;
};

class KernelFooter {
public:
    static KernelFooterColumns Allocate(int available, int kernel, int document, int renderer);
    static std::wstring Label(const std::wstring& status,const std::wstring& version,bool connected);
    static bool CanConnect(const std::wstring& status,bool connected,bool busy);
    static bool HitConnect(const KernelFooterRegion& region,int x,int y);
};
