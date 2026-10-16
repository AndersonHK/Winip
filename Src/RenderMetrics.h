#pragma once
#include <array>
#include <cstdint>

// Render-thread-only diagnostics. Disabled in ordinary launches; no polling timer.
class RenderMetrics {
public:
    enum class Stage { Display, Resize, Layout, EndDraw, Present, Commit, Count };
    struct Sample { uint64_t calls=0, cycles=0, wallTicks=0; };
    class Scope {
    public:
        Scope(RenderMetrics& owner, Stage stage);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        RenderMetrics& owner;
        Stage stage;
        uint64_t cycles=0, wallTicks=0;
    };
    explicit RenderMetrics(bool enabled=false);
    const std::array<Sample, static_cast<size_t>(Stage::Count)>& Samples() const;
    double Milliseconds(uint64_t ticks) const;
private:
    bool enabled;
    uint64_t frequency=1;
    std::array<Sample, static_cast<size_t>(Stage::Count)> samples{};
};
