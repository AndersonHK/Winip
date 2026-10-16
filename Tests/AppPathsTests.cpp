#include "../Src/AppPaths.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int wmain(int argc, wchar_t** argv) {
    try {
        Require(argc == 2, "Pass an absolute disposable test directory");
        const std::filesystem::path workspace(argv[1]);
        AppPaths paths(workspace / L"Roaming 日本語");
        Require(paths.Root() == workspace / L"Roaming 日本語" / L"Winip", "Wrong storage root");
        const auto runtime = paths.Directory(L"runtime/session");
        { std::ofstream file(runtime / L"state.txt"); file << "writable"; }
        Require(std::filesystem::file_size(runtime / L"state.txt") == 8, "State is not writable");
        Require(paths.Directory(L"runtime/session") == runtime, "Directory reuse failed");
        for (const auto* value : {L"", L"..", L"runtime/../../outside", L"C:\\outside", L"C:outside", L"name:stream"}) {
            bool rejected = false;
            try { paths.Directory(value); } catch (const std::invalid_argument&) { rejected = true; }
            Require(rejected, "Escaping/invalid path was accepted");
        }
        { std::ofstream file(paths.Root() / L"collision"); file << "keep"; }
        bool rejected = false;
        try { paths.Directory(L"collision/child"); } catch (const std::filesystem::filesystem_error&) { rejected = true; }
        Require(rejected, "Creation failure was concealed");
        Require(std::filesystem::file_size(paths.Root() / L"collision") == 4, "Existing file changed");
        rejected = false;
        try { AppPaths relative(L"relative"); } catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected, "Relative AppData root accepted");
        // Resolve the actual known folder without creating/writing in the user's profile.
        PWSTR folder = nullptr;
        Require(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &folder)), "Roaming known folder lookup failed");
        const std::filesystem::path known(folder);
        CoTaskMemFree(folder);
        Require(known.is_absolute(), "Roaming known folder is not absolute");
        std::cout << "AppPaths: Unicode, containment, writable/reused directories, collision and known-folder checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
