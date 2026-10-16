#pragma once
#include "framework.h"
#include <array>
#include "../ThirdParty/nlohmann/json.hpp"
// Opt-in, thread-local diagnostics for opening a notebook. Stages can be nested.
class LoadMetrics {
public:
    enum Stage { FileRead, JsonParse, Model, ImageTotal, ImageDecode, MarkdownPrepare, TextLayout, NotebookLayout, Count };
    LoadMetrics();
    ~LoadMetrics();
    LoadMetrics(const LoadMetrics&)=delete;
    LoadMetrics& operator=(const LoadMetrics&)=delete;
    nlohmann::json Report() const;
    class Scope {
    public:
        explicit Scope(Stage stage);
        ~Scope();
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
    private:
        LoadMetrics* owner;
        Stage stage;
        uint64_t start=0,cycles=0;
    };
private:
    struct Sample {uint64_t calls=0,ticks=0,cycles=0;};
    std::array<Sample,Count> samples{};
    LoadMetrics* previous;
    static thread_local LoadMetrics* active;
};
