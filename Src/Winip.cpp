#include "framework.h"
#include "Winip.h"
#include "AppPaths.h"
#include "AppSettings.h"
#include "CellOutput.h"
#include "ImageUpload.h"
#include <shlobj.h>
#include "KernelController.h"
#include "NotebookToolbar.h"
#include "CellScrollbar.h"
#include "NotebookNavigator.h"
#include "NotebookViewport.h"
#include <deque>
#include "Notebook.h"
#include "Renderer.h"
#include "RasterContract.h"
#include "TitleEditor.h"
#include "CellEditor.h"
#include "NotebookStorage.h"
#include "Markdown.h"
#include "NotebookFlow.h"
#include "NotebookShortcuts.h"
#include "Menus.h"
#include "PythonDiscovery.h"
#include <commdlg.h>
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <array>
#include <map>

namespace {
constexpr UINT Loaded = WM_APP + 1, Failed = WM_APP + 2, KernelUpdate = WM_APP + 3, OperationFailed = WM_APP + 4, PreviewReady = WM_APP + 5;
constexpr int OpenId = 101;
using Clock = std::chrono::steady_clock;
struct MessageCount {uint64_t calls=0,wallNs=0,maxNs=0;};
struct State {
    HWND window = nullptr;
    HANDLE frameReady=nullptr;
    std::atomic<uint64_t> windowMaxNs{0},windowOverBudget{0};
    HICON bigIcon=nullptr,smallIcon=nullptr;
    HMONITOR monitor=nullptr;
    std::mutex mutex;
    std::condition_variable renderWake, loadWake, kernelWake;
    bool stop = false, dirty = true, displayChanged = true;
    bool forceSdr = true;
    std::unique_ptr<AppSettings> settings;
    bool renamePending=false, renameRequest=false, interactionProbe=false,resizeProbe=false,noSnapshots=false;
    std::atomic<bool> fullscreen{false};
    WINDOWPLACEMENT windowedPlacement{sizeof(WINDOWPLACEMENT)};
    LONG_PTR windowedStyle=0;
    TitleEditor titleEdit;
    std::unique_ptr<CellEditor> editor;
    std::unique_ptr<KernelController> kernel;
    std::deque<KernelEvent> kernelEvents;
    std::wstring kernelStatus=L"Kernel not started", kernelRuntime, kernelBridge;
    bool kernelProbe=false,navigatorProbe=false,imageProbe=false;
    bool openErrorProbe=false,mediaProbe=false;
    Microsoft::WRL::ComPtr<ID3D11Device> uploadDevice;
    bool uploadDeviceReady=false;
    std::shared_ptr<const ImageUpload> imageUploads;
    ExpandedImages expandedImages;
    uint64_t imageViewRevision=0;
    std::shared_ptr<const Notebook> previewRequest, previewReady;
    std::shared_ptr<const Notebook> savedDocument, saveSnapshot, saveExpected;
    std::wstring savePath;
    bool saveRequest=false, savePending=false, editingProbe=false;
    wchar_t pendingSurrogate=0;
    wchar_t suppressedChar=0;
    NotebookShortcuts shortcuts;
    int menu=-1,menuItem=0;
    std::wstring loadedPath, renameTitle, pythonOverride, pythonStatus=L"Finding Python...";
    std::wstring operationError, operationErrorTitle;
    uint64_t operationErrorRequest = 0;
    PythonIdentity python;
    std::atomic<bool> cancelPython{false},pythonDone{false};
    std::thread pythonThread;
    int width = 1200, height = 850, scroll = 0, clickY = -1;
    int horizontal = 0;
    CellScrollbar scrollbar;
    NotebookNavigator navigator;
    bool mouseTracking=false;
    float scale = 1;
    std::wstring path, status = L"Open a notebook to begin", error, metricsPath, snapshotPath;
    uint64_t request = 0;
    uint64_t revision = 0;
    std::atomic<uint64_t> presentedRevision{0};
    std::atomic<float> whiteScale{1};
    std::atomic<unsigned> refreshHz{0};
    nlohmann::json textCache;
    nlohmann::json renderStages=nlohmann::json::array();
    nlohmann::json frameStatistics=nlohmann::json::array();
    std::atomic<bool> hdr{false};
    std::wstring adapter;
    std::wstring probeDirectory;
    nlohmann::json probeResults = nlohmann::json::array();
    std::map<UINT,MessageCount> probeMessages;
    std::shared_ptr<const Notebook> document;
    std::thread renderThread, loadThread;
    std::atomic<int> documentHeight{0};
    std::atomic<int> documentWidth{0};
    std::atomic<uint64_t> frames{0}, uiCount{0}, uiOverBudget{0}, uiMaxNs{0}, renderMaxNs{0};
    std::atomic<bool> renderFailed{false};
} app;
void Max(std::atomic<uint64_t>& dest, uint64_t value) {
    uint64_t prior = dest.load(); while (value > prior && !dest.compare_exchange_weak(prior, value)) {}
}
struct InputTiming {
    Clock::time_point start = Clock::now();
    ~InputTiming() {
        auto ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
        ++app.uiCount; if (ns > 6000000) ++app.uiOverBudget; Max(app.uiMaxNs, ns);
    }
};
struct WindowTiming {
    Clock::time_point start=Clock::now();
    ~WindowTiming(){auto ns=(uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();Max(app.windowMaxNs,ns);if(ns>6000000)++app.windowOverBudget;}
};
void Dirty();
void CompleteResizeFrame() {
    // Keep the native size transaction open until its complete, full-resolution frame
    // is presented. Pump sent messages only: DXGI can synchronously call the UI thread.
    if(!app.renderThread.joinable() || !app.width || !app.height)return;
    uint64_t revision;
    {std::lock_guard<std::mutex> lock(app.mutex);revision=app.revision;}
    auto deadline=Clock::now()+std::chrono::seconds(3);
    while(app.presentedRevision<revision && !app.renderFailed) {
        if(Clock::now()>=deadline) {
            std::lock_guard<std::mutex> lock(app.mutex);app.error=L"The GPU did not complete the window resize frame.";app.renderFailed=true;break;
        }
        DWORD result=MsgWaitForMultipleObjects(1,&app.frameReady,FALSE,100,QS_SENDMESSAGE);
        if(result==WAIT_OBJECT_0+1){MSG sent{};PeekMessageW(&sent,nullptr,0,0,PM_NOREMOVE);}
    }
}
struct ProbeMessageTiming {
    UINT message;
    bool enabled;
    Clock::time_point start{};
    explicit ProbeMessageTiming(UINT value):message(value),enabled(!app.probeDirectory.empty()) {if(enabled)start=Clock::now();}
    ~ProbeMessageTiming() {
        if(!enabled)return;
        auto ns=(uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();
        auto& count=app.probeMessages[message];++count.calls;count.wallNs+=ns;count.maxNs=std::max(count.maxNs,ns);
    }
};
void Dirty() { app.dirty = true; ++app.revision; app.renderWake.notify_one(); } // mutex held
void UpdateWindowIcons(HWND window,float scale) {
    HINSTANCE instance=GetModuleHandleW(nullptr);
    int large=RasterContract::RequiredPixels(32,96*scale),smallPixels=RasterContract::RequiredPixels(16,96*scale);
    auto bigIcon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_WINIP),IMAGE_ICON,large,large,0);
    auto smallIcon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_WINIP),IMAGE_ICON,smallPixels,smallPixels,0);
    if(bigIcon){SendMessageW(window,WM_SETICON,ICON_BIG,(LPARAM)bigIcon);if(app.bigIcon)DestroyIcon(app.bigIcon);app.bigIcon=bigIcon;}
    if(smallIcon){SendMessageW(window,WM_SETICON,ICON_SMALL,(LPARAM)smallIcon);if(app.smallIcon)DestroyIcon(app.smallIcon);app.smallIcon=smallIcon;}
}
void RequestOpen(std::wstring path) {
    std::lock_guard<std::mutex> lock(app.mutex);
    if(app.renamePending || app.savePending)return;
    app.menu=-1;app.titleEdit.End();app.renameRequest=false;
    app.previewRequest.reset();app.previewReady.reset();
    app.path = std::move(path); ++app.request;
    app.status = L"Loading notebook..."; Dirty(); app.loadWake.notify_one();
}
int HorizontalPage() {
    int width=int(app.width/app.scale), left=NotebookViewport::SideMargin(width), gutter=width<650?88:128;
    return std::max(1,width-2*left-gutter-46);
}
void UpdateHorizontal(HWND) {
    std::lock_guard<std::mutex> lock(app.mutex);
    app.horizontal=std::clamp(app.horizontal,0,std::max(0,app.documentWidth.load()-HorizontalPage()));
}
void ScrollHorizontal(HWND window,int code,int wheelDelta=0) {

    {
        std::lock_guard<std::mutex> lock(app.mutex);
        int position=app.horizontal,page=HorizontalPage(),maximum=std::max(0,app.documentWidth.load()-page);
        switch(code) {
        case SB_LINELEFT: position-=44;break;case SB_LINERIGHT:position+=44;break;
        case SB_PAGELEFT:position-=page;break;case SB_PAGERIGHT:position+=page;break;
        case SB_THUMBPOSITION:case SB_THUMBTRACK:position=wheelDelta;break;
        case SB_LEFT:position=0;break;case SB_RIGHT:position=maximum;break;
        default:position+=wheelDelta;break;
        }
        position=std::clamp(position,0,maximum);
        if(position!=app.horizontal){app.horizontal=position;Dirty();}
    }
    UpdateHorizontal(window);
}
bool ConfirmDiscard(HWND window);
void OpenDialog(HWND window) {
    if(!ConfirmDiscard(window))return;
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT result=CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog));
    if(SUCCEEDED(result)) {
        COMDLG_FILTERSPEC filters[]={{L"Jupyter notebooks (*.ipynb)",L"*.ipynb"},{L"All files",L"*.*"}};
        result=dialog->SetFileTypes(2,filters);
        if(SUCCEEDED(result))result=dialog->SetOptions(FOS_FILEMUSTEXIST|FOS_PATHMUSTEXIST|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR);
        Microsoft::WRL::ComPtr<IShellItem> folder;
        std::wstring initial;
        {std::lock_guard<std::mutex> lock(app.mutex);initial=app.settings->OpenFolder();}
        HRESULT location=SHCreateItemFromParsingName(initial.c_str(),nullptr,IID_PPV_ARGS(&folder));
        if(FAILED(location)) {
            PWSTR downloads=nullptr;
            if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads,0,nullptr,&downloads))) {
                location=SHCreateItemFromParsingName(downloads,nullptr,IID_PPV_ARGS(&folder));CoTaskMemFree(downloads);
            }
        }
        if(SUCCEEDED(result) && SUCCEEDED(location))result=dialog->SetFolder(folder.Get());
        if(SUCCEEDED(result))result=dialog->Show(window);
        if(SUCCEEDED(result)) {
            Microsoft::WRL::ComPtr<IShellItem> file;result=dialog->GetResult(&file);
            PWSTR path=nullptr;
            if(SUCCEEDED(result))result=file->GetDisplayName(SIGDN_FILESYSPATH,&path);
            if(SUCCEEDED(result)){RequestOpen(path);CoTaskMemFree(path);}
        }
    }
    dialog.Reset();if(SUCCEEDED(initialized))CoUninitialize();
    if(FAILED(result) && result!=HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        std::lock_guard<std::mutex> lock(app.mutex);app.status=L"Could not open notebook dialog";Dirty();
    }
}

std::wstring DocumentStatus() { // mutex held
    return app.document?(app.savedDocument && app.savedDocument->ContentRevision()==app.document->ContentRevision()?L"Saved  |  ":L"Unsaved changes  |  ")+std::to_wstring(app.document->Cells().size())+L" cells":L"Open a notebook to begin";
}
void PublishEdit(bool reveal);
void BeginTitleEdit() { // mutex held
    if(app.document && !app.renamePending && !app.savePending && app.status!=L"Loading notebook...") {
        if(app.editor) {app.editor->End();PublishEdit(false);}
        app.titleEdit.Begin(app.document->Title());Dirty();
    }
}
void ApplyTitleEdit() { // mutex held; all file work is queued
    if(!ValidNotebookTitle(app.titleEdit.Text())){app.status=L"Use a valid filename (1–120 characters, no reserved names).";Dirty();return;}
    app.renameTitle=app.titleEdit.Text();app.titleEdit.End();
    if(app.renameTitle==app.document->Title()){app.status=DocumentStatus();Dirty();return;}
    app.renameRequest=app.renamePending=true;++app.request;
    app.status=L"Renaming notebook...";Dirty();app.loadWake.notify_one();
}
void PythonLoop() {
    PythonIdentity identity;
    try {identity=DiscoverPython(ReadPythonEnvironment(app.pythonOverride),app.cancelPython);}
    catch(const std::exception& e){identity.error=Utf16(e.what());}
    std::lock_guard<std::mutex> lock(app.mutex);
    if(app.stop)return;
    app.python=std::move(identity);app.pythonDone=true;
    app.pythonStatus=app.python.error.empty()?L"Python "+app.python.version+L" (64-bit) | "+app.kernelStatus:app.python.error;
    Dirty();
}
bool Unsaved() { // mutex held
    return app.document && (!app.savedDocument || app.document->ContentRevision()!=app.savedDocument->ContentRevision());
}
void PublishEdit(bool reveal=true) { // UI thread, mutex held
    const auto previous=app.document?app.document->ContentRevision():0;
    app.document=app.editor->Document();
    if(!app.document)return;
    app.documentHeight=app.document->Height();app.documentWidth=app.document->ContentWidth();
    app.horizontal=std::clamp(app.horizontal,0,std::max(0,app.documentWidth.load()-HorizontalPage()));
    if(reveal && app.editor->Active()) {
        const auto point=app.editor->CaretPoint();
        int page=HorizontalPage();
        if(point.x<app.horizontal)app.horizontal=std::max(0,int(point.x));
        if(point.x>app.horizontal+page-5)app.horizontal=std::max(0,int(std::ceil(point.x-page+5)));
        const auto& cell=app.document->Cells()[app.editor->Selected()];
        int top=200+cell.top+int(point.y)-app.scroll, bottom=int(app.height/app.scale)-38;
        if(top<177)app.scroll=std::max(0,app.scroll+top-177);
        else if(top+25>bottom)app.scroll+=top+25-bottom;
    } else if(reveal && !app.document->Cells().empty()) {
        const auto& cell=app.document->Cells()[app.editor->Selected()];
        int top=190+cell.top-app.scroll;
        if(top<177)app.scroll=std::max(0,cell.top+13);
        else if(top>int(app.height/app.scale)-80)app.scroll=std::max(0,cell.top-10);
    }
    app.scroll=std::clamp(app.scroll,0,std::max(0,app.documentHeight.load()-std::max(22,int(app.height/app.scale)-232)));
    if(auto preview=app.editor->TakePreviewRequest()){app.previewRequest=std::move(preview);app.loadWake.notify_one();}
    app.status=DocumentStatus();Dirty();
    if(previous!=app.document->ContentRevision())PostMessageW(app.window,Loaded,0,0);
}
template<class Action> void EditAction(Action action, bool reveal=true) { // UI thread, mutex held
    if(!app.editor || app.renamePending || app.status==L"Loading notebook...")return;
    try {
        if(app.editor->Document()!=app.document)app.editor->Attach(app.document);
        action();PublishEdit(reveal);
    } catch(const std::exception& error) {app.status=L"Cannot edit cell: "+Utf16(error.what());Dirty();}
}
void SaveDocument(HWND window,bool saveAs=false) {
    std::wstring path;
    {
        std::lock_guard<std::mutex> lock(app.mutex);
        if(!app.document || app.savePending || app.renamePending)return;
        path=app.loadedPath;
    }
    if(saveAs || path.empty()) {
        wchar_t name[32768]=L"Untitled.ipynb";
        OPENFILENAMEW dialog{sizeof(dialog)};
        dialog.hwndOwner=window;dialog.lpstrFile=name;dialog.nMaxFile=32768;
        dialog.lpstrFilter=L"Jupyter notebooks (*.ipynb)\0*.ipynb\0";dialog.lpstrDefExt=L"ipynb";
        dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(!GetSaveFileNameW(&dialog))return;
        path=name;
    }
    std::lock_guard<std::mutex> lock(app.mutex);
    const auto title=std::filesystem::path(path).stem().wstring();
    if(!ValidNotebookTitle(title)){app.status=L"Choose a valid notebook name of 1–120 characters.";Dirty();return;}
    app.savePath=path;app.saveSnapshot=std::make_shared<const Notebook>(app.document->WithTitle(title));
    app.saveExpected=path==app.loadedPath?app.savedDocument:nullptr;
    app.saveRequest=app.savePending=true;app.status=L"Saving notebook...";++app.request;
    app.loadWake.notify_one();Dirty();
}
bool ConfirmDiscard(HWND window) {
    {
        std::lock_guard<std::mutex> lock(app.mutex);
        if(app.savePending || app.renamePending) {app.status=L"Wait for the file operation to finish.";Dirty();return false;}
        if(!Unsaved())return true;
    }
    return MessageBoxW(window,L"Discard unsaved notebook changes?",L"Unsaved notebook",MB_OKCANCEL|MB_ICONWARNING)==IDOK;
}
void ClipboardCommand(HWND window,int command) { // UI thread, mutex held
    if(!app.editor->Active())return;
    if(!OpenClipboard(window)) {app.status=L"Clipboard is busy.";Dirty();return;}
    try {
        if(command==Menus::Paste) {
            auto memory=GetClipboardData(CF_UNICODETEXT);
            if(memory) {
                auto text=static_cast<const wchar_t*>(GlobalLock(memory));
                if(text) {
                    size_t units=GlobalSize(memory)/sizeof(wchar_t), length=0;
                    while(length<units && text[length])++length;
                    std::wstring value;
                    if(length<=1024*1024)value.assign(text,length);
                    GlobalUnlock(memory);
                    if(length>1024*1024)throw std::runtime_error("Clipboard text exceeds cell size limit");
                    app.editor->InsertText(value);PublishEdit();
                }
            }
        } else {
            auto value=app.editor->Selection();
            if(!value.empty()) {
                std::wstring windowsText;
                for(auto character:value) {if(character==L'\n')windowsText+=L'\r';windowsText+=character;}
                value=std::move(windowsText);
                auto memory=GlobalAlloc(GMEM_MOVEABLE,(value.size()+1)*sizeof(wchar_t));
                if(!memory)throw std::runtime_error("Cannot allocate clipboard text");
                auto target=GlobalLock(memory);
                if(!target){GlobalFree(memory);throw std::runtime_error("Cannot access clipboard text");}
                memcpy(target,value.c_str(),(value.size()+1)*sizeof(wchar_t));GlobalUnlock(memory);
                if(!EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT,memory)) {GlobalFree(memory);throw std::runtime_error("Cannot copy text");}
                if(command==Menus::Cut) {app.editor->Erase(false);PublishEdit();}
            }
        }
    } catch(const std::exception& error) {app.status=Utf16(error.what());Dirty();}
    CloseClipboard();
}
void InsertCell(bool above,bool markdown) { // UI thread, mutex held
    if(app.renamePending || app.savePending || app.status==L"Loading notebook...")return;
    if(!app.document) {
        app.document=std::make_shared<const Notebook>(Notebook().WithLayout({}));
        app.savedDocument.reset();app.loadedPath.clear();app.editor->Attach(app.document);
    }
    size_t index=app.document->Cells().empty()?0:app.editor->Selected()+(above?0:1);
    app.titleEdit.End();EditAction([&]{app.editor->InsertCell(index,markdown?L"markdown":L"code");});
}
void RunKernel(bool all,bool connectOnly=false) { // UI thread, mutex held
    if(!app.pythonDone || !app.python.error.empty()) {app.status=L"Python unavailable: "+app.python.error;Dirty();return;}
    if(!connectOnly && (!app.document || app.document->Cells().empty()))return;
    auto cwd=app.loadedPath.empty()?std::filesystem::path(app.kernelRuntime).parent_path():std::filesystem::path(app.loadedPath).parent_path();
    if(app.kernel->Submit(connectOnly?nullptr:app.document,app.editor->Selected(),all,app.python.executable,
        app.kernelBridge,app.kernelRuntime,cwd.wstring())) {app.status=L"Kernel request queued";app.kernelStatus=app.kernel->Connected()?L"Kernel busy":L"Kernel starting…";}
    else app.status=L"Kernel busy; wait for completion or stop the kernel";
    Dirty();
}
void Shortcut(NotebookShortcuts::Action action) { // UI thread, mutex held
    using Action=NotebookShortcuts::Action;
    if(action==Action::RunAll){RunKernel(true);return;}
    if(action==Action::Help) {app.menu=8;app.menuItem=0;Dirty();return;}
    if(action==Action::InsertAbove || action==Action::InsertBelow) {
        InsertCell(action==Action::InsertAbove,false);
        EditAction([&]{app.editor->End();});return;
    }
    if(action==Action::Undo || action==Action::Redo) {
        EditAction([&]{if(action==Action::Undo)app.editor->Undo();else app.editor->Redo();});return;
    }
    if(!app.document || app.document->Cells().empty())return;
    const size_t selected=app.editor->Selected();
    switch(action) {
    case Action::EnterEdit:EditAction([&]{app.editor->Begin(selected,0,0);});break;
    case Action::ExitEdit:EditAction([&]{app.editor->End();},false);break;
    case Action::DeleteCell:EditAction([&]{app.editor->DeleteCell();});break;
    case Action::Undo:EditAction([&]{app.editor->Undo();});break;
    case Action::Redo:EditAction([&]{app.editor->Redo();});break;
    case Action::Markdown:case Action::Code:EditAction([&]{app.editor->ChangeKind(action==Action::Markdown?L"markdown":L"code");});break;
    case Action::Previous:case Action::Next:
        EditAction([&]{app.editor->Select(action==Action::Previous?(selected?selected-1:0):std::min(selected+1,app.document->Cells().size()-1));});break;
    case Action::RenderStay:case Action::RenderNext:
        if(app.document->Cells()[selected].text->kind==L"markdown") {
            EditAction([&]{app.editor->End();});
            if(action==Action::RenderNext) {
                if(selected+1==app.document->Cells().size()) {InsertCell(false,false);EditAction([&]{app.editor->End();});}
                else EditAction([&]{app.editor->Select(selected+1);});
            }
        } else {RunKernel(false);
            if(action==Action::RenderNext && selected+1<app.document->Cells().size())EditAction([&]{app.editor->Select(selected+1);});
        }
        break;
    default:break;
    }
}
void ApplyFullscreen(HWND window,bool desired) {
    if(app.fullscreen==desired)return;
    if(desired) {
        app.windowedPlacement.length=sizeof(WINDOWPLACEMENT);GetWindowPlacement(window,&app.windowedPlacement);
        app.windowedStyle=GetWindowLongPtrW(window,GWL_STYLE);
        app.fullscreen=true;
        SetWindowLongPtrW(window,GWL_STYLE,(app.windowedStyle & ~(WS_OVERLAPPEDWINDOW|WS_MAXIMIZE|WS_MINIMIZE)) | WS_POPUP);
        MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
        SetWindowPos(window,nullptr,monitor.rcMonitor.left,monitor.rcMonitor.top,
            monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,SWP_NOZORDER|SWP_FRAMECHANGED|SWP_NOCOPYBITS);
    } else {
        app.fullscreen=false;
        SetWindowLongPtrW(window,GWL_STYLE,app.windowedStyle);
        SetWindowPlacement(window,&app.windowedPlacement);
        SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED|SWP_NOCOPYBITS);
    }
}
void ToggleFullscreen(HWND window) {ApplyFullscreen(window,!app.fullscreen);}

void LoadLoop() {
    uint64_t seen = 0;
    for (;;) {
        std::wstring path;
        uint64_t request;bool rename=false,save=false,preview=false;std::wstring title;
        std::shared_ptr<const Notebook> expected;
        std::shared_ptr<const Notebook> source;
        {
            std::unique_lock<std::mutex> lock(app.mutex);
            app.loadWake.wait(lock, [&] { return app.stop || app.request != seen || app.previewRequest; });
            if (app.stop) return;
            if(app.request==seen && app.previewRequest){preview=true;source=std::move(app.previewRequest);request=app.request;}
            else {
            request = seen = app.request; rename=app.renameRequest;app.renameRequest=false;
            save=app.saveRequest;app.saveRequest=false;
            path=save?app.savePath:rename?app.loadedPath:app.path;title=app.renameTitle;
            source=save?app.saveSnapshot:app.document;expected=app.saveExpected;
            }
        }
        try {
            if(preview) {
                auto prepared=std::make_shared<Notebook>(*source);PrepareNotebookLayout(*prepared);
                std::lock_guard<std::mutex> lock(app.mutex);
                if(!app.stop && request==app.request){app.previewReady=std::move(prepared);PostMessageW(app.window,PreviewReady,0,0);}
                continue;
            }
            if(save) {
                NotebookStorage::Save(path,*source,expected.get());
                {
                    std::lock_guard<std::mutex> lock(app.mutex);
                    app.savedDocument=source;app.loadedPath=path;app.savePending=false;
                    if(app.document->Title()!=source->Title())app.document=std::make_shared<const Notebook>(app.document->WithTitle(source->Title()));
                    app.status=DocumentStatus();Dirty();PostMessageW(app.window,Loaded,0,0);
                }
                continue;
            }
            auto doc = rename?std::make_shared<Notebook>(*source):std::make_shared<Notebook>(Notebook::Load(path, [request] {
                std::lock_guard<std::mutex> lock(app.mutex);
                return app.stop || request != app.request;
            }));
            int contentWidth=rename?app.documentWidth.load():PrepareNotebookLayout(*doc);
            if(!rename) {
                int width,height;
                {std::lock_guard<std::mutex> lock(app.mutex);width=int(app.width/app.scale);height=int(app.height/app.scale);}
                NotebookFlow flow;
                *doc=*flow.Prepare(doc,width,height,{},0);
                contentWidth=doc->ContentWidth();
            }
            if(rename) {
                *doc = doc->WithTitle(title);
                path=RenameNotebookFile(path,title);
            }
            std::shared_ptr<const ImageUpload> uploads;
            if(!rename) {
                Microsoft::WRL::ComPtr<ID3D11Device> device;
                {std::unique_lock<std::mutex> lock(app.mutex);
                    app.loadWake.wait(lock,[&]{return app.stop || app.renderFailed || app.uploadDeviceReady;});
                    if(app.stop || app.renderFailed)return;
                    device=app.uploadDevice;
                }
                uploads=std::make_shared<const ImageUpload>(device.Get(),*doc,[request]{
                    std::lock_guard<std::mutex> lock(app.mutex);return app.stop || request!=app.request;
                });
            }
            std::shared_ptr<const ImageUpload> oldUploads;
            std::shared_ptr<const Notebook> old;
            {
                std::lock_guard<std::mutex> lock(app.mutex);
                if (app.stop) return;
                if (request != app.request) continue;
                old = std::move(app.document); app.document = std::move(doc);
                if(!rename) {
                    oldUploads=std::move(app.imageUploads);app.imageUploads=std::move(uploads);
                    app.savedDocument=app.document;
                    app.kernel->Stop();app.kernelStatus=L"Kernel disconnected";
                    app.pythonStatus=L"Python "+app.python.version+L" (64-bit) | "+app.kernelStatus;
                    app.settings->RememberNotebook(path);
                }
                app.loadedPath=path;app.renamePending=false;
                app.documentHeight = app.document->Height();
                app.documentWidth=contentWidth;if(!rename){app.scroll=0;app.horizontal=0;app.expandedImages.clear();++app.imageViewRevision;}
                app.status=DocumentStatus();
                Dirty(); PostMessageW(app.window, Loaded, 0, 0);
            } // old notebook freed on loader, outside mutex
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(app.mutex);
            if (app.stop) return;
            if (request != app.request) continue;
            app.renamePending=false;app.savePending=false;
            std::wstring reason;
            if (dynamic_cast<const std::bad_alloc*>(&e))
                reason = L"There is not enough available memory to complete this operation. Close other large documents and try again.";
            else {
                // Parser diagnostics can themselves contain malformed source bytes.
                // They must not throw out of this error handler instead of showing a dialog.
                try { reason = Utf16(e.what()).substr(0, 2048); }
                catch (const std::exception&) { reason = L"The file operation failed. Its error details contain invalid text."; }
            }
            app.operationErrorTitle = preview?L"Could not prepare Markdown":save?L"Could not save notebook":rename?L"Could not rename notebook":L"Could not open notebook";
            app.status = app.operationErrorTitle + L": " + reason;
            app.operationError = path + L"\n\n" + reason + (app.document?L"\n\nThe current notebook has been kept open.":L"\n\nNo notebook was opened.");
            app.operationErrorRequest = request;
            Dirty(); PostMessageW(app.window, OperationFailed, 0, 0);
        }
    }
}
struct Canvas {
    Renderer& renderer;
    int width=0,height=0;
    float scale=1;
    void Box(int x,int y,int w,int h,COLORREF color){renderer.Box(float(x),float(y),float(w),float(h),color);}
    void Text(const std::wstring& text,int x,int y,int w,int h,int size=17,
              COLORREF color=RGB(35,35,35),bool mono=false,bool bold=false,bool ellipsis=false) {
        renderer.Text(text,float(x),float(y),float(w),float(h),float(size),color,mono,bold,ellipsis);
    }
};
void Draw(Canvas& c, const Notebook* doc, int scroll, int horizontal, int contentWidth, int selected, const std::wstring& status, bool hdr, const TitleEditor& titleEdit,const CellEditView& editView,int menu,int menuItem,const std::vector<Menus::Item>& menuItems,const NotebookNavigator& navigator,uint64_t animationTime,bool kernelBusy,bool kernelConnected,const std::wstring& kernelStatus) {
    int w = int(c.width / c.scale), h = int(c.height / c.scale);
    c.Box(0,0,w+1,h+1,RGB(238,238,238));
    int left = NotebookViewport::SideMargin(w), paperWidth = w-2*left;
    int gutter=w<650?88:128;
    c.Box(left+3,171,paperWidth,h-196,RGB(221,221,221));
    c.Box(left,168,paperWidth,h-196,RGB(255,255,255));
    c.renderer.PushClip(float(left),177.f,float(w-left),float(h-34));
    if (!doc) {
        c.Text(L"Your notebook, on Windows.",left+24,232,paperWidth-48,50,30,RGB(35,35,35),false,true);
        c.Text(L"Open an .ipynb file to view its cells and saved text outputs.",left+24,299,paperWidth-48,50,18);
        c.Text(L"Ctrl+O  Open notebook\n\nMouse wheel / Page Up / Page Down  Scroll\n\nFiles open without running any code.",left+24,370,paperWidth-48,170,17,RGB(90,90,90));
    } else {
        const auto& cells = doc->Cells();
        auto first = std::lower_bound(cells.begin(), cells.end(), scroll - 22,
            [](const Cell& cell, int y) { return cell.top + cell.height < y; });
        for (auto it = first; it != cells.end(); ++it) {
            const auto& cell = *it;
            int y = 190 + cell.top - scroll;
            if (y > h-34) break;
            int index = (int)(it-cells.begin());
            if (index == selected) c.Box(left+14,y-4,4,cell.height+8,editView.id==cell.id?RGB(52,160,90):RGB(66,165,245));
            int sourceHeight = cell.sourceHeight;
            int page=paperWidth-gutter-46,cellMaximum=std::max(0,cell.contentWidth-page);
            int cellPan=std::min(horizontal,cellMaximum);
            if (cell.text->kind == L"code" || cell.sourceMode || editView.id==cell.id) {
                c.Text(cell.text->prompt,left+20,y+10,gutter-24,30,17,RGB(48,63,159),true);
                c.Box(left+gutter,y,paperWidth-gutter-20,sourceHeight+24,RGB(211,211,211));
                c.Box(left+gutter+1,y+1,paperWidth-gutter-22,sourceHeight+22,RGB(247,247,247));
                c.renderer.PushClip(float(left+gutter+12),float(y+1),float(w-left-32),float(y+sourceHeight+23));
                if(editView.id==cell.id)c.renderer.CellInput(editView,float(left+gutter+12-cellPan),float(y+10));
                else c.Text(cell.text->source,left+gutter+12-cellPan,y+10,std::max(contentWidth,paperWidth-gutter-46),sourceHeight+8,17,RGB(25,25,25),true);
                c.renderer.PopClip();
            } else {
                bool heading = cell.text->kind == L"markdown" && cell.text->source.rfind(L"# ",0) == 0 && cell.text->source.find(L'\n') == std::wstring::npos;
                c.renderer.PushClip(float(left+gutter),float(y),float(w-left-22),float(y+sourceHeight+32));
                if(cell.markdown) {
                    c.renderer.PreparedText(cell.markdown->Layout(),float(left+gutter-cellPan),float(y+8));
                    for(const auto& image:cell.markdown->Images())
                        if(y+8+image.y+image.height>=177 && y+8+image.y<h-34)
                            c.renderer.Image(image.image,float(left+gutter-cellPan)+image.x,float(y+8)+image.y,image.width,image.height);
                }
                else c.Text(heading?cell.text->source.substr(2):cell.text->source,left+gutter-cellPan,y+8,std::max(contentWidth,paperWidth-gutter-30),sourceHeight+24,
                       heading?27:18,RGB(25,25,25),cell.text->kind==L"raw",heading);
                c.renderer.PopClip();
            }
            if (!cell.text->output.empty()) {
                c.renderer.PushClip(float(left+gutter+12),float(y+sourceHeight+34),float(w-left-32),float(y+cell.height+12));
                if(cell.output) {
                    for(const auto& block:cell.output->Blocks()) {
                        float outputY=y+sourceHeight+34+block.top;
                        if(outputY+block.height<177 || outputY>h-34)continue;
                        if(block.image)c.renderer.Image(block.image,float(left+gutter+12-cellPan),outputY,block.width,block.height);
                        else c.renderer.Text(block.text,float(left+gutter+12-cellPan),outputY,std::max(1.f,block.width+4),block.height+4,17,RGB(45,45,45),true,false);
                    }
                } else c.Text(cell.text->output,left+gutter+12-cellPan,y+sourceHeight+34,std::max(contentWidth,paperWidth-gutter-46),
                       cell.height-sourceHeight,17,RGB(45,45,45),true);
                c.renderer.PopClip();
            }
            if(cellMaximum>0) {
                int track=page,thumb=CellScrollbar::ThumbWidth(page,cell.contentWidth);
                int thumbX=CellScrollbar::ThumbLeft(page,cell.contentWidth,cellPan);
                c.Box(left+gutter+12,y+cell.height-7,track,5,RGB(235,235,235));
                c.Box(left+gutter+12+thumbX,y+cell.height-7,thumb,5,RGB(166,177,187));
            }
        }
    }
    c.renderer.PopClip();
    if(doc)navigator.Draw(c.renderer,*doc,NotebookNavigator::Geometry(w,h,doc->Height(),scroll),animationTime);
    c.Box(0,0,w,74,RGB(255,255,255));
    c.Text(L"winip",22,12,125,48,34,RGB(226,116,38),false,true);
    int titleAvailable=w-170;
    if(titleEdit.Active()) {
        int field=std::min(titleAvailable,std::max(200,std::min(600,titleAvailable-230)));
        c.renderer.TitleInput(titleEdit.Text(),titleEdit.Caret(),titleEdit.Anchor(),150,20,float(field));
        if(titleAvailable-field>=220)c.Text(L"Enter: rename · Esc: cancel",150+field+16,26,204,32,15,RGB(110,110,110));
    } else {
        int reserve=doc && titleAvailable>=450?185:0;
        float titleWidth=c.renderer.Text(doc?doc->Title():L"Untitled",150,20,float(titleAvailable-reserve),38,24,RGB(35,35,35),false,false,true);
        if(reserve)c.Text(L"Click title to rename",150+int(std::ceil(titleWidth))+16,26,169,32,15,RGB(110,110,110));
    }
    c.Box(0,73,w,1,RGB(218,218,218));
    c.Box(0,74,w,32,RGB(247,247,247));
    for(int i=0;i<8;++i) {
        int x=Menus::Left(i,w),slot=Menus::Left(i+1,w)-x;
        if(menu==i)c.Box(x,76,slot,28,RGB(224,233,241));
        c.Text(Menus::Names[i],x+5,80,slot-8,22,14);
    }
    c.Box(0,105,w,1,RGB(219,219,219));
    c.Box(0,106,w,48,RGB(255,255,255));
    const auto kind=doc && selected>=0 && selected<int(doc->Cells().size())?doc->Cells()[selected].text->kind:L"";
    for(const auto& tool:NotebookToolbar::Items(w,kind)) {
        c.Box(tool.x,114,tool.width,32,RGB(207,207,207));
        c.Box(tool.x+1,115,tool.width-2,30,RGB(255,255,255));
        c.Text(tool.command==Menus::RunAll && kernelBusy?L"↻ Run all":tool.label,tool.x+5,119,tool.width-10,25,14);
    }
    c.Box(0,153,w,1,RGB(218,218,218));
    c.Box(0,h-28,w,28,RGB(250,250,250));
    int badge=w>=650?210:72;
    const int kernelWidth=w>=650?165:125;
    const int kernelX=w-badge-kernelWidth-24;
    c.Text(status,18,h-23,std::max(0,kernelX-28),24,13,RGB(85,85,85),false,false,true);
    c.renderer.RoundedBox(float(kernelX),float(h-18),10,10,5,
        !kernelConnected?RGB(145,145,145):kernelBusy?RGB(206,55,55):RGB(45,157,83));
    c.Text(kernelStatus,kernelX+17,h-23,kernelWidth-17,24,13,RGB(85,85,85),false,false,true);
    c.Text(w>=650?(hdr?L"HDR · scRGB  |  GPU":L"SDR · sRGB  |  GPU"):(hdr?L"HDR · GPU":L"SDR · GPU"),w-badge-10,h-23,badge,24,13,RGB(85,85,85));    if(menu>=0 && !menuItems.empty()) {
        int x=Menus::PopupLeft(menu,w),row=Menus::RowHeight((int)menuItems.size(),h),popupHeight=row*(int)menuItems.size()+8;
        c.Box(x+3,109,314,popupHeight,RGB(206,206,206));c.Box(x,106,314,popupHeight,RGB(195,195,195));c.Box(x+1,107,312,popupHeight-2,RGB(255,255,255));
        for(int i=0;i<(int)menuItems.size();++i) {
            auto& item=menuItems[i];int y=110+i*row;
            if(i==menuItem && item.enabled)c.Box(x+3,y,308,row,RGB(224,237,248));
            auto color=item.enabled || item.information?RGB(45,45,45):RGB(155,155,155);
            c.Text(item.label,x+12,y+2,item.shortcut.empty()?290:174,row-2,14,color,false,false,true);
            c.Text(item.shortcut,x+188,y+2,114,row-2,12,color,false,false,true);
        }
    }
}

std::shared_ptr<const NotebookImage> HitImage(int x,int y) { // mutex held; published geometry only
    if(!app.document || y<177 || y>=int(app.height/app.scale)-34 || app.menu>=0)return {};
    const int width=int(app.width/app.scale),left=NotebookViewport::SideMargin(width),gutter=width<650?88:128;
    const int contentY=y-190+app.scroll,page=HorizontalPage();
    const auto& cells=app.document->Cells();
    auto it=std::upper_bound(cells.begin(),cells.end(),contentY,[](int pos,const Cell& cell){return pos<cell.top;});
    if(it==cells.begin())return {};
    --it;const int pan=std::min(app.horizontal,std::max(0,it->contentWidth-page));
    auto inside=[&](float ix,float iy,float w,float h) {return x>=ix && x<ix+w && y>=iy && y<iy+h;};
    if(x>=left+gutter && x<width-left-22 && it->markdown && !it->sourceMode)for(const auto& image:it->markdown->Images())
        if(inside(left+gutter-pan+image.x,190+it->top-app.scroll+8+image.y,image.width,image.height))return image.image;
    if(x>=left+gutter+12 && x<width-left-32 && it->output)for(const auto& image:it->output->Blocks())
        if(image.image && inside(float(left+gutter+12-pan),190+it->top-app.scroll+it->sourceHeight+34+image.top,image.width,image.height))return image.image;
    return {};
}

void RenderLoop() {
    try {
        Renderer renderer(app.window,app.forceSdr,!app.metricsPath.empty());
        app.adapter=renderer.AdapterName();
        {std::lock_guard<std::mutex> lock(app.mutex);app.uploadDevice=renderer.UploadDevice();app.uploadDeviceReady=true;app.loadWake.notify_one();}
        Canvas canvas{renderer};
        NotebookFlow flow;
        int selected = 0;
        int retries = 0;
        for (;;) {
            int width,height,scroll,click,horizontal,contentWidth; float scale; bool refresh,forceSdr,kernelBusy,kernelConnected;std::wstring kernelStatus;
            NotebookNavigator navigator;uint64_t animationTime=0;
            std::wstring status, snapshot;TitleEditor titleEdit;CellEditView editView;int menu,menuItem;std::vector<Menus::Item> menuItems;
            uint64_t revision,imageViewRevision;ExpandedImages expanded;
            std::shared_ptr<const ImageUpload> imageUploads;
            std::shared_ptr<const Notebook> doc;
            {
                std::unique_lock<std::mutex> lock(app.mutex);
                app.renderWake.wait(lock,[&]{return app.stop || app.dirty;});
                if (app.stop) {
                    app.textCache={{"bytes",renderer.TextCacheBytes()},{"hits",renderer.TextCacheHits()},{"misses",renderer.TextCacheMisses()}};
                    const char* names[]={"display","resize_including_display","text_layout_creation","end_draw","present","commit"};
                    const auto& metrics=renderer.Metrics();size_t i=0;
                    for(const auto& sample:metrics.Samples())app.renderStages.push_back({{"stage",names[i++]},{"calls",sample.calls},{"cpu_cycles",sample.cycles},{"wall_ms",metrics.Milliseconds(sample.wallTicks)}});
                    return;
                }
                expanded=app.expandedImages;imageUploads=app.imageUploads;imageViewRevision=app.imageViewRevision;
                navigator=app.navigator;animationTime=GetTickCount64();
                app.dirty=false; width=app.width; height=app.height; scroll=app.scroll;
                forceSdr=app.forceSdr;kernelBusy=app.kernel->Busy();kernelConnected=app.kernel->Connected();kernelStatus=app.kernelStatus;
                scale=app.scale; refresh=app.displayChanged; app.displayChanged=false;
                click=app.clickY; app.clickY=-1; doc=app.document; status=app.status+L"  |  "+(app.python.error.empty()?L"Python "+app.python.version:app.python.error);titleEdit=app.titleEdit;menu=app.menu;menuItem=app.menuItem;
                horizontal=app.horizontal;contentWidth=app.documentWidth;
                if(app.editor){editView=app.editor->View();selected=(int)app.editor->Selected();}
                menuItems=Menus::Items(menu,doc!=nullptr,(titleEdit.Active() || editView.id!=0),app.fullscreen,app.pythonStatus,editView.id!=0,!app.forceSdr,app.settings->UseLastFolder());
                auto settingsError=app.settings->Error();if(!settingsError.empty())status+=L" | "+settingsError;
                revision=app.revision; snapshot=app.noSnapshots?L"":app.snapshotPath;app.snapshotPath.clear();
            }
            if (!width || !height) continue;
            auto start=Clock::now();
            renderer.SetHdrEnabled(!forceSdr);renderer.Resize(width,height,refresh);
            if(!renderer.Width() || !renderer.Height()) continue;
            // A resize can race the UI snapshot; redraw using the actual surface extent.
            canvas.width=renderer.Width(); canvas.height=renderer.Height(); canvas.scale=scale;
            const auto sourceDocument=doc;
            doc=flow.Prepare(doc,int(canvas.width/scale),int(canvas.height/scale),expanded,imageViewRevision);
            if(doc) {
                if(sourceDocument && doc!=sourceDocument && !sourceDocument->Cells().empty()) {
                    auto it=std::upper_bound(sourceDocument->Cells().begin(),sourceDocument->Cells().end(),scroll,[](int y,const Cell& cell){return y<cell.top;});
                    if(it!=sourceDocument->Cells().begin())--it;
                    const size_t index=size_t(it-sourceDocument->Cells().begin());
                    scroll=doc->Cells()[index].top+std::min(std::max(0,scroll-it->top),doc->Cells()[index].height);
                }
                contentWidth=doc->ContentWidth();
                const int logicalWidth=int(canvas.width/scale);
                const int page=std::max(1,logicalWidth-2*NotebookViewport::SideMargin(logicalWidth)-(logicalWidth<650?88:128)-46);
                scroll=std::clamp(scroll,0,std::max(0,doc->Height()-std::max(22,int(canvas.height/scale)-232)));
                horizontal=std::clamp(horizontal,0,std::max(0,contentWidth-page));
            }
            renderer.SetImageUploads(imageUploads);
            renderer.Begin(scale);
            Draw(canvas,doc.get(),scroll,horizontal,contentWidth,selected,status,renderer.IsHdr(),titleEdit,editView,menu,menuItem,menuItems,navigator,animationTime,kernelBusy,kernelConnected,kernelStatus);
            Max(app.renderMaxNs,(uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count());
            if (renderer.Present(snapshot)) {
                if(doc) {
                    std::lock_guard<std::mutex> lock(app.mutex);
                    if(app.document==sourceDocument && app.revision==revision) {
                        if(doc!=sourceDocument) {
                            if(app.editor && app.editor->Document()==sourceDocument)app.editor->ApplyLayout(doc);
                            app.document=doc;app.documentHeight=doc->Height();app.documentWidth=doc->ContentWidth();
                        }
                        app.scroll=scroll;app.horizontal=horizontal;
                    }
                }
                if(navigator.Animating(animationTime)) {
                    std::lock_guard<std::mutex> lock(app.mutex);Dirty();
                }
                ++app.frames; retries=0; app.hdr=renderer.IsHdr(); app.whiteScale=renderer.WhiteScale();
                app.refreshHz=renderer.RefreshHz();
                app.presentedRevision=revision;SetEvent(app.frameReady);
                if(!app.metricsPath.empty() && app.frameStatistics.size()<512) {
                    DXGI_FRAME_STATISTICS stats{};unsigned lastPresent=0;auto hr=renderer.FrameStatistics(stats,lastPresent);
                    app.frameStatistics.push_back({{"revision",revision},{"width",canvas.width},{"height",canvas.height},{"allocation_width",renderer.AllocationWidth()},{"allocation_height",renderer.AllocationHeight()},{"snapshot",!snapshot.empty()},
                        {"frame_work_ms",std::chrono::duration<double,std::milli>(Clock::now()-start).count()},{"statistics_hresult",(int64_t)hr},{"last_present",lastPresent},{"displayed_present",stats.PresentCount},
                        {"present_refresh",stats.PresentRefreshCount},{"sync_refresh",stats.SyncRefreshCount},{"sync_qpc",stats.SyncQPCTime.QuadPart}});
                }
            }
            else {
                std::lock_guard<std::mutex> lock(app.mutex);
                app.displayChanged=true;
                // At most two immediate rebuilds; avoid spinning on an unusable surface.
                if(++retries<=2) Dirty();
            }
        }
    } catch(const std::exception& error) {
        std::lock_guard<std::mutex> lock(app.mutex);
        app.error=Utf16(error.what()); app.renderFailed=true;app.loadWake.notify_one();SetEvent(app.frameReady);
        PostMessageW(app.window,Failed,0,0);
    }
}
uint64_t CpuTicks() {
    FILETIME created,exited,kernel,user;
    GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user);
    ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return k.QuadPart+u.QuadPart;
}
uint64_t ThreadCpuTicks(HANDLE thread) {
    FILETIME created{},exited{},kernel{},user{};
    if(!GetThreadTimes(thread,&created,&exited,&kernel,&user)) return 0;
    ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return k.QuadPart+u.QuadPart;
}
std::array<uint64_t,3> AppThreadCpuTicks() {
    return {ThreadCpuTicks(GetCurrentThread()),ThreadCpuTicks(app.renderThread.native_handle()),ThreadCpuTicks(app.loadThread.native_handle())};
}
void Probe(HWND window) {
    // Explicit integration test mode; there are no timers in normal operation.
    static int phase=0;
    static uint64_t waitingFor=0,idleFrames=0,idleCpu=0;
    static std::array<uint64_t,3> idleThreads{};
    static int idleSamples=0;
    static int quietStreak=0;
    static std::map<UINT,MessageCount> idleMessages;
    static auto started=Clock::now();
    if(app.renderFailed || Clock::now()-started>std::chrono::seconds(30)) {
        app.probeResults.push_back({{"failure","Renderer failure or probe timeout"}});DestroyWindow(window);return;
    }
    if(waitingFor && app.presentedRevision<waitingFor) return;
    if(!app.documentHeight || !app.frames || !app.pythonDone) return;
    if(phase==0) {
        auto style=GetWindowLongPtrW(window,GWL_STYLE);
        RECT rect{};GetWindowRect(window,&rect);
        auto hit=SendMessageW(window,WM_NCHITTEST,0,MAKELPARAM(rect.right-2,(rect.top+rect.bottom)/2));
        app.probeResults.push_back({{"thick_frame",bool(style&WS_THICKFRAME)},{"native_menu_absent",GetMenu(window)==nullptr},{"right_edge_hit",hit}});
    }
    if(phase>=1 && phase<=4) {
        std::lock_guard<std::mutex> lock(app.mutex);
        app.probeResults.push_back({{"phase",phase-1},{"width_px",app.width},{"height_px",app.height},{"scale",app.scale},{"presented_revision",app.presentedRevision.load()}});
    }
    if(phase<4) {
        const int widths[]={420,640,960,1200};
        const float scales[]={1.f,1.5f,2.f,1.f};
        float scale=scales[phase];
        {
            std::lock_guard<std::mutex> lock(app.mutex);
            app.scale=scale;
        }
        RECT outer{},client{};GetWindowRect(window,&outer);GetClientRect(window,&client);
        int frameWidth=outer.right-outer.left-client.right,frameHeight=outer.bottom-outer.top-client.bottom;
        SetWindowPos(window,nullptr,0,0,int(widths[phase]*scale)+frameWidth,int(600*scale)+frameHeight,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        {std::lock_guard<std::mutex> lock(app.mutex);app.snapshotPath=app.probeDirectory+L"/layout-"+std::to_wstring(phase)+L".bmp";Dirty();waitingFor=app.revision;}
        ++phase;return;
    }
    if(phase==4) {
        const auto& cell=app.document->Cells().front();
        int page=HorizontalPage(),maximum=std::max(0,cell.contentWidth-page);
        if(maximum) {
            {std::lock_guard<std::mutex> lock(app.mutex);app.scroll=cell.top;app.horizontal=0;}
            int logicalWidth=int(app.width/app.scale),left=NotebookViewport::SideMargin(logicalWidth),gutter=logicalWidth<650?88:128;
            int start=left+gutter+12,y=190+cell.height-5;
            auto point=[&](int x){return MAKELPARAM(int(x*app.scale),int(y*app.scale));};
            SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,point(start+5));
            SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,point(start+page));
            bool end=app.horizontal==maximum;
            SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,point(start+page/2));
            bool middle=app.horizontal>0 && app.horizontal<maximum;
            SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,point(start+5));
            bool begin=app.horizontal==0;
            SendMessageW(window,WM_LBUTTONUP,0,point(start+5));
            bool released=app.scrollbar.CellId()==0;
            app.probeResults.push_back({{"scrollbar_drag",end && middle && begin && released}});
            if(!end || !middle || !begin || !released) {
                app.probeResults.push_back({{"failure","Cell scrollbar drag failed"}});DestroyWindow(window);return;
            }
        }
        SendMessageW(window,WM_HSCROLL,SB_RIGHT,0);
        SendMessageW(window,WM_KEYDOWN,VK_END,0);
        {
            std::lock_guard<std::mutex> lock(app.mutex);
            app.snapshotPath=app.probeDirectory+L"/line-end.bmp";Dirty();
        }
        phase=5;waitingFor=0;return;
    }
    if(phase==5) {
        std::lock_guard<std::mutex> lock(app.mutex);
        if(app.presentedRevision<app.revision) return;
        app.probeResults.push_back({{"scroll_end",app.scroll},{"horizontal_end",app.horizontal},{"content_width",app.documentWidth.load()},
            {"horizontal_page",HorizontalPage()},{"python_executable",Utf8(app.python.executable)},{"python_origin",Utf8(app.python.origin)},{"python_version",Utf8(app.python.version)},{"python_bits",app.python.bits},{"python_error",Utf8(app.python.error)},
            {"hdr",app.hdr.load()},{"sdr_white_scale",app.whiteScale.load()}});
        app.snapshotPath.clear();idleFrames=app.frames;idleCpu=CpuTicks();idleThreads=AppThreadCpuTicks();idleMessages=app.probeMessages;
        phase=6;SetTimer(window,41,2000,nullptr);return;
    }
    if(phase==6) {
        auto threads=AppThreadCpuTicks();uint64_t process=CpuTicks();
        auto messages=nlohmann::json::array();
        bool inputActive=false;
        for(const auto& [message,count]:app.probeMessages) {
            auto before=idleMessages.find(message);
            MessageCount prior=before==idleMessages.end()?MessageCount{}:before->second;
            if(count.calls!=prior.calls) messages.push_back({{"message",message},{"calls",count.calls-prior.calls},{"wall_ms",(count.wallNs-prior.wallNs)/1e6}});
            if(count.calls!=prior.calls && ((message>=WM_MOUSEFIRST && message<=WM_MOUSELAST) ||
                (message>=WM_KEYFIRST && message<=WM_KEYLAST) || message==WM_NCMOUSEMOVE ||
                message==WM_ACTIVATE || message==WM_ACTIVATEAPP || message==WM_SETFOCUS || message==WM_KILLFOCUS ||
                message==WM_SIZE || message==WM_MOVE)) inputActive=true;
        }
        quietStreak=inputActive?0:quietStreak+1;
        app.probeResults.push_back({{"idle_sample",++idleSamples},{"idle_seconds",2},{"idle_presented_frames",app.frames.load()-idleFrames},
            {"idle_cpu_ms",(process-idleCpu)/10000.0},{"idle_ui_cpu_ms",(threads[0]-idleThreads[0])/10000.0},
            {"idle_render_cpu_ms",(threads[1]-idleThreads[1])/10000.0},{"idle_loader_cpu_ms",(threads[2]-idleThreads[2])/10000.0},
            {"input_active",inputActive},{"quiet_streak",quietStreak},{"messages",messages}});
        if(quietStreak<3) {idleCpu=process;idleThreads=threads;idleFrames=app.frames;idleMessages=app.probeMessages;return;}
        SetTimer(window,41,100,nullptr); ShowWindow(window,SW_MINIMIZE);phase=7;return;
    }
    if(phase==7) {ShowWindow(window,SW_RESTORE);phase=8;waitingFor=0;return;}
    if(phase==8) {
        std::lock_guard<std::mutex> lock(app.mutex);
        if(app.presentedRevision<app.revision) return;
        app.probeResults.push_back({{"restored",true}});
        phase=9;
    }
    if(phase==9) {KillTimer(window,41);DestroyWindow(window);}
}
void InteractionProbe(HWND window) {
    static int phase=0;static uint64_t waitingFor=0;static RECT restored{};
    static auto started=Clock::now();
    auto require=[&](bool condition,const char* text) {if(!condition)throw std::runtime_error(text);};
    auto snapshot=[&](const wchar_t* name) {
        std::lock_guard<std::mutex> lock(app.mutex);app.snapshotPath=app.probeDirectory+L"/"+name+L".bmp";Dirty();waitingFor=app.revision;
    };
    auto altEnter=[&] {SendMessageW(window,WM_SYSKEYDOWN,VK_RETURN,1LL<<29);};
    try {
        require(!app.renderFailed && Clock::now()-started<std::chrono::seconds(25),"Interaction probe timed out or renderer failed");
        if((waitingFor && app.presentedRevision<waitingFor) || !app.documentHeight || !app.pythonDone)return;
        switch(phase) {
        case 0:
            require(IsZoomed(window),"Default launch was not maximized");
            app.probeResults.push_back({{"default_maximized",true}});snapshot(L"maximized");break;
        case 1: altEnter();snapshot(L"fullscreen");break;
        case 2: {
            MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
            RECT rect{};GetWindowRect(window,&rect);
            require(EqualRect(&rect,&monitor.rcMonitor) && !(GetWindowLongPtrW(window,GWL_STYLE)&WS_OVERLAPPEDWINDOW),"Fullscreen does not cover the monitor");
            RECT client{};GetClientRect(window,&client);
            app.probeResults.push_back({{"outer_width",rect.right-rect.left},{"outer_height",rect.bottom-rect.top},{"client_width",client.right},{"client_height",client.bottom}});
            require(client.right==rect.right-rect.left && client.bottom==rect.bottom-rect.top,"Fullscreen client area leaves unrendered regions");
            app.probeResults.push_back({{"fullscreen_client_width",client.right},{"fullscreen_client_height",client.bottom},{"model_width",app.width},{"model_height",app.height}});
            SendMessageW(window,WM_SYSKEYDOWN,VK_RETURN,(1LL<<29)|(1LL<<30));require(app.fullscreen,"Key repeat exited fullscreen");
            altEnter();
            app.probeResults.push_back({{"fullscreen_monitor_bounds",true},{"maximized_restored",true},{"repeat_ignored",true}});
            snapshot(L"maximized-restored");break;
        }
        case 3: require(IsZoomed(window),"Fullscreen did not restore maximized state");ShowWindow(window,SW_RESTORE);snapshot(L"restored");break;
        case 4: GetWindowRect(window,&restored);altEnter();snapshot(L"fullscreen-from-restored");break;
        case 5: altEnter();snapshot(L"restored-again");break;
        case 6: {
            RECT rect{};GetWindowRect(window,&rect);
            require(EqualRect(&rect,&restored) && !IsZoomed(window),"Fullscreen changed restored placement");
            app.probeResults.push_back({{"restored_placement_preserved",true},{"restored_width_px",rect.right-rect.left},{"restored_height_px",rect.bottom-rect.top}});
            SendMessageW(window,WM_LBUTTONDOWN,0,MAKELPARAM(int(170*app.scale),int(30*app.scale)));
            require(app.titleEdit.Active(),"Clicking the title did not begin editing");
            for(auto ch:std::wstring(L"bad/name"))SendMessageW(window,WM_CHAR,ch,0);
            SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
            require(app.titleEdit.Active() && !app.renamePending,"Invalid filename was queued");snapshot(L"invalid-title");break;
        }
        case 7:
            SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,0);require(!app.titleEdit.Active(),"Escape did not cancel title editing");
            SendMessageW(window,WM_KEYDOWN,VK_F2,0);
            for(auto ch:std::wstring(L"Renamed 日本語 notebook"))SendMessageW(window,WM_CHAR,ch,0);
            snapshot(L"editing-title");break;
        case 8: SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);waitingFor=0;break;
        case 9: {
            std::lock_guard<std::mutex> lock(app.mutex);if(app.renamePending || app.savePending)return;
            require(app.document->Title()==L"Renamed 日本語 notebook","Rename did not publish the new title");
            app.probeResults.push_back({{"click_title_edits",true},{"invalid_title_rejected",true},{"escape_cancels",true},{"rename_published",true},{"renamed_path",Utf8(app.loadedPath)}});
            app.snapshotPath=app.probeDirectory+L"/renamed.bmp";Dirty();waitingFor=app.revision;break;
        }
        case 10:
            SendMessageW(window,WM_LBUTTONDOWN,0,MAKELPARAM(int(30*app.scale),int(90*app.scale)));
            require(app.menu==0,"File dropdown did not open");snapshot(L"file-menu");break;
        case 11:
            SendMessageW(window,WM_KEYDOWN,VK_DOWN,0);SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);waitingFor=0;break;
        case 12:
            require(app.titleEdit.Active() && app.menu==-1,"Menu rename command did not focus title editing");
            SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,0);
            app.probeResults.push_back({{"file_menu_opens",true},{"menu_keyboard_rename",true},{"native_scrollbar_absent",!(GetWindowLongPtrW(window,GWL_STYLE)&WS_HSCROLL)}});
            snapshot(L"two-row-header");break;
        default:
            if(phase>=13 && phase<19) {
                int clientWidth=(phase%2?640:1200)*2,clientHeight=600*2;
                RECT outer{},client{};GetWindowRect(window,&outer);GetClientRect(window,&client);
                SetWindowPos(window,nullptr,0,0,clientWidth+outer.right-outer.left-client.right,clientHeight+outer.bottom-outer.top-client.bottom,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                GetClientRect(window,&client);
                require(client.right==clientWidth && client.bottom==clientHeight,"Repeated shrink/expand changed client dimensions");
                app.probeResults.push_back({{"resize_cycle",phase-13},{"client_width",client.right},{"client_height",client.bottom}});
                auto name=L"resize-cycle-"+std::to_wstring(phase-13);snapshot(name.c_str());break;
            }
            KillTimer(window,41);DestroyWindow(window);return;
        }
        ++phase;
    } catch(const std::exception& error) {
        app.probeResults.push_back({{"failure",error.what()}});KillTimer(window,41);DestroyWindow(window);
    }
}
void ResizeProbe(HWND window) {
    if(!app.documentHeight || !app.pythonDone || app.presentedRevision<app.revision)return;
    KillTimer(window,41);
    RECT outer{},client{};GetWindowRect(window,&outer);GetClientRect(window,&client);
    int borderX=outer.right-outer.left-client.right,borderY=outer.bottom-outer.top-client.bottom;
    const int steps=320;
    auto cpu=CpuTicks(),frames=app.frames.load();auto start=Clock::now();
    for(int step=0;step<steps && !app.renderFailed;++step) {
        int ramp=step%80; if(ramp>=40)ramp=79-ramp;
        int width=1280+ramp*24,height=900+ramp*12;
        SetWindowPos(window,nullptr,0,0,width+borderX,height+borderY,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        GetClientRect(window,&client);
        if(client.right!=width || client.bottom!=height || app.presentedRevision<app.revision) {
            app.probeResults.push_back({{"failure","Resize geometry or atomic presentation mismatch"}});break;
        }
    }
    auto wall=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    app.probeResults.push_back({{"resize_steps",steps},{"process_cpu_ms",(CpuTicks()-cpu)/10000.0},{"wall_ms",wall},{"frames",app.frames.load()-frames}});
    DestroyWindow(window);
}
void NavigatorProbe(HWND window) {
    static int phase=0;static uint64_t phaseAt=0,frames=0,cpu=0,waiting=0;
    static auto started=Clock::now();
    auto require=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    try {
        require(Clock::now()-started<std::chrono::seconds(30),"Navigator probe timed out");
        std::shared_ptr<const Notebook> document;
        {std::lock_guard<std::mutex> lock(app.mutex);
            if(!app.document || !app.pythonDone || (waiting && app.presentedRevision<waiting) || (phase==0 && app.presentedRevision<app.revision))return;
            document=app.document;
        }
        auto geometry=NotebookNavigator::Geometry(int(app.width/app.scale),int(app.height/app.scale),document->Height(),app.scroll);
        auto point=[&](float y){return MAKELPARAM(int((geometry.x+11)*app.scale),int(y*app.scale));};
        auto snapshot=[&](const wchar_t* name){std::lock_guard<std::mutex> lock(app.mutex);app.snapshotPath=app.probeDirectory+L"/"+name+L".bmp";Dirty();waiting=app.revision;};
        if(phase==0) {
            require(geometry.maximum>0,"Navigator fixture is not scrollable");
            SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,point(geometry.thumbY+5));
            require(app.navigator.Held(),"Vertical thumb did not capture");
            SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,point(geometry.y+geometry.height));
            require(app.scroll==geometry.maximum,"Vertical drag cannot reach notebook end");
            SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,point(geometry.y+geometry.height*.5f));
            require(app.scroll>0 && app.scroll<geometry.maximum,"Vertical drag did not reach middle");
            frames=app.frames;phaseAt=GetTickCount64();++phase;return;
        }
        if(phase==1) {
            if(GetTickCount64()-phaseAt<300)return;
            require(app.frames>frames+1,"Navigator did not animate");
            require(!app.navigator.Animating(GetTickCount64()),"Navigator animation did not settle");
            size_t headings=0;for(const auto& cell:document->Cells())if(cell.markdown)headings+=cell.markdown->Headings().size();
            require(headings>=8,"Markdown heading index incomplete");
            snapshot(L"navigator-expanded");++phase;return;
        }
        if(phase==2) {frames=app.frames;phaseAt=GetTickCount64();++phase;return;}
        if(phase==3) {
            if(GetTickCount64()-phaseAt<300)return;
            require(app.frames==frames,"Holding a stationary navigator kept rendering");
            SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,point(geometry.y+5));
            require(app.scroll==0,"Vertical drag cannot reach notebook beginning");
            SendMessageW(window,WM_LBUTTONUP,0,point(geometry.y+5));
            require(!app.navigator.Held(),"Vertical drag survived mouse release");
            phaseAt=GetTickCount64();++phase;return;
        }
        if(phase==4) {
            if(GetTickCount64()-phaseAt<300)return;
            require(app.navigator.Expansion(GetTickCount64())==0,"Navigator did not retract");
            {std::lock_guard<std::mutex> lock(app.mutex);app.scale=2;Dirty();}
            SetWindowPos(window,nullptr,0,0,1300,1100,SWP_NOMOVE|SWP_NOZORDER);
            snapshot(L"navigator-200dpi");++phase;return;
        }
        if(phase==5) {
            SendMessageW(window,WM_MOUSEMOVE,0,MAKELPARAM(10,200));
            require(!app.navigator.Visible(geometry),"Narrow navigator did not hide");
            snapshot(L"navigator-narrow-hidden");++phase;return;
        }
        if(phase==6) {
            SendMessageW(window,WM_MOUSEMOVE,0,MAKELPARAM(int((geometry.x-100)*app.scale),int((geometry.y+20)*app.scale)));
            require(app.navigator.Visible(geometry),"Narrow navigator did not reveal near pointer");
            snapshot(L"navigator-narrow-revealed");++phase;return;
        }
        if(phase==7) {
            SendMessageW(window,WM_MOUSELEAVE,0,0);
            require(!app.navigator.Visible(geometry),"Narrow navigator did not hide on pointer leave");
            app.probeResults.push_back({{"vertical_drag",true},{"heading_index",true},{"animation_settled",true},{"stationary_hold_idle",true},{"narrow_auto_hide",true}});
            waiting=app.revision;++phase;return;
        }
        if(phase==8) {frames=app.frames;cpu=CpuTicks();SetTimer(window,41,2000,nullptr);++phase;return;}
        app.probeResults.push_back({{"idle_frames",app.frames.load()-frames},{"idle_cpu_ms",(CpuTicks()-cpu)/10000.0}});
        frames=app.frames;cpu=CpuTicks();if(++phase<12)return;DestroyWindow(window);
    } catch(const std::exception& error) {app.probeResults.push_back({{"failure",error.what()}});DestroyWindow(window);}
}
void OpenErrorProbe(HWND window) {
    static int phase=0;
    static uint64_t original=0;
    static const auto started=Clock::now();
    static const wchar_t* files[]={L"missing-notebook.ipynb",L"malformed-notebook.ipynb",L"invalid-utf8.ipynb"};
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    try {
        require(Clock::now()-started<std::chrono::seconds(25),"Error dialog probe timed out");
        if(!app.document || !app.pythonDone || app.editor->Document()!=app.document)return;
        if(phase==0) {
            original=app.document->ContentRevision();
            phase=1;RequestOpen(app.probeDirectory+L"/"+files[0]);return;
        }
        if(phase==1 || phase==3 || phase==5) {
            HWND popup=GetLastActivePopup(window);
            if(popup==window || !IsWindowVisible(popup))return;
            wchar_t title[256]{};GetWindowTextW(popup,title,256);
            require(std::wstring(title)==L"Could not open notebook","Wrong error dialog title");
            bool details=false;
            for(HWND child=GetWindow(popup,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
                wchar_t text[4096]{};GetWindowTextW(child,text,4096);
                if(std::wstring(text).find(files[phase/2])!=std::wstring::npos)details=true;
            }
            require(details && !IsWindowEnabled(window),"Error is not an owned modal dialog with file details");
            ++phase;PostMessageW(popup,WM_COMMAND,IDOK,0);return;
        }
        require(app.document->ContentRevision()==original,"Failed open replaced the current document");
        if(phase<6) {const auto file=files[phase/2];++phase;RequestOpen(app.probeDirectory+L"/"+file);return;}
        if(phase==6) {
            app.probeResults.push_back({{"missing_file_modal",true},{"malformed_file_modal",true},{"invalid_utf8_modal",true},{"previous_document_preserved",true}});
            DestroyWindow(window);
        }
    } catch(const std::exception& error) {app.probeResults.push_back({{"failure",error.what()}});DestroyWindow(window);}
}
void MediaProbe(HWND window) {
    static int phase=0;
    static uint64_t waiting=0,frames=0,cpu=0,contentRevision=0;
    static size_t firstCell=0,visit=0;
    static RECT reloadRect{};
    static int loadResizeSteps=0;
    static double loadResizeMaxMs=0;
    static std::vector<std::pair<size_t,float>> locations;
    static std::shared_ptr<const NotebookImage> selected;
    static int fittedCellHeight=0,fittedNextTop=0;
    static float fittedImageHeight=0;
    static const auto started=Clock::now();
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    auto invalidate=[&](const wchar_t* capture=nullptr) {
        std::lock_guard<std::mutex> lock(app.mutex);
        if(capture && !app.noSnapshots)app.snapshotPath=app.probeDirectory+L"/"+capture+L".bmp";
        Dirty();waiting=app.revision;
    };
    try {
        require(Clock::now()-started<std::chrono::seconds(90),"Media probe timed out");
        require(!app.renderFailed,"Media renderer failed");
        if(phase==8 && app.status==L"Loading notebook...") {
            const auto before=Clock::now();
            const int delta=(loadResizeSteps%4)*12;
            SetWindowPos(window,nullptr,reloadRect.left,reloadRect.top,reloadRect.right-reloadRect.left-delta,
                reloadRect.bottom-reloadRect.top-delta,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
            loadResizeMaxMs=std::max(loadResizeMaxMs,std::chrono::duration<double,std::milli>(Clock::now()-before).count());
            ++loadResizeSteps;return;
        }
        if(!app.document || !app.pythonDone || app.editor->Document()!=app.document || app.presentedRevision<app.revision || (waiting && app.presentedRevision<waiting))return;
        for(const auto& cell:app.document->Cells())if(cell.markdown && !cell.markdown->Images().empty() && cell.markdown->Text().back()==L'\ufffc') {
            const auto& last=cell.markdown->Images().back();
            require(cell.sourceHeight<=last.y+last.height+30,"Markdown image retained an oversized trailing line");
        }
        if(phase==0) {
            contentRevision=app.document->ContentRevision();
            for(size_t i=0;i<app.document->Cells().size();++i) {
                const auto& cell=app.document->Cells()[i];
                if(cell.markdown) {
                    require(!cell.markdown->NeedsImages(),"File opening left image decoding for later");
                    if(!selected && !cell.markdown->Images().empty()){firstCell=i;selected=cell.markdown->Images()[0].image;}
                }
            }
            require(selected!=nullptr,"No Markdown image to test");
            const auto& cell=app.document->Cells()[firstCell];
            {std::lock_guard<std::mutex> lock(app.mutex);app.scroll=cell.top+int(cell.markdown->Images()[0].y);}
            invalidate(L"media-fit");++phase;return;
        }
        if(phase==1 || phase==3) {
            const auto& cell=app.document->Cells()[firstCell];const auto& image=cell.markdown->Images()[0];
            if(phase==1) {
                fittedCellHeight=cell.height;fittedImageHeight=image.height;
                fittedNextTop=firstCell+1<app.document->Cells().size()?app.document->Cells()[firstCell+1].top:app.document->Height();
            }
            int w=int(app.width/app.scale),left=NotebookViewport::SideMargin(w),gutter=w<650?88:128;
            int x=left+gutter+int(image.x)+10,y=190+cell.top-app.scroll+8+int(image.y)+10;
            require(y>=177 && y<int(app.height/app.scale)-34,"Image target is outside viewport");
            const auto point=MAKELPARAM(int(x*app.scale),int(y*app.scale));
            SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,point);
            SendMessageW(window,WM_LBUTTONUP,0,point);
            SendMessageW(window,WM_LBUTTONDBLCLK,MK_LBUTTON,point);
            SendMessageW(window,WM_LBUTTONUP,0,point);
            require(!app.editor->Active(),"Image double-click entered Markdown source");
            invalidate(phase==1?L"media-full":L"media-fit-again");++phase;return;
        }
        if(phase==2) {
            const auto& cell=app.document->Cells()[firstCell];
            const auto& image=cell.markdown->Images()[0];
            require(image.width==selected->Width() && image.height==selected->Height(),"Double-click did not expand original image");
            require(std::abs((cell.height-fittedCellHeight)-(image.height-fittedImageHeight))<=2,
                "Expanded Markdown cell height does not match the image height change");
            SendMessageW(window,WM_HSCROLL,SB_RIGHT,0);
            require(app.horizontal>0,"Full-size image cannot scroll horizontally");
            SendMessageW(window,WM_HSCROLL,SB_LEFT,0);
            invalidate();++phase;return;
        }
        if(phase==4) {
            const auto& image=app.document->Cells()[firstCell].markdown->Images()[0];
            require(image.width<selected->Width(),"Second double-click did not fit image");
            const auto nextTop=firstCell+1<app.document->Cells().size()?app.document->Cells()[firstCell+1].top:app.document->Height();
            require(app.document->Cells()[firstCell].height==fittedCellHeight && nextTop==fittedNextTop,
                "Collapsing Markdown image retained stale cell spacing");
            app.probeResults.push_back({{"markdown_occupied_height",true},{"fitted_cell_height",fittedCellHeight},{"fitted_image_height",fittedImageHeight}});
            require(app.document->ContentRevision()==contentRevision && !app.kernel->Connected(),"Viewing images changed or executed notebook");
            RECT rect{};GetWindowRect(window,&rect);
            SendMessageW(window,WM_DPICHANGED,MAKELONG(192,192),reinterpret_cast<LPARAM>(&rect));
            invalidate(L"media-200dpi");++phase;return;
        }
        if(phase==5) {
            for(size_t i=0;i<app.document->Cells().size();++i) {
                const auto& cell=app.document->Cells()[i];
                if(cell.markdown)for(const auto& image:cell.markdown->Images())locations.push_back({i,8+image.y});
                if(cell.output)for(const auto& image:cell.output->Blocks())if(image.image)locations.push_back({i,float(cell.sourceHeight+34)+image.top});
            }
            app.probeResults.push_back({{"uploaded_images",app.imageUploads?app.imageUploads->Count():0},{"uploaded_bytes",app.imageUploads?app.imageUploads->Bytes():0},{"eager_images",locations.size()},{"fit_full_fit",true},{"dpi",192},{"no_execution",true}});
            ++phase;
        }
        if(phase==6) {
            if(visit<locations.size()*2) {
                const auto location=locations[visit%locations.size()];
                {std::lock_guard<std::mutex> lock(app.mutex);app.scroll=app.document->Cells()[location.first].top+int(location.second);}
                ++visit;invalidate();return;
            }
            invalidate();++phase;return;
        }
        if(phase==7) {GetWindowRect(window,&reloadRect);RequestOpen(app.loadedPath);++phase;return;}
        if(phase==8) {
            app.probeResults.push_back({{"resize_during_load_steps",loadResizeSteps},{"resize_during_load_max_ms",loadResizeMaxMs}});
            frames=app.frames;cpu=CpuTicks();SetTimer(window,41,2000,nullptr);++phase;return;
        }
        app.probeResults.push_back({{"idle_frames",app.frames.load()-frames},{"idle_cpu_ms",(CpuTicks()-cpu)/10000.0}});
        frames=app.frames;cpu=CpuTicks();if(++phase>=12)DestroyWindow(window);
    } catch(const std::exception& error) {app.probeResults.push_back({{"failure",error.what()}});DestroyWindow(window);}
}
void ImageProbe(HWND window) {
    static int phase=0;static uint64_t waiting=0,frames=0,cpu=0;
    static std::array<uint64_t,3> idleThreads{};
    static std::map<UINT,MessageCount> idleMessages;
    static auto started=Clock::now();
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    auto snapshot=[&](const wchar_t* name){std::lock_guard<std::mutex> lock(app.mutex);app.snapshotPath=app.probeDirectory+L"/"+name+L".bmp";Dirty();waiting=app.revision;};
    try {
        require(Clock::now()-started<std::chrono::seconds(55),"Image probe timed out");
        require(!app.renderFailed,"Image renderer failed");
        require(app.kernelStatus.find(L"Kernel error:")!=0,Utf8(app.kernelStatus).c_str());
        std::shared_ptr<const Notebook> document;
        {std::lock_guard<std::mutex> lock(app.mutex);
            if(!app.document || !app.pythonDone || app.editor->Document()!=app.document || (waiting && app.presentedRevision<waiting) || (phase==0 && app.presentedRevision<app.revision))return;
            document=app.document;
        }
        if(phase==0) {
            require(document->Cells()[0].text->prompt==L"In [500]:","Opening ran image fixture");
            require(!app.hdr,"Default HDR must be off");
            snapshot(L"image-saved-sdr");SendMessageW(window,WM_COMMAND,Menus::RunAll,0);++phase;return;
        }
        if(phase==1) {
            if(!app.kernel->Connected())return;
            require(app.kernel->Busy(),"Image busy indicator phase missed execution");
            snapshot(L"image-busy");++phase;return;
        }
        if(phase==2) {
            if(app.kernel->Busy() || app.kernelStatus!=L"Kernel idle")return;
            require(document->Cells()[0].text->prompt==L"In [1]:",Utf8(L"Fresh image kernel count: "+document->Cells()[0].text->prompt).c_str());
            auto output=document->Cells()[0].output;
            require(output && output->Blocks().size()==1 && output->Blocks()[0].image,"Live image missing");
            snapshot(L"image-live-sdr");++phase;return;
        }
        if(phase==3) {
            SendMessageW(window,WM_COMMAND,Menus::ToggleHdr,0);waiting=app.revision;++phase;return;
        }
        if(phase==4) {snapshot(L"image-live-hdr");++phase;return;}
        if(phase==5) {
            {std::lock_guard<std::mutex> lock(app.mutex);app.scale=2;Dirty();}
            SetWindowPos(window,nullptr,0,0,1600,1300,SWP_NOMOVE|SWP_NOZORDER);++phase;return;
        }
        if(phase==6) {
            {std::lock_guard<std::mutex> lock(app.mutex);app.scroll=std::max(0,app.documentHeight.load()-int(app.height/app.scale)+232);Dirty();}
            snapshot(L"image-200dpi");++phase;return;
        }
        if(phase==7) {SendMessageW(window,WM_COMMAND,Menus::ToggleHdr,0);SaveDocument(window);waiting=app.revision;++phase;return;}
        if(phase==8) {
            if(app.savePending)return;
            require(!app.hdr,"HDR toggle did not return SDR");
            require(Notebook::Load(app.loadedPath).Serialize()==document->Serialize(),"Image save lost MIME data");
            RequestOpen(app.loadedPath);++phase;return;
        }
        if(phase==9) {
            if(app.status==L"Loading notebook..." || app.kernel->Busy())return;
            require(!app.kernel->Connected(),"Opening notebook retained previous kernel");
            SendMessageW(window,WM_COMMAND,Menus::RunAll,0);++phase;return;
        }
        if(phase==10) {
            if(app.kernel->Busy() || app.kernelStatus!=L"Kernel idle")return;
            require(document->Cells()[0].text->prompt==L"In [1]:","Reopened notebook did not use fresh counter");
            app.probeResults.push_back({{"live_image",true},{"saved_image",true},{"fresh_count",true},{"reopen_resets_kernel",true},{"hdr_toggle",true}});
            {std::lock_guard<std::mutex> lock(app.mutex);Dirty();waiting=app.revision;}++phase;return;
        }
        if(phase==11) {
            frames=app.frames;cpu=CpuTicks();idleThreads=AppThreadCpuTicks();idleMessages=app.probeMessages;
            SetTimer(window,41,2000,nullptr);++phase;return;
        }
        const auto threads=AppThreadCpuTicks();
        const auto process=CpuTicks();
        auto messages=nlohmann::json::array();
        for(const auto& [message,count]:app.probeMessages) {
            const auto before=idleMessages.find(message);
            const auto prior=before==idleMessages.end()?MessageCount{}:before->second;
            if(count.calls!=prior.calls) messages.push_back({{"message",message},{"calls",count.calls-prior.calls},
                {"wall_ms",(count.wallNs-prior.wallNs)/1e6}});
        }
        app.probeResults.push_back({{"idle_frames",app.frames.load()-frames},{"idle_cpu_ms",(process-cpu)/10000.0},
            {"idle_ui_cpu_ms",(threads[0]-idleThreads[0])/10000.0},
            {"idle_render_cpu_ms",(threads[1]-idleThreads[1])/10000.0},
            {"idle_loader_cpu_ms",(threads[2]-idleThreads[2])/10000.0},{"messages",messages}});
        frames=app.frames;cpu=process;idleThreads=threads;idleMessages=app.probeMessages;
        if(++phase>=15)DestroyWindow(window);
    } catch(const std::exception& error) {app.probeResults.push_back({{"failure",error.what()}});DestroyWindow(window);}
}
void KernelProbe(HWND window) {
    static int phase=0;
    static auto started=Clock::now();
    static uint64_t frames=0,cpu=0,kernelCpu=0,waiting=0;
    auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    try {
        require(Clock::now()-started<std::chrono::seconds(55),"Kernel probe timed out");
        require(app.kernelStatus.find(L"Kernel error:")!=0,Utf8(app.kernelStatus).c_str());
        if(!app.document || !app.pythonDone || app.editor->Document()!=app.document)return;
        if(waiting && app.presentedRevision<waiting)return;
        if(phase==0) {
            require(app.document->Cells()[1].text->output.empty(),"Opening notebook executed code");
            SendMessageW(window,WM_COMMAND,Menus::ConnectKernel,0);++phase;return;
        }
        if(phase==1) {
            if(app.kernelStatus!=L"Kernel idle" || app.kernel->Busy())return;
            SendMessageW(window,WM_COMMAND,Menus::RunAll,0);
            {std::lock_guard<std::mutex> lock(app.mutex);
                EditAction([&]{app.editor->Begin(0,0,0);app.editor->SelectAll();app.editor->InsertText(L"#Edited while Python runs");app.editor->End();},false);
            }
            SetWindowPos(window,nullptr,0,0,1100,820,SWP_NOMOVE|SWP_NOZORDER);
            ++phase;return;
        }
        if(phase==2) {
            if(app.kernel->Busy() || app.kernelStatus!=L"Kernel idle" || app.document->Cells()[2].text->output.empty())return;
            require(app.document->Cells()[1].text->output==L"Hello, world!\n","Hello World output mismatch");
            require(app.document->Cells()[2].text->output==L"42\n","Kernel state was not shared across cells");
            require(app.document->Cells()[0].text->source==L"#Edited while Python runs","Execution replaced a concurrent edit");
            SaveDocument(window);++phase;return;
        }
        if(phase==3) {
            if(app.savePending)return;
            require(!Unsaved(),"Kernel output save did not finish");
            require(Notebook::Load(app.loadedPath).Serialize()==app.document->Serialize(),"Saved kernel output changed on reopen");
            {std::lock_guard<std::mutex> lock(app.mutex);app.snapshotPath=app.probeDirectory+L"/hello-world.bmp";Dirty();waiting=app.revision;}
            app.probeResults.push_back({{"hello_world",true},{"run_all",true},{"shared_state",true},{"edit_during_execution",true},{"save_reopen",true}});
            ++phase;return;
        }
        if(phase==4) {frames=app.frames;cpu=CpuTicks();kernelCpu=app.kernel->CpuTicks();SetTimer(window,41,2000,nullptr);++phase;return;}
        app.probeResults.push_back({{"idle_frames",app.frames.load()-frames},{"idle_cpu_ms",(CpuTicks()-cpu)/10000.0},{"kernel_idle_cpu_ms",(app.kernel->CpuTicks()-kernelCpu)/10000.0}});
        frames=app.frames;cpu=CpuTicks();kernelCpu=app.kernel->CpuTicks();
        if(++phase<8)return;
        SendMessageW(window,WM_COMMAND,Menus::StopKernel,0);DestroyWindow(window);
    } catch(const std::exception& error) {
        app.probeResults.push_back({{"failure",error.what()}});DestroyWindow(window);
    }
}
void EditingProbe(HWND window) {
    static int phase=0;
    static uint64_t waitingFor=0,idleCpu=0,idleFrames=0;
    static auto started=Clock::now();
    auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    auto snapshot=[&](const wchar_t* name){std::lock_guard<std::mutex> lock(app.mutex);app.snapshotPath=app.probeDirectory+L"/"+name+L".bmp";Dirty();waitingFor=app.revision;};
    auto type=[&](const std::wstring& text){for(auto ch:text)SendMessageW(window,WM_CHAR,ch,0);};
    auto clickCell=[&](size_t index){
        int width=int(app.width/app.scale),left=NotebookViewport::SideMargin(width),gutter=width<650?88:128;
        int y=202+app.document->Cells()[index].top-app.scroll;
        // Prior caret reveal can scroll the target above the viewport. Move it
        // into view before sending a real hit-test coordinate to the window.
        if(y<190 || y>int(app.height/app.scale)-60) {
            std::lock_guard<std::mutex> lock(app.mutex);
            app.scroll=app.document->Cells()[index].top;Dirty();y=202;
        }
        SendMessageW(window,WM_LBUTTONDOWN,0,MAKELPARAM(int((left+gutter+20)*app.scale),int(y*app.scale)));
        SendMessageW(window,WM_LBUTTONUP,0,0);
    };
    try {
        require(Clock::now()-started<std::chrono::seconds(35) && !app.renderFailed,"Editing probe timed out or renderer failed");
        if(!app.document || app.editor->Document()!=app.document || !app.pythonDone || (waitingFor && app.presentedRevision<waitingFor))return;
        switch(phase) {
        case 0:
            clickCell(1);require(app.editor->Active() && app.editor->Selected()==1,"Click did not edit code cell");
            require(!Unsaved(),"Click dirtied the notebook");
            SendMessageW(window,WM_KILLFOCUS,0,0);
            require(app.editor->Active(),"Window deactivation cancelled cell editing");
            SendMessageW(window,WM_COMMAND,Menus::SelectAll,0);type(L"value = '日本語 😀'\rprint(value)");
            snapshot(L"code-edit");break;
        case 1:
            require(app.document->Cells()[1].text->source==L"value = '日本語 😀'\nprint(value)",("Code text input failed: "+Utf8(app.document->Cells()[1].text->source)).c_str());
            clickCell(0);require(app.editor->Active() && app.editor->Selected()==0,"Click did not edit Markdown");
            SendMessageW(window,WM_COMMAND,Menus::SelectAll,0);type(L"# Edited Markdown\r\rText with Unicode: Ελληνικά and **bold**, *italic*, `code`.\r\r- First item\r- Second item");
            snapshot(L"markdown-edit");break;
        case 2:
            SendMessageW(window,WM_COMMAND,Menus::CodeBelow,0);type(L"answer = 42");
            require(app.document->Cells().size()==9 && app.editor->Selected()==1,"Insert below lost selection");
            SendMessageW(window,WM_COMMAND,Menus::Undo,0);SendMessageW(window,WM_COMMAND,Menus::Redo,0);
            require(app.document->Cells()[1].text->source==L"answer = 42","Undo/redo lost cell source");
            SaveDocument(window);type(L"\rprint(answer)");
            snapshot(L"edit-during-save");break;
        case 3:
            if(app.savePending)return;
            require(Unsaved(),"Save completion incorrectly cleared newer edits");
            SaveDocument(window);waitingFor=0;break;
        case 4:
            if(app.savePending)return;
            require(!Unsaved(),"Saving final revision did not clear dirty state");
            require(Notebook::Load(app.loadedPath).Serialize()==app.document->Serialize(),"Save/reopen differs from edited document");
            app.probeResults.push_back({{"click_code",true},{"click_markdown",true},{"insert",true},{"undo_redo",true},{"save_snapshot",true},{"save_reopen",true}});
            {std::lock_guard<std::mutex> lock(app.mutex);app.scale=2;Dirty();}
            SetWindowPos(window,nullptr,0,0,1000,900,SWP_NOMOVE|SWP_NOZORDER);
            snapshot(L"editing-200dpi");break;
        case 5:
            SendMessageW(window,WM_SYSKEYDOWN,VK_RETURN,1LL<<29);snapshot(L"editing-fullscreen");break;
        case 6:
            SendMessageW(window,WM_SYSKEYDOWN,VK_RETURN,1LL<<29);
            SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,0);
            SendMessageW(window,WM_KEYDOWN,'D',0);require(app.document->Cells().size()==9,"Single D deleted a cell");
            SendMessageW(window,WM_KEYDOWN,'D',1LL<<30);require(app.document->Cells().size()==9,"Held D deleted a cell");
            SendMessageW(window,WM_KEYDOWN,'D',0);require(app.document->Cells().size()==8,"D,D did not delete selected cell");
            SendMessageW(window,WM_KEYDOWN,'Z',0);require(app.document->Cells().size()==9,"Z did not restore deleted cell");
            SendMessageW(window,WM_KEYDOWN,'B',0);require(app.document->Cells().size()==10 && !app.editor->Active(),"B did not insert in command mode");
            SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);SendMessageW(window,WM_CHAR,L'\r',0);
            require(app.editor->Active() && app.document->Cells()[app.editor->Selected()].text->source.empty(),"Enter inserted text when entering edit mode");
            type(L"temporary");SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,0);
            SendMessageW(window,WM_KEYDOWN,'D',0);SendMessageW(window,WM_KEYDOWN,'D',0);
            require(app.document->Cells().size()==9,"Temporary cell deletion failed");
            {std::lock_guard<std::mutex> lock(app.mutex);app.scroll=0;Dirty();}
            clickCell(0);require(app.editor->Active(),"Markdown could not re-enter editing");
            SendMessageW(window,WM_LBUTTONDOWN,0,MAKELPARAM(int(10*app.scale),int(160*app.scale)));
            require(!app.editor->Active() && !app.document->Cells()[0].sourceMode && app.document->Cells()[0].markdown,"Click-away kept Markdown in source mode");
            require(app.document->Cells()[0].markdown->Text().find(L"**")==std::wstring::npos,"Markdown preview contains formatting markers");
            app.probeResults.push_back({{"markdown_click_away",true},{"rich_markdown",true},{"delete_and_undo",true},{"modal_shortcuts",true}});
            SaveDocument(window);snapshot(L"rich-markdown-deselected");break;
        case 7:
            if(app.savePending)return;
            require(!Unsaved(),"Final command edits did not save");
            idleCpu=CpuTicks();idleFrames=app.frames;SetTimer(window,41,2000,nullptr);break;
        case 8:case 9:case 10:
            app.probeResults.push_back({{"idle_cpu_ms",(CpuTicks()-idleCpu)/10000.0},{"idle_frames",app.frames.load()-idleFrames}});
            idleCpu=CpuTicks();idleFrames=app.frames;
            if(phase==10)DestroyWindow(window);
            break;
        }
        ++phase;
    } catch(const std::exception& error) {app.probeResults.push_back({{"failure",error.what()}});DestroyWindow(window);}
}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    ProbeMessageTiming probeTiming(message);
    switch(message) {
    case WM_WINDOWPOSCHANGING: {
        auto position=reinterpret_cast<WINDOWPOS*>(lp);
        if(!(position->flags&SWP_NOSIZE))position->flags|=SWP_NOCOPYBITS;
        break;
    }
    case WM_NCCALCSIZE:
        if(wp) {
            WindowTiming timing;
            if(!app.fullscreen)DefWindowProcW(window,message,wp,lp);
            auto params=reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
            int width=params->rgrc[0].right-params->rgrc[0].left,height=params->rgrc[0].bottom-params->rgrc[0].top;
            if(app.renderThread.joinable() && width>0 && height>0 && !IsIconic(window)) {
                {std::lock_guard<std::mutex> lock(app.mutex);app.width=width;app.height=height;Dirty();}
                UpdateHorizontal(window);CompleteResizeFrame();
            }
            return WVR_REDRAW;
        }
        break;
    case WM_SYSKEYDOWN:
        if(wp==VK_RETURN && (lp&(1LL<<29))) {if(!(lp&(1LL<<30))){WindowTiming timing;ToggleFullscreen(window);}return 0;}
        if(wp=='F' || wp=='E' || wp=='V' || wp=='I' || wp=='C' || wp=='K' || wp=='W' || wp=='H') {
            std::lock_guard<std::mutex> lock(app.mutex);auto found=std::wstring(L"FEVICKWH").find((wchar_t)wp);app.menu=(int)found;app.menuItem=0;Dirty();return 0;
        }
        break;
    case WM_SYSCHAR: if(wp==VK_RETURN || std::wstring(L"fevickwh").find((wchar_t)wp)!=std::wstring::npos)return 0;break;
    case WM_CHAR: {
        InputTiming timing;std::lock_guard<std::mutex> lock(app.mutex);
        if(app.suppressedChar && wp==app.suppressedChar){app.suppressedChar=0;return 0;}
        app.suppressedChar=0;
        if(app.menu<0 && app.titleEdit.Active() && wp>=32 && wp!=127){app.titleEdit.Insert((wchar_t)wp);Dirty();}
        else if(app.menu<0 && app.editor->Active() && (wp>=32 || wp==L'\r' || wp==L'\t') && wp!=127) {
            wchar_t ch=static_cast<wchar_t>(wp);
            if(ch>=0xd800 && ch<=0xdbff) {app.pendingSurrogate=ch;return 0;}
            std::wstring value;
            if(ch>=0xdc00 && ch<=0xdfff) {if(!app.pendingSurrogate)return 0;value+=app.pendingSurrogate;}
            app.pendingSurrogate=0;value+=ch;
            EditAction([&]{app.editor->InsertText(value);});
        }
        return 0;
    }
    case WM_TIMER: if(wp==41 && !app.probeDirectory.empty()) {if(app.mediaProbe)MediaProbe(window);else if(app.openErrorProbe)OpenErrorProbe(window);else if(app.imageProbe)ImageProbe(window);else if(app.navigatorProbe)NavigatorProbe(window);else if(app.kernelProbe)KernelProbe(window);else if(app.editingProbe)EditingProbe(window);else if(app.resizeProbe)ResizeProbe(window);else if(app.interactionProbe)InteractionProbe(window);else Probe(window);} return 0;
    case WM_SIZE: {
        WindowTiming timing;
        {
        std::lock_guard<std::mutex> lock(app.mutex);
        int width=LOWORD(lp),height=HIWORD(lp);if(wp==SIZE_MINIMIZED)width=height=0;
        // Modern composition prepared this geometry before the native transaction.
        if(app.width==width && app.height==height && app.presentedRevision>=app.revision)return 0;
        app.width=width;app.height=height;
        int maxScroll=std::max(0,app.documentHeight.load()-int(app.height/app.scale)+232);
        app.scroll=std::clamp(app.scroll,0,maxScroll); Dirty();
        }
        UpdateHorizontal(window);CompleteResizeFrame();return 0;
    }
    case WM_HSCROLL: {InputTiming timing;ScrollHorizontal(window,LOWORD(wp));return 0;}
    case WM_MOUSEHWHEEL: {InputTiming timing;ScrollHorizontal(window,-1,GET_WHEEL_DELTA_WPARAM(wp)*66/WHEEL_DELTA);return 0;}
    case WM_DPICHANGED: {
        { std::lock_guard<std::mutex> lock(app.mutex); app.scale=HIWORD(wp)/96.0f; Dirty(); }
        UpdateWindowIcons(window,HIWORD(wp)/96.0f);
        auto rect=reinterpret_cast<RECT*>(lp);
        SetWindowPos(window,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE); return 0;
    }
    case WM_MOVE: {
        auto monitor=MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST);
        std::lock_guard<std::mutex> lock(app.mutex);
        if(monitor!=app.monitor){app.monitor=monitor;app.displayChanged=true;Dirty();}
        return 0;
    }
    case WM_DISPLAYCHANGE: case WM_EXITSIZEMOVE: case WM_SETTINGCHANGE: {
        std::lock_guard<std::mutex> lock(app.mutex); app.displayChanged=true; Dirty(); return 0;
    }
    case WM_MOUSEWHEEL: case WM_KEYDOWN: {
        if(message==WM_KEYDOWN) {
            InputTiming timing;std::lock_guard<std::mutex> lock(app.mutex);
            if(app.menu>=0) {
                auto items=Menus::Items(app.menu,app.document!=nullptr,(app.titleEdit.Active() || (app.editor && app.editor->Active())),app.fullscreen,app.pythonStatus,app.editor && app.editor->Active(),!app.forceSdr,app.settings->UseLastFolder());
                if(wp==VK_ESCAPE)app.menu=-1;
                else if(wp==VK_LEFT || wp==VK_RIGHT){app.menu=(std::min(7,app.menu)+(wp==VK_LEFT?7:1))%8;app.menuItem=0;}
                else if(wp==VK_UP || wp==VK_DOWN){int count=(int)items.size();for(int i=0;i<count;++i){app.menuItem=(app.menuItem+count+(wp==VK_UP?-1:1))%count;if(items[app.menuItem].enabled)break;}}
                else if(wp==VK_RETURN && !items.empty() && items[app.menuItem].enabled){PostMessageW(window,WM_COMMAND,items[app.menuItem].command,0);app.menu=-1;}
                Dirty();return 0;
            }
            if(GetKeyState(VK_CONTROL)<0 && GetKeyState(VK_SHIFT)<0 && (wp=='K' || wp=='I' || wp=='R' || wp==VK_OEM_PERIOD)) {PostMessageW(window,WM_COMMAND,wp=='K'?Menus::ConnectKernel:wp=='I'?Menus::InterruptKernel:wp=='R'?Menus::RestartKernel:Menus::StopKernel,0);return 0;}
            if(GetKeyState(VK_CONTROL)<0 && !app.titleEdit.Active()) {
                int command=wp=='S'?(GetKeyState(VK_SHIFT)<0?Menus::SaveAs:Menus::Save):wp=='O'?Menus::Open:
                    wp=='Z'?Menus::Undo:wp=='Y'?Menus::Redo:wp=='X'?Menus::Cut:wp=='C'?Menus::Copy:wp=='V'?Menus::Paste:0;
                if(command){PostMessageW(window,WM_COMMAND,command,0);return 0;}
            }
            if(wp==VK_F2){BeginTitleEdit();return 0;}
            if(app.titleEdit.Active()) {
                auto& edit=app.titleEdit;bool shift=GetKeyState(VK_SHIFT)<0;
                if(wp==VK_ESCAPE){edit.End();app.status=DocumentStatus();}
                else if(wp==VK_RETURN)ApplyTitleEdit();
                else if(wp=='A' && GetKeyState(VK_CONTROL)<0){edit.SelectAll();}
                else if(wp==VK_BACK)edit.Backspace();else if(wp==VK_DELETE)edit.Delete();
                else if(wp==VK_HOME)edit.Move(0,shift);else if(wp==VK_END)edit.Move(edit.Text().size(),shift);
                else if(wp==VK_LEFT)edit.Move(!shift && edit.Anchor()!=edit.Caret()?std::min(edit.Anchor(),edit.Caret()):edit.Previous(edit.Caret()),shift);
                else if(wp==VK_RIGHT)edit.Move(!shift && edit.Anchor()!=edit.Caret()?std::max(edit.Anchor(),edit.Caret()):edit.Next(edit.Caret()),shift);
                Dirty();return 0;
            }
            auto shortcut=app.shortcuts.Key(static_cast<unsigned>(wp),GetKeyState(VK_CONTROL)<0,GetKeyState(VK_SHIFT)<0,app.editor->Active(),(lp&(1LL<<30))!=0,GetTickCount64());
            if(shortcut!=NotebookShortcuts::Action::None) {
                if(wp==VK_RETURN)app.suppressedChar=L'\r';
                Shortcut(shortcut);return 0;
            }
            if(app.editor->Active()) {
                bool shift=GetKeyState(VK_SHIFT)<0,control=GetKeyState(VK_CONTROL)<0;
                if(wp==VK_ESCAPE){EditAction([&]{app.editor->End();},false);app.pendingSurrogate=0;return 0;}
                if(wp=='A' && control)EditAction([&]{app.editor->SelectAll();});
                else if(wp==VK_BACK || wp==VK_DELETE)EditAction([&]{app.editor->Erase(wp==VK_BACK);});
                else if(wp==VK_LEFT || wp==VK_RIGHT || wp==VK_HOME || wp==VK_END || wp==VK_UP || wp==VK_DOWN)
                    EditAction([&]{app.editor->Move((int)wp,shift,control);});
                if(wp!=VK_PRIOR && wp!=VK_NEXT)return 0;
            }
        }
        if(message==WM_KEYDOWN && wp=='O' && GetKeyState(VK_CONTROL)<0) { PostMessageW(window,WM_COMMAND,OpenId,0); return 0; }
        InputTiming timing;
        if(message==WM_MOUSEWHEEL && (GET_KEYSTATE_WPARAM(wp)&MK_SHIFT)) {ScrollHorizontal(window,-1,-GET_WHEEL_DELTA_WPARAM(wp)*66/WHEEL_DELTA);return 0;}
        if(message==WM_KEYDOWN && (wp==VK_LEFT || wp==VK_RIGHT)) {ScrollHorizontal(window,wp==VK_LEFT?SB_LINELEFT:SB_LINERIGHT);return 0;}
        std::lock_guard<std::mutex> lock(app.mutex);
        int delta=0, page=std::max(22,int(app.height/app.scale)-232);
        if(message==WM_MOUSEWHEEL) delta=-GET_WHEEL_DELTA_WPARAM(wp)*66/WHEEL_DELTA;
        else if(wp==VK_NEXT) delta=page; else if(wp==VK_PRIOR) delta=-page;
        else if(wp==VK_DOWN) delta=44; else if(wp==VK_UP) delta=-44;
        else if(wp==VK_HOME) delta=-app.documentHeight.load(); else if(wp==VK_END) delta=app.documentHeight.load();
        int next=std::clamp(app.scroll+delta,0,std::max(0,app.documentHeight.load()-page));
        if(next!=app.scroll) {app.scroll=next; Dirty();} return 0;
    }
    case WM_LBUTTONDBLCLK: {
        InputTiming timing;
        std::lock_guard<std::mutex> lock(app.mutex);
        const auto image=HitImage(int(GET_X_LPARAM(lp)/app.scale),int(GET_Y_LPARAM(lp)/app.scale));
        if(image) {
            if(!app.expandedImages.erase(image))app.expandedImages.insert(image);
            ++app.imageViewRevision;Dirty();return 0;
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        InputTiming timing;
        std::lock_guard<std::mutex> lock(app.mutex);
        int x=int(GET_X_LPARAM(lp)/app.scale), y=int(GET_Y_LPARAM(lp)/app.scale);
        int width=int(app.width/app.scale),height=int(app.height/app.scale);
        app.shortcuts.Reset();
        if(HitImage(x,y)) {
            // Select without entering source, preserving the double-click image target.
            const auto& cells=app.document->Cells();
            auto it=std::upper_bound(cells.begin(),cells.end(),y-190+app.scroll,[](int pos,const Cell& cell){return pos<cell.top;});
            const auto index=static_cast<size_t>(it-cells.begin()-1);
            EditAction([&]{app.editor->Select(index);},false);
            return 0;
        }
        if(app.document && app.menu<0) {
            auto geometry=NotebookNavigator::Geometry(width,height,app.document->Height(),app.scroll);
            if(NotebookNavigator::Hit(float(x),float(y),geometry)) {
                EditAction([&]{app.editor->End();},false);
                geometry=NotebookNavigator::Geometry(width,height,app.document->Height(),app.scroll);
                app.navigator.Begin(float(y),geometry,GetTickCount64());
                app.scroll=app.navigator.Drag(float(y),geometry);SetCapture(window);Dirty();return 0;
            }
        }
        if(app.editor->Active() && (y<177 || y>=height-34))EditAction([&]{app.editor->End();},false);
        if(y>=74 && y<106){app.menu=Menus::Hit(x,width);app.menuItem=0;Dirty();return 0;}
        if(app.menu>=0) {
            auto items=Menus::Items(app.menu,app.document!=nullptr,(app.titleEdit.Active() || (app.editor && app.editor->Active())),app.fullscreen,app.pythonStatus,app.editor && app.editor->Active(),!app.forceSdr,app.settings->UseLastFolder());
            int left=Menus::PopupLeft(app.menu,width),row=Menus::RowHeight((int)items.size(),height),index=(y-110)/row;
            if(x>=left && x<left+314 && y>=110 && index<(int)items.size()) {
                if(items[index].enabled){PostMessageW(window,WM_COMMAND,items[index].command,0);app.menu=-1;}
            } else app.menu=-1;
            Dirty();return 0;
        }
        if(x>=150 && x<int(app.width/app.scale)-20-(app.width/app.scale>=850?200:0) && y>=16 && y<=62){BeginTitleEdit();return 0;}
        app.titleEdit.End();Dirty();
        if(y>=114 && y<=146) {
            const auto kind=app.document && !app.document->Cells().empty()?app.document->Cells()[app.editor->Selected()].text->kind:L"";
            int command=NotebookToolbar::Hit(x,width,kind);
            if(command)PostMessageW(window,WM_COMMAND,command,0);
        }
        else if(y>=177 && y<int(app.height/app.scale)-34) {
            app.clickY=y;
            if(app.document) {
                int contentY=y-190+app.scroll,left=NotebookViewport::SideMargin(width),gutter=width<650?88:128;
                auto& cells=app.document->Cells();
                auto it=std::upper_bound(cells.begin(),cells.end(),contentY,[](int pos,const Cell& cell){return pos<cell.top;});
                if(it!=cells.begin()) {
                    --it;int page=HorizontalPage(),maximum=std::max(0,it->contentWidth-page),start=left+gutter+12;
                    if(maximum && contentY>=it->top+it->height-12 && contentY<=it->top+it->height+3 && x>=start && x<=start+page)
                        {
                        app.scrollbar.Begin(it->id,x-start,page,it->contentWidth,std::min(app.horizontal,maximum));
                        app.horizontal=app.scrollbar.Drag(x-start,page,it->contentWidth);SetCapture(window);
                    }
                    else {
                        size_t index=static_cast<size_t>(it-cells.begin());
                        if(x>=left+gutter && contentY>=it->top && contentY<it->top+it->sourceHeight+24) {
                            app.pendingSurrogate=0;
                            EditAction([&]{app.editor->Begin(index,float(x-start+std::min(app.horizontal,maximum)),float(contentY-it->top-10),GetKeyState(VK_SHIFT)<0);});
                            SetCapture(window);
                        } else EditAction([&]{app.editor->Select(index);},false);
                    }
                }
            }
            Dirty();
        } return 0;
    }
    case WM_LBUTTONUP: {
        {std::lock_guard<std::mutex> lock(app.mutex);app.scrollbar.End();
            if(app.navigator.Held()){app.navigator.End(GetTickCount64());Dirty();}}
        if(GetCapture()==window)ReleaseCapture();return 0;
    }
    case WM_CAPTURECHANGED: {
        std::lock_guard<std::mutex> lock(app.mutex);app.scrollbar.End();
        if(app.navigator.Held()){app.navigator.End(GetTickCount64());Dirty();}return 0;
    }
    case WM_KILLFOCUS: {
        InputTiming timing;std::lock_guard<std::mutex> lock(app.mutex);app.shortcuts.Reset();
        if(app.menu>=0){app.menu=-1;Dirty();}
        // Deactivating the window is not deselecting its cell (IME popups and
        // switching applications must not cancel an in-progress edit).
        return 0;
    }
    case WM_MOUSELEAVE: {
        std::lock_guard<std::mutex> lock(app.mutex);
        app.mouseTracking=false;
        auto geometry=NotebookNavigator::Geometry(int(app.width/app.scale),int(app.height/app.scale),app.documentHeight,app.scroll);
        if(app.navigator.Hover(-1,-1,geometry))Dirty();return 0;
    }
    case WM_MOUSEMOVE: {
        {
            std::lock_guard<std::mutex> lock(app.mutex);
            auto geometry=NotebookNavigator::Geometry(int(app.width/app.scale),int(app.height/app.scale),app.documentHeight,app.scroll);
            if(app.navigator.Hover(GET_X_LPARAM(lp)/app.scale,GET_Y_LPARAM(lp)/app.scale,geometry))Dirty();
            if(!app.mouseTracking) {
                TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window,0};
                app.mouseTracking=TrackMouseEvent(&track)!=FALSE;
            }
        }
        if(GetCapture()!=window || !(wp&MK_LBUTTON))break;
        InputTiming timing;std::lock_guard<std::mutex> lock(app.mutex);
        if(app.navigator.Held() && app.document) {
            auto geometry=NotebookNavigator::Geometry(int(app.width/app.scale),int(app.height/app.scale),app.document->Height(),app.scroll);
            int next=app.navigator.Drag(GET_Y_LPARAM(lp)/app.scale,geometry);
            if(next!=app.scroll){app.scroll=next;Dirty();}return 0;
        }
        if(app.scrollbar.CellId()) {
            int logicalWidth=int(app.width/app.scale),left=NotebookViewport::SideMargin(logicalWidth),gutter=logicalWidth<650?88:128;
            if(app.document)for(const auto& cell:app.document->Cells())if(cell.id==app.scrollbar.CellId()) {
                int next=app.scrollbar.Drag(int(GET_X_LPARAM(lp)/app.scale)-left-gutter-12,HorizontalPage(),cell.contentWidth);
                if(next!=app.horizontal){app.horizontal=next;Dirty();}return 0;
            }
            app.scrollbar.End();return 0;
        }
        if(!app.editor->Active())return 0;
        int width=int(app.width/app.scale),left=NotebookViewport::SideMargin(width),gutter=width<650?88:128;
        const auto& cell=app.document->Cells()[app.editor->Selected()];
        int maximum=std::max(0,cell.contentWidth-HorizontalPage());
        float x=GET_X_LPARAM(lp)/app.scale-left-gutter-12+std::min(app.horizontal,maximum);
        float y=GET_Y_LPARAM(lp)/app.scale-200-cell.top+app.scroll;
        EditAction([&]{app.editor->Begin(app.editor->Selected(),x,y,true);});return 0;
    }
    case WM_CLOSE:
        if(ConfirmDiscard(window))DestroyWindow(window);return 0;
    case KernelUpdate: {
        std::deque<KernelEvent> events;
        {std::lock_guard<std::mutex> lock(app.mutex);events.swap(app.kernelEvents);app.kernelWake.notify_one();}
        std::lock_guard<std::mutex> lock(app.mutex);
        for(auto& event:events) {
            if(event.generation!=app.kernel->Generation())continue;
            app.kernelStatus=event.status;
            if(event.cellId)EditAction([&]{app.editor->ApplyExecution(event.cellId,event.result);},false);
        }
        app.pythonStatus=L"Python "+app.python.version+L" (64-bit) | "+app.kernelStatus;
        app.status=DocumentStatus();Dirty();return 0;
    }
    case WM_COMMAND:
        if(LOWORD(wp)==Menus::SelectInterpreter) {
            if(!app.pythonDone)return 0;
            wchar_t path[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};
            dialog.hwndOwner=window;dialog.lpstrFile=path;dialog.nMaxFile=32768;
            dialog.lpstrFilter=L"Python executable\0*.exe\0";
            dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
            if(!GetOpenFileNameW(&dialog))return 0;
            if(app.pythonThread.joinable())app.pythonThread.join();
            {std::lock_guard<std::mutex> lock(app.mutex);
                app.kernel->Stop();app.kernelStatus=L"Kernel disconnected";
                app.pythonDone=false;app.pythonOverride=path;app.pythonStatus=L"Checking interpreter…";Dirty();
            }
            app.pythonThread=std::thread(PythonLoop);return 0;
        }
        if(LOWORD(wp)==Menus::ChooseOpenFolder) {
            HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
            IFileOpenDialog* dialog=nullptr;
            if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) {
                dialog->SetOptions(FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
                if(SUCCEEDED(dialog->Show(window))) {
                    IShellItem* item=nullptr;
                    if(SUCCEEDED(dialog->GetResult(&item))) {
                        PWSTR path=nullptr;
                        if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))) {
                            std::lock_guard<std::mutex> lock(app.mutex);
                            app.settings->SetFixedFolder(path);app.settings->SetUseLastFolder(false);Dirty();CoTaskMemFree(path);
                        }
                        item->Release();
                    }
                }
                dialog->Release();
            }
            if(SUCCEEDED(initialized))CoUninitialize();return 0;
        }
        if(LOWORD(wp)>=Menus::InterruptKernel && LOWORD(wp)<=Menus::ToggleOpenFolder) {
            InputTiming timing;std::lock_guard<std::mutex> lock(app.mutex);
            switch(LOWORD(wp)) {
            case Menus::InterruptKernel:
                if(app.kernel->Interrupt())app.kernelStatus=L"Kernel interrupting…";
                break;
            case Menus::RestartKernel:
                if(app.kernel->Restart())app.kernelStatus=L"Kernel restarting…";else RunKernel(false,true);
                break;
            case Menus::Settings:app.menu=11;app.menuItem=0;break;
            case Menus::ToggleHdr:app.forceSdr=!app.forceSdr;app.settings->SetHdrEnabled(!app.forceSdr);app.displayChanged=true;break;
            case Menus::ToggleOpenFolder:app.settings->SetUseLastFolder(!app.settings->UseLastFolder());break;
            }
            Dirty();return 0;
        }
        if(LOWORD(wp)>=Menus::RunCell && LOWORD(wp)<=Menus::StopKernel) {
            InputTiming timing;std::lock_guard<std::mutex> lock(app.mutex);
            switch(LOWORD(wp)) {
            case Menus::RunCell:Shortcut(NotebookShortcuts::Action::RenderStay);break;
            case Menus::RunAll:RunKernel(true);break;
            case Menus::ConnectKernel:RunKernel(false,true);break;
            case Menus::StopKernel:app.kernel->Stop();app.kernelStatus=L"Kernel stopped";app.pythonStatus=L"Python "+app.python.version+L" | "+app.kernelStatus;app.status=DocumentStatus();Dirty();break;
            case Menus::CellType:app.menu=10;app.menuItem=0;Dirty();break;
            case Menus::ToCode:case Menus::ToMarkdown:EditAction([&]{app.editor->ChangeKind(LOWORD(wp)==Menus::ToCode?L"code":L"markdown");});break;
            }
            return 0;
        }
        if(LOWORD(wp)==Menus::DeleteCell) {InputTiming timing;std::lock_guard<std::mutex> lock(app.mutex);EditAction([&]{app.editor->DeleteCell();});return 0;}
        if(LOWORD(wp)==Menus::Save || LOWORD(wp)==Menus::SaveAs){SaveDocument(window,LOWORD(wp)==Menus::SaveAs);return 0;}
        if(LOWORD(wp)>=Menus::Undo && LOWORD(wp)<=Menus::MarkdownBelow) {
            InputTiming timing;std::lock_guard<std::mutex> lock(app.mutex);
            int command=LOWORD(wp);
            if(command==Menus::Undo)EditAction([&]{app.editor->Undo();});
            else if(command==Menus::Redo)EditAction([&]{app.editor->Redo();});
            else if(command>=Menus::Cut && command<=Menus::Paste)ClipboardCommand(window,command);
            else InsertCell(command==Menus::CodeAbove || command==Menus::MarkdownAbove,command==Menus::MarkdownAbove || command==Menus::MarkdownBelow);
            return 0;
        }
        if(LOWORD(wp)==Menus::Rename){std::lock_guard<std::mutex> lock(app.mutex);BeginTitleEdit();}
        else if(LOWORD(wp)==Menus::Fullscreen){WindowTiming timing;ToggleFullscreen(window);}
        else if(LOWORD(wp)==Menus::SelectAll){std::lock_guard<std::mutex> lock(app.mutex);if(app.titleEdit.Active()){app.titleEdit.SelectAll();Dirty();}else EditAction([&]{app.editor->SelectAll();});}
        else if(LOWORD(wp)==Menus::Shortcuts || LOWORD(wp)==Menus::About){std::lock_guard<std::mutex> lock(app.mutex);app.menu=LOWORD(wp)==Menus::Shortcuts?8:9;app.menuItem=0;Dirty();}
        else if(LOWORD(wp)==Menus::Close)PostMessageW(window,WM_CLOSE,0,0);
        else if(LOWORD(wp)==OpenId) OpenDialog(window);
        else if(LOWORD(wp)==IDM_EXIT) PostMessageW(window,WM_CLOSE,0,0);
        else if(LOWORD(wp)==IDM_ABOUT) MessageBoxW(window,L"Winip — native x64 notebook reader checkpoint\nGPU drawing; title rename and Python discovery. Cell execution is not implemented.",L"About Winip",MB_OK);
        return 0;
    case PreviewReady: {
        InputTiming timing;std::lock_guard<std::mutex> lock(app.mutex);
        if(app.previewReady && app.editor && app.editor->Document()==app.document) {
            app.editor->ApplyPreview(*app.previewReady);PublishEdit(false);
        }
        app.previewReady.reset();return 0;
    }
    case OperationFailed: {
        std::wstring messageText, title;
        {std::lock_guard<std::mutex> lock(app.mutex);
            if(app.operationErrorRequest!=app.request || app.operationError.empty())return 0;
            messageText=std::move(app.operationError);title=app.operationErrorTitle;
        }
        MessageBoxW(window,messageText.c_str(),title.c_str(),MB_OK|MB_ICONERROR);
        return 0;
    }
    case Loaded: {
        std::wstring title;
        {std::lock_guard<std::mutex> lock(app.mutex);
            if(app.editor->Document()!=app.document) {
                if(app.document && app.editor->Document() && app.document->ContentRevision()==app.editor->Document()->ContentRevision()) {
                    app.editor->Retitle(app.document->Title());app.document=app.editor->Document();
                } else app.editor->Attach(app.document);
            }
            title=(app.document?app.document->Title():L"Untitled")+(Unsaved()?L" *":L"");
        }
        SetWindowTextW(window,(title+(app.probeDirectory.empty()?L" — Winip":L" — Winip AUTOMATED CHECK (closes itself)")).c_str()); UpdateHorizontal(window);return 0;
    }
    case Failed: InvalidateRect(window,nullptr,FALSE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        InputTiming timing;
        PAINTSTRUCT paint; auto dc=BeginPaint(window,&paint);
        {
            std::lock_guard<std::mutex> lock(app.mutex);
            if(app.renderFailed) {
                FillRect(dc,&paint.rcPaint,(HBRUSH)(COLOR_WINDOW+1));
                RECT r{24,24,900,250}; DrawTextW(dc,app.error.c_str(),-1,&r,DT_WORDBREAK|DT_NOPREFIX);
            } else if(!CompositionAvailable() || app.presentedRevision<app.revision) Dirty();
        }
        EndPaint(window,&paint); return 0;
    }
    case WM_GETMINMAXINFO: {
        auto info=reinterpret_cast<MINMAXINFO*>(lp); info->ptMinTrackSize={LONG(420*app.scale),LONG(320*app.scale)}; return 0;
    }
    case WM_DESTROY: {
        {std::lock_guard<std::mutex> lock(app.mutex); app.stop=true;}
        app.cancelPython=true;app.renderWake.notify_one(); app.loadWake.notify_one();app.kernelWake.notify_all(); PostQuitMessage(0); return 0;
    }
    }
    return DefWindowProcW(window,message,wp,lp);
}
}
int APIENTRY wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int) {
    static_assert(sizeof(void*)==8,"Winip is 64-bit only");
    try {
        AppPaths paths;app.settings=std::make_unique<AppSettings>(paths.Root());
        app.forceSdr=!app.settings->HdrEnabled();app.kernelRuntime=(paths.Root()/L"runtime").wstring();
        wchar_t module[32768];DWORD length=GetModuleFileNameW(nullptr,module,32768);
        if(!length || length==32768)throw std::runtime_error("Cannot locate application runtime");
        app.kernelBridge=(std::filesystem::path(module).parent_path()/L"runtime"/L"kernel_bridge.py").wstring();
    }
    catch (const std::exception& error) {
        MessageBoxA(nullptr,error.what(),"Cannot initialize Winip AppData",MB_OK|MB_ICONERROR);
        return 1;
    }
    auto user=GetModuleHandleW(L"user32.dll");
    using DpiContext=BOOL(WINAPI*)(HANDLE);
    auto dpi=reinterpret_cast<DpiContext>(GetProcAddress(user,"SetProcessDpiAwarenessContext"));
    if(!dpi || !dpi(reinterpret_cast<HANDLE>(-4))) SetProcessDPIAware();
    try { app.editor=std::make_unique<CellEditor>(); } catch(const std::exception& e) { MessageBoxA(nullptr,e.what(),"Cannot start cell editor",MB_OK|MB_ICONERROR);return 1; }
    app.frameReady=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!app.frameReady)return 1;
    WNDCLASSEXW cls{sizeof(cls)};cls.style=CS_HREDRAW|CS_VREDRAW; cls.lpfnWndProc=WindowProc; cls.hInstance=instance;
    cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_WINIP)); cls.hCursor=LoadCursor(nullptr,IDC_ARROW);
    cls.style|=CS_DBLCLKS;
    cls.lpszClassName=L"WinipNotebook";
    if(!RegisterClassExW(&cls)) return 1;
    app.window=CreateWindowExW(CompositionAvailable()?0x00200000L:0,cls.lpszClassName,L"Untitled — Winip",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,1250,900,nullptr,nullptr,instance,nullptr);
    if(!app.window) return 1;
    using GetDpi=UINT(WINAPI*)(HWND);
    auto getDpi=reinterpret_cast<GetDpi>(GetProcAddress(user,"GetDpiForWindow"));
    if(getDpi) app.scale=getDpi(app.window)/96.0f;
    else {auto dc=GetDC(app.window); app.scale=GetDeviceCaps(dc,LOGPIXELSX)/96.0f; ReleaseDC(app.window,dc);}
    UpdateWindowIcons(app.window,app.scale);
    bool renderOverride=false;
    int argc=0; auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    for(int i=1;i<argc;++i) {
        if(std::wstring(argv[i])==L"--metrics" && i+1<argc) app.metricsPath=argv[++i];
        else if(std::wstring(argv[i])==L"--snapshot" && i+1<argc) app.snapshotPath=argv[++i];
        else if(std::wstring(argv[i])==L"--python" && i+1<argc) app.pythonOverride=argv[++i];
        else if(std::wstring(argv[i])==L"--no-snapshots") app.noSnapshots=true;
        else if(std::wstring(argv[i])==L"--sdr") {app.forceSdr=true;renderOverride=true;}
        else if(std::wstring(argv[i])==L"--hdr") {app.forceSdr=false;renderOverride=true;}
        else if((std::wstring(argv[i])==L"--media-probe" || std::wstring(argv[i])==L"--open-error-probe" || std::wstring(argv[i])==L"--image-probe" || std::wstring(argv[i])==L"--navigator-probe" || std::wstring(argv[i])==L"--kernel-probe" || std::wstring(argv[i])==L"--editing-probe" || std::wstring(argv[i])==L"--probe" || std::wstring(argv[i])==L"--interaction-probe" || std::wstring(argv[i])==L"--resize-probe") && i+1<argc) {
            app.mediaProbe=std::wstring(argv[i])==L"--media-probe";
            app.openErrorProbe=std::wstring(argv[i])==L"--open-error-probe";
            app.imageProbe=std::wstring(argv[i])==L"--image-probe";
            app.navigatorProbe=std::wstring(argv[i])==L"--navigator-probe";
            app.kernelProbe=std::wstring(argv[i])==L"--kernel-probe";
            app.editingProbe=std::wstring(argv[i])==L"--editing-probe";
            app.interactionProbe=std::wstring(argv[i])==L"--interaction-probe";
            app.resizeProbe=std::wstring(argv[i])==L"--resize-probe";
            app.probeDirectory=argv[++i];app.metricsPath=app.probeDirectory+L"/metrics.json";
        }
        else RequestOpen(argv[i]);
    }
    LocalFree(argv);
    if(!app.probeDirectory.empty()) {
        app.settings=std::make_unique<AppSettings>(app.probeDirectory);
        if(!renderOverride)app.forceSdr=!app.settings->HdrEnabled();
    }
    if(app.kernelProbe || app.imageProbe)app.kernelRuntime=app.probeDirectory+L"/runtime";
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(app.window,MONITOR_DEFAULTTONEAREST),&monitor);
    int workWidth=monitor.rcWork.right-monitor.rcWork.left,workHeight=monitor.rcWork.bottom-monitor.rcWork.top;
    int restoredWidth=std::min(int(1600*app.scale),workWidth*9/10),restoredHeight=std::min(int(1000*app.scale),workHeight*9/10);
    SetWindowPos(app.window,nullptr,monitor.rcWork.left+(workWidth-restoredWidth)/2,monitor.rcWork.top+(workHeight-restoredHeight)/2,
        restoredWidth,restoredHeight,SWP_NOZORDER|SWP_NOACTIVATE);
    // Diagnostic layout probes deliberately use a restored window.
    ShowWindow(app.window,app.probeDirectory.empty() || app.interactionProbe?SW_SHOWMAXIMIZED:SW_SHOWNOACTIVATE);
    app.kernel=std::make_unique<KernelController>([](KernelEvent event){
        std::unique_lock<std::mutex> lock(app.mutex);
        app.kernelWake.wait(lock,[]{return app.stop || app.kernelEvents.size()<8;});
        if(app.stop)return;
        app.kernelEvents.push_back(std::move(event));PostMessageW(app.window,KernelUpdate,0,0);
    });
    app.renderThread=std::thread(RenderLoop); app.loadThread=std::thread(LoadLoop);app.pythonThread=std::thread(PythonLoop);
    if(!app.probeDirectory.empty()) SetTimer(app.window,41,100,nullptr);
    MSG message{}; int result;
    while((result=GetMessageW(&message,nullptr,0,0))>0) {TranslateMessage(&message); DispatchMessageW(&message);}
    {std::lock_guard<std::mutex> lock(app.mutex); app.stop=true;}
    app.renderWake.notify_one(); app.loadWake.notify_one();app.kernelWake.notify_all();
    app.kernel.reset();
    app.cancelPython=true;app.renderThread.join(); app.loadThread.join();app.pythonThread.join();
    CloseHandle(app.frameReady);
    if(app.bigIcon)DestroyIcon(app.bigIcon);
    if(app.smallIcon)DestroyIcon(app.smallIcon);
    if(!app.metricsPath.empty()) {
        std::ofstream stream(std::filesystem::path(app.metricsPath),std::ios::binary);
        auto messageMaxima=nlohmann::json::array();
        for(const auto& [messageId,count]:app.probeMessages)messageMaxima.push_back({{"message",messageId},{"calls",count.calls},{"max_ms",count.maxNs/1e6}});
        stream << nlohmann::json{{"final_status",Utf8(app.status)},{"final_title_input",Utf8(app.titleEdit.Text())},{"text_cache",app.textCache},{"render_stages",app.renderStages},{"refresh_hz",app.refreshHz.load()},{"frame_budget_ms",app.refreshHz>1?1000.0/app.refreshHz.load():0},{"frame_statistics",app.frameStatistics},{"message_maxima",messageMaxima},{"presented_frames",app.frames.load()},{"ui_samples",app.uiCount.load()},
            {"window_transition_max_ms",app.windowMaxNs.load()/1e6},{"window_transitions_over_6ms",app.windowOverBudget.load()},{"ui_over_6ms",app.uiOverBudget.load()},{"ui_max_ms",app.uiMaxNs.load()/1e6},
            {"render_prepare_max_ms",app.renderMaxNs.load()/1e6},{"renderer_failed",app.renderFailed.load()},
            {"renderer","Direct2D / hardware D3D11"},{"adapter",Utf8(app.adapter)},
            {"python_executable",Utf8(app.python.executable)},{"python_origin",Utf8(app.python.origin)},{"python_version",Utf8(app.python.version)},{"python_bits",app.python.bits},{"python_error",Utf8(app.python.error)},
            {"hdr",app.hdr.load()},{"sdr_white_scale",app.whiteScale.load()},{"probe",app.probeResults}}.dump(2);
    }
    return result==-1 || app.renderFailed ? 1 : 0;
}
