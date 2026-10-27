#pragma once
#include <filesystem>
#include <string>

void RunKernelSetupTests(const std::filesystem::path& root, const std::wstring& python, const std::wstring& self);
void RunKernelConnectionTest(const std::filesystem::path& root, const std::wstring& python);
