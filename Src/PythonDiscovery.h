#pragma once
#include <atomic>
#include <string>
#include <vector>
struct PythonEnvironment {
    std::wstring explicitPath,virtualEnv,condaPrefix,inheritedPath,userPath,systemPath;
    bool terminal=false;
    std::vector<std::wstring> registered;
};
struct PythonIdentity {
    std::wstring executable,origin,version,error;
    int bits=0;
    bool kernelReady=false, pipAvailable=false, ensurePipAvailable=false, virtualEnvironment=false, userSiteEnabled=false;
    std::wstring packageError, installDirectory, systemInstallDirectory;
    std::vector<std::wstring> missingPackages;
};
PythonEnvironment ReadPythonEnvironment(const std::wstring& explicitPath);
PythonIdentity SelectPython(const PythonEnvironment& environment);
PythonIdentity DiscoverPython(const PythonEnvironment& environment,const std::atomic<bool>& cancel);
void CheckPythonPackages(PythonIdentity& identity,const std::atomic<bool>& cancel);
