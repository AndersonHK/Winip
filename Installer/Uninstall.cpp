#include <windows.h>
#include <msi.h>
#include <shellapi.h>
#include <string>

// Windows Installer owns removal, rollback, registry cleanup and elevation.
// Exit as soon as it starts so this launcher can itself be removed.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    static_assert(sizeof(void*) == 8, "Winip is x64 only");
    wchar_t productCode[39]{};
    const UINT result = MsiEnumRelatedProductsW(
        L"{C99B13C5-F87D-45DA-9E97-80900BA0B3DA}", 0, 0, productCode);
    if (result != ERROR_SUCCESS) {
        MessageBoxW(nullptr, L"Windows Installer could not find an installed copy of Winip."
            L"\nYou can also remove Winip from Windows Settings > Apps.", L"Uninstall Winip", MB_OK | MB_ICONINFORMATION);
        return 1;
    }
    wchar_t systemDirectory[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH);
    if (!length || length >= MAX_PATH) return 1;
    const std::wstring installer = std::wstring(systemDirectory) + L"\\msiexec.exe";
    const std::wstring arguments = L"/x " + std::wstring(productCode);
    SHELLEXECUTEINFOW execute{sizeof(execute)};
    execute.fMask = SEE_MASK_FLAG_NO_UI;
    execute.lpFile = installer.c_str();
    execute.lpParameters = arguments.c_str();
    execute.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute)) {
        MessageBoxW(nullptr, L"Windows Installer could not be started. Use Windows Settings > Apps to remove Winip.",
                    L"Uninstall Winip", MB_OK | MB_ICONERROR);
        return 1;
    }
    return 0;
}
