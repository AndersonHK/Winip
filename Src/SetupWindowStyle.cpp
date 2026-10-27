#include "SetupWindowStyle.h"
#include <algorithm>

SetupWindowStyle::SetupWindowStyle(const std::filesystem::path& directory) {
    for (const auto* name : {L"NotoSans-Regular.ttf", L"NotoSans-Bold.ttf"}) {
        const auto path = directory / L"fonts" / name;
        if (AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr)) fonts.push_back(path);
    }
    background = CreateSolidBrush(RGB(238,238,238));
    paper = CreateSolidBrush(RGB(255,255,255));
}
SetupWindowStyle::~SetupWindowStyle() {
    if (body) DeleteObject(body);
    if (heading) DeleteObject(heading);
    if (background) DeleteObject(background);
    if (paper) DeleteObject(paper);
    for (const auto& path : fonts) RemoveFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
}
RECT SetupWindowStyle::Centered(RECT parent, RECT work, int width, int height) {
    width = std::min(width, int(work.right-work.left));
    height = std::min(height, int(work.bottom-work.top));
    const int x = std::clamp(int(parent.left+(parent.right-parent.left-width)/2), int(work.left), int(work.right-width));
    const int y = std::clamp(int(parent.top+(parent.bottom-parent.top-height)/2), int(work.top), int(work.bottom-height));
    return {x,y,x+width,y+height};
}
void SetupWindowStyle::Initialize(HWND window, HWND parent, bool center) {
    using GetDpi = UINT(WINAPI*)(HWND);
    const auto getDpi = reinterpret_cast<GetDpi>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    const UINT dpi = getDpi ? getDpi(center && parent ? parent : window) : 96;
    auto px = [dpi](int dip) { return MulDiv(dip, int(dpi), 96); };
    const auto oldBody = body, oldHeading = heading;
    body = CreateFontW(-px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Noto Sans");
    heading = CreateFontW(-px(21),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Noto Sans");
    for (HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
        SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetDlgCtrlID(child)==1010?heading:body),TRUE);
        const int id=GetDlgCtrlID(child);
        if (id==IDOK || id==IDCANCEL || (id>=1001 && id<=1004) || id==1007) {
            const auto style=GetWindowLongPtrW(child,GWL_STYLE);
            SetWindowLongPtrW(child,GWL_STYLE,(style & ~BS_TYPEMASK) | BS_OWNERDRAW);
        }
    }
    if(oldBody)DeleteObject(oldBody);
    if(oldHeading)DeleteObject(oldHeading);
    if(center) {
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(parent?parent:window,MONITOR_DEFAULTTONEAREST),&monitor);
        RECT owner=monitor.rcWork;
        if(parent && IsWindowVisible(parent))GetWindowRect(parent,&owner);
        auto work=monitor.rcWork;InflateRect(&work,-px(12),-px(12));
        const auto position=Centered(owner,work,px(790),px(640));
        SetWindowPos(window,nullptr,position.left,position.top,position.right-position.left,position.bottom-position.top,SWP_NOZORDER|SWP_NOACTIVATE);
    }
    RECT client{};GetClientRect(window,&client);
    const int width=MulDiv(client.right,96,int(dpi)),height=MulDiv(client.bottom,96,int(dpi));
    auto place=[&](int id,int x,int y,int w,int h) {
        SetWindowPos(GetDlgItem(window,id),nullptr,px(x),px(y),px(w),px(h),SWP_NOZORDER|SWP_NOACTIVATE);
    };
    place(1010,18,14,width-36,32);
    place(1011,18,54,width-36,48);
    place(1005,18,108,width-36,42);
    place(1000,18,158,width-36,std::max(40,height-300));
    place(1006,18,height-134,width-36,26);
    const int slot=(width-56)/3;
    place(1002,18,height-94,slot,32);
    place(1003,28+slot,height-94,slot,32);
    place(1004,38+2*slot,height-94,slot,32);
    place(1007,18,height-48,90,32);
    place(IDCANCEL,width-272,height-48,120,32);
    place(1001,width-142,height-48,124,32);
    place(IDOK,width-142,height-48,124,32);
    InvalidateRect(window,nullptr,TRUE);
}
HBRUSH SetupWindowStyle::ControlColor(HDC dc, HWND control) const {
    const bool details=GetDlgCtrlID(control)==1000;
    SetTextColor(dc,RGB(35,35,35));SetBkColor(dc,details?RGB(255,255,255):RGB(238,238,238));
    return details?paper:background;
}
bool SetupWindowStyle::DrawButton(const DRAWITEMSTRUCT& item) const {
    if(item.CtlType!=ODT_BUTTON)return false;
    const auto fill=CreateSolidBrush(item.itemState&ODS_SELECTED?RGB(235,235,235):RGB(255,255,255));
    FillRect(item.hDC,&item.rcItem,fill);DeleteObject(fill);
    const auto border=CreateSolidBrush(RGB(207,207,207));FrameRect(item.hDC,&item.rcItem,border);DeleteObject(border);
    wchar_t label[256]{};GetWindowTextW(item.hwndItem,label,256);
    const auto previous=SelectObject(item.hDC,body);
    SetBkMode(item.hDC,TRANSPARENT);SetTextColor(item.hDC,item.itemState&ODS_DISABLED?RGB(145,145,145):RGB(35,35,35));
    auto text=item.rcItem;InflateRect(&text,-6,0);
    DrawTextW(item.hDC,label,-1,&text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if((item.itemState&ODS_FOCUS) && !(item.itemState&ODS_NOFOCUSRECT)) {
        auto focus=item.rcItem;InflateRect(&focus,-4,-4);DrawFocusRect(item.hDC,&focus);
    }
    SelectObject(item.hDC,previous);return true;
}
