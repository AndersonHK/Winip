#include "framework.h"
#include "ChildProcess.h"
#include <algorithm>
#include <chrono>

namespace {
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
}

std::wstring ChildProcess::Quote(const std::wstring& value) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') { ++slashes; continue; }
        result.append(slashes * (ch == L'"' ? 2 : 1), L'\\');
        slashes = 0;
        if (ch == L'"') result += L'\\';
        result += ch;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}

ChildProcessResult ChildProcess::Run(const std::wstring& executable,
    const std::vector<std::wstring>& arguments, const std::atomic<bool>& cancel,
    unsigned timeoutMs, const std::wstring& directory) {
    ChildProcessResult result;
    auto fail = [&](const wchar_t* operation) {
        result.error = std::wstring(operation) + L" (Windows error " + std::to_wstring(GetLastError()) + L")";
        return result;
    };
    if (cancel) { result.error = L"Operation cancelled"; return result; }
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    Handle reader, writer, input, job, process, thread;
    if (!CreatePipe(&reader.value, &writer.value, &security, 0) ||
        !SetHandleInformation(reader.value, HANDLE_FLAG_INHERIT, 0)) return fail(L"Cannot create operation pipe");
    input.value = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security, OPEN_EXISTING, 0, nullptr);
    if (input.value == INVALID_HANDLE_VALUE) return fail(L"Cannot open operation input");
    job.value = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation,
        &limits, sizeof(limits))) return fail(L"Cannot own operation process");
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.value;
    startup.StartupInfo.hStdOutput = startup.StartupInfo.hStdError = writer.value;
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    std::vector<unsigned char> attributes(size);
    startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &size)) return fail(L"Cannot initialize operation handles");
    struct Cleanup { LPPROC_THREAD_ATTRIBUTE_LIST list; ~Cleanup() { DeleteProcThreadAttributeList(list); } } cleanup{startup.lpAttributeList};
    HANDLE inherited[] = {input.value, writer.value};
    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        inherited, sizeof(inherited), nullptr, nullptr)) return fail(L"Cannot restrict operation handles");
    auto command = Quote(executable);
    for (const auto& argument : arguments) command += L" " + Quote(argument);
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr,
        directory.empty() ? nullptr : directory.c_str(), &startup.StartupInfo, &child)) return fail(L"Cannot launch operation");
    process.value = child.hProcess;
    thread.value = child.hThread;
    if (!AssignProcessToJobObject(job.value, process.value)) {
        TerminateProcess(process.value, 1);
        return fail(L"Cannot assign operation ownership");
    }
    if (ResumeThread(thread.value) == static_cast<DWORD>(-1)) return fail(L"Cannot resume operation");
    CloseHandle(writer.value);
    writer.value = nullptr;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    bool exited = false;
    for (;;) {
        DWORD available = 0;
        // Limit each drain so a noisy child cannot prevent cancellation or timeout.
        for (int batch = 0; batch < 32 && PeekNamedPipe(reader.value, nullptr, 0, nullptr, &available, nullptr) && available; ++batch) {
            char bytes[4096]; DWORD count = 0;
            if (!ReadFile(reader.value, bytes, std::min<DWORD>(available, sizeof(bytes)), &count, nullptr) || !count) break;
            result.output.append(bytes, count);
            if (result.output.size() > 262144) result.output.erase(0, result.output.size() - 262144);
        }
        if (exited) break;
        if (cancel || std::chrono::steady_clock::now() >= deadline) {
            result.error = cancel ? L"Operation cancelled; external changes may be incomplete" : L"Operation timed out";
            TerminateJobObject(job.value, 1);
            WaitForSingleObject(process.value, 5000);
            return result;
        }
        exited = WaitForSingleObject(process.value, 25) == WAIT_OBJECT_0;
    }
    GetExitCodeProcess(process.value, &result.exitCode);
    return result;
}
