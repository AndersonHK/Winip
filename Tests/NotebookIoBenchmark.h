#pragma once
#include <filesystem>

class NotebookIoBenchmark {
public:
    static void Run(const std::filesystem::path& root);
};
