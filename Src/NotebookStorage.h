#pragma once
#include "Notebook.h"

// Blocking file operations: call on the document worker, never the input thread.
class NotebookStorage {
public:
    static void Save(const std::wstring& path, const Notebook& document, const Notebook* expected);
private:
    static void CheckCurrent(const std::wstring& path, const Notebook* expected);
};
