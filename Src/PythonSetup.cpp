#include "PythonSetup.h"
#include "PythonSetupPlan.h"
#include "SetupWindowStyle.h"
#include "ChildProcess.h"
#include "TextEncoding.h"
#include "../ThirdParty/nlohmann/json.hpp"
#include "Resource.h"
#include "AppPaths.h"
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace {
constexpr int Details = 1000, Install = 1001, Official = 1002, Check = 1003, Select = 1004,
    Consent = 1005, UserScope = 1006, Back = 1007, EnablePip = 1008, Heading = 1010, Description = 1011;
enum class Page { Introduction, Review };
constexpr UINT ActivateSetup = WM_APP + 47;
PythonIdentity DiscoverInstalledPython(const std::atomic<bool>& cancel) {
    auto selection = ReadPythonEnvironment(L"");
    selection.terminal = false;
    selection.virtualEnv.clear();
    selection.condaPrefix.clear();
    return DiscoverPython(selection, cancel);
}
struct SetupDialog {
    PythonIdentity identity;
    PythonEnvironment environment;
    std::wstring runtimeDirectory, failure, log;
    std::thread worker;
    std::atomic<bool> cancel{false}, done{false};
    bool busy = false, installation = false, closeWhenDone = false;
    Page page = Page::Introduction;
    SetupWindowStyle* style = nullptr;
    HWND parent = nullptr;
    ~SetupDialog() { cancel = true; if (worker.joinable()) worker.join(); }

    void Refresh(HWND window) {
        const auto stage = PythonSetupPlan::Stage(identity);
        const bool canInstall = PythonSetupPlan::CanInstall(stage);
        const bool review = page == Page::Review;
        const bool ready = stage == PythonSetupStage::Ready;
        auto show = [&](int id, bool visible) { ShowWindow(GetDlgItem(window,id),visible?SW_SHOW:SW_HIDE); };
        SetDlgItemTextW(window,Heading,busy?L"Setting up your notebook environment":review?L"Review and confirm installation":
            !failure.empty()?L"Setup needs attention":PythonSetupPlan::Title(stage));
        SetDlgItemTextW(window,Description,busy?L"Please wait. You can cancel the operation; changes already made may remain.":
            review?L"Check the selected Python environment and destination below. Click Install to confirm this step.":
            !failure.empty() && ready?L"The packages are available, but the last kernel operation failed. Review the error below before trying again.":PythonSetupPlan::Description(stage));
        SetDlgItemTextW(window,Consent,PythonSetupPlan::Choice(stage));
        show(Consent,!busy && !review && canInstall);
        std::wstring text;
        if (stage == PythonSetupStage::Python) {
            text=L"1. Install the latest compatible stable Python with pip for your Windows account.\r\n"
                L"2. Install Jupyter kernel support using pip.\r\n3. Check that your environment is ready.\r\n\r\n"
                L"Existing Python installations, PATH and file associations will be preserved.";
        } else {
            text=L"Python " + identity.version + L"\r\n" + identity.executable;
            if (!identity.error.empty()) text+=L"\r\n\r\n"+identity.error;
            if (stage == PythonSetupStage::Pip) text+=L"\r\n\r\nThis step enables pip. Jupyter kernel support is installed on the next page.";
            else if (stage == PythonSetupStage::Kernel) text+=L"\r\n\r\nJupyter kernel support lets Winip run your notebook cells.";
            if (review) {
                text+=L"\r\n\r\nPackages: ";
                if(stage==PythonSetupStage::Pip)text+=L"pip (from Python's bundled ensurepip)";
                else for(const auto& package:identity.missingPackages)text+=package+L" ";
                text+=L"\r\nDestination: "+(IsDlgButtonChecked(window,UserScope)==BST_CHECKED?identity.installDirectory:identity.systemInstallDirectory);
            }
            if(!identity.packageError.empty())text+=L"\r\n\r\n"+identity.packageError;
            if(stage==PythonSetupStage::Repair && identity.error.empty() && identity.packageError.empty() && !identity.pipAvailable)
                text+=L"\r\n\r\nThis Python has neither usable pip nor ensurepip. Use its distributor's installer to enable pip, or select another Python environment.";
        }
        if(!failure.empty())text+=L"\r\n\r\n"+failure;
        if(!log.empty())text+=L"\r\n\r\n"+log;
        std::wstring normalized;
        for(wchar_t ch:text) {if(ch==L'\r')continue;if(ch==L'\n')normalized+=L'\r';normalized+=ch;}
        SetDlgItemTextW(window,Details,normalized.c_str());
        show(UserScope,!busy && review && stage!=PythonSetupStage::Python && !identity.virtualEnvironment && identity.userSiteEnabled);
        show(Back,!busy && review);
        show(Install,!busy && canInstall);
        SetDlgItemTextW(window,Install,review?L"Install":L"Next");
        EnableWindow(GetDlgItem(window,Install),review || IsDlgButtonChecked(window,Consent)==BST_CHECKED);
        show(IDOK,!busy && ready);
        SetDlgItemTextW(window,IDOK,installation?L"Finish setup":L"Start kernel");
        // The app dialog is dismissed with X/Escape. Only the installer needs
        // a Skip action; cancellation remains visible during active work.
        show(IDCANCEL,busy || installation);
        SetDlgItemTextW(window,IDCANCEL,busy?L"Cancel operation":L"Skip setup");
        for(int id:{Official,Check,Select}) {show(id,!busy && !review && !ready);EnableWindow(GetDlgItem(window,id),!busy);}
        SendMessageW(window,DM_SETDEFID,ready?IDOK:canInstall?Install:Check,0);
        InvalidateRect(window,nullptr,TRUE);
    }

    void Start(HWND window, int operation, bool userScope) {
        if (worker.joinable()) worker.join();
        cancel = false; done = false; busy = true;
        if (operation != Check) failure.clear();
        log.clear();
        // The UI reads identity/log only after the worker publishes done.
        Refresh(window);
        SetTimer(window,1,200,nullptr);
        worker = std::thread([this, operation, userScope] {
            try {
                log.clear();
                auto run = [&](const std::wstring& executable, std::vector<std::wstring> arguments) {
                    auto result = ChildProcess::Run(executable, arguments, cancel, 15 * 60 * 1000,
                        std::filesystem::path(executable).parent_path().wstring());
                    log += Utf16(result.output);
                    if (!result.error.empty()) throw std::runtime_error(Utf8(result.error));
                    if (result.exitCode) throw std::runtime_error("Setup command failed with exit code " + std::to_string(result.exitCode));
                };
                if (operation == Install && identity.executable.empty()) {
                    const auto existing = DiscoverInstalledPython(cancel);
                    if (!existing.executable.empty()) {
                        identity = existing;
                        throw std::runtime_error("An existing Python installation was found. No new Python was installed. Review this environment or select another interpreter.");
                    }
                    AppPaths paths;
                    auto work = std::filesystem::path(paths.Directory(L"setup")) /
                        (L"download-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
                    std::filesystem::create_directory(work);
                    struct WorkCleanup {
                        std::filesystem::path path;
                        std::wstring& log;
                        ~WorkCleanup() {
                            std::error_code error;
                            std::filesystem::remove(path / L"python-installer.exe", error);
                            if (error) log += L"\nCould not remove temporary download: " + path.wstring();
                            std::filesystem::remove(path, error);
                            if (error) log += L"\nTemporary setup directory remains: " + path.wstring();
                        }
                    } cleanup{work, log};
                    wchar_t system[32768]; GetSystemDirectoryW(system, 32768);
                    const auto powershell = (std::filesystem::path(system) / L"WindowsPowerShell/v1.0/powershell.exe").wstring();
                    auto script = (std::filesystem::path(runtimeDirectory) / L"install_python.ps1").wstring();
                    ChildProcessResult installed;
                    std::atomic<bool> installerDone{false};
                    std::thread installer([&] {
                        installed = ChildProcess::Run(powershell, {L"-NoProfile", L"-NonInteractive", L"-ExecutionPolicy", L"Bypass",
                            L"-File", script, L"-WorkDirectory", work.wstring()}, cancel, 15 * 60 * 1000);
                        installerDone = true;
                    });
                    struct JoinInstaller {
                        std::thread& worker;
                        std::atomic<bool>& cancel;
                        ~JoinInstaller() { if (worker.joinable()) { cancel = true; worker.join(); } }
                    } joinInstaller{installer, cancel};
                    // Detection is bounded to setup, never a permanent idle poll.
                    while (!installerDone) {
                        for (int tick = 0; tick < 10 && !installerDone; ++tick) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        if (!installerDone && !cancel) identity = DiscoverInstalledPython(cancel);
                    }
                    installer.join();
                    log += Utf16(installed.output);
                    if (!installed.error.empty()) throw std::runtime_error(Utf8(installed.error));
                    if (installed.exitCode) throw std::runtime_error("Python installer failed. Use the official website to continue.");
                    for (int attempt = 0; attempt < 15 && !cancel; ++attempt) {
                        identity = DiscoverInstalledPython(cancel);
                        if (identity.error.empty() && !identity.executable.empty()) break;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                    if (!identity.error.empty() || identity.executable.empty())
                        throw std::runtime_error("The installer finished, but no usable Python was detected. Temporary-download cleanup was attempted. Use the official website or select an interpreter.");
                } else if (operation == EnablePip) {
                    if (!identity.pipAvailable) {
                        if (!identity.ensurePipAvailable) throw std::runtime_error("This Python has no ensurepip. Use its distributor's installer or select another interpreter.");
                        std::vector<std::wstring> arguments{L"-X", L"utf8", L"-B", L"-m", L"ensurepip"};
                        if (userScope) arguments.push_back(L"--user");
                        run(identity.executable, arguments);
                    }
                    CheckPythonPackages(identity,cancel);
                    if(!identity.pipAvailable)throw std::runtime_error("pip could not be enabled. Review the error or use the official instructions.");
                } else if (operation == Install) {
                    if(!identity.pipAvailable)throw std::runtime_error("pip is not available. Check again to return to the pip setup step.");
                    std::vector<std::wstring> arguments{L"-X", L"utf8", L"-B", L"-m", L"pip", L"--disable-pip-version-check", L"--no-input", L"install"};
                    if (userScope) arguments.push_back(L"--user");
                    arguments.insert(arguments.end(), identity.missingPackages.begin(), identity.missingPackages.end());
                    run(identity.executable, arguments);
                    CheckPythonPackages(identity, cancel);
                    if (!identity.kernelReady) throw std::runtime_error("Installation finished, but kernel support still fails validation. Review the details and select or repair this environment.");
                } else if (operation == Official) {
                    const bool lackedPython = identity.executable.empty();
                    for (int attempt = 0; attempt < 600 && !cancel; ++attempt) {
                        identity = lackedPython ? DiscoverInstalledPython(cancel) : DiscoverPython(ReadPythonEnvironment(identity.executable), cancel);
                        if (identity.error.empty() && (identity.kernelReady || (lackedPython && !identity.executable.empty()))) break;
                        for (int tick = 0; tick < 10 && !cancel; ++tick) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    if (!cancel && (!identity.error.empty() || (lackedPython ? identity.executable.empty() : !identity.kernelReady)))
                        throw std::runtime_error("Setup was not detected within ten minutes. You can check again or select an interpreter.");
                } else {
                    auto selection = ReadPythonEnvironment(identity.executable.empty() ? environment.explicitPath : identity.executable);
                    identity = DiscoverPython(selection, cancel);
                }
                if (cancel) failure = L"Setup was cancelled. Check again before retrying; external changes may remain.";
            } catch (const std::exception& error) {
                failure = Utf16(error.what());
            }
            try {
                AppPaths paths;
                std::ofstream saved(std::filesystem::path(paths.Directory(L"setup")) / L"last-setup.log", std::ios::binary);
                saved << Utf8(identity.executable + L"\n" + failure + L"\n" + log);
            } catch (...) { failure += L"\r\nCould not save the setup log."; }
            done = true;
        });
    }
};

INT_PTR CALLBACK DialogProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto state = reinterpret_cast<SetupDialog*>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<SetupDialog*>(lp);
        SetWindowLongPtrW(window, DWLP_USER, lp);
        state->style->Initialize(window,state->parent);
        CheckDlgButton(window,Consent,BST_CHECKED);
        CheckDlgButton(window,UserScope,!state->identity.virtualEnvironment && state->identity.userSiteEnabled?BST_CHECKED:BST_UNCHECKED);
        state->Refresh(window);
        if(!state->failure.empty())state->Start(window,Check,false);
        else SetFocus(GetDlgItem(window,state->identity.kernelReady?IDOK:PythonSetupPlan::CanInstall(PythonSetupPlan::Stage(state->identity))?Install:Check));
        // WM_INITDIALOG precedes the modal manager's first show/activation. Make
        // the one-time foreground handoff after that, not during geometry setup.
        PostMessageW(window,ActivateSetup,0,0);
        return FALSE;
    }
    if (!state) return FALSE;
    if(message==ActivateSetup) {
        SetWindowPos(window,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
        SetForegroundWindow(window);
        SetActiveWindow(window);
        const int control=state->busy?IDCANCEL:state->identity.kernelReady?IDOK:
            PythonSetupPlan::CanInstall(PythonSetupPlan::Stage(state->identity))?Install:Check;
        SetFocus(GetDlgItem(window,control));
        return TRUE;
    }
    if(message==WM_CTLCOLORDLG || message==WM_CTLCOLORSTATIC || message==WM_CTLCOLOREDIT || message==WM_CTLCOLORBTN)
        return reinterpret_cast<INT_PTR>(state->style->ControlColor(reinterpret_cast<HDC>(wp),reinterpret_cast<HWND>(lp)));
    if(message==WM_DRAWITEM)return state->style->DrawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));
    if(message==WM_DPICHANGED) {
        const auto& position=*reinterpret_cast<const RECT*>(lp);
        SetWindowPos(window,nullptr,position.left,position.top,position.right-position.left,position.bottom-position.top,SWP_NOZORDER|SWP_NOACTIVATE);
        state->style->Initialize(window,state->parent,false);return TRUE;
    }
    if (message == WM_TIMER && state->busy && state->done) {
        state->worker.join(); state->busy = false;KillTimer(window,1);
        if(state->closeWhenDone){EndDialog(window,IDCANCEL);return TRUE;}
        state->page=Page::Introduction;
        CheckDlgButton(window, Consent, BST_CHECKED);
        CheckDlgButton(window, UserScope, !state->identity.virtualEnvironment && state->identity.userSiteEnabled ? BST_CHECKED : BST_UNCHECKED);
        state->Refresh(window);
        return TRUE;
    }
    if (message == WM_COMMAND) {
        const int command = LOWORD(wp);
        if (command == IDCANCEL && state->busy) { state->cancel = true; return TRUE; }
        if (state->busy) return TRUE;
        if(command==Consent || command==UserScope){state->Refresh(window);return TRUE;}
        if(command==Back){state->page=Page::Introduction;state->Refresh(window);return TRUE;}
        if(command==Install) {
            const auto stage=PythonSetupPlan::Stage(state->identity);
            if(!PythonSetupPlan::CanInstall(stage) || IsDlgButtonChecked(window,Consent)!=BST_CHECKED)return TRUE;
            if(state->page==Page::Introduction) {
                state->page=Page::Review;state->Refresh(window);
            } else {
                state->Start(window,stage==PythonSetupStage::Pip?EnablePip:Install,IsDlgButtonChecked(window,UserScope)==BST_CHECKED);
            }
            return TRUE;
        }
        if (command == Official) {
            const auto url = state->identity.executable.empty() ? L"https://www.python.org/downloads/windows/" :
                !state->identity.pipAvailable?L"https://pip.pypa.io/en/stable/installation/":L"https://ipython.readthedocs.io/en/stable/install/kernel_install.html";
            const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", url, nullptr, nullptr, SW_SHOWNORMAL));
            if (result <= 32) { state->failure = L"Could not open https://www.python.org/downloads/windows/"; state->Refresh(window); }
            else state->Start(window, Official, false);
            return TRUE;
        }
        if (command == Check) { state->Start(window, Check, false); return TRUE; }
        if (command == IDOK && !state->identity.kernelReady) return TRUE;
        if (command == IDOK || command == IDCANCEL || command == Select) { EndDialog(window, command); return TRUE; }
    }
    if (message == WM_CLOSE) {
        if (state->busy) {state->closeWhenDone=true;state->cancel = true;}
        else EndDialog(window, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}
}

int PythonSetup::Show(HWND parent, PythonIdentity& identity, const PythonEnvironment& environment,
    const std::wstring& runtimeDirectory, const std::wstring& failure,HINSTANCE resources,bool installation) {
    SetupDialog state;
    state.identity = identity; state.environment = environment;
    state.runtimeDirectory = runtimeDirectory; state.failure = failure;
    SetupWindowStyle style(std::filesystem::path(runtimeDirectory).parent_path());
    state.style=&style;state.parent=parent;state.installation=installation;
    if(installation && PythonSetupPlan::CanInstall(PythonSetupPlan::Stage(identity)))state.page=Page::Review;
    const auto result = DialogBoxParamW(resources?resources:GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_PYTHON_SETUP), parent,
        DialogProc, reinterpret_cast<LPARAM>(&state));
    identity = state.identity;
    return static_cast<int>(result);
}
