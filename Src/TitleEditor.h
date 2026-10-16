#pragma once
#include <string>

// Copyable view snapshot. Only operations can change the buffer and selection.
class TitleEditor {
public:
    TitleEditor();
    ~TitleEditor();
    void Begin(const std::wstring& value);
    void End();
    bool Active() const;
    const std::wstring& Text() const;
    size_t Caret() const;
    size_t Anchor() const;
    void SelectAll();
    void Insert(wchar_t character);
    void Backspace();
    void Delete();
    void Move(size_t position, bool select);
    size_t Previous(size_t position) const;
    size_t Next(size_t position) const;

private:
    bool EraseSelection();
    size_t Boundary(size_t position) const;
    bool active = false;
    std::wstring text;
    size_t caret = 0;
    size_t anchor = 0;
};
