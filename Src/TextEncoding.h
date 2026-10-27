#pragma once
#include <string>

std::wstring Utf16(const std::string& text);
std::string Utf8(const std::wstring& text);
