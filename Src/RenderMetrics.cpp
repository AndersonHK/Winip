#include "framework.h"
#include "RenderMetrics.h"

RenderMetrics::RenderMetrics(bool active):enabled(active) {
    LARGE_INTEGER value{};QueryPerformanceFrequency(&value);frequency=value.QuadPart;
}
RenderMetrics::Scope::Scope(RenderMetrics& metrics,Stage value):owner(metrics),stage(value) {
    if(!owner.enabled)return;
    QueryThreadCycleTime(GetCurrentThread(),&cycles);
    LARGE_INTEGER counter{};QueryPerformanceCounter(&counter);wallTicks=counter.QuadPart;
}
RenderMetrics::Scope::~Scope() {
    if(!owner.enabled)return;
    LARGE_INTEGER counter{};QueryPerformanceCounter(&counter);
    uint64_t endCycles=0;QueryThreadCycleTime(GetCurrentThread(),&endCycles);
    auto& sample=owner.samples[static_cast<size_t>(stage)];
    ++sample.calls;sample.cycles+=endCycles-cycles;sample.wallTicks+=counter.QuadPart-wallTicks;
}
const std::array<RenderMetrics::Sample,static_cast<size_t>(RenderMetrics::Stage::Count)>& RenderMetrics::Samples()const{return samples;}
double RenderMetrics::Milliseconds(uint64_t ticks)const{return ticks*1000.0/frequency;}
