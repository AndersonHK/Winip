#include "framework.h"
#include "PythonDiscovery.h"
#include "Notebook.h"
#include <tlhelp32.h>
#include <filesystem>
#include <vector>
#include <chrono>
#include <cwctype>

namespace {
struct Handle {HANDLE value=nullptr;~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}operator HANDLE()const{return value;}};
std::wstring Environment(const wchar_t* name) {
    auto size=GetEnvironmentVariableW(name,nullptr,0);if(!size)return {};
    std::wstring result(size,L'\0');auto written=GetEnvironmentVariableW(name,result.data(),size);
    if(!written || written>=size)return {};result.resize(written);return result;
}
std::wstring Expand(const std::wstring& value) {
    auto size=ExpandEnvironmentStringsW(value.c_str(),nullptr,0);if(!size)return value;
    std::wstring result(size,L'\0');if(!ExpandEnvironmentStringsW(value.c_str(),result.data(),size))return value;
    result.resize(size-1);return result;
}
std::wstring RegisteredPath(HKEY root,const wchar_t* key) {
    DWORD size=0,type=0;
    if(RegGetValueW(root,key,L"Path",RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ|RRF_NOEXPAND,&type,nullptr,&size)!=ERROR_SUCCESS)return {};
    std::vector<wchar_t> value(size/sizeof(wchar_t)+1);
    if(RegGetValueW(root,key,L"Path",RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ|RRF_NOEXPAND,&type,value.data(),&size)!=ERROR_SUCCESS)return {};
    return Expand(value.data());
}
bool TerminalParent() {
    Handle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)};
    PROCESSENTRY32W entry{sizeof(entry)};DWORD parent=0;
    if(Process32FirstW(snapshot,&entry))do {if(entry.th32ProcessID==GetCurrentProcessId()){parent=entry.th32ParentProcessID;break;}}while(Process32NextW(snapshot,&entry));
    if(Process32FirstW(snapshot,&entry))do {if(entry.th32ProcessID==parent) {
        std::wstring name=entry.szExeFile;for(auto& ch:name)ch=(wchar_t)towlower(ch);
        return name==L"cmd.exe" || name==L"powershell.exe" || name==L"pwsh.exe" || name==L"bash.exe" || name==L"sh.exe" || name==L"zsh.exe" || name==L"fish.exe" || name==L"nu.exe" || name==L"python.exe" || name==L"pythonw.exe";
    }}while(Process32NextW(snapshot,&entry));
    return false;
}
bool ExecutableFile(const std::filesystem::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA info{};
    // Do not invoke zero-byte Store install aliases during automatic discovery.
    return GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info) && !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && (info.nFileSizeHigh || info.nFileSizeLow);
}
std::wstring InPath(const std::wstring& path) {
    size_t begin=0;
    while(begin<=path.size()) {
        auto end=path.find(L';',begin);auto part=path.substr(begin,end==std::wstring::npos?end:end-begin);
        if(part.size()>=2 && part.front()==L'"' && part.back()==L'"')part=part.substr(1,part.size()-2);
        if(!part.empty()) {auto candidate=std::filesystem::path(Expand(part))/L"python.exe";if(ExecutableFile(candidate))return std::filesystem::absolute(candidate).wstring();}
        if(end==std::wstring::npos)break;begin=end+1;
    }
    return {};
}
}
PythonEnvironment ReadPythonEnvironment(const std::wstring& explicitPath) {
    return {explicitPath,Environment(L"VIRTUAL_ENV"),Environment(L"CONDA_PREFIX"),Environment(L"PATH"),
        RegisteredPath(HKEY_CURRENT_USER,L"Environment"),RegisteredPath(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"),TerminalParent()};
}
PythonIdentity SelectPython(const PythonEnvironment& env) {
    PythonIdentity result;
    auto required=[&](const std::filesystem::path& path,const wchar_t* origin) {
        result.origin=origin;
        if(ExecutableFile(path))result.executable=std::filesystem::absolute(path).wstring();
        else result.error=L"Selected Python is missing or is an install alias: "+path.wstring();
    };
    if(!env.explicitPath.empty())required(env.explicitPath,L"--python");
    else if(!env.virtualEnv.empty())required(std::filesystem::path(env.virtualEnv)/L"Scripts/python.exe",L"VIRTUAL_ENV");
    else if(!env.condaPrefix.empty())required(std::filesystem::path(env.condaPrefix)/L"python.exe",L"CONDA_PREFIX");
    else if(env.terminal) {result.origin=L"terminal PATH";result.executable=InPath(env.inheritedPath);}
    else {result.origin=L"user PATH";result.executable=InPath(env.userPath);if(result.executable.empty()){result.origin=L"system PATH";result.executable=InPath(env.systemPath);}}
    if(result.executable.empty() && result.error.empty())result.error=L"Python not found. Select an interpreter with --python <python.exe>.";
    return result;
}
PythonIdentity DiscoverPython(const PythonEnvironment& env,const std::atomic<bool>& cancel) {
    auto result=SelectPython(env);if(!result.error.empty() || cancel)return result;
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};Handle reader,writer;
    if(!CreatePipe(&reader.value,&writer.value,&security,0) || !SetHandleInformation(reader,HANDLE_FLAG_INHERIT,0)){result.error=L"Cannot create Python discovery pipe";return result;}
    Handle input{CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr)};
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdInput=input;startup.hStdOutput=writer;startup.hStdError=writer;
    // A fixed isolated stdlib probe: no notebook code, cwd imports, site hooks, or package installation.
    std::wstring command=L"\""+result.executable+L"\" -I -S -c \"import sys,struct,json;print(json.dumps({'executable':sys.executable,'version':sys.version.split()[0],'bits':struct.calcsize('P')*8}))\"";
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(result.executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)) {
        result.error=L"Cannot start selected Python (Windows error "+std::to_wstring(GetLastError())+L")";return result;
    }
    Handle child{process.hProcess},thread{process.hThread};CloseHandle(writer.value);writer.value=nullptr;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);std::string output;bool done=false;
    for(;;) {
        DWORD available=0;
        while(PeekNamedPipe(reader,nullptr,0,nullptr,&available,nullptr) && available) {
            char buffer[1024];DWORD read=0;
            if(!ReadFile(reader,buffer,std::min<DWORD>(available,sizeof(buffer)),&read,nullptr) || !read)break;
            output.append(buffer,read);if(output.size()>16384)break;
        }
        if(done)break;
        if(cancel || output.size()>16384 || std::chrono::steady_clock::now()>=deadline) {
            TerminateProcess(child,1);WaitForSingleObject(child,1000);result.error=cancel?L"Discovery cancelled":L"Python discovery timed out or exceeded its output limit";return result;
        }
        done=WaitForSingleObject(child,20)==WAIT_OBJECT_0;
    }
    DWORD code=1;GetExitCodeProcess(child,&code);
    try {
        if(code)throw std::runtime_error("exit");
        auto value=nlohmann::json::parse(output);
        result.version=Utf16(value.at("version").get<std::string>());result.bits=value.at("bits").get<int>();
        result.executable=Utf16(value.at("executable").get<std::string>());
        if(result.bits!=64)result.error=L"Winip requires a 64-bit Python interpreter";
    } catch(...) {result.error=L"Selected executable did not return a valid Python identity";}
    return result;
}
