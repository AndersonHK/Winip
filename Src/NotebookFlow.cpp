#include "NotebookFlow.h"
#include "CellOutput.h"
#include "NotebookViewport.h"
#include <algorithm>
std::shared_ptr<const Notebook> NotebookFlow::Prepare(std::shared_ptr<const Notebook> doc,int w,int h,const ExpandedImages& expanded,uint64_t revision) {
    if(!doc)return {};
    const int page=std::max(1,w-2*NotebookViewport::SideMargin(w)-(w<650?88:128)-46);
    const int availableWidth=std::max(1,page-4);
    if(previous!=doc) {
        responsive=false;hasImages=false;
        for(const auto& cell:doc->Cells()) {
            if(cell.markdown && !cell.sourceMode) {responsive=true;hasImages|=!cell.markdown->Images().empty();}
            if(cell.output){responsive=true;hasImages=true;}
        }
    }
    const int imageHeight=hasImages?std::max(1,h-250):0;
    if(previous==doc && width==availableWidth && height==imageHeight && imageRevision==revision)return doc;
    width=availableWidth;height=imageHeight;imageRevision=revision;
    if(!responsive){previous=doc;return doc;}
    std::vector<CellLayout> bounds;bounds.reserve(doc->Cells().size());
    for(const auto& cell:doc->Cells()) {
        CellLayout layout{cell.sourceHeight,cell.outputHeight,cell.contentWidth,cell.markdown,cell.output,cell.sourceWidth};
        if(cell.markdown && !cell.sourceMode) {
            layout.markdown=cell.markdown->Reflow(float(availableWidth),float(std::max(1,imageHeight)),expanded);
            layout.sourceHeight=layout.markdown->Height();layout.contentWidth=layout.markdown->Width();
        }
        if(cell.output) {
            layout.output=cell.output->Fit(float(availableWidth),float(std::max(1,imageHeight)),expanded);
            layout.outputHeight=layout.output->Height();
            // Source extent is independently measured; images must not retain a prior full-size scrollbar.
            int sourceWidth=cell.sourceWidth;
            layout.contentWidth=std::max(sourceWidth,layout.output->Width());
        }
        bounds.push_back(std::move(layout));
    }
    previous=std::make_shared<const Notebook>(doc->WithLayout(bounds));
    return previous;
}
