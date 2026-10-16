#include "NotebookInput.h"
#include "LoadMetrics.h"
#include <filesystem>
#include <stdexcept>

NotebookInput::NotebookInput(const std::wstring& path, std::function<bool()> check)
    : file(std::filesystem::path(path), std::ios::binary), cancelled(std::move(check)) {
    if (!file) throw std::runtime_error("Cannot open notebook. Check that the file exists and is readable.");
}

size_t NotebookInput::ReadBlock() {
    LoadMetrics::Scope timing(LoadMetrics::FileRead);
    if (cancelled && cancelled()) throw std::runtime_error("Notebook loading was cancelled.");
    file.read(buffer.data(), buffer.size());
    if (file.bad()) throw std::runtime_error("Cannot read notebook. Check the drive or network connection.");
    return static_cast<size_t>(file.gcount());
}

NotebookInput::Iterator::Iterator() = default;
NotebookInput::Iterator::Iterator(NotebookInput* owner) : input(owner) {
    const auto count = input->ReadBlock();
    if (count) { current = input->buffer.data(); end = current + count; }
}
NotebookInput::Iterator::reference NotebookInput::Iterator::operator*() const { return *current; }
NotebookInput::Iterator& NotebookInput::Iterator::operator++() {
    if (++current == end) {
        const auto count = input->ReadBlock();
        current = count ? input->buffer.data() : nullptr;
        end = count ? current + count : nullptr;
    }
    return *this;
}
NotebookInput::Iterator::Previous NotebookInput::Iterator::operator++(int) {
    Previous previous{*current};
    ++*this;
    return previous;
}
char NotebookInput::Iterator::Previous::operator*() const { return value; }
bool NotebookInput::Iterator::operator==(const Iterator& other) const { return current == other.current; }
bool NotebookInput::Iterator::operator!=(const Iterator& other) const { return !(*this == other); }

nlohmann::json NotebookInput::Read() {
    LoadMetrics::Scope timing(LoadMetrics::JsonParse);
    return nlohmann::json::parse(Iterator(this), Iterator(), [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
        if (depth > 128) throw std::runtime_error("Notebook JSON nesting exceeds 128 levels");
        return true;
    });
}
