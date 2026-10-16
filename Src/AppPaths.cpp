#include "AppPaths.h"
#include <windows.h>
#include <shlobj.h>
#include <memory>
#include <stdexcept>

namespace {
struct KnownFolderDeleter {
    void operator()(wchar_t* value) const { CoTaskMemFree(value); }
};
}

std::filesystem::path AppPaths::RoamingFolder() {
    PWSTR value = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &value);
    std::unique_ptr<wchar_t, KnownFolderDeleter> folder(value);
    if (FAILED(result)) {
        throw std::runtime_error("Cannot resolve the current user's Roaming AppData folder");
    }
    return std::filesystem::path(folder.get());
}

AppPaths::AppPaths() : AppPaths(RoamingFolder()) {}

AppPaths::AppPaths(const std::filesystem::path& roamingFolder) {
    if (!roamingFolder.is_absolute()) {
        throw std::invalid_argument("Roaming AppData must be an absolute path");
    }
    root = roamingFolder / L"Winip";
    std::filesystem::create_directories(root);
}

const std::filesystem::path& AppPaths::Root() const { return root; }

std::filesystem::path AppPaths::Directory(const std::filesystem::path& relative) const {
    if (relative.empty() || relative.has_root_path()) {
        throw std::invalid_argument("Application state directory must be relative");
    }
    for (const auto& part : relative) {
        if (part == L".." || part == L"." || part.wstring().find(L':') != std::wstring::npos) {
            throw std::invalid_argument("Invalid application state directory");
        }
    }
    const auto directory = root / relative;
    std::filesystem::create_directories(directory);
    return directory;
}
