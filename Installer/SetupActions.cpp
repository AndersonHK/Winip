#include "../Src/framework.h"
#include "../Src/PythonDiscovery.h"
#include "../Src/PythonSetup.h"
#include "../Src/PythonSetupPlan.h"
#include "../Src/SetupWindowOwner.h"
#include <msi.h>
#include <msiquery.h>
#include <commdlg.h>
#include <filesystem>
#include <stdexcept>

namespace {
std::wstring Property(MSIHANDLE installer, const wchar_t* name) {
    DWORD length = 0; wchar_t empty[1]{};
    auto result = MsiGetPropertyW(installer, name, empty, &length);
    if (result != ERROR_SUCCESS && result != ERROR_MORE_DATA) throw std::runtime_error("Cannot read installer property");
    std::wstring value(length + 1, L'\0'); ++length;
    if (MsiGetPropertyW(installer, name, value.data(), &length) != ERROR_SUCCESS) throw std::runtime_error("Cannot read installer property");
    value.resize(length); return value;
}
void Set(MSIHANDLE installer, const wchar_t* name, const std::wstring& value) {
    if (MsiSetPropertyW(installer, name, value.c_str()) != ERROR_SUCCESS) throw std::runtime_error("Cannot update installer setup page");
}
PythonEnvironment Selection(const std::wstring& path = {}) {
    auto environment = ReadPythonEnvironment(path);
    environment.terminal = false;
    return environment;
}
HWND InstallerWindow(MSIHANDLE installer) {
    // MSI can host this DLL outside the process owning its wizard. Do not reject
    // the real owner merely because its PID differs from the custom-action host.
    DWORD client = 0;
    const auto value = Property(installer,L"CLIENTPROCESSID");
    if (!value.empty()) {
        wchar_t* end = nullptr;
        const auto parsed = wcstoul(value.c_str(),&end,10);
        if (end && !*end) client = parsed;
    }
    return SetupWindowOwner::FindInstaller(Property(installer,L"ProductName")+L" Setup",client);
}
}

// Read-only prerequisite detection belongs to the client UI sequence, before
// the wizard pages. No Python/package installation occurs in this action.
extern "C" __declspec(dllexport) UINT __stdcall DetectKernelSupport(MSIHANDLE installer) {
    try {
        std::atomic<bool> cancel{false};
        const auto identity = DiscoverPython(Selection(), cancel);
        const auto stage = PythonSetupPlan::Stage(identity);
        Set(installer, L"WINIP_SETUP_STATE", PythonSetupPlan::Name(stage));
        Set(installer, L"WINIP_SETUP_TITLE", PythonSetupPlan::Title(stage));
        Set(installer, L"WINIP_SETUP_DESCRIPTION", PythonSetupPlan::Description(stage));
        Set(installer, L"WINIP_SETUP_CHOICE", PythonSetupPlan::Choice(stage));
        Set(installer, L"WINIP_SETUP_REQUESTED", stage == PythonSetupStage::Ready ? L"" : L"1");
        Set(installer, L"WINIP_SETUP_PYTHON", identity.executable);
        return ERROR_SUCCESS;
    } catch (...) {
        // Failed discovery must never masquerade as a working environment or
        // prevent installing Winip as a notebook editor.
        MsiSetPropertyW(installer, L"WINIP_SETUP_STATE", L"repair");
        MsiSetPropertyW(installer, L"WINIP_SETUP_TITLE", L"Python setup needs attention");
        MsiSetPropertyW(installer, L"WINIP_SETUP_DESCRIPTION", L"Automatic checking could not finish. Continue with guided setup or skip it and use Winip as a notebook editor.");
        MsiSetPropertyW(installer, L"WINIP_SETUP_REQUESTED", L"1");
        return ERROR_SUCCESS;
    }
}

// ExecuteAction has returned and InstallFinalize has committed before this UI
// continuation runs. Dependency setup is skippable and never affects MSI rollback.
extern "C" __declspec(dllexport) UINT __stdcall CompleteKernelSetup(MSIHANDLE installer) {
    try {
        std::atomic<bool> cancel{false};
        auto environment = Selection(Property(installer, L"WINIP_SETUP_PYTHON"));
        auto identity = DiscoverPython(environment, cancel);
        if (identity.kernelReady) return ERROR_SUCCESS;
        const auto runtime = (std::filesystem::path(Property(installer, L"INSTALLFOLDER")) / L"runtime").wstring();
        static const wchar_t moduleAnchor[] = L"Winip setup resources";
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            moduleAnchor, &module)) throw std::runtime_error("Cannot locate setup dialog resources");
        for (;;) {
            auto parent = InstallerWindow(installer);
            const int action = PythonSetup::Show(parent, identity, environment, runtime, {}, module, true);
            if (action != 1004) break;
            wchar_t path[32768]{}; OPENFILENAMEW picker{sizeof(picker)};
            picker.hwndOwner = parent; picker.lpstrFile = path; picker.nMaxFile = 32768;
            picker.lpstrFilter = L"Python executable\0*.exe\0";
            picker.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameW(&picker)) {
                environment = Selection(path);
                identity = DiscoverPython(environment, cancel);
            }
        }
    } catch (...) {
        MessageBoxW(SetupWindowOwner::FindInstaller(L"Winip Setup"), L"Winip was installed, but guided Python setup could not finish. Open Winip and choose Kernel > Connect kernel to continue.",
            L"Winip setup", MB_OK | MB_ICONWARNING);
    }
    return ERROR_SUCCESS;
}
