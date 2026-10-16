#include "CellEditor.h"
#include "CellOutput.h"
#include "FontSet.h"
#include "Markdown.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

using Microsoft::WRL::ComPtr;
namespace {
void CheckEdit(HRESULT result) { if (FAILED(result)) throw std::runtime_error("Cannot prepare cell text"); }
size_t Boundary(const std::wstring& text, size_t position) {
    position = std::min(position, text.size());
    if (position && position < text.size() && text[position] >= 0xdc00 && text[position] <= 0xdfff &&
        text[position-1] >= 0xd800 && text[position-1] <= 0xdbff) --position;
    return position;
}
}
struct CellEditor::Impl {
    ComPtr<IDWriteFactory> factory;
    std::unique_ptr<FontSet> fonts;
    ComPtr<IDWriteTextFormat> format;
    std::shared_ptr<const Notebook> document;
    CellEditView view;
    size_t selected = 0;
    bool previewRequest=false;
    uint64_t outputCellId = 0;
    int outputWidth = 0;
    struct History { std::shared_ptr<const Notebook> document; size_t selected; CellEditView view; };
    std::vector<History> undo, redo;
    ComPtr<IDWriteTextLayout> Shape(const std::wstring& text) {
        ComPtr<IDWriteTextLayout> layout;
        CheckEdit(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format.Get(), 16777216, 16777216, &layout));
        return layout;
    }
    CellLayout Bounds(IDWriteTextLayout* layout, const Cell* cell = nullptr) {
        DWRITE_TEXT_METRICS metrics{};
        CheckEdit(layout->GetMetrics(&metrics));
        int width=0;
        if(cell && !cell->text->output.empty()) {
            if(outputCellId!=cell->id) {
                auto output=Shape(cell->text->output); DWRITE_TEXT_METRICS measured{};
                CheckEdit(output->GetMetrics(&measured));outputWidth=static_cast<int>(std::ceil(measured.widthIncludingTrailingWhitespace))+4;
                outputCellId=cell->id;
            }
            width=cell->output?cell->output->Width():outputWidth;
        }
        return {static_cast<int>(std::ceil(metrics.height)), cell ? cell->outputHeight : 0,
            std::max(width, static_cast<int>(std::ceil(metrics.widthIncludingTrailingWhitespace)) + 4), {}, {}, static_cast<int>(std::ceil(metrics.widthIncludingTrailingWhitespace))+4};
    }
    History Current() const { return {document, selected, view}; }
    void Publish(std::shared_ptr<const Notebook> replacement, CellEditView next, size_t index, bool history) {
        if (history) {
            undo.push_back(Current());
            if (undo.size() > 128) undo.erase(undo.begin());
            redo.clear();
        }
        document = std::move(replacement); view = std::move(next); selected = index;
    }
    void Replace(size_t first, size_t last, const std::wstring& value) {
        if (!view.id) return;
        auto source = document->Cells()[selected].text->source;
        if (source.size() - (last-first) + value.size() > 1024 * 1024) throw std::runtime_error("Cell source exceeds 1 Mi-character limit");
        source.replace(first, last-first, value);
        if (source == document->Cells()[selected].text->source) return;
        auto layout = Shape(source);
        auto next = std::make_shared<const Notebook>(document->WithSource(selected, source, Bounds(layout.Get(), &document->Cells()[selected])));
        Publish(std::move(next), {layout, view.id, first + value.size(), first + value.size()}, selected, true);
    }
};
CellEditor::CellEditor() : impl(std::make_unique<Impl>()) {
    CheckEdit(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(impl->factory.GetAddressOf())));
    impl->fonts = std::make_unique<FontSet>(impl->factory.Get());
    CheckEdit(impl->factory->CreateTextFormat(L"Noto Sans Mono", impl->fonts->Collection(), DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 17, L"en-us", &impl->format));
    CheckEdit(impl->format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
}
CellEditor::~CellEditor() = default;
void CellEditor::Attach(std::shared_ptr<const Notebook> notebook) {
    impl->document = std::move(notebook); impl->view = {}; impl->selected = 0; impl->undo.clear(); impl->redo.clear(); impl->previewRequest=false;
}
std::shared_ptr<const Notebook> CellEditor::TakePreviewRequest() {
    if(!impl->previewRequest)return {};
    impl->previewRequest=false;return impl->document;
}
void CellEditor::ApplyPreview(const Notebook& prepared) {
    if(!impl->document)return;
    std::unordered_map<uint64_t,const Cell*> readyById;
    for(const auto& ready:prepared.Cells())readyById.emplace(ready.id,&ready);
    std::vector<CellLayout> layouts;
    layouts.reserve(impl->document->Cells().size());
    bool changed=false;
    for(const auto& cell:impl->document->Cells()) {
        CellLayout bounds{cell.sourceHeight,cell.outputHeight,cell.contentWidth,cell.markdown,cell.output,cell.sourceWidth};
        if(!cell.sourceMode && cell.markdown && cell.markdown->NeedsImages()) {
            auto found=readyById.find(cell.id);
            if(found!=readyById.end()) {
                const auto& ready=*found->second;
                if(ready.text==cell.text && ready.markdown && !ready.markdown->NeedsImages()) {
                    bounds.markdown=ready.markdown;
                    bounds.sourceHeight=ready.markdown->Height();
                    bounds.contentWidth=std::max(ready.markdown->Width(),cell.output?cell.output->Width():0);
                    bounds.sourceWidth=ready.markdown->Width();
                    changed=true;
                }
            }
        }
        layouts.push_back(std::move(bounds));
    }
    if(changed)impl->document=std::make_shared<const Notebook>(impl->document->WithLayout(layouts));
}
void CellEditor::ApplyLayout(std::shared_ptr<const Notebook> notebook) {
    if(!impl->document || notebook->ContentRevision()!=impl->document->ContentRevision())
        throw std::invalid_argument("Cannot apply geometry from another document revision");
    impl->document=std::move(notebook);
}
void CellEditor::Retitle(const std::wstring& title) {
    if(!impl->document)return;
    auto replacement=std::make_shared<const Notebook>(impl->document->WithTitle(title));
    auto undo=impl->undo,redo=impl->redo;
    for(auto* history:{&undo,&redo})for(auto& state:*history)
        state.document=std::make_shared<const Notebook>(state.document->WithTitle(title));
    impl->document=std::move(replacement);impl->undo=std::move(undo);impl->redo=std::move(redo);
}
void CellEditor::Begin(size_t index, float x, float y, bool extend) {
    if (!impl->document || index >= impl->document->Cells().size()) return;
    if(Active() && index!=impl->selected)End();
    const auto& cell = impl->document->Cells()[index];
    if (cell.text->kind != L"code" && cell.text->kind != L"markdown") { End(); return; }
    auto layout = impl->view.id == cell.id ? impl->view.layout : impl->Shape(cell.text->source);
    BOOL trailing = FALSE, inside = FALSE; DWRITE_HIT_TEST_METRICS hit{};
    CheckEdit(layout->HitTestPoint(x, y, &trailing, &inside, &hit));
    size_t caret = Boundary(cell.text->source, hit.textPosition + (trailing ? hit.length : 0));
    size_t anchor = extend && impl->view.id == cell.id ? impl->view.anchor : caret;
    // Source mode uses the editor's measured height, including Markdown markers.
    auto replacement = cell.sourceMode ? impl->document : std::make_shared<const Notebook>(impl->document->WithPresentation(index, impl->Bounds(layout.Get(), &cell),true));
    impl->Publish(std::move(replacement), {layout, cell.id, caret, anchor}, index, false);
}
void CellEditor::End() {
    if(Active() && impl->document->Cells()[impl->selected].text->kind==L"markdown") {
        const auto& cell=impl->document->Cells()[impl->selected];
        auto preview=cell.markdown?cell.markdown:Markdown::Prepare(impl->factory.Get(),impl->fonts->Collection(),cell.text->source,cell.original->contains("attachments")?&cell.original->at("attachments"):nullptr,impl->document->Directory(),false);
        if(preview->NeedsImages())impl->previewRequest=true;
        CellLayout bounds{preview->Height(),cell.outputHeight,preview->Width(),preview};
        auto replacement=std::make_shared<const Notebook>(impl->document->WithPresentation(impl->selected,bounds,false));
        impl->document=std::move(replacement);
    }
    impl->view = {};
}
void CellEditor::Select(size_t index) { if(impl->document && index<impl->document->Cells().size()) { End();impl->selected=index; } }
void CellEditor::InsertCell(size_t index, const std::wstring& kind) {
    if (!impl->document) return;
    End();
    auto layout = impl->Shape(L"");
    auto document = std::make_shared<const Notebook>(impl->document->Insert(index, kind, impl->Bounds(layout.Get())));
    auto id = document->Cells()[index].id;
    impl->Publish(std::move(document), {layout, id, 0, 0}, index, true);
}
void CellEditor::InsertText(const std::wstring& value) {
    // Windows clipboard CRLF and CR become notebook LF; no code is executed.
    std::wstring normalized;
    for (size_t i=0; i<value.size(); ++i) {
        wchar_t ch=value[i];
        if (ch==L'\r') { normalized+=L'\n'; if (i+1<value.size() && value[i+1]==L'\n') ++i; }
        else if (ch==L'\n' || ch==L'\t' || ch>=32) normalized+=ch;
    }
    impl->Replace(std::min(impl->view.caret, impl->view.anchor), std::max(impl->view.caret, impl->view.anchor), normalized);
}
void CellEditor::Erase(bool backwards) {
    if (!Active()) return;
    size_t first=std::min(impl->view.caret,impl->view.anchor), last=std::max(impl->view.caret,impl->view.anchor);
    const auto& text=impl->document->Cells()[impl->selected].text->source;
    if (first==last) {
        if (backwards && first) first=Boundary(text,first-1);
        else if (!backwards && last<text.size()) { ++last; if (Boundary(text,last)!=last) ++last; }
    }
    impl->Replace(first,last,L"");
}
void CellEditor::Move(int key, bool extend, bool control) {
    if (!Active()) return;
    const auto& text=impl->document->Cells()[impl->selected].text->source;
    size_t pos=impl->view.caret;
    if (key==VK_LEFT) pos=!extend && pos!=impl->view.anchor ? std::min(pos,impl->view.anchor) : Boundary(text,pos ? pos-1 : 0);
    else if (key==VK_RIGHT) {
        if (!extend && pos!=impl->view.anchor) pos=std::max(pos,impl->view.anchor);
        else if(pos<text.size()) { ++pos; if(Boundary(text,pos)!=pos) ++pos; }
    } else if (key==VK_HOME) { auto at=pos ? text.rfind(L'\n',pos-1) : std::wstring::npos; pos=control || at==std::wstring::npos ? 0 : at+1; }
    else if (key==VK_END) { auto at=text.find(L'\n',pos); pos=control || at==std::wstring::npos ? text.size() : at; }
    else if (key==VK_UP || key==VK_DOWN) {
        FLOAT x=0,y=0; DWRITE_HIT_TEST_METRICS hit{}; BOOL trailing=FALSE,inside=FALSE;
        CheckEdit(impl->view.layout->HitTestTextPosition(static_cast<UINT32>(pos),FALSE,&x,&y,&hit));
        float nextY=y+(key==VK_UP ? -hit.height/2 : hit.height*1.5f);
        CheckEdit(impl->view.layout->HitTestPoint(x,nextY,&trailing,&inside,&hit));
        pos=Boundary(text,hit.textPosition+(trailing?hit.length:0));
    }
    impl->view.caret=pos;
    if (!extend) impl->view.anchor=pos;
}
void CellEditor::SelectAll() { if (Active()) { impl->view.anchor=0; impl->view.caret=impl->document->Cells()[impl->selected].text->source.size(); } }
void CellEditor::Undo() {
    if (impl->undo.empty()) return;
    auto next=impl->undo.back(); impl->redo.push_back(impl->Current()); impl->undo.pop_back();
    impl->document=std::move(next.document); impl->selected=next.selected; impl->view=std::move(next.view);
}
void CellEditor::Redo() {
    if (impl->redo.empty()) return;
    auto next=impl->redo.back(); impl->undo.push_back(impl->Current()); impl->redo.pop_back();
    impl->document=std::move(next.document); impl->selected=next.selected; impl->view=std::move(next.view);
}
std::wstring CellEditor::Selection() const {
    if (!Active()) return {};
    auto first=std::min(impl->view.caret,impl->view.anchor),last=std::max(impl->view.caret,impl->view.anchor);
    return impl->document->Cells()[impl->selected].text->source.substr(first,last-first);
}
bool CellEditor::Active() const { return impl->view.id!=0; }
size_t CellEditor::Selected() const { return impl->selected; }
CellEditView CellEditor::View() const { return impl->view; }
std::shared_ptr<const Notebook> CellEditor::Document() const { return impl->document; }
D2D1_POINT_2F CellEditor::CaretPoint() const {
    FLOAT x=0,y=0; DWRITE_HIT_TEST_METRICS hit{};
    if (Active()) CheckEdit(impl->view.layout->HitTestTextPosition(static_cast<UINT32>(impl->view.caret),FALSE,&x,&y,&hit));
    return {x,y};
}

void CellEditor::DeleteCell() {
    if(!impl->document || impl->document->Cells().empty())return;
    auto replacement=std::make_shared<const Notebook>(impl->document->Delete(impl->selected));
    size_t next=replacement->Cells().empty()?0:std::min(impl->selected,replacement->Cells().size()-1);
    impl->Publish(std::move(replacement),{},next,true);
}
void CellEditor::ChangeKind(const std::wstring& kind) {
    if(!impl->document || impl->document->Cells().empty())return;
    auto replacement=std::make_shared<const Notebook>(impl->document->WithKind(impl->selected,kind));
    auto layout=impl->Shape(replacement->Cells()[impl->selected].text->source);
    auto id=replacement->Cells()[impl->selected].id;
    replacement=std::make_shared<const Notebook>(replacement->WithPresentation(impl->selected,impl->Bounds(layout.Get(),&replacement->Cells()[impl->selected]),true));
    impl->Publish(std::move(replacement),{layout,id,0,0},impl->selected,true);
    End();
}

void CellEditor::ApplyExecution(uint64_t cellId,const Cell& prepared) {
    if(!impl->document)return;
    impl->document=std::make_shared<const Notebook>(impl->document->WithExecution(cellId,prepared));
    impl->outputCellId=0;
}
