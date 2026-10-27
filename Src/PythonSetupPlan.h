#pragma once
#include "PythonDiscovery.h"

enum class PythonSetupStage { Ready, Python, Pip, Kernel, Repair };

// Shared by MSI prerequisite pages and the application's guided setup.
class PythonSetupPlan {
public:
    static PythonSetupStage Stage(const PythonIdentity& identity);
    static const wchar_t* Name(PythonSetupStage stage);
    static const wchar_t* Title(PythonSetupStage stage);
    static const wchar_t* Description(PythonSetupStage stage);
    static const wchar_t* Choice(PythonSetupStage stage);
    static bool CanInstall(PythonSetupStage stage);
};
