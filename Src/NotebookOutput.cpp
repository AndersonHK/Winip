#include "NotebookOutput.h"

NotebookOutput::NotebookOutput(HANDLE handle) : file(handle) {
    setp(buffer.data(), buffer.data() + buffer.size());
}
int NotebookOutput::sync() {
    const auto size = static_cast<DWORD>(pptr() - pbase());
    DWORD offset = 0;
    while (offset < size) {
        DWORD written = 0;
        if (!WriteFile(file, buffer.data() + offset, size - offset, &written, nullptr) || !written) return -1;
        offset += written;
    }
    setp(buffer.data(), buffer.data() + buffer.size());
    return 0;
}
NotebookOutput::int_type NotebookOutput::overflow(int_type ch) {
    if (sync() != 0) return traits_type::eof();
    if (!traits_type::eq_int_type(ch, traits_type::eof())) {
        *pptr() = traits_type::to_char_type(ch);
        pbump(1);
    }
    return traits_type::not_eof(ch);
}
