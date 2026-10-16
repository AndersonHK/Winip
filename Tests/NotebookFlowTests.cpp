#include "NotebookFlowTests.h"
#include "../Src/NotebookFlow.h"
#include "../Src/NotebookViewport.h"
#include "../Src/Renderer.h"
#include "../Src/CellOutput.h"
#include "../Src/CellEditor.h"
#include <stdexcept>
#include <iostream>
namespace { void Require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);} }
void RunNotebookFlowTests(const std::filesystem::path& root) {
    auto document=Notebook::Load((root/L"Tests/Fixtures/markdown-images.ipynb").wstring());PrepareNotebookLayout(document);
    auto original=std::make_shared<const Notebook>(document);NotebookFlow flow;
    Require(original->Cells()[0].markdown->Images().size()==1 && original->Cells()[1].markdown->Images().size()==1 && original->Cells()[2].markdown->Images().size()==1,"Markdown data URI, attachment or relative JPEG missing");
    Require(original->Cells()[3].markdown->Images().empty() && original->Cells()[3].markdown->Text().find(L"Image unavailable")!=std::wstring::npos,"Unsupported image needs an explicit diagnostic");
    auto escaped=original->Serialize();
    escaped["cells"][1]["attachments"]["plot name.png"]=escaped["cells"][1]["attachments"]["plot.png"];
    escaped["cells"][1]["source"]="![attachment](attachment:plot%20name.png)";
    auto attachment=Notebook::FromJson(std::move(escaped),L"Escaped attachment");PrepareNotebookLayout(attachment);
    Require(attachment.Cells()[1].markdown->Images().size()==1,"Encoded attachment name did not resolve");
    auto shortJson=original->Serialize();shortJson["cells"][0]["source"]="# Short heading";
    auto shortNotebook=Notebook::FromJson(std::move(shortJson),L"Short");PrepareNotebookLayout(shortNotebook);
    auto shortLayout=shortNotebook.Cells()[0].markdown->Reflow(600,500,{});
    Require(shortLayout->Reflow(700,400,{})==shortLayout,"Resize invalidated a heading whose line breaks cannot change");
    Require(shortLayout->Reflow(60,400,{})!=shortLayout,"Narrow heading incorrectly reused an unwrapped layout");
    auto source=original->Cells()[0].markdown->Images()[0].image;
    auto fitted=flow.Prepare(original,700,600,{},0);
    const auto& image=fitted->Cells()[0].markdown->Images()[0];
    Require(image.image==source && source->Width()==1600 && image.width<=490 && image.height<=350,"Fit discarded source pixels or exceeded viewport");
    DWRITE_TEXT_METRICS metrics{};fitted->Cells()[0].markdown->Layout()->GetMetrics(&metrics);
    Require(metrics.lineCount>10,"Markdown prose did not wrap");
    Require(fitted->Cells()[0].contentWidth<=700-2*NotebookViewport::SideMargin(700)-128-46,"Fitted Markdown created a horizontal overflow");
    Require(fitted->Cells()[3].markdown->Reflow(float(700-2*NotebookViewport::SideMargin(700)-128-50),999,{})==fitted->Cells()[3].markdown,"Height-only resize rebuilt plain Markdown");
    auto expanded=flow.Prepare(fitted,700,600,{source},1);
    Require(expanded->Cells()[0].markdown->Images()[0].width==1600 && expanded->Cells()[0].contentWidth>=1600,"Expanded image not fully scrollable");
    auto collapsed=flow.Prepare(expanded,700,600,{},2);
    Require(collapsed->Height()==fitted->Height() && collapsed->SameContent(*original),"Image toggle changed source or failed to restore geometry");
    Require(flow.Prepare(collapsed,700,600,{},2)==collapsed,"Unchanged presentation rebuilt layout");
    auto narrow=flow.Prepare(collapsed,350,600,{},2);
    Require(narrow->Cells()[0].markdown->Height()>collapsed->Cells()[0].markdown->Height(),"Narrow paragraphs lost lines");
    for(size_t i=1;i<narrow->Cells().size();++i)Require(narrow->Cells()[i].top==narrow->Cells()[i-1].top+narrow->Cells()[i-1].height+22,"Responsive cells overlap or leave stale positions");
    Require(narrow->SameContent(*original),"Responsive geometry changed notebook JSON");
    auto outputImage=original->Cells().back().output->Blocks()[0].image;
    auto outputFit=narrow->Cells().back().output;
    Require(outputFit->Blocks()[0].width<4097 && outputFit->Blocks()[0].image==outputImage,"Code output image did not fit");
    auto outputFull=flow.Prepare(narrow,350,600,{outputImage},3);
    Require(outputFull->Cells().back().output->Blocks()[0].width==4097,"Code image toggle lost full size");
    // Check occupied height against the actual displayed image, not merely
    // consistency between two snapshots that could both retain excess space.
    auto imageOnlyJson=original->Serialize();
    auto imageOnlyCell=imageOnlyJson["cells"][1];
    imageOnlyCell["source"]="![image](attachment:plot.png)";
    imageOnlyJson["cells"]=nlohmann::json::array({imageOnlyCell,{{"cell_type","markdown"},{"metadata",nlohmann::json::object()},{"source","# Following cell"}}});
    auto imageOnly=Notebook::FromJson(std::move(imageOnlyJson),L"Image height");PrepareNotebookLayout(imageOnly);
    NotebookFlow imageFlow;
    auto heightFit=imageFlow.Prepare(std::make_shared<const Notebook>(imageOnly),700,600,{},0);
    const auto fittedImage=heightFit->Cells()[0].markdown->Images()[0];
    auto heightFull=imageFlow.Prepare(heightFit,700,600,{fittedImage.image},1);
    const auto fullImage=heightFull->Cells()[0].markdown->Images()[0];
    const float imageDelta=fullImage.height-fittedImage.height;
    const int layoutDelta=heightFull->Cells()[1].top-heightFit->Cells()[1].top;
    std::cout<<"Image occupied height: fitted="<<fittedImage.height<<" full="<<fullImage.height
        <<" fitted cell="<<heightFit->Cells()[0].height<<" full cell="<<heightFull->Cells()[0].height<<"\n";
    Require(std::abs(float(layoutDelta)-imageDelta)<=2,"Following cell did not move by the change in displayed image height");
    Require(heightFit->Cells()[0].sourceHeight<fittedImage.height+100,"Fitted Markdown retained an oversized image line");
    for(const auto* markdownSource:{
        "![image](attachment:plot.png)\n\n",
        "# Header\n\n![image](attachment:plot.png)",
        "![image](attachment:plot.png)\n\nFollowing paragraph",
        "![first](attachment:plot.png)\n\n![last](attachment:plot.png)",
        "![first](attachment:plot.png)\n\nText between images\n\n![last](attachment:plot.png)"}) {
        auto variant=imageOnly.Serialize();variant["cells"][0]["source"]=markdownSource;
        auto prepared=Notebook::FromJson(std::move(variant),L"Image separators");PrepareNotebookLayout(prepared);
        const auto markdown=prepared.Cells()[0].markdown;
        ExpandedImages allImages;
        for(const auto& item:markdown->Images())allImages.insert(item.image);
        for(const int viewportWidth:{350,700,1600}) {
            NotebookFlow variantFlow;
            auto fit=variantFlow.Prepare(std::make_shared<const Notebook>(prepared),viewportWidth,600,{},0);
            auto full=variantFlow.Prepare(fit,viewportWidth,600,allImages,1);
            float delta=0;
            const auto& fittedImages=fit->Cells()[0].markdown->Images();
            for(size_t i=0;i<fittedImages.size();++i)
                delta+=full->Cells()[0].markdown->Images()[i].height-fittedImages[i].height;
            Require(std::abs(float(full->Cells()[1].top-fit->Cells()[1].top)-delta)<=2,
                "Markdown image separators retained extra height or overlapped following text");
            const auto& last=fittedImages.back();
            if(fit->Cells()[0].markdown->Text().back()==L'\ufffc')
                Require(fit->Cells()[0].sourceHeight<=last.y+last.height+30,
                    "Markdown ending in an image retained an image-sized empty line");
            auto restored=variantFlow.Prepare(full,viewportWidth,600,{},2);
            Require(restored->Cells()[1].top==fit->Cells()[1].top && restored->SameContent(prepared),
                "Collapsing images failed to restore following-cell positions and source");
        }
    }
    CellEditor editor;editor.Attach(fitted);editor.Begin(0,0,0);editor.SelectAll();editor.InsertText(original->Cells()[0].text->source+L"\nEdited");editor.End();
    auto request=editor.TakePreviewRequest();Require(request && request->Cells()[0].markdown->NeedsImages(),"Image edits performed synchronous decoding");
    Notebook ready=*request;PrepareNotebookLayout(ready);
    editor.Begin(4,0,0);editor.InsertText(L"# changed while loading\n");
    editor.ApplyPreview(ready);
    Require(editor.Document()->Cells()[0].markdown->Images().size()==1 && !editor.Document()->Cells()[0].markdown->NeedsImages(),"Asynchronous preview did not publish");
    Require(editor.Active() && editor.Document()->Cells()[4].text->source.find(L"# changed while loading")==0,"Preview replaced a newer edit");
    std::cout<<"Responsive Markdown/images: wrapping, image sources, fit/full toggle, geometry, source retention and asynchronous edit tests passed\n";
}
