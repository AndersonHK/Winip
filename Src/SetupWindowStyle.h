#pragma once
#include "framework.h"
#include <filesystem>
#include <vector>

class SetupWindowStyle {
public:
    explicit SetupWindowStyle(const std::filesystem::path& applicationDirectory);
    ~SetupWindowStyle();
    SetupWindowStyle(const SetupWindowStyle&) = delete;
    SetupWindowStyle& operator=(const SetupWindowStyle&) = delete;
    void Initialize(HWND window, HWND parent, bool center = true);
    HBRUSH ControlColor(HDC dc, HWND control) const;
    bool DrawButton(const DRAWITEMSTRUCT& item) const;
    static RECT Centered(RECT parent, RECT work, int width, int height);
private:
    std::vector<std::filesystem::path> fonts;
    HFONT body = nullptr, heading = nullptr;
    HBRUSH background = nullptr, paper = nullptr;
};
