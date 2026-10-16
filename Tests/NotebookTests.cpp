#include "TextRasterCacheTests.h"
#include "NotebookIoBenchmark.h"
#include "NotebookFlowTests.h"
#include "../Src/LoadMetrics.h"
#include "../Src/ImageUpload.h"
#include "../Src/NotebookFlow.h"
#include "../Src/framework.h"
#include "../Src/Notebook.h"
#include "../Src/RenderPolicy.h"
#include "../Src/RasterContract.h"
#include "../Src/Renderer.h"
#include "../Src/TitleEditor.h"
#include "../Src/CellEditor.h"
#include "../Src/NotebookStorage.h"
#include "../Src/NotebookShortcuts.h"
#include "../Src/Markdown.h"
#include "../Src/FontSet.h"
#include "../Src/Menus.h"
#include "../Src/NotebookToolbar.h"
#include "../Src/CellScrollbar.h"
#include "../Src/NotebookNavigator.h"
#include "../Src/AppSettings.h"
#include "../Src/CellOutput.h"
#include "../Src/PythonDiscovery.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <chrono>

namespace {
void Require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
void Reject(const std::filesystem::path& path) {
    bool rejected = false;
    try { LoadNotebook(path.wstring()); } catch (const std::exception&) { rejected = true; }
    Require(rejected, "Invalid notebook was accepted");
}
}
void RunKernelControllerTests(const std::filesystem::path&,const std::wstring&);
int wmain(int argc, wchar_t** argv) {
    try {
        if(argc==3 && std::wstring(argv[1])==L"--inspect-image-layout") {
            auto notebook=Notebook::Load(argv[2]);PrepareNotebookLayout(notebook);
            auto report=nlohmann::json::array();
            for(const auto& cell:notebook.Cells())if(cell.markdown && !cell.markdown->Images().empty()) {
                auto views=nlohmann::json::array();
                ExpandedImages expanded;
                for(const auto& image:cell.markdown->Images())expanded.insert(image.image);
                for(float width:{450.f,900.f,1400.f}) {
                    const auto fit=cell.markdown->Reflow(width,650,{});
                    const auto full=cell.markdown->Reflow(width,650,expanded);
                    auto images=nlohmann::json::array();
                    for(const auto& image:fit->Images())images.push_back({{"y",image.y},{"width",image.width},{"height",image.height},
                        {"source_height",image.image->Height()}});
                    views.push_back({{"width",width},{"fit_height",fit->Height()},{"full_height",full->Height()},{"images",images}});
                }
                report.push_back({{"cell",size_t(&cell-notebook.Cells().data())},{"views",views}});
            }
            std::cout<<report.dump(2)<<"\n";return 0;
        }
        if(argc==3 && (std::wstring(argv[1])==L"--load-benchmark" || std::wstring(argv[1])==L"--gpu-load-benchmark")) {
            LoadMetrics metrics;const auto start=std::chrono::steady_clock::now();
            auto notebook=Notebook::Load(argv[2]);PrepareNotebookLayout(notebook);
            size_t markdownImages=0,codeImages=0;uint64_t decoded=0;
            for(const auto& cell:notebook.Cells()) {
                if(cell.markdown)for(const auto& image:cell.markdown->Images()){++markdownImages;decoded+=image.image->Pixels().size();}
                if(cell.output)for(const auto& block:cell.output->Blocks())if(block.image){++codeImages;decoded+=block.image->Pixels().size();}
            }
            auto report=metrics.Report();
            if(std::wstring(argv[1])==L"--gpu-load-benchmark") {
                NotebookFlow flow;
                notebook=*flow.Prepare(std::make_shared<const Notebook>(notebook),1600,900,{},0);
                Microsoft::WRL::ComPtr<ID3D11Device> device;
                const auto created=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr);
                Require(SUCCEEDED(created),"Hardware upload benchmark device unavailable");
                const auto gpuStart=std::chrono::steady_clock::now();
                ImageUpload uploads(device.Get(),notebook,{});
                report["gpu_image_prepare_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-gpuStart).count();
                report["gpu_images"]=uploads.Count();report["gpu_bytes"]=uploads.Bytes();
            }
            report["total_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            report["cells"]=notebook.Cells().size();report["markdown_images"]=markdownImages;report["code_images"]=codeImages;report["decoded_bytes"]=decoded;report["cells_executed"]=false;
            std::cout<<report.dump(2)<<"\n";return 0;
        }
        if(argc==3 && std::wstring(argv[1])==L"--io-benchmark") {NotebookIoBenchmark::Run(argv[2]);return 0;}
        if (argc==3 && (std::wstring(argv[1])==L"--load-notebook" || std::wstring(argv[1])==L"--roundtrip-notebook")) {
            const auto start=std::chrono::steady_clock::now();
            auto loaded=Notebook::Load(argv[2]);
            PrepareNotebookLayout(loaded);
            size_t images=0;uint64_t decoded=0;
            for(const auto& cell:loaded.Cells())if(cell.output)for(const auto& block:cell.output->Blocks())if(block.image){++images;decoded+=block.image->Pixels().size();}
            if(std::wstring(argv[1])==L"--roundtrip-notebook") {
                auto destination=std::wstring(argv[2])+L".roundtrip.ipynb";
                NotebookStorage::Save(destination,loaded,nullptr);
                Require(loaded.SameContent(Notebook::Load(destination)),"Large notebook roundtrip changed content");
            }
            const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            std::cout<<"Loaded and measured "<<loaded.Cells().size()<<" cells, "<<images<<" images, "<<decoded<<" decoded bytes in "<<seconds<<" seconds. No cells executed.\n";
            return 0;
        }
        Require(argc == 2, "Expected repository root");
        const std::filesystem::path root = argv[1];
        auto fixtures = root / "Tests/Fixtures";
        RunNotebookFlowTests(root);
        auto imageNotebook=Notebook::Load((fixtures/"images.ipynb").wstring());PrepareNotebookLayout(imageNotebook);
        auto image=imageNotebook.Cells()[0].output;
        Require(image && image->Blocks().size()==1 && image->Blocks()[0].image->Width()==512 && image->Height()>=256,"Image output layout failed");
        auto imageJson=imageNotebook.Serialize();imageJson["cells"][0]["outputs"][0]["metadata"]["image/png"]={{"width",256}};
        auto retina=Notebook::FromJson(imageJson,L"Retina");PrepareNotebookLayout(retina);
        Require(retina.Cells()[0].output->Blocks()[0].width==256 && retina.Cells()[0].output->Blocks()[0].height==128 && retina.Cells()[0].output->Blocks()[0].image->Width()==512,"Retina metadata downscaled source decode");
        imageJson["cells"][0]["outputs"][0]["metadata"]["image/png"]={{"height",16384}};
        auto excessive=Notebook::FromJson(imageJson,L"Excessive dimensions");
        Require(!excessive.Cells()[0].output->Blocks()[0].image && excessive.Serialize()==imageJson,"Derived image dimension escaped the layout bound");
        imageJson["cells"][0]["outputs"][0]["data"]["image/png"]="broken base64";
        auto broken=Notebook::FromJson(imageJson,L"Broken image");PrepareNotebookLayout(broken);
        Require(!broken.Cells()[0].output->Blocks()[0].image && broken.Serialize()==imageJson,"Malformed image lost original output");

        auto settingsRoot=root/"Artifacts/settings-tests";std::filesystem::create_directories(settingsRoot);
        std::filesystem::remove(settingsRoot/"settings.json");
        std::wstring downloads;
        {AppSettings settings(settingsRoot);
            Require(!settings.HdrEnabled() && settings.UseLastFolder(),"Settings defaults failed");
            downloads=settings.OpenFolder();Require(!downloads.empty(),"Downloads default missing");
            settings.SetHdrEnabled(true);settings.SetFixedFolder(root.wstring());settings.SetUseLastFolder(false);
            settings.RememberNotebook((root/"Tests/Fixtures/hello-world.ipynb").wstring());
        }
        {AppSettings settings(settingsRoot);
            Require(settings.HdrEnabled() && !settings.UseLastFolder() && settings.OpenFolder()==root.wstring(),"Fixed settings roundtrip failed");
            settings.SetUseLastFolder(true);
            Require(std::filesystem::path(settings.OpenFolder()).lexically_normal()==(root/"Tests/Fixtures").lexically_normal(),"Remembered folder lost");
        }
        {std::ofstream file(settingsRoot/"settings.json");file<<"{invalid";}
        {AppSettings settings(settingsRoot);
            Require(!settings.HdrEnabled() && settings.UseLastFolder() && !settings.Error().empty(),"Invalid settings did not use safe defaults");
            settings.SetHdrEnabled(true);
        }
        {std::ifstream file(settingsRoot/"settings.json.invalid");std::string saved((std::istreambuf_iterator<char>(file)),{});
            Require(saved=="{invalid","Malformed settings were not preserved before replacement");}
        {AppSettings settings(settingsRoot);Require(settings.HdrEnabled() && settings.Error().empty(),"Settings did not recover after explicit change");}

        auto doc = LoadNotebook((fixtures / "visualization.ipynb").wstring());
        std::ostringstream streamed;doc.Write(streamed);
        Require(nlohmann::json::parse(streamed.str())==doc.Serialize(),"Streaming serialization differs from snapshot");
        bool cancelled=false;
        try {Notebook::Load((fixtures/"visualization.ipynb").wstring(),[]{return true;});}catch(const std::exception&){cancelled=true;}
        Require(cancelled,"Notebook input ignored cancellation");
        const auto boundaryPath = settingsRoot / "buffer-boundary.ipynb";
        for (size_t offset = 65528; offset < 65540; ++offset) {
            const std::string prefix = "{\"nbformat\":4,\"cells\":[],\"metadata\":{\"text\":\"";
            const auto bytes = prefix + std::string(offset - prefix.size(), 'x') + "日本語 😀\\n\\uD83D\\uDE00\"}}";
            { std::ofstream file(boundaryPath, std::ios::binary); file << bytes; }
            Require(Notebook::Load(boundaryPath.wstring()).Original() == nlohmann::json::parse(bytes),
                "UTF-8 or JSON escape corrupted across a refill boundary");
        }
        int refills = 0;
        cancelled = false;
        try { Notebook::Load(boundaryPath.wstring(), [&] { return ++refills == 2; }); }
        catch (const std::exception&) { cancelled = true; }
        Require(cancelled && refills == 2, "Notebook input ignored cancellation after a partial read");
        std::filesystem::remove(boundaryPath);
        Require(doc.Cells().size() == 8, "Missing cells");
        Require(doc.Cells()[1].text->source.find(L"import numpy as np\n") == 0, "Text arrays not concatenated");
        Require(doc.Cells()[1].text->prompt == L"In [1]:", "Execution count lost");
        Require(doc.Cells()[5].text->prompt == L"In [ ]:", "Null execution count lost");
        Require(doc.Cells()[1].text->output == L"Ready for interactive visualization.\n", "Stream output lost");
        Require(doc.Cells()[3].text->output.find(L"<Plotly figure") == 0, "Plain output fallback missing");
        Require(doc.Cells()[5].text->output.find(L"preview is not available") != std::wstring::npos, "Unsupported output is silent");
        Require(doc.Cells()[6].text->source.find(L"日本語 — 😀") != std::wstring::npos, "Unicode corrupted");
        Require(Utf16(Utf8(doc.Cells()[6].text->source))==doc.Cells()[6].text->source,"UTF-8 roundtrip corrupted");
        Require(doc.Cells()[7].text->output.find(L"ZeroDivisionError") != std::wstring::npos, "Error output lost");
        std::ifstream original(fixtures / "visualization.ipynb");
        Require(doc.Original() == nlohmann::json::parse(original), "Unknown notebook content was changed");
        auto measured=doc;
        int measuredWidth=PrepareNotebookLayout(measured);
        Require(measuredWidth>0 && measured.Height()>0,"Font-based notebook layout is empty");
        Require(measured.Original()==doc.Original(),"Layout changed notebook data");
        Require(!doc.HasMeasuredLayout() && measured.HasMeasuredLayout(), "Layout publication changed the source snapshot");
        Notebook empty;
        Require(empty.Cells().empty() && empty.Original().at("cells").empty() && empty.Title()==L"Untitled", "Default notebook is not coherent");
        auto measuredEmpty=empty.WithLayout({});
        Require(measuredEmpty.HasMeasuredLayout() && measuredEmpty.Height()==0 && measuredEmpty.ContentWidth()==0, "Empty measured layout is inconsistent");
        auto shared=measured;
        auto retitled=measured.WithTitle(L"Snapshot title");
        Require(&shared.Cells()==&measured.Cells() && &retitled.Cells()==&measured.Cells(), "Notebook copies or renaming duplicate cell storage");
        Require(&retitled.Original()==&measured.Original() && retitled.Title()!=measured.Title(), "Title change corrupted shared notebook content");
        std::vector<CellLayout> layout;
        for(const auto& cell:measured.Cells())layout.push_back({cell.sourceHeight,cell.outputHeight,cell.contentWidth});
        auto rejectLayout=[&](std::vector<CellLayout> proposed) {
            bool rejected=false;
            try {measured=measured.WithLayout(proposed);} catch(const std::invalid_argument&) {rejected=true;}
            Require(rejected && &measured.Cells()==&shared.Cells() && measured.Height()==shared.Height(), "Rejected layout partially changed its notebook");
        };
        rejectLayout({});
        auto invalidLayout=layout;invalidLayout[0].sourceHeight=-1;rejectLayout(invalidLayout);
        invalidLayout=layout;invalidLayout[0].sourceHeight=INT_MAX;rejectLayout(invalidLayout);
        invalidLayout=layout;invalidLayout[0].outputHeight=22;rejectLayout(invalidLayout);
        invalidLayout=layout;invalidLayout[0].contentWidth=-1;rejectLayout(invalidLayout);
        auto revisedLayout=layout;revisedLayout[0].sourceHeight+=10;
        auto relaid=measured.WithLayout(revisedLayout);
        Require(relaid.Cells()[1].top==measured.Cells()[1].top+10 && relaid.Height()==measured.Height()+10, "Layout replacement did not update following cell positions atomically");
        Require(relaid.Original()==measured.Original() && shared.Cells()[0].sourceHeight==layout[0].sourceHeight, "Layout replacement modified shared source data");
        auto multilineJson = doc.Original();
        multilineJson["cells"] = nlohmann::json::array({{{"cell_type", "markdown"}, {"metadata", nlohmann::json::object()}, {"source", ""}}});
        std::wstring multilineSource;
        for(int i=0;i<100;++i) multilineSource+=L"Åccents, gyp descenders, Ελληνικά, 日本語  \n";
        multilineSource+=L"LAST_LINE_MUST_BE_ACCESSIBLE";
        multilineJson["cells"][0]["source"] = Utf8(multilineSource);
        auto multiline = Notebook::FromJson(std::move(multilineJson), L"Multiline");
        PrepareNotebookLayout(multiline);
        Require(multiline.Cells()[0].sourceHeight>101*22,"Prose still uses hard-coded short line boxes");
        Require(multiline.Cells()[0].height>=multiline.Cells()[0].sourceHeight,"Multiline content exceeds its cell");
        auto large = LoadNotebook((fixtures / "large.ipynb").wstring());
        NotebookShortcuts shortcuts;
        using Shortcut=NotebookShortcuts::Action;
        Require(shortcuts.Key('D',false,false,true,false,100)==Shortcut::None,"Typing D triggered a cell command");
        Require(shortcuts.Key('D',false,false,false,false,200)==Shortcut::None,"One D deleted a cell");
        Require(shortcuts.Key('D',false,false,false,true,250)==Shortcut::None,"Key repeat deleted a cell");
        Require(shortcuts.Key('D',false,false,false,false,300)==Shortcut::DeleteCell,"D,D did not delete");
        Require(shortcuts.Key('A',false,false,false,false,400)==Shortcut::InsertAbove,"A shortcut missing");
        Require(shortcuts.Key('B',false,false,false,false,500)==Shortcut::InsertBelow,"B shortcut missing");
        Require(shortcuts.Key(VK_RETURN,false,false,false,false,600)==Shortcut::EnterEdit,"Enter did not enter edit mode");
        Require(shortcuts.Key(VK_RETURN,false,false,true,false,700)==Shortcut::None,"Plain Enter intercepted a text newline");
        Require(shortcuts.Key('M',true,false,true,false,800)==Shortcut::ExitEdit,"Ctrl+M did not enter command mode");
        Microsoft::WRL::ComPtr<IDWriteFactory> markdownFactory;
        Require(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(markdownFactory.GetAddressOf()))),"Markdown test factory failed");
        FontSet markdownFonts(markdownFactory.Get());
        auto rich=Markdown::Prepare(markdownFactory.Get(),markdownFonts.Collection(),L"# Heading\n\n**Bold** and *italic* with `code` and [link](https://example.com).\n\n- First\n- Second\n\n&#x1F600; &amp; &alpha;\n\n```python\nprint('hello')\n```");
        Require(rich->Text().find(L"**")==std::wstring::npos && rich->Text().find(L"https://")==std::wstring::npos && rich->Text().find(L"• First")!=std::wstring::npos,"Markdown syntax leaked into rich text");
        Require(rich->Text().find(L"😀 & α")!=std::wstring::npos,"Markdown entity decoding failed");
        DWRITE_FONT_WEIGHT weight{};DWRITE_FONT_STYLE style{};FLOAT headingSize=0;
        rich->Layout()->GetFontSize(0,&headingSize);rich->Layout()->GetFontWeight(static_cast<UINT32>(rich->Text().find(L"Bold")),&weight);rich->Layout()->GetFontStyle(static_cast<UINT32>(rich->Text().find(L"italic")),&style);
        Require(headingSize==27 && weight==DWRITE_FONT_WEIGHT_BOLD && style==DWRITE_FONT_STYLE_ITALIC,"Markdown formatting was not applied to DirectWrite runs");
        Require(shortcuts.Key(VK_RETURN,true,true,true,false,900)==Shortcut::RunAll,"Run-all shortcut missing in edit mode");
        for(int width:{420,480,600,760,1200}) {
            int right=0;
            for(const auto& item:NotebookToolbar::Items(width,L"markdown")) {
                Require(item.x>=right && item.x+item.width<=width-18,"Toolbar overlaps or clips at narrow width");
                Require(NotebookToolbar::Hit(item.x+2,width,L"markdown")==item.command,"Toolbar command mismatch");right=item.x+item.width;
            }
        }
        auto compact=Markdown::Prepare(markdownFactory.Get(),markdownFonts.Collection(),L"#test");
        auto spaced=Markdown::Prepare(markdownFactory.Get(),markdownFonts.Collection(),L"# test");
        Require(compact->Text()==spaced->Text() && compact->Height()==spaced->Height(),"Permissive Markdown heading mismatch");
        NotebookNavigator navigator;auto geometry=NotebookNavigator::Geometry(1200,900,10000,0);
        navigator.Begin(geometry.thumbY+5,geometry,100);
        Require(navigator.Drag(geometry.y+geometry.height,geometry)==geometry.maximum,"Vertical end mapping failed");
        Require(navigator.Drag(geometry.y+5,geometry)==0,"Vertical beginning mapping failed");
        Require(navigator.Animating(180) && navigator.Expansion(180)>0 && navigator.Expansion(180)<1,"Navigator animation missing");
        Require(!navigator.Animating(300) && navigator.Expansion(300)==1,"Held navigator does not settle");
        navigator.End(310);Require(navigator.Expansion(510)==0 && !navigator.Held(),"Navigator release does not settle");
        Require(compact->Headings().size()==1 && compact->Headings()[0].text==L"test","Markdown heading index mismatch");
        auto narrow=NotebookNavigator::Geometry(640,800,10000,0);
        Require(!navigator.Visible(narrow),"Narrow navigator is not hidden by default");
        navigator.Hover(narrow.x-100,narrow.y+20,narrow);
        Require(navigator.Visible(narrow),"Narrow navigator did not reveal near longest mark");
        navigator.Hover(10,200,narrow);Require(!navigator.Visible(narrow),"Narrow navigator remained after pointer left");
        Require(!navigator.Visible(NotebookNavigator::Geometry(1200,900,10000,0)),"Navigator overlaps paper at intermediate width");
        Require(!navigator.Visible(NotebookNavigator::Geometry(1284,900,10000,0)),"Navigator ignores paper shadow clearance");
        Require(navigator.Visible(NotebookNavigator::Geometry(1286,900,10000,0)),"Navigator remains hidden when margin fits");
        CellScrollbar scrollbar;
        scrollbar.Begin(123,5,400,4000,0);
        Require(scrollbar.Drag(5,400,4000)==0,"Thumb jumped on initial grab");
        Require(scrollbar.Drag(365,400,4000)==3600,"Drag cannot reach long-line end");
        Require(scrollbar.Drag(-50,400,4000)==0 && scrollbar.Drag(900,400,4000)==3600,"Drag escaped limits");
        scrollbar.End();Require(scrollbar.CellId()==0,"Scrollbar capture was not cleared");
        CellEditor editor;
        auto baseline=std::make_shared<const Notebook>(measured);
        editor.Attach(baseline);
        editor.Begin(1,0,0);
        Require(editor.Active() && editor.Document()->ContentRevision()==baseline->ContentRevision(),"Clicking a cell dirtied its source");
        Require(editor.Document()->Serialize()==baseline->Serialize(),"Clicking a cell rewrote saved representations");
        editor.SelectAll();editor.InsertText(L"value = '日本語 😀'\r\nprint(value)");
        auto changed=editor.Document();
        Require(changed->Cells()[1].text->source==L"value = '日本語 😀'\nprint(value)","Cell edit lost Unicode or newline normalization");
        Require(changed->Cells()[1].id==baseline->Cells()[1].id,"Editing changed cell identity");
        Require(changed->Cells()[2].text==baseline->Cells()[2].text,"Editing copied an unchanged cell's text");
        auto serialized=changed->Serialize();auto expected=baseline->Serialize();expected["cells"][1]["source"]=Utf8(changed->Cells()[1].text->source);
        Require(serialized==expected,"Source editing changed outputs or unknown fields");
        editor.Undo();Require(editor.Document()->Serialize()==baseline->Serialize(),"Undo failed to restore source");
        editor.Redo();Require(editor.Document()->Serialize()==serialized,"Redo failed");
        editor.InsertCell(1,L"markdown");editor.InsertText(L"# Added markdown\n\nText");
        Require(editor.Document()->Cells().size()==baseline->Cells().size()+1 && editor.Document()->Cells()[2].id==baseline->Cells()[1].id,"Insertion redirected existing cell identities");
        Require(editor.Document()->Serialize()["cells"][1]["cell_type"]=="markdown","Inserted Markdown has wrong kind");
        editor.SelectAll();editor.InsertText(L"# Restored heading");auto headingRevision=editor.Document()->ContentRevision();
        editor.End();Require(!editor.Document()->Cells()[1].sourceMode && editor.Document()->ContentRevision()==headingRevision,"Leaving Markdown editing changed content or kept source presentation");
        editor.Begin(1,0,0);
        editor.SelectAll();editor.InsertText(L"A😀B");editor.Move(VK_HOME,false,true);editor.Move(VK_RIGHT,false,false);editor.Erase(false);
        Require(editor.Document()->Cells()[1].text->source==L"AB","Cell deletion split a surrogate pair");
        editor.SelectAll();editor.InsertText(std::wstring(2000,L'x'));auto longWidth=editor.Document()->Cells()[1].contentWidth;
        editor.SelectAll();editor.InsertText(L"short");Require(editor.Document()->Cells()[1].contentWidth<longWidth,"Shortened cell retained obsolete horizontal extent");
        editor.Retitle(L"Saved title");editor.Undo();Require(editor.Document()->Title()==L"Saved title","Undo restored an obsolete filename");
        auto savePath=root/L"artifacts"/(L"editor-save-"+std::to_wstring(GetCurrentProcessId())+L".ipynb");
        NotebookStorage::Save(savePath.wstring(),*baseline,nullptr);
        NotebookStorage::Save(savePath.wstring(),*changed,baseline.get());
        Require(Notebook::Load(savePath.wstring()).Serialize()==changed->Serialize(),"Save/reopen lost notebook data");
        bool conflict=false;try{NotebookStorage::Save(savePath.wstring(),*baseline,baseline.get());}catch(const std::exception&){conflict=true;}
        Require(conflict && Notebook::Load(savePath.wstring()).Serialize()==changed->Serialize(),"Save overwrote an externally changed file");
        bool overwrite=false;try{NotebookStorage::Save(savePath.wstring(),*baseline,nullptr);}catch(const std::exception&){overwrite=true;}
        Require(overwrite,"Save As overwrote an existing path");
        std::filesystem::remove(savePath);
        editor.Attach(std::make_shared<const Notebook>(Notebook().WithLayout({})));editor.InsertCell(0,L"code");editor.InsertText(L"answer = 42");
        Require(editor.Document()->Serialize()["cells"][0]["outputs"].is_array(),"New code cell lacks outputs array");
        auto beforeDelete=editor.Document();editor.DeleteCell();Require(editor.Document()->Cells().empty(),"Cannot delete the last cell");
        editor.Undo();Require(editor.Document()->Serialize()==beforeDelete->Serialize(),"Undo did not restore deleted cell data");
        editor.ChangeKind(L"markdown");Require(editor.Document()->Cells()[0].text->kind==L"markdown" && editor.Document()->Cells()[0].markdown,"M conversion did not create a Markdown preview");
        editor.Undo();Require(editor.Document()->Serialize()==beforeDelete->Serialize(),"Type conversion undo lost content");
        Require(large.Cells().size() == 1000, "Large notebook incomplete");
        for(size_t i=1; i<large.Cells().size(); ++i)
            Require(large.Cells()[i].top > large.Cells()[i-1].top + large.Cells()[i-1].height, "Cell layout overlaps");
        Reject(fixtures / "malformed.ipynb"); Reject(fixtures / "missing.ipynb");
        auto scratch = root / "Artifacts/test-input.ipynb";
        const auto valid = doc.Original();
        auto rejectJson = [&](nlohmann::json value) {
            { std::ofstream file(scratch); file << value; } Reject(scratch);
        };
        auto invalid = valid; invalid["nbformat"] = 5; rejectJson(invalid);
        invalid = valid; invalid["cells"] = 4; rejectJson(invalid);
        invalid = valid; invalid["cells"][1]["source"] = {1}; rejectJson(invalid);
        invalid = valid; invalid["cells"][1]["execution_count"] = "1"; rejectJson(invalid);
        invalid = valid; invalid["cells"][1]["outputs"] = nullptr; rejectJson(invalid);
        invalid = valid; invalid["cells"][1]["source"] = std::string(1024*1024+1, 'x');
        Require(Notebook::FromJson(invalid,L"Long source").Cells()[1].text->source.size()==1024*1024+1,"Large cell source rejected");
        { std::ofstream file(scratch); file << std::string(140,'[') << "0" << std::string(140,']'); } Reject(scratch);
        { std::ofstream file(scratch); file << std::string(32*1024*1024+1,' '); } Reject(scratch);
        bool rejectedUtf8=false;
        try { Utf16(std::string("\xC0\xAF")); } catch(const std::exception&) { rejectedUtf8=true; }
        Require(rejectedUtf8, "Invalid UTF-8 accepted");
        Require(!RenderPolicy::UseHdr(false,true) && !RenderPolicy::UseHdr(true,false) && RenderPolicy::UseHdr(true,true), "HDR capability gate wrong");
        Require(RenderPolicy::Half(0)==0 && RenderPolicy::Half(1)==0x3c00 && RenderPolicy::Half(4)==0x4400, "HDR channel conversion clips or misencodes");
        Require(RenderPolicy::Half(65504)==0x7bff && RenderPolicy::Half(-1)==0, "Half range clamp wrong");
        Require(RenderPolicy::Half(0.00006103515625f)==0x0400 && RenderPolicy::Half(0.000000059604644775390625f)==1, "Half subnormal wrong");
        Require(std::abs(RenderPolicy::SrgbToLinear(0.5f)-0.214041f)<0.000001f, "sRGB transfer wrong");
        for(int channel=0;channel<=255;++channel) {
            float originalChannel=channel/255.f;
            float restored=RenderPolicy::LinearToSrgb(RenderPolicy::FromHalf(RenderPolicy::Half(RenderPolicy::SrgbToLinear(originalChannel)*3.5f))/3.5f);
            Require(std::abs(originalChannel-restored)<0.001f,"HDR white-normalized roundtrip failed");
        }
        Require(RasterContract::RequiredPixels(17,192)==34,"200% raster density wrong");
        Require(RasterContract::RequiredPixels(17,144)==26,"Fractional DPI raster rounds down");
        Require(!RasterContract::CacheIsSufficient(16,16,32,32),"Low-resolution cache accepted");
        Require(RasterContract::CacheIsSufficient(64,64,32,32),"Downsampling cache rejected");
        for(int width:{394,420,640,1200}) {
            Require(Menus::Left(8,width)<=width-18,"Menu row overflows narrow window");
            for(int index=0;index<8;++index)Require(Menus::Hit(Menus::Left(index,width)+1,width)==index,"Menu hit testing differs from layout");
        }
        Require(!Menus::Items(0,false,false,false,L"")[1].enabled,"Rename enabled without a document");
        Require(Menus::Items(2,false,false,false,L"")[0].command==Menus::Fullscreen,"Fullscreen missing from View menu");
        TitleEditor edit;edit.Begin(L"original");edit.Insert(L'X');
        Require(edit.Text()==L"X","Typing must replace selected title");
        edit.Begin(L"A😀B");edit.Move(3,false);edit.Backspace();
        Require(edit.Text()==L"AB" && edit.Caret()==1,"Backspace split a surrogate pair");
        edit.Begin(L"A😀B");edit.Move(1,false);edit.Delete();Require(edit.Text()==L"AB","Delete split a surrogate pair");
        edit.Begin(L"A😀B");edit.Move(2,false);
        Require(edit.Caret()==1 && edit.Anchor()==1,"Caret accepted an interior surrogate position");
        edit.Move(SIZE_MAX,true);
        Require(edit.Caret()==4 && edit.Anchor()==1,"Caret escaped the buffer");
        edit.Insert(L'X');Require(edit.Text()==L"AX" && edit.Caret()==2,"Selection replacement broke editor state");
        edit.End();edit.Insert(L'Y');edit.Backspace();edit.Delete();edit.Move(0,false);
        Require(!edit.Active() && edit.Text()==L"AX" && edit.Caret()==2,"Inactive editor accepted mutations");
        edit.Begin(std::wstring(200,L'a'));Require(edit.Text().size()==200,"Beginning an edit truncated an existing filename");
        edit.Insert(L'Z');Require(edit.Text()==L"Z","Long filename selection could not be replaced");
        edit.Begin(std::wstring(120,L'a'));edit.Move(120,false);edit.Insert(L'Z');
        Require(edit.Text().size()==120 && edit.Caret()==120,"Title input limit corrupted the buffer");
        Require(ValidNotebookTitle(L"Notebook 日本語 😀"),"Valid Unicode title rejected");
        for(const auto* title:{L"",L"CON",L"nul.ipynb",L"LPT1",L"COM¹",L"bad/name",L"..",L"bad.",L"bad ",L"bad:name"})
            Require(!ValidNotebookTitle(title),"Unsafe filename accepted");
        auto renameFolder=root/"Artifacts"/("rename-test-"+std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(renameFolder);
        auto renameSource=renameFolder/"source.ipynb",collision=renameFolder/"existing.ipynb";
        std::filesystem::copy_file(fixtures/"visualization.ipynb",renameSource,std::filesystem::copy_options::overwrite_existing);
        {std::ofstream file(collision);file<<"DO NOT REPLACE";}
        bool collisionRejected=false;try {RenameNotebookFile(renameSource.wstring(),L"existing");}catch(const std::exception&){collisionRejected=true;}
        Require(collisionRejected && std::filesystem::exists(renameSource),"Rename overwrote an existing file");
        {std::ifstream file(collision);std::string text((std::istreambuf_iterator<char>(file)),{});Require(text=="DO NOT REPLACE","Collision destination changed");}
        auto renamed=RenameNotebookFile(renameSource.wstring(),L"Renamed 日本語");
        Require(!std::filesystem::exists(renameSource) && LoadNotebook(renamed).Original()==doc.Original(),"Rename changed notebook data");
        auto pythonFolder=root/"Artifacts/python-selection";
        for(const auto* folder:{L"user",L"system",L"terminal",L"venv/Scripts",L"conda"}) {
            std::filesystem::create_directories(pythonFolder/folder);
            std::ofstream file(pythonFolder/folder/L"python.exe");file<<"fixture only";
        }
        PythonEnvironment env;env.userPath=(pythonFolder/L"user").wstring();env.systemPath=(pythonFolder/L"system").wstring();env.inheritedPath=(pythonFolder/L"terminal").wstring();
        Require(SelectPython(env).origin==L"user PATH","User PATH must precede system PATH");
        env.userPath=L"";Require(SelectPython(env).origin==L"system PATH","Missing user Python should fall back to system PATH");
        env.terminal=true;Require(SelectPython(env).origin==L"terminal PATH","Terminal environment ignored");
        env.condaPrefix=(pythonFolder/L"conda").wstring();Require(SelectPython(env).origin==L"CONDA_PREFIX","Conda ignored");
        env.virtualEnv=(pythonFolder/L"venv").wstring();Require(SelectPython(env).origin==L"VIRTUAL_ENV","Venv ignored");
        env.explicitPath=(pythonFolder/L"user/python.exe").wstring();Require(SelectPython(env).origin==L"--python","Explicit interpreter ignored");
        env.explicitPath=(pythonFolder/L"missing/python.exe").wstring();Require(!SelectPython(env).error.empty(),"Invalid override silently fell back");
        env={};env.terminal=true;Require(!SelectPython(env).error.empty(),"Missing terminal Python silently used global Python");
        std::atomic<bool> cancel{false};auto identity=DiscoverPython(ReadPythonEnvironment(L""),cancel);
        Require(identity.error.empty() && identity.bits==64 && !identity.version.empty(),"Host Python identity probe failed");
        std::wcout<<L"Python identity: "<<identity.version<<L" ("<<identity.bits<<L"-bit), "<<identity.origin<<L"\n";
        RunTextRasterCacheTests();
        if(GetEnvironmentVariableW(L"WINIP_TEST_KERNEL",nullptr,0))RunKernelControllerTests(root,identity.executable);
        std::cout << "Notebook and render policy tests passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
