#pragma once
#include "Notebook.h"
#include "Markdown.h"
// Render-worker owner of responsive geometry; publication keeps source/history intact.
class NotebookFlow {
public:
    std::shared_ptr<const Notebook> Prepare(std::shared_ptr<const Notebook> document,int width,int height,const ExpandedImages& expanded,uint64_t imageRevision);
private:
    std::shared_ptr<const Notebook> previous;
    int width=0,height=0;
    uint64_t imageRevision=0;
    bool responsive=false, hasImages=false;
};
