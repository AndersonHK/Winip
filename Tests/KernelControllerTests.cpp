#include "../Src/KernelController.h"
#include "../Src/Renderer.h"
#include "../Src/CellEditor.h"
#include "../Src/CellOutput.h"
#include <fstream>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <iostream>
#include <stdexcept>
#include <thread>

void RunKernelControllerTests(const std::filesystem::path& root,const std::wstring& python) {
    std::mutex mutex;std::condition_variable wake;std::vector<KernelEvent> events;
    KernelController kernel([&](KernelEvent event){std::lock_guard<std::mutex> lock(mutex);events.push_back(std::move(event));wake.notify_one();});
    auto make=[](const std::vector<std::string>& sources) {
        auto cells=nlohmann::json::array();
        for(const auto& source:sources)cells.push_back({{"cell_type","code"},{"source",source},{"execution_count",nullptr},{"outputs",nlohmann::json::array()},{"metadata",{{"keep",true}}}});
        auto doc=Notebook::FromJson({{"nbformat",4},{"cells",cells}},L"Kernel tests");PrepareNotebookLayout(doc);
        return std::make_shared<const Notebook>(doc);
    };
    std::wstring workingDirectory=(root/L"Artifacts").wstring();
    auto run=[&](std::shared_ptr<const Notebook> doc,bool all=true) {
        {std::lock_guard<std::mutex> lock(mutex);events.clear();}
        if(!kernel.Submit(doc,0,all,python,(root/L"Runtime/kernel_bridge.py").wstring(),
            (root/L"Artifacts/kernel-controller/runtime").wstring(),workingDirectory))throw std::runtime_error("Kernel rejected idle request");
        std::unique_lock<std::mutex> lock(mutex);
        if(!wake.wait_for(lock,std::chrono::seconds(40),[&]{return !events.empty() && (events.back().status==L"Kernel idle" || events.back().status.find(L"Kernel error:")==0);}))
            throw std::runtime_error("Kernel test timed out");
        if(events.back().status!=L"Kernel idle")throw std::runtime_error(Utf8(events.back().status));
        std::vector<KernelEvent> results;
        for(const auto& event:events)if(event.cellId)results.push_back(event);
        return results;
    };
    auto doc=make({"print('Hello, world!')\nvalue = 41", "print(value + 1)", "import sys; print(sys.executable)"});
    auto results=run(doc);
    if(results.size()!=3 || results[0].result.text->output!=L"Hello, world!\n" || results[1].result.text->output!=L"42\n" ||
       results[2].result.text->output.find(python)==std::wstring::npos)throw std::runtime_error("Hello World, persistent state or interpreter identity failed");
    CellEditor editor;editor.Attach(doc);editor.Begin(0,0,0);
    editor.ApplyExecution(results[0].cellId,results[0].result);
    auto saved=editor.Document()->Serialize();
    if(!editor.Active() || saved["cells"][0]["outputs"][0]["text"]!="Hello, world!\n" || saved["cells"][0]["metadata"]["keep"]!=true)
        throw std::runtime_error("Execution output did not preserve editing/metadata");
    auto deleted=doc->Delete(0);auto late=deleted.WithExecution(results[0].cellId,results[0].result);
    if(late.ContentRevision()!=deleted.ContentRevision())throw std::runtime_error("Late output targeted a deleted cell");
    editor.SelectAll();editor.InsertText(L"print('new source')");auto before=editor.Document()->ContentRevision();
    editor.ApplyExecution(results[0].cellId,results[0].result);
    if(editor.Document()->ContentRevision()!=before)throw std::runtime_error("Late output attached to changed source");
    // Saved counts are display history, never input to the kernel counter.
    if(results[0].result.text->prompt!=L"In [1]:")throw std::runtime_error("Fresh kernel did not start at one");
    results=run(make({"raise ValueError('expected')", "print('must not run')"}));
    if(results.size()!=1 || results[0].result.text->output.find(L"ValueError: expected")==std::wstring::npos)
        throw std::runtime_error("Run-all error policy failed");
    results=run(make({"print(value)"}));
    if(results[0].result.text->output!=L"41\n")throw std::runtime_error("Ordinary Python error destroyed kernel state");
    results=run(make({"print('日本語 😀')\nimport sys; print('stderr', file=sys.stderr)\n6*7"}));
    if(results[0].result.text->output.find(L"日本語 😀")==std::wstring::npos || results[0].result.text->output.find(L"42")==std::wstring::npos)
        throw std::runtime_error("Unicode/result output failed");
    {std::lock_guard<std::mutex> lock(mutex);events.clear();}
    if(!kernel.Submit(make({"while True:\n    pass", "raise RuntimeError('batch continued after interrupt')"}),0,true,python,
        (root/L"Runtime/kernel_bridge.py").wstring(),(root/L"Artifacts/kernel-controller/runtime").wstring(),(root/L"Artifacts").wstring()))
        throw std::runtime_error("Interrupt setup failed");
    {std::unique_lock<std::mutex> lock(mutex);
        if(!wake.wait_for(lock,std::chrono::seconds(10),[&]{return !events.empty() && events.back().status==L"Kernel busy";}))
            throw std::runtime_error("Interrupt wait failed");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    if(!kernel.Interrupt())throw std::runtime_error("Interrupt signal failed");
    {std::unique_lock<std::mutex> lock(mutex);
        if(!wake.wait_for(lock,std::chrono::seconds(10),[&]{return !events.empty() && events.back().status==L"Kernel idle";}))
            throw std::runtime_error("Interrupt did not return idle; status="+Utf8(events.empty()?L"none":events.back().status));
        size_t count=0;for(const auto& event:events)if(event.cellId) {
            ++count;if(event.result.text->output.find(L"KeyboardInterrupt")==std::wstring::npos)throw std::runtime_error("Missing interrupt output");
        }
        if(count!=1)throw std::runtime_error("Interrupt did not cancel Run all queue");
    }
    results=run(make({"print(value)"}));
    if(results[0].result.text->output!=L"41\n")throw std::runtime_error("Interrupt destroyed namespace");
    {std::lock_guard<std::mutex> lock(mutex);events.clear();}
    if(!kernel.Restart())throw std::runtime_error("Restart rejected");
    {std::unique_lock<std::mutex> lock(mutex);
        if(!wake.wait_for(lock,std::chrono::seconds(40),[&]{return !events.empty() && events.back().status==L"Kernel idle";}))
            throw std::runtime_error("Restart did not connect");
    }
    auto numbered=make({"print('value' in globals())"})->Serialize();numbered["cells"][0]["execution_count"]=500;
    auto fresh=Notebook::FromJson(numbered,L"Saved counts");PrepareNotebookLayout(fresh);
    results=run(std::make_shared<const Notebook>(fresh));
    if(results[0].result.text->prompt!=L"In [1]:" || results[0].result.text->output!=L"False\n")
        throw std::runtime_error("Restart retained namespace or saved execution counter");
    {std::lock_guard<std::mutex> lock(mutex);events.clear();}
    if(!kernel.Submit(make({"import time; time.sleep(60)"}),0,false,python,(root/L"Runtime/kernel_bridge.py").wstring(),
        (root/L"Artifacts/kernel-controller/runtime").wstring(),(root/L"Artifacts").wstring()))throw std::runtime_error("Busy-stop setup failed");
    {
        std::unique_lock<std::mutex> lock(mutex);
        if(!wake.wait_for(lock,std::chrono::seconds(10),[&]{return !events.empty() && events.back().status==L"Kernel busy";}))
            throw std::runtime_error("Kernel did not become busy");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    kernel.Stop();
    if(kernel.Restart())throw std::runtime_error("Stopped kernel retained stale interpreter/working-directory configuration");
    {
        std::unique_lock<std::mutex> lock(mutex);
        if(!wake.wait_for(lock,std::chrono::seconds(10),[&]{return !events.empty() && events.back().status==L"Kernel stopped";}))
            throw std::runtime_error("Force stop did not release execution worker");
    }
    results=run(make({"print('fresh session')"}));
    if(results[0].result.text->prompt!=L"In [1]:")throw std::runtime_error("Reconnect retained old kernel execution state");
    results=run(make({"from PIL import Image\nfrom IPython.display import display\nprint('before')\ndisplay(Image.new('RGBA', (80, 40), (120, 50, 200, 128)))\nprint('after')"}));
    auto rich=results[0].result.output;
    if(!rich || rich->Blocks().size()!=3 || !rich->Blocks()[1].image || rich->Blocks()[1].image->Width()!=80 || rich->Blocks()[2].text!=L"after\n")
        throw std::runtime_error("Live Pillow output ordering failed");
    results=run(make({"from PIL import Image\nfrom IPython.display import display, Image as DisplayImage\nimport io\nbuffer = io.BytesIO()\nImage.new('RGB', (100, 60), (30, 100, 220)).save(buffer, format='JPEG')\ndisplay(DisplayImage(data=buffer.getvalue(), format='jpeg', width=50))"}));
    rich=results[0].result.output;
    if(!rich || !rich->Blocks()[0].image || rich->Blocks()[0].width!=50 || rich->Blocks()[0].height!=30)
        throw std::runtime_error("Live JPEG output metadata failed");
    std::ofstream(root/L"Artifacts/kernel-image-output.json")<<results[0].result.original->dump(2);
    results=run(make({"%matplotlib inline\nimport matplotlib.pyplot as plt\nplt.plot([0,1,2],[0,1,4], label='quadratic')\nplt.title('Winip graph test')\nplt.legend()\nplt.show()"}));
    rich=results[0].result.output;
    if(!rich || rich->Blocks().empty() || !rich->Blocks()[0].image)
        throw std::runtime_error("Matplotlib inline graph failed: "+Utf8(results[0].result.text->output));
    std::ofstream(root/L"Artifacts/kernel-matplotlib-output.json")<<results[0].result.original->dump(2);
    wchar_t localModule[32768]{};
    if(GetEnvironmentVariableW(L"WINIP_TEST_LOCAL_MODULE",localModule,32768)) {
        workingDirectory=std::filesystem::path(localModule).parent_path().wstring();
        kernel.Stop();
        results=run(make({"from modding_tools import *\nimport modding_tools\nprint(modding_tools.__file__)\nprint(callable(load_dat_file), callable(save_custom_dat_file), callable(create_table_classes))"}));
        if(results[0].result.text->output.find(localModule)==std::wstring::npos || results[0].result.text->output.find(L"True True True")==std::wstring::npos)
            throw std::runtime_error("User notebook local-library import failed");
        std::cout<<"User local module import passed; no load, mutation or save cell executed\n";
    }
    std::cout<<"Real kernel: Hello World, persistent state, run-all/error, Unicode, identity and stale-output tests passed\n";
}
