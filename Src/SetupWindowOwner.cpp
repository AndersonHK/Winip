#include "SetupWindowOwner.h"
#include <algorithm>

HWND SetupWindowOwner::SelectInstaller(const std::vector<SetupOwnerCandidate>& candidates,
    const std::wstring& title, DWORD clientProcessId) {
    auto matches = [&](const SetupOwnerCandidate& candidate) {
        return candidate.visible && candidate.windowClass == L"MsiDialogCloseClass" && candidate.title == title;
    };
    const bool hasClient = clientProcessId && std::any_of(candidates.begin(), candidates.end(),
        [&](const auto& candidate) { return matches(candidate) && candidate.processId == clientProcessId; });
    HWND selected = nullptr;
    size_t count = 0;
    for (const auto& candidate : candidates) {
        if (!matches(candidate) || (hasClient && candidate.processId != clientProcessId)) continue;
        if (candidate.foreground) return candidate.window;
        selected = candidate.window;
        ++count;
    }
    // Never attach to an arbitrary other installer when the session is ambiguous.
    return count == 1 ? selected : nullptr;
}

HWND SetupWindowOwner::FindInstaller(const std::wstring& title, DWORD clientProcessId) {
    struct Enumeration {
        std::vector<SetupOwnerCandidate> windows;
        HWND foreground = GetForegroundWindow();
        bool failed = false;
    } enumeration;
    EnumWindows([](HWND window, LPARAM context) -> BOOL {
        auto& state = *reinterpret_cast<Enumeration*>(context);
        if (!IsWindowVisible(window)) return TRUE;
        wchar_t windowClass[256]{};
        if (!GetClassNameW(window,windowClass,256) || wcscmp(windowClass,L"MsiDialogCloseClass") != 0) return TRUE;
        wchar_t caption[512]{}; DWORD process = 0;
        GetWindowTextW(window,caption,512);GetWindowThreadProcessId(window,&process);
        try { state.windows.push_back({window,process,windowClass,caption,true,window==state.foreground}); }
        catch (...) { state.failed=true;return FALSE; }
        return TRUE;
    },reinterpret_cast<LPARAM>(&enumeration));
    return enumeration.failed ? nullptr : SelectInstaller(enumeration.windows,title,clientProcessId);
}
