#pragma once
#include <string>
#include <vector>

struct ToolbarItem {int x, width, command; std::wstring label;};
class NotebookToolbar {
public:
    static std::vector<ToolbarItem> Items(int width, const std::wstring& kind);
    static int Hit(int x, int width, const std::wstring& kind);
};
