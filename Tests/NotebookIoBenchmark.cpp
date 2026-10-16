#include "NotebookIoBenchmark.h"
#include "../Src/Notebook.h"
#include "../Src/NotebookStorage.h"
#include "../Src/framework.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>

namespace {
// Retained 0.2.0 algorithms for an isolated, same-process IO comparison.
Notebook LegacyLoad(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::string bytes;
    char block[65536];
    while (stream.read(block, sizeof(block)) || stream.gcount()) bytes.append(block, static_cast<size_t>(stream.gcount()));
    return Notebook::FromJson(nlohmann::json::parse(bytes, [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
        if (depth>128) throw std::runtime_error("Nesting limit");
        return true;
    }), path.stem().wstring());
}
void LegacySave(const std::filesystem::path& path, const Notebook& notebook) {
    auto result=notebook.Original();
    auto cells=nlohmann::json::array();
    for(const auto& cell:notebook.Cells())cells.push_back(*cell.original);
    result["cells"]=std::move(cells);
    const auto bytes=result.dump(1)+"\n";
    auto temporary=path;temporary+=L".tmp";
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create comparison file");
    DWORD written=0;
    const bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) && written==bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if(!ok || !MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Comparison write failed");
}
template<class Work> nlohmann::json Measure(const Work& work) {
    ULONG64 before=0,after=0;QueryThreadCycleTime(GetCurrentThread(),&before);
    const auto start=std::chrono::steady_clock::now();work();
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    QueryThreadCycleTime(GetCurrentThread(),&after);
    return {{"ms",ms},{"thread_cycles",after-before}};
}
}
void NotebookIoBenchmark::Run(const std::filesystem::path& root) {
    auto folder=root/L"artifacts"/(L"io-comparison-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);
    auto metadata=folder/L"metadata.ipynb";
    {std::ofstream file(metadata,std::ios::binary);
        file<<"{\"nbformat\":4,\"metadata\":{\"payload\":\""<<std::string(16*1024*1024,'x')<<"\"},\"cells\":[]}";}
    auto rows=nlohmann::json::array();
    for(const auto& path:{root/L"Tests/Fixtures/visualization.ipynb",root/L"Tests/Fixtures/large.ipynb",metadata}) {
        auto notebook=Notebook::Load(path.wstring());
        if(!notebook.SameContent(LegacyLoad(path)))throw std::runtime_error("IO comparison inputs disagree");
        for(int trial=0;trial<9;++trial)for(int turn=0;turn<2;++turn) {
            bool legacy=(trial+turn)%2==0;
            auto read=Measure([&]{auto document=legacy?LegacyLoad(path):Notebook::Load(path.wstring());
                if(document.Cells().size()!=notebook.Cells().size())throw std::runtime_error("Benchmark load failed");});
            auto output=folder/(L"output-"+std::to_wstring(trial)+L"-"+std::to_wstring(turn)+L".ipynb");
            auto write=Measure([&]{if(legacy)LegacySave(output,notebook);else NotebookStorage::Save(output.wstring(),notebook,nullptr);});
            if(!notebook.SameContent(Notebook::Load(output.wstring())))throw std::runtime_error("Benchmark save changed content");
            std::filesystem::remove(output);
            rows.push_back({{"fixture",path.filename().string()},{"legacy",legacy},{"trial",trial},{"load",read},{"save_as",write}});
        }
    }
    std::ofstream(folder/L"report.json")<<rows.dump(2)<<"\n";
    std::wcout<<folder.wstring()<<L"\n";
}
