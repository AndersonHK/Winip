#pragma once
#include <array>
#include <streambuf>
#include "framework.h"

// The staging-file owner outlives this buffered writer. Flush explicitly before publication.
class NotebookOutput final : public std::streambuf {
public:
    explicit NotebookOutput(HANDLE file);
    NotebookOutput(const NotebookOutput&) = delete;
    NotebookOutput& operator=(const NotebookOutput&) = delete;
private:
    int_type overflow(int_type ch) override;
    int sync() override;
    HANDLE file;
    std::array<char, 65536> buffer;
};
