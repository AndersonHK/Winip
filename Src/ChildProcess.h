#pragma once
#include <atomic>
#include <string>
#include <vector>

struct ChildProcessResult {
    unsigned long exitCode = 1;
    std::string output;
    std::wstring error;
};

// Hidden, bounded child operation. Its job owns descendants, including on cancel.
class ChildProcess {
public:
    static std::wstring Quote(const std::wstring& value);
    static ChildProcessResult Run(const std::wstring& executable,
        const std::vector<std::wstring>& arguments, const std::atomic<bool>& cancel,
        unsigned timeoutMs = 30000, const std::wstring& directory = {});
};
