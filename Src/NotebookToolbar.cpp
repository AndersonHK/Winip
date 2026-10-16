#include "NotebookToolbar.h"
#include "Menus.h"

std::vector<ToolbarItem> NotebookToolbar::Items(int width,const std::wstring& kind) {
    std::vector<ToolbarItem> items;
    int x=18;
    auto add=[&](int size,int command,const std::wstring& label){items.push_back({x,size,command,label});x+=size+6;};
    if(width>=760) {add(85,Menus::Open,L"Open…");add(58,Menus::Save,L"Save");}
    add(76,Menus::CodeBelow,L"+ Code");
    add(112,Menus::MarkdownBelow,L"+ Markdown");
    add(54,Menus::RunCell,L"▶ Run");
    if(width>=600)add(78,Menus::RunAll,L"Run all");
    add(100,Menus::CellType,kind==L"markdown"?L"Markdown ▾":kind==L"code"?L"Code ▾":L"Cell type ▾");
    return items;
}
int NotebookToolbar::Hit(int x,int width,const std::wstring& kind) {
    for(const auto& item:Items(width,kind))if(x>=item.x && x<item.x+item.width)return item.command;
    return Menus::None;
}
