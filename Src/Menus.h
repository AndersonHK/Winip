#pragma once
#include <algorithm>
#include <string>
#include <vector>
namespace Menus {
enum Command {None=0,Open=101,Rename=401,Fullscreen,SelectAll,Shortcuts,About,Close,Save,SaveAs,Undo,Redo,Cut,Copy,Paste,CodeAbove,CodeBelow,MarkdownAbove,MarkdownBelow,DeleteCell,RunCell,RunAll,CellType,ToCode,ToMarkdown,ConnectKernel,StopKernel,InterruptKernel,RestartKernel,SelectInterpreter,Settings,ToggleHdr,ToggleOpenFolder,ChooseOpenFolder};
struct Item {std::wstring label,shortcut;int command=None;bool enabled=false,information=false;};
enum Menu { FileMenu, EditMenu, ViewMenu, InsertMenu, CellMenu, KernelMenu, WidgetsMenu, SettingsMenu, HelpMenu, ShortcutsMenu, AboutMenu, CellTypeMenu };
inline constexpr int MenuCount=9, TotalWidth=462;
inline const wchar_t* Names[]={L"File",L"Edit",L"View",L"Insert",L"Cell",L"Kernel",L"Widgets",L"Settings",L"Help"};
inline const int Widths[]={42,42,46,48,40,58,70,70,46};
inline const int CompactWidths[]={31,32,37,42,31,47,56,57,35};
inline int TextSize(int width) {return width<TotalWidth+36?12:14;}
inline int Left(int index,int width) {
    const bool compact=TextSize(width)==12;
    const int total=compact?368:TotalWidth;
    int x=0;for(int i=0;i<index;++i)x+=compact?CompactWidths[i]:Widths[i];
    return 18+x*std::min(total,std::max(0,width-36))/total;
}
inline int Hit(int x,int width) {for(int i=0;i<MenuCount;++i)if(x>=Left(i,width) && x<Left(i+1,width))return i;return -1;}
inline std::vector<Item> Items(int menu,bool document,bool editing,bool fullscreen,const std::wstring& python,bool cellEditing=false,bool hdrEnabled=false,bool lastFolder=true,bool connected=false,bool busy=false,const std::wstring& version=L"Unknown") {
    switch(menu) {
    case 0:return {{L"Open notebook…",L"Ctrl+O",Open,true},{L"Rename notebook…",L"F2",Rename,document},{L"Save",L"Ctrl+S",Save,document},{L"Save as…",L"Ctrl+Shift+S",SaveAs,document},{L"Close window",L"Alt+F4",Close,true}};
    case 1:return {{L"Undo",L"Ctrl+Z",Undo,document},{L"Redo",L"Ctrl+Y",Redo,document},{L"Cut",L"Ctrl+X",Cut,cellEditing},{L"Copy",L"Ctrl+C",Copy,cellEditing},{L"Paste",L"Ctrl+V",Paste,cellEditing},{L"Select all",L"Ctrl+A",SelectAll,editing}};
    case 2:return {{fullscreen?L"Exit fullscreen":L"Fullscreen",L"Alt+Enter",Fullscreen,true},{L"Zoom in",L"Ctrl++"},{L"Zoom out",L"Ctrl+−"},{L"Reset zoom",L"Ctrl+0"}};
    case 3:return {{L"Code cell above",L"",CodeAbove,true},{L"Code cell below",L"",CodeBelow,true},{L"Markdown cell above",L"",MarkdownAbove,true},{L"Markdown cell below",L"",MarkdownBelow,true}};
    case SettingsMenu:return {{hdrEnabled?L"HDR: On":L"HDR: Off (default)",L"",ToggleHdr,true},
        {lastFolder?L"Open folder: Last opened":L"Open folder: Fixed path",L"",ToggleOpenFolder,true},
        {L"Choose fixed open folder…",L"",ChooseOpenFolder,true}};
    case CellTypeMenu:return {{L"Code",L"Y",ToCode,document},{L"Markdown",L"M",ToMarkdown,document}};
    case 4:return {{L"Run cell",L"Shift+Enter",RunCell,document},{L"Run all cells",L"Ctrl+Shift+Enter",RunAll,document},{L"Change cell type…",L"M / Y",CellType,document},{L"Clear output",L""},{L"Delete cell",L"D, D",DeleteCell,document}};
    case 5:return {{python,L"",None,false,true},{L"Connect kernel",L"Ctrl+Shift+K",ConnectKernel,!busy && !connected},{L"Select interpreter…",L"",SelectInterpreter,!busy},{L"Interrupt",L"Ctrl+Shift+I",InterruptKernel,connected && busy},{L"Restart kernel",L"Ctrl+Shift+R",RestartKernel,connected},{L"Stop kernel (force)",L"Ctrl+Shift+.",StopKernel,connected || busy}};
    case 6:return {{L"Save widget state",L""},{L"Clear widget state",L""},{L"Widget settings…",L""}};
    case HelpMenu:return {{L"Keyboard shortcuts",L"",Shortcuts,true},{L"About Winip",L"",About,true}};
    case ShortcutsMenu:return {{L"Edit / command mode",L"Enter / Esc",None,false,true},{L"Insert above / below",L"A / B",None,false,true},{L"Delete cell (command mode)",L"D, D",None,false,true},{L"Undo / redo",L"Ctrl+Z / Y",None,false,true},{L"Markdown / code",L"M / Y",None,false,true},{L"Previous / next cell",L"K / J",None,false,true},{L"Save notebook",L"Ctrl+S",None,false,true},{L"Run cell / render Markdown",L"Shift+Enter",None,false,true},{L"Run all cells",L"Ctrl+Shift+Enter",None,false,true},{L"Fullscreen",L"Alt+Enter",None,false,true}};
    case AboutMenu:return {{L"Winip · Native notebook",L"",None,false,true},{L"Version "+version+L" — Windows x86-64",L"",None,false,true},{L"Hardware GPU · HDR / SDR",L"",None,false,true},{L"Noto fonts · Open Font License",L"",None,false,true}};
    default:return {};
    }
}
inline int PopupLeft(int menu,int width) {return std::clamp(Left(std::min(menu,MenuCount-1),width),8,std::max(8,width-322));}
inline int RowHeight(int count,int height) {return std::clamp((height-144)/std::max(1,count),18,30);}
}
