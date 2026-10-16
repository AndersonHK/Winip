#pragma once
#include <atomic>
#include <string>
struct PythonEnvironment {
    std::wstring explicitPath,virtualEnv,condaPrefix,inheritedPath,userPath,systemPath;
    bool terminal=false;
};
struct PythonIdentity {
    std::wstring executable,origin,version,error;
    int bits=0;
};
PythonEnvironment ReadPythonEnvironment(const std::wstring& explicitPath);
PythonIdentity SelectPython(const PythonEnvironment& environment);
PythonIdentity DiscoverPython(const PythonEnvironment& environment,const std::atomic<bool>& cancel);
