#include "NotebookStorage.h"
#include "NotebookOutput.h"
#include "framework.h"
#include <filesystem>
#include <atomic>
#include <stdexcept>

namespace {
class StagedFile {
public:
    explicit StagedFile(std::wstring name) : path(std::move(name)) {
        handle=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create temporary notebook file");
    }
    ~StagedFile() { Close(); DeleteFileW(path.c_str()); }
    StagedFile(const StagedFile&)=delete;
    StagedFile& operator=(const StagedFile&)=delete;
    void Close() { if(handle!=INVALID_HANDLE_VALUE) { CloseHandle(handle); handle=INVALID_HANDLE_VALUE; } }
    HANDLE handle=INVALID_HANDLE_VALUE;
    std::wstring path;
};
}
void NotebookStorage::CheckCurrent(const std::wstring& path, const Notebook* expected) {
    if (expected && !Notebook::Load(path).SameContent(*expected))
        throw std::runtime_error("The notebook changed on disk. Save a copy with Save As or reopen it before replacing that file.");
}
void NotebookStorage::Save(const std::wstring& path, const Notebook& document, const Notebook* expected) {
    CheckCurrent(path,expected);
    static std::atomic<unsigned> next{0};
    const auto temporary=path+L".winip-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++next)+L".tmp";
    StagedFile staged(temporary);
    NotebookOutput buffer(staged.handle);
    std::ostream stream(&buffer);
    document.Write(stream);
    stream.flush();
    if(!stream || !FlushFileBuffers(staged.handle))
        throw std::runtime_error("Cannot write the complete notebook; destination was not replaced");
    staged.Close();
    CheckCurrent(path,expected);
    // Move within one directory after the complete file is flushed. Save As never
    // overwrites an existing path; a normal save checks the last saved semantics.
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH | (expected ? MOVEFILE_REPLACE_EXISTING : 0)))
        throw std::runtime_error("Cannot publish saved notebook (Windows error "+std::to_string(GetLastError())+")");
}
