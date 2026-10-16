#pragma once
#include <filesystem>

// Writable application state belongs to the current user's Roaming known folder.
// Notebook documents and their atomic save files stay at user-selected paths.
class AppPaths {
public:
    AppPaths();
    explicit AppPaths(const std::filesystem::path& roamingFolder);
    const std::filesystem::path& Root() const;
    std::filesystem::path Directory(const std::filesystem::path& relative) const;

private:
    static std::filesystem::path RoamingFolder();
    std::filesystem::path root;
};
