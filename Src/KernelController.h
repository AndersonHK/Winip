#pragma once
#include "Notebook.h"
#include <functional>
#include <memory>

struct KernelEvent {
    std::wstring status;
    uint64_t generation = 0;
    uint64_t cellId = 0;
    Cell result;
};

// Owns its worker, bridge pipes and process job. No Python or pipe IO on UI.
class KernelController {
public:
    using Notify = std::function<void(KernelEvent)>;
    explicit KernelController(Notify notify);
    ~KernelController();
    KernelController(const KernelController&) = delete;
    KernelController& operator=(const KernelController&) = delete;
    bool Submit(std::shared_ptr<const Notebook> document, size_t selected, bool all,
                std::wstring python, std::wstring bridge, std::wstring runtime, std::wstring cwd);
    void Stop();
    bool Interrupt();
    bool Restart();
    bool Connected() const;
    bool Busy() const;
    uint64_t Generation() const;
    uint64_t CpuTicks() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
