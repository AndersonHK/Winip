#pragma once
#include "framework.h"
#include "Notebook.h"
#include <dwrite.h>
#include <d2d1.h>
#include <wrl/client.h>

// Immutable shaping result shared by input hit testing and GPU rendering.
struct CellEditView {
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    uint64_t id = 0;
    size_t caret = 0, anchor = 0;
};

class CellEditor {
public:
    CellEditor();
    ~CellEditor();
    CellEditor(const CellEditor&) = delete;
    CellEditor& operator=(const CellEditor&) = delete;
    void Attach(std::shared_ptr<const Notebook> notebook);
    void ApplyExecution(uint64_t cellId, const Cell& prepared);
    std::shared_ptr<const Notebook> TakePreviewRequest();
    void ApplyPreview(const Notebook& prepared);
    void ApplyLayout(std::shared_ptr<const Notebook> notebook);
    void Retitle(const std::wstring& title);
    void Begin(size_t index, float x, float y, bool extend = false);
    void End();
    void Select(size_t index);
    void InsertCell(size_t index, const std::wstring& kind);
    void DeleteCell();
    void ChangeKind(const std::wstring& kind);
    void InsertText(const std::wstring& value);
    void Erase(bool backwards);
    void Move(int key, bool extend, bool control);
    void SelectAll();
    void Undo();
    void Redo();
    std::wstring Selection() const;
    bool Active() const;
    size_t Selected() const;
    CellEditView View() const;
    std::shared_ptr<const Notebook> Document() const;
    D2D1_POINT_2F CaretPoint() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
