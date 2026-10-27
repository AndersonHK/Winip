#pragma once
#include "framework.h"
#include <string>
#include <vector>

struct SetupOwnerCandidate {
    HWND window = nullptr;
    DWORD processId = 0;
    std::wstring windowClass, title;
    bool visible = false, foreground = false;
};

class SetupWindowOwner {
public:
    static HWND FindInstaller(const std::wstring& title, DWORD clientProcessId = 0);
    static HWND SelectInstaller(const std::vector<SetupOwnerCandidate>& candidates,
        const std::wstring& title, DWORD clientProcessId = 0);
};
