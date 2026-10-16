#include "TitleEditor.h"
#include <algorithm>

TitleEditor::TitleEditor() = default;
TitleEditor::~TitleEditor() = default;

void TitleEditor::Begin(const std::wstring& value) {
    // Do not truncate an existing filename that is longer than the editing limit.
    std::wstring replacement = value;
    text.swap(replacement);
    caret = text.size();
    anchor = 0;
    active = true;
}

void TitleEditor::End() { active = false; }
bool TitleEditor::Active() const { return active; }
const std::wstring& TitleEditor::Text() const { return text; }
size_t TitleEditor::Caret() const { return caret; }
size_t TitleEditor::Anchor() const { return anchor; }

size_t TitleEditor::Boundary(size_t position) const {
    position = std::min(position, text.size());
    if (position && position < text.size() && text[position] >= 0xdc00 && text[position] <= 0xdfff &&
        text[position - 1] >= 0xd800 && text[position - 1] <= 0xdbff) {
        --position;
    }
    return position;
}

size_t TitleEditor::Previous(size_t position) const {
    position = Boundary(position);
    return position ? Boundary(position - 1) : 0;
}

size_t TitleEditor::Next(size_t position) const {
    position = Boundary(position);
    if (position + 1 < text.size() && text[position] >= 0xd800 && text[position] <= 0xdbff &&
        text[position + 1] >= 0xdc00 && text[position + 1] <= 0xdfff) {
        return position + 2;
    }
    return std::min(position + 1, text.size());
}

void TitleEditor::SelectAll() {
    if (!active) return;
    anchor = 0;
    caret = text.size();
}

bool TitleEditor::EraseSelection() {
    const auto first = std::min(caret, anchor), last = std::max(caret, anchor);
    if (first == last) return false;
    text.erase(first, last - first);
    caret = anchor = first;
    return true;
}

void TitleEditor::Insert(wchar_t character) {
    if (!active || character < 32 || character == 127) return;
    const auto first = std::min(caret, anchor), last = std::max(caret, anchor);
    if (text.size() - (last - first) >= 120) return;
    // A single replacement preserves the selection if allocation fails.
    text.replace(first, last - first, 1, character);
    caret = anchor = first + 1;
}

void TitleEditor::Backspace() {
    if (!active) return;
    if (!EraseSelection() && caret) {
        const auto previous = Previous(caret);
        text.erase(previous, caret - previous);
        caret = anchor = previous;
    }
}

void TitleEditor::Delete() {
    if (!active) return;
    if (!EraseSelection() && caret < text.size()) text.erase(caret, Next(caret) - caret);
}

void TitleEditor::Move(size_t position, bool select) {
    if (!active) return;
    caret = Boundary(position);
    if (!select) anchor = caret;
}
