#pragma once
#include "framework.h"
#include "PythonDiscovery.h"

class PythonSetup {
public:
    // Modal native advisory owns and joins all setup work. Returns IDOK only
    // when the user explicitly chooses Start kernel, or 1004 to select Python.
    static int Show(HWND parent, PythonIdentity& identity, const PythonEnvironment& environment,
        const std::wstring& runtimeDirectory, const std::wstring& failure = {},
        HINSTANCE resources = nullptr, bool installation = false);
};
