#include "KernelController.h"
#include "Renderer.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <chrono>

namespace {
struct Handle {
    Handle()=default;
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
    HANDLE value = nullptr;
    ~Handle() { Reset(); }
    void Reset() { if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value); value=nullptr; }
};
std::wstring Quote(const std::wstring& value) {
    std::wstring result=L"\""; size_t slashes=0;
    for(wchar_t ch:value) {
        if(ch==L'\\') {++slashes;continue;}
        result.append(slashes*(ch==L'"'?2:1),L'\\');slashes=0;
        if(ch==L'"')result+=L'\\';
        result+=ch;
    }
    result.append(slashes*2,L'\\');return result+L'"';
}
void Require(bool ok,const char* text) {if(!ok)throw std::runtime_error(text);}
}
struct KernelController::Impl {
    Notify notify;
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    Handle job, process, input, output, interrupt;
    std::atomic<bool> connected{false}, interrupted{false};
    uint64_t processGeneration=0;
    std::atomic<bool> busy{false};
    std::atomic<uint64_t> generation{1};
    bool exit=false, pending=false, running=false;
    struct Request {
        std::shared_ptr<const Notebook> document;
        size_t selected=0;
        bool all=false;
        std::wstring python,bridge,runtime,cwd;
        uint64_t generation=0;
    } request, configuration;
    std::string buffered;
    std::filesystem::path diagnosticPath;
    void Publish(std::wstring status,uint64_t version,uint64_t id=0,Cell cell={}) {
        notify({std::move(status),version,id,std::move(cell)});
    }
    void Close() {
        input.Reset();output.Reset();process.Reset();buffered.clear();connected=false;
        std::lock_guard<std::mutex> lock(mutex);interrupt.Reset();job.Reset();
    }
    nlohmann::json Read(bool starting = false) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
        for(;;) {
            auto end=buffered.find('\n');
            if(end!=std::string::npos) {
                auto line=buffered.substr(0,end);buffered.erase(0,end+1);
                auto value=nlohmann::json::parse(line);
                if(value.value("event","")=="interrupt_sent") {Publish(L"Kernel interrupting…",generation);continue;}
                if(value.value("event","")=="error")throw std::runtime_error(value.value("message","Kernel failed"));
                return value;
            }
            if (starting) {
                DWORD available = 0;
                Require(PeekNamedPipe(output.value,nullptr,0,nullptr,&available,nullptr)!=0,"Kernel bridge disconnected during startup");
                if (!available) {
                    Require(std::chrono::steady_clock::now()<deadline,"Kernel startup timed out after 45 seconds");
                    Require(WaitForSingleObject(process.value,25)==WAIT_TIMEOUT,"Kernel bridge exited during startup");
                    continue;
                }
            }
            char bytes[16384];DWORD count=0;
            Require(ReadFile(output.value,bytes,sizeof(bytes),&count,nullptr) && count,"Kernel bridge disconnected");
            buffered.append(bytes,count);
            Require(buffered.size()<=8*1024*1024,"Kernel response exceeds transport limit");
        }
    }
    void Write(const nlohmann::json& value) {
        auto bytes=value.dump()+"\n";size_t offset=0;
        while(offset<bytes.size()) {
            DWORD count=0;
            Require(WriteFile(input.value,bytes.data()+offset,static_cast<DWORD>(bytes.size()-offset),&count,nullptr) && count,"Cannot send kernel request");
            offset+=count;
        }
    }
    void Start(const Request& task) {
        diagnosticPath.clear();
        {
            std::lock_guard<std::mutex> lock(mutex);
            job.Reset();job.value=CreateJobObjectW(nullptr,nullptr);
            Require(job.value!=nullptr,"Cannot create kernel job");
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            Require(SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits))!=0,"Cannot set kernel ownership policy");
        }
        Require(std::filesystem::is_regular_file(task.bridge),"Bundled runtime/kernel_bridge.py is missing; repair the application files");
        SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
        Handle childInput,childOutput,errors;
        Require(CreatePipe(&childInput.value,&input.value,&security,0)!=0,"Cannot create kernel input pipe");
        Require(CreatePipe(&output.value,&childOutput.value,&security,0)!=0,"Cannot create kernel output pipe");
        Require(SetHandleInformation(input.value,HANDLE_FLAG_INHERIT,0) && SetHandleInformation(output.value,HANDLE_FLAG_INHERIT,0),"Cannot protect kernel pipes");
        std::filesystem::create_directories(task.runtime);
        diagnosticPath=std::filesystem::path(task.runtime)/L"last-kernel-stderr.log";
        errors.value=CreateFileW(diagnosticPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        Require(errors.value!=INVALID_HANDLE_VALUE,"Cannot open kernel diagnostics log");
        STARTUPINFOEXW start{};start.StartupInfo.cb=sizeof(start);
        start.StartupInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
        start.StartupInfo.wShowWindow=SW_HIDE;
        start.StartupInfo.hStdInput=childInput.value;start.StartupInfo.hStdOutput=childOutput.value;start.StartupInfo.hStdError=errors.value;
        SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);
        std::vector<unsigned char> attributes(size);
        start.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        Require(InitializeProcThreadAttributeList(start.lpAttributeList,1,0,&size)!=0,"Cannot initialize kernel handle list");
        struct AttributeCleanup {LPPROC_THREAD_ATTRIBUTE_LIST value;~AttributeCleanup(){DeleteProcThreadAttributeList(value);}} cleanup{start.lpAttributeList};
        interrupt.value=CreateEventW(&security,FALSE,FALSE,nullptr);
        Require(interrupt.value!=nullptr,"Cannot create interrupt event");
        HANDLE inherited[]={childInput.value,childOutput.value,errors.value,interrupt.value};
        Require(UpdateProcThreadAttribute(start.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)!=0,"Cannot restrict inherited kernel handles");
        auto command=Quote(task.python)+L" -X utf8 -u -B "+Quote(task.bridge)+L" "+Quote(task.runtime)+L" "+Quote(task.cwd)+L" "+std::to_wstring(reinterpret_cast<uintptr_t>(interrupt.value));
        PROCESS_INFORMATION child{};
        Require(CreateProcessW(task.python.c_str(),command.data(),nullptr,nullptr,TRUE,
            CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,task.cwd.c_str(),&start.StartupInfo,&child)!=0,"Cannot launch selected Python");
        process.value=child.hProcess;Handle thread;thread.value=child.hThread;
        if(!AssignProcessToJobObject(job.value,process.value)) {
            TerminateProcess(process.value,1);throw std::runtime_error("Cannot assign kernel process ownership");
        }
        {std::lock_guard<std::mutex> lock(mutex);
            if(exit || generation!=task.generation) {TerminateJobObject(job.value,1);throw std::runtime_error("Kernel start cancelled");}
            Require(ResumeThread(thread.value)!=static_cast<DWORD>(-1),"Cannot resume kernel bridge");
        }
        childInput.Reset();childOutput.Reset();errors.Reset();
        auto ready=Read(true);Require(ready.value("event","")=="ready","Invalid kernel startup reply");
        Require(Utf16(ready.at("executable").get<std::string>())==task.python,"Kernel interpreter identity mismatch");
        {std::lock_guard<std::mutex> lock(mutex);
            Require(generation==task.generation,"Kernel start cancelled");
            processGeneration=task.generation;connected=true;
        }
    }
    void Run() {
        for(;;) {
            Request task;
            {std::unique_lock<std::mutex> lock(mutex);wake.wait(lock,[&]{return exit || pending;});
                if(exit)break;
                task=std::move(request);pending=false;running=true;
            }
            try {
                if(process.value && (processGeneration!=task.generation || WaitForSingleObject(process.value,0)!=WAIT_TIMEOUT))Close();
                if(!process.value) {Publish(L"Kernel starting\u2026",task.generation);Start(task);}
                if(task.document) {
                    const auto& cells=task.document->Cells();
                    for(size_t i=0;i<cells.size();++i) {
                        if(generation!=task.generation)break;
                        if((!task.all && i!=task.selected) || cells[i].text->kind!=L"code")continue;
                        Publish(L"Kernel busy",task.generation);
                        Write({{"command","execute"},{"source",Utf8(cells[i].text->source)}});
                        auto reply=Read();Require(reply.value("event","")=="result","Invalid execution reply");
                        auto notebook=Notebook::FromJson({{"nbformat",4},{"cells",nlohmann::json::array({{
                            {"cell_type","code"},{"source",Utf8(cells[i].text->source)},
                            {"execution_count",reply.at("execution_count")},{"outputs",reply.at("outputs")}}})}},L"Kernel result");
                        PrepareNotebookLayout(notebook);
                        if(generation==task.generation)Publish(L"Kernel busy",task.generation,cells[i].id,notebook.Cells()[0]);
                        if(reply.value("status","")!="ok" || interrupted)break;
                    }
                }
                bool queued;
                {std::lock_guard<std::mutex> lock(mutex);queued=pending;busy=queued;running=false;}
                if(!queued)Publish(generation==task.generation?L"Kernel idle":L"Kernel stopped",generation);
            } catch(const std::exception& error) {
                const bool startupFailure = !connected;
                TerminateJobObject(job.value,1);Close();
                std::wstring detail = Utf16(error.what());
                if (!diagnosticPath.empty()) {
                    std::ifstream diagnostic(diagnosticPath,std::ios::binary | std::ios::ate);
                    if (diagnostic) {
                        const auto size=diagnostic.tellg();
                        diagnostic.seekg(std::max<std::streamoff>(0,static_cast<std::streamoff>(size)-65536));
                        std::string tail((std::istreambuf_iterator<char>(diagnostic)),std::istreambuf_iterator<char>());
                        if (!tail.empty()) {
                            try { detail += L"\n\n" + Utf16(tail); }
                            catch (...) { detail += L"\nDiagnostic log contains non-UTF-8 text: " + diagnosticPath.wstring(); }
                        }
                    }
                }
                bool queued;
                {std::lock_guard<std::mutex> lock(mutex);queued=pending;busy=queued;running=false;}
                if(!queued) {
                    if(generation==task.generation)notify({L"Kernel error: "+detail,generation,0,{},true,startupFailure});
                    else Publish(L"Kernel stopped",generation);
                }
            }
        }
        Close();
    }
};
KernelController::KernelController(Notify notify) : impl(std::make_unique<Impl>()) {
    impl->notify=std::move(notify);
    impl->worker=std::thread([this]{impl->Run();});
}
KernelController::~KernelController() {
    {std::lock_guard<std::mutex> lock(impl->mutex);impl->exit=true;++impl->generation;TerminateJobObject(impl->job.value,1);}
    impl->wake.notify_one();impl->worker.join();
}
bool KernelController::Submit(std::shared_ptr<const Notebook> document,size_t selected,bool all,
    std::wstring python,std::wstring bridge,std::wstring runtime,std::wstring cwd) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if(impl->busy)return false;
    impl->interrupted=false;impl->busy=true;impl->request={std::move(document),selected,all,std::move(python),std::move(bridge),std::move(runtime),std::move(cwd),impl->generation};
    impl->configuration=impl->request;impl->configuration.document.reset();
    impl->pending=true;impl->wake.notify_one();return true;
}
void KernelController::Stop() {
    std::lock_guard<std::mutex> lock(impl->mutex);++impl->generation;impl->connected=false;
    impl->configuration={};
    impl->pending=false;if(!impl->running)impl->busy=false;TerminateJobObject(impl->job.value,1);
}
bool KernelController::Busy() const {return impl->busy;}
uint64_t KernelController::Generation() const {return impl->generation;}

uint64_t KernelController::CpuTicks() const {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if(!impl->job.value)return 0;
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
    if(!QueryInformationJobObject(impl->job.value,JobObjectBasicAccountingInformation,&info,sizeof(info),nullptr))return 0;
    return static_cast<uint64_t>(info.TotalKernelTime.QuadPart+info.TotalUserTime.QuadPart);
}

bool KernelController::Connected() const {return impl->connected;}
bool KernelController::Interrupt() {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if(!impl->connected || !impl->busy)return false;
    impl->interrupted=true;
    return SetEvent(impl->interrupt.value)!=FALSE;
}
bool KernelController::Restart() {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if(impl->configuration.python.empty() || !impl->connected)return false;
    ++impl->generation;impl->connected=false;impl->interrupted=false;
    TerminateJobObject(impl->job.value,1);
    impl->request=impl->configuration;impl->request.generation=impl->generation;
    impl->busy=true;impl->pending=true;impl->wake.notify_one();return true;
}
