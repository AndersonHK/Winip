#include "KernelSetupTests.h"
#include "../Src/framework.h"
#include "../Src/ChildProcess.h"
#include "../Src/KernelController.h"
#include "../Src/KernelFooter.h"
#include "../Src/PythonSetupPlan.h"
#include "../Src/SetupWindowStyle.h"
#include "../Src/SetupWindowOwner.h"
#include "../Src/Menus.h"
#include "../Src/TextLayoutCache.h"
#include "../Src/FontSet.h"
#include "../Src/RenderMetrics.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <fstream>
#include <stdexcept>

namespace {
void Require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
}

void RunKernelConnectionTest(const std::filesystem::path& root, const std::wstring& python) {
    std::mutex mutex; std::condition_variable wake;
    bool finished = false; std::wstring status;
    KernelController kernel([&](KernelEvent event) {
        if(event.failed || event.status == L"Kernel idle") {
            std::lock_guard<std::mutex> lock(mutex);
            status = event.status; finished = true; wake.notify_one();
        }
    });
    const auto folder = root / L"Artifacts/kernel-setup-tests/real-connection";
    std::filesystem::create_directories(folder);
    Require(kernel.Submit(nullptr, 0, false, python, (root / L"Runtime/kernel_bridge.py").wstring(),
        folder.wstring(), folder.wstring()), "Real connection was rejected");
    {
        std::unique_lock<std::mutex> lock(mutex);
        Require(wake.wait_for(lock, std::chrono::seconds(60), [&] { return finished; }), "Real connection timed out");
        if(status != L"Kernel idle") throw std::runtime_error(Utf8(status));
        finished = false;
    }
    const auto previous = kernel.Generation();
    Require(kernel.Connected() && kernel.Restart(), "Ready kernel did not allow restart");
    {
        std::unique_lock<std::mutex> lock(mutex);
        Require(wake.wait_for(lock, std::chrono::seconds(60), [&] { return finished; }), "Real restart timed out");
        if(status != L"Kernel idle") throw std::runtime_error(Utf8(status));
    }
    Require(kernel.Connected() && kernel.Generation() > previous, "Restart did not establish a new session");
    kernel.Stop();
    Require(!kernel.Connected() && !kernel.Restart(), "Stopped kernel retained restart availability");
}

void RunKernelSetupTests(const std::filesystem::path& root, const std::wstring& python, const std::wstring& self) {
    // Presentation doubles: no desktop windows or focus changes in model tests.
    const auto installer=reinterpret_cast<HWND>(uintptr_t(1));
    const auto unrelated=reinterpret_cast<HWND>(uintptr_t(2));
    const auto otherSession=reinterpret_cast<HWND>(uintptr_t(3));
    std::vector<SetupOwnerCandidate> owners{
        {installer,42,L"MsiDialogCloseClass",L"Winip Setup",true,true},
        {unrelated,77,L"OtherWindow",L"Winip Setup",true,false}};
    Require(SetupWindowOwner::SelectInstaller(owners,L"Winip Setup",123)==installer,"MSI owner rejected because the custom-action process differs");
    owners[0].foreground=false;owners[1].foreground=true;
    Require(SetupWindowOwner::SelectInstaller(owners,L"Winip Setup")==installer,"Unrelated foreground window hid the real MSI owner");
    owners.push_back({otherSession,80,L"MsiDialogCloseClass",L"Winip Setup",true,false});
    Require(!SetupWindowOwner::SelectInstaller(owners,L"Winip Setup"),"Ambiguous installer session selected arbitrarily");
    Require(SetupWindowOwner::SelectInstaller(owners,L"Winip Setup",42)==installer,"Client-process hint failed to select its installer");
    owners[2].title=L"Other product Setup";
    Require(SetupWindowOwner::SelectInstaller(owners,L"Winip Setup")==installer,"Another product captured setup ownership");
    owners[0].visible=false;
    Require(!SetupWindowOwner::SelectInstaller(owners,L"Winip Setup"),"Hidden installer selected as modal owner");
    PythonIdentity setup;
    Require(PythonSetupPlan::Stage(setup)==PythonSetupStage::Python,"Missing Python did not enter guided setup");
    setup.executable=L"fixture/python.exe";setup.ensurePipAvailable=true;setup.missingPackages={L"ipykernel"};
    Require(PythonSetupPlan::Stage(setup)==PythonSetupStage::Pip,"Missing pip was not its own setup step");
    setup.pipAvailable=true;
    Require(PythonSetupPlan::Stage(setup)==PythonSetupStage::Kernel,"pip-ready environment did not offer Jupyter installation");
    setup.packageError=L"ipykernel: No module named IPython";setup.missingPackages={L"ipython"};
    Require(PythonSetupPlan::Stage(setup)==PythonSetupStage::Kernel,"A cascading import error hid the confirmed missing package");
    setup.packageError=L"Broken DLL import";setup.missingPackages.clear();
    Require(PythonSetupPlan::Stage(setup)==PythonSetupStage::Repair,"Broken imports were mislabeled as missing packages");
    setup.packageError.clear();setup.kernelReady=true;setup.pipAvailable=false;
    Require(PythonSetupPlan::Stage(setup)==PythonSetupStage::Ready,"Working kernel unnecessarily required pip");
    Require(!PythonSetupPlan::CanInstall(PythonSetupStage::Ready) && !PythonSetupPlan::CanInstall(PythonSetupStage::Repair),"Installation offered for ready/broken states");
    const auto centered=SetupWindowStyle::Centered({200,100,1400,900},{0,0,1920,1080},790,640);
    Require(centered.left==405 && centered.top==180,"Setup was not centered over its owner");
    const auto secondary=SetupWindowStyle::Centered({-1800,100,-200,1000},{-1920,0,0,1080},1580,1280);
    Require(secondary.left==-1790 && secondary.top==0 && secondary.bottom==1080,"High-DPI secondary-monitor centering/clamping failed");
    auto wide = KernelFooter::Allocate(1000, 230, 400, 210);
    Require(KernelFooter::Label(L"Kernel idle",L"3.9.7",true)==L"Kernel Idle — Python 3.9.7","Active kernel label lost its Python version or separator");
    Require(KernelFooter::Label(L"Kernel busy",L"3.9.7",true)==L"Kernel Busy — Python 3.9.7","Busy kernel version is missing");
    for(const auto* status:{L"Kernel stopped",L"Kernel disconnected",L"Kernel not started"}) {
        Require(KernelFooter::Label(status,L"3.9.7",false).find(L"Python")==std::wstring::npos,"Inactive kernel displays a Python version");
        Require(KernelFooter::Label(status,L"3.9.7",true).find(L"Python")==std::wstring::npos,"Inactive status leaked a version during a state transition");
        Require(KernelFooter::CanConnect(status,false,false),"Inactive kernel column cannot connect");
        Require(!KernelFooter::CanConnect(status,false,true) && !KernelFooter::CanConnect(status,true,false),"Column permits duplicate starts or restarting a live kernel");
    }
    for(const auto* status:{L"Kernel idle",L"Kernel busy",L"Kernel starting…",L"Checking Python…",L"Kernel failed",L"Kernel setup required"})
        Require(!KernelFooter::CanConnect(status,false,false),"Non-inactive status gained a connection shortcut");
    const KernelFooterRegion region{18,572,248,600,true};
    Require(KernelFooter::HitConnect(region,18,572) && KernelFooter::HitConnect(region,247,599),"Kernel column endpoints are not clickable");
    Require(!KernelFooter::HitConnect(region,248,580) && !KernelFooter::HitConnect(region,17,580) && !KernelFooter::HitConnect(region,30,571),"Neighboring status/error columns trigger connection");
    auto medium = KernelFooter::Allocate(700, 230, 400, 210);
    auto narrow = KernelFooter::Allocate(380, 230, 400, 210);
    Require(wide.kernel == 230 && wide.document == 400 && wide.renderer == 210, "Wide footer allocation failed");
    Require(medium.document == 400 && !medium.renderer, "Renderer must disappear first");
    Require(narrow.kernel == 230 && !narrow.document && !narrow.renderer, "Kernel state lost narrow-width priority");
    for (bool connected : {false, true}) for (bool busy : {false, true}) {
        const auto menu = Menus::Items(5, true, false, false, L"Python", false, false, true, connected, busy);
        for (const auto& item : menu) {
            if (item.command == Menus::RestartKernel) Require(item.enabled == connected, "Restart offered without a live kernel");
            if (item.command == Menus::InterruptKernel) Require(item.enabled == (connected && busy), "Interrupt state is wrong");
            if (item.command == Menus::StopKernel) Require(item.enabled == (connected || busy), "Stop state is wrong");
        }
    }
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    Require(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.GetAddressOf()))), "Cannot create text shaper");
    FontSet fonts(factory.Get()); RenderMetrics metrics;
    TextLayoutCache layouts(factory.Get(), fonts.Collection(), metrics);
    Require(std::wstring(Menus::Names[Menus::SettingsMenu-1])==L"Widgets" && std::wstring(Menus::Names[Menus::SettingsMenu+1])==L"Help","Settings menu order is wrong");
    const auto settings=Menus::Items(Menus::SettingsMenu,true,false,false,L"Python");
    Require(settings.size()==3 && settings[0].command==Menus::ToggleHdr && settings[2].command==Menus::ChooseOpenFolder,"Dedicated Settings dropdown lost its controls");
    for(const auto& item:Menus::Items(Menus::FileMenu,true,false,false,L"Python"))Require(item.command!=Menus::Settings,"Settings remained duplicated in File");
    for(int width:{404,420,640,1200})for(int menu=0;menu<Menus::MenuCount;++menu) {
        const int left=Menus::Left(menu,width),right=Menus::Left(menu+1,width);
        Require(Menus::Hit((left+right)/2,width)==menu,"Menu label and hit testing disagree");
        auto label=layouts.Get(Menus::Names[menu],10000,float(Menus::TextSize(width)),false,false,false);
        DWRITE_TEXT_METRICS labelMetrics{};label->GetMetrics(&labelMetrics);
        if(labelMetrics.width>right-left-8)throw std::runtime_error("Menu label clipped: "+Utf8(Menus::Names[menu])+" at "+std::to_string(width)+" DIPs, text="+std::to_string(labelMetrics.width)+", available="+std::to_string(right-left-8));
    }
    const std::wstring error = L"Kernel failed: ModuleNotFoundError: No module named 'ipykernel'. Open Python setup to install the missing support. 日本語";
    auto narrowLayout = layouts.Get(error, 360, 13, false, false, false, false, true);
    auto large = layouts.Get(error, 1400, 13, false, false, false, false, true);
    DWRITE_TEXT_METRICS smallMetrics{}, largeMetrics{};
    narrowLayout->GetMetrics(&smallMetrics); large->GetMetrics(&largeMetrics);
    Require(smallMetrics.height > largeMetrics.height && smallMetrics.width <= 360.1f, "Footer error did not wrap");
    DWRITE_TRIMMING trimming{};Microsoft::WRL::ComPtr<IDWriteInlineObject> sign;
    narrowLayout->GetTrimming(&trimming, &sign);
    Require(trimming.granularity == DWRITE_TRIMMING_GRANULARITY_NONE, "Full error was trimmed");
    Require(narrowLayout.Get() == layouts.Get(error, 360, 13, false, false, false, false, true).Get(), "Wrapped layout cache did not reuse geometry");

    std::atomic<bool> cancel{false};
    const std::vector<std::wstring> arguments{L"--setup-child", L"space path 日本語", L"quote\"inside", L"trailing\\", L""};
    auto echoed = ChildProcess::Run(self, arguments, cancel);
    Require(echoed.exitCode == 0 && echoed.error.empty(), "Hidden subprocess failed");
    const auto values = nlohmann::json::parse(echoed.output);
    for (size_t i = 1; i < arguments.size(); ++i) Require(values.at(i - 1) == Utf8(arguments[i]), "Argument quoting changed data");
    auto timed = ChildProcess::Run(self, {L"--setup-sleep"}, cancel, 150);
    Require(timed.error.find(L"timed out") != std::wstring::npos, "Hidden subprocess timeout failed");
    std::thread cancellation([&] { std::this_thread::sleep_for(std::chrono::milliseconds(100)); cancel = true; });
    auto cancelled = ChildProcess::Run(self, {L"--setup-sleep"}, cancel, 5000);
    cancellation.join();
    Require(cancelled.error.find(L"cancelled") != std::wstring::npos, "Hidden subprocess cancellation failed");

    const auto folder = root / L"Artifacts/kernel-setup-tests";
    std::filesystem::create_directories(folder);
    const auto bridge = folder / L"failure_bridge.py";
    std::ofstream(bridge) << "raise RuntimeError('specific early startup diagnostic')\n";
    std::mutex mutex;std::condition_variable wake;KernelEvent final;bool completed = false;
    KernelController kernel([&](KernelEvent event) {
        if (event.failed) { std::lock_guard<std::mutex> lock(mutex); final = std::move(event); completed = true; wake.notify_one(); }
    });
    Require(!kernel.Restart(), "Disconnected kernel restarted");
    Require(kernel.Submit(nullptr, 0, false, python, bridge.wstring(), (folder / L"runtime").wstring(), folder.wstring()), "Startup test rejected");
    std::unique_lock<std::mutex> lock(mutex);
    Require(wake.wait_for(lock, std::chrono::seconds(15), [&] { return completed; }), "Startup failure did not complete");
    Require(final.startupFailure && final.status.find(L"specific early startup diagnostic") != std::wstring::npos, "Startup stderr diagnosis was lost");
    lock.unlock();
    Require(!kernel.Connected() && !kernel.Restart(), "Failed startup was treated as restartable");
}
