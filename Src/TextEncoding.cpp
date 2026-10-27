#include "TextEncoding.h"
#include "framework.h"
#include <climits>
#include <stdexcept>

std::wstring Utf16(const std::string& text) {
    if (text.empty()) return {};
    if (text.size() > INT_MAX) throw std::runtime_error("A single text value exceeds the Windows text conversion range");
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), (int)text.size(), nullptr, 0);
    if (!length) throw std::runtime_error("Invalid UTF-8 text");
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), (int)text.size(), result.data(), length);
    return result;
}
std::string Utf8(const std::wstring& text) {
    if(text.empty()) return {};
    if (text.size() > INT_MAX) throw std::runtime_error("A single text value exceeds the Windows text conversion range");
    int length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),(int)text.size(),nullptr,0,nullptr,nullptr);
    if(!length) throw std::runtime_error("Invalid UTF-16 text");
    std::string result(length,'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),(int)text.size(),result.data(),length,nullptr,nullptr);
    return result;
}
