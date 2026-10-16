#include "LoadMetrics.h"
thread_local LoadMetrics* LoadMetrics::active=nullptr;
LoadMetrics::LoadMetrics():previous(active) {active=this;}
LoadMetrics::~LoadMetrics() {active=previous;}
LoadMetrics::Scope::Scope(Stage value):owner(active),stage(value) {
    if(owner){LARGE_INTEGER now{};QueryPerformanceCounter(&now);start=now.QuadPart;QueryThreadCycleTime(GetCurrentThread(),&cycles);}
}
LoadMetrics::Scope::~Scope() {
    if(owner){LARGE_INTEGER now{};uint64_t end=0;QueryPerformanceCounter(&now);QueryThreadCycleTime(GetCurrentThread(),&end);
        auto& sample=owner->samples[stage];++sample.calls;sample.ticks+=now.QuadPart-start;sample.cycles+=end-cycles;}
}
nlohmann::json LoadMetrics::Report() const {
    const char* names[]={"file_read","json_including_file_read","model_including_code_images","image_total","image_decode","markdown_including_images_and_layout","text_layout","notebook_layout_including_markdown"};
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    auto result=nlohmann::json::object();
    for(size_t i=0;i<samples.size();++i)result[names[i]]={{"calls",samples[i].calls},{"wall_ms",samples[i].ticks*1000.0/frequency.QuadPart},{"cpu_cycles",samples[i].cycles}};
    return result;
}
