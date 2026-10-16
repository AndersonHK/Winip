#pragma once
#include <filesystem>
#include <memory>
#include <string>

// UI-owned values, asynchronously persisted snapshots. Destruction flushes writes.
class AppSettings {
public:
    explicit AppSettings(const std::filesystem::path& root);
    ~AppSettings();
    bool HdrEnabled() const;
    bool UseLastFolder() const;
    std::wstring OpenFolder() const;
    void SetHdrEnabled(bool enabled);
    void SetUseLastFolder(bool enabled);
    void SetFixedFolder(const std::wstring& folder);
    void RememberNotebook(const std::wstring& path);
    std::wstring Error() const;
private:
    void Queue();
    struct Impl;
    std::unique_ptr<Impl> impl;
};
