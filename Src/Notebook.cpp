#include "Notebook.h"
#include "LoadMetrics.h"
#include "NotebookInput.h"
#include "CellOutput.h"
#include "framework.h"
#include <cwctype>
#include <utility>
#include <atomic>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iomanip>

bool ValidNotebookTitle(const std::wstring& title) {
    if(title.empty() || title.size()>120 || title.back()==L'.' || title.back()==L' ')return false;
    for(size_t i=0;i<title.size();++i) {
        auto ch=title[i];if(ch<32 || std::wstring(L"<>:\"/\\|?*").find(ch)!=std::wstring::npos)return false;
        if(ch>=0xd800 && ch<=0xdbff){if(++i>=title.size() || title[i]<0xdc00 || title[i]>0xdfff)return false;}
        else if(ch>=0xdc00 && ch<=0xdfff)return false;
    }
    auto base=title.substr(0,title.find(L'.'));while(!base.empty() && base.back()==L' ')base.pop_back();
    std::transform(base.begin(),base.end(),base.begin(),[](wchar_t c){return (wchar_t)towupper(c);});
    if(base==L"CON" || base==L"PRN" || base==L"AUX" || base==L"NUL" || base==L"CONIN$" || base==L"CONOUT$")return false;
    if(base.size()==4 && (base.substr(0,3)==L"COM" || base.substr(0,3)==L"LPT") &&
       ((base[3]>=L'1' && base[3]<=L'9') || base[3]==L'¹' || base[3]==L'²' || base[3]==L'³'))return false;
    return true;
}

std::wstring RenameNotebookFile(const std::wstring& path,const std::wstring& title) {
    if(!ValidNotebookTitle(title))throw std::runtime_error("Invalid notebook title");
    const auto previous=std::filesystem::path(path);
    const auto destination=previous.parent_path()/(title+previous.extension().wstring());
    auto result=destination.wstring(); // Allocate before the filesystem mutation.
    if(!MoveFileExW(previous.c_str(),destination.c_str(),0))
        throw std::runtime_error("Rename failed (Windows error "+std::to_string(GetLastError())+"). The original file is unchanged.");
    return result;
}

namespace {
std::wstring Multiline(const nlohmann::json& value) {
    if (value.is_string()) return Utf16(value.get_ref<const std::string&>());
    if (!value.is_array()) throw std::runtime_error("Expected notebook text or an array of text lines");
    std::wstring text;
    for (const auto& line : value) {
        if (!line.is_string()) throw std::runtime_error("Notebook text array contains a non-string");
        text += Utf16(line.get_ref<const std::string&>());
    }
    return text;
}
uint64_t NextCellId() { static std::atomic<uint64_t> next{1}; return next++; }
int InitialTextHeight(const std::wstring& text) {
    const auto height = (1LL + std::count(text.begin(), text.end(), L'\n')) * 22;
    if (height > 20000000) throw std::runtime_error("Notebook exceeds the preview layout limit");
    return static_cast<int>(height);
}
}
Notebook Notebook::Load(const std::wstring& path, std::function<bool()> cancelled) {
    NotebookInput input(path, std::move(cancelled));
    auto original = input.Read();
    return FromJson(std::move(original), std::filesystem::path(path).stem().wstring(), std::filesystem::path(path).parent_path().wstring());
}

struct Notebook::Data {
    std::wstring directory;
    std::shared_ptr<const nlohmann::json> original;
    std::vector<Cell> cells;
    int height = 0;
    int contentWidth = 0;
    bool measured = false;
    uint64_t revision = NextCellId();
};

Notebook Notebook::FromJson(nlohmann::json original, std::wstring title, std::wstring directory) {
    LoadMetrics::Scope timing(LoadMetrics::Model);
    auto parsed = std::make_shared<Data>();
    parsed->directory=std::move(directory);
    parsed->original = std::make_shared<const nlohmann::json>(std::move(original));
    const auto& json = *parsed->original;
    if (json.at("nbformat") != 4 || !json.at("cells").is_array())
        throw std::runtime_error("Expected an nbformat 4 notebook with a cells array");
    if (json.at("cells").size() > 10000) throw std::runtime_error("This reader checkpoint supports up to 10,000 cells");
    for (const auto& value : json.at("cells")) {
        Cell cell;
        auto content = std::make_shared<CellText>();
        cell.id = NextCellId();
        cell.original = std::shared_ptr<const nlohmann::json>(parsed->original, &value);
        content->kind = Utf16(value.at("cell_type").get<std::string>());
        content->source = Multiline(value.at("source"));
        if (content->kind == L"code") {
            const auto count = value.value("execution_count", nlohmann::json());
            if (!count.is_null() && !count.is_number_integer()) throw std::runtime_error("Invalid execution count");
            content->prompt = L"In [" + (count.is_null() ? L" " : Utf16(count.dump())) + L"]:";
            if (value.contains("outputs")) {
                if (!value.at("outputs").is_array()) throw std::runtime_error("Expected an outputs array");
                for (const auto& output : value.at("outputs")) {
                    const auto type = output.value("output_type", std::string());
                    if (type == "stream") content->output += Multiline(output.at("text"));
                    else if (type == "error") content->output += Utf16(output.value("ename", "Error") + ": " + output.value("evalue", "")) + L"\n";
                    else if (output.contains("data") && output.at("data").contains("text/plain"))
                        content->output += Multiline(output.at("data").at("text/plain")) + L"\n";
                    else content->output += L"[Rich output is preserved; preview is not available yet.]\n";
                }
            }
        } else if (content->kind != L"markdown" && content->kind != L"raw") content->output = L"[Unknown cell type: displayed as source.]";
        if(content->kind==L"code" && value.contains("outputs")) {
            bool images=false;
            for(const auto& output:value.at("outputs"))if(output.contains("data")) {
                const auto& data=output.at("data");images=images || data.contains("image/png") || data.contains("image/jpeg");
            }
            if(images) {
                cell.output=std::make_shared<const CellOutput>(value.at("outputs"));content->output=cell.output->PlainText();
            }
        }
        cell.top = parsed->height;
        cell.sourceHeight=InitialTextHeight(content->source);
        cell.outputHeight=content->output.empty()?0:InitialTextHeight(content->output);
        cell.height = 32 + cell.sourceHeight + (content->output.empty() ? 0 : 20 + cell.outputHeight);
        parsed->height += cell.height + 22;
        if (parsed->height > 20000000) throw std::runtime_error("Notebook exceeds the preview layout limit");
        cell.text = std::move(content);
        parsed->cells.push_back(std::move(cell));
    }
    return Notebook(std::move(title), std::move(parsed));
}

Notebook::Notebook() : Notebook(FromJson({{"nbformat", 4}, {"nbformat_minor", 5},
    {"metadata", nlohmann::json::object()}, {"cells", nlohmann::json::array()}}, L"Untitled")) {}
Notebook::~Notebook() = default;
Notebook::Notebook(const Notebook& other) = default;
Notebook::Notebook(std::wstring name, std::shared_ptr<const Data> contents)
    : title(std::move(name)), data(std::move(contents)) {}
Notebook& Notebook::operator=(Notebook other) {
    Swap(other);
    return *this;
}
void Notebook::Swap(Notebook& other) noexcept {
    title.swap(other.title);
    data.swap(other.data);
}
const nlohmann::json& Notebook::Original() const { return *data->original; }
const std::vector<Cell>& Notebook::Cells() const { return data->cells; }
const std::wstring& Notebook::Title() const { return title; }
const std::wstring& Notebook::Directory() const { return data->directory; }
int Notebook::Height() const { return data->height; }
int Notebook::ContentWidth() const { return data->contentWidth; }
bool Notebook::HasMeasuredLayout() const { return data->measured; }
uint64_t Notebook::ContentRevision() const { return data->revision; }
Notebook Notebook::WithTitle(std::wstring name) const {
    if (!ValidNotebookTitle(name)) throw std::invalid_argument("Invalid notebook title");
    return Notebook(std::move(name), data);
}
Notebook Notebook::WithLayout(const std::vector<CellLayout>& layout) const {
    if (layout.size() != data->cells.size()) throw std::invalid_argument("Layout cell count mismatch");
    // Stage every allocation and validation; neither this nor shared snapshots mutate.
    auto measured = std::make_shared<Data>(*data);
    int64_t height = 0;
    measured->contentWidth = 0;
    for (size_t i = 0; i < layout.size(); ++i) {
        const auto& bounds = layout[i];
        auto& cell = measured->cells[i];
        if (bounds.sourceHeight < 0 || bounds.outputHeight < 0 || bounds.contentWidth < 0 ||
            (cell.text->output.empty() && bounds.outputHeight != 0)) {
            throw std::invalid_argument("Invalid cell layout");
        }
        int64_t cellHeight = 32LL + bounds.sourceHeight + (cell.text->output.empty() ? 0 : 20LL + bounds.outputHeight);
        if (height + cellHeight + 22 > 20000000) throw std::invalid_argument("Notebook exceeds the measured layout limit");
        cell.top = static_cast<int>(height);
        cell.height = static_cast<int>(cellHeight);
        cell.sourceHeight = bounds.sourceHeight;
        cell.outputHeight = bounds.outputHeight;
        cell.contentWidth = bounds.contentWidth;
        if(bounds.sourceWidth)cell.sourceWidth=bounds.sourceWidth;
        cell.markdown = bounds.markdown;
        if(bounds.output)cell.output=bounds.output;
        measured->contentWidth = std::max(measured->contentWidth, bounds.contentWidth);
        height += cellHeight + 22;
    }
    measured->height = static_cast<int>(height);
    measured->measured = true;
    return Notebook(title, std::move(measured));
}
Notebook LoadNotebook(const std::wstring& path) { return Notebook::Load(path); }

void Notebook::Reflow(Data& contents) {
    int64_t height = 0;
    contents.contentWidth = 0;
    for (auto& cell : contents.cells) {
        if (cell.sourceHeight < 0 || cell.outputHeight < 0 || cell.contentWidth < 0 || (cell.text->output.empty() && cell.outputHeight!=0))
            throw std::invalid_argument("Invalid cell dimensions");
        const int64_t extent = 32LL + cell.sourceHeight + (cell.text->output.empty() ? 0 : 20LL + cell.outputHeight);
        if (height + extent + 22 > 20000000) throw std::invalid_argument("Notebook exceeds the layout limit");
        cell.top = static_cast<int>(height);
        cell.height = static_cast<int>(extent);
        height += extent + 22;
        contents.contentWidth = std::max(contents.contentWidth, cell.contentWidth);
    }
    contents.height = static_cast<int>(height);
}

Notebook Notebook::WithSource(size_t index, const std::wstring& source, const CellLayout& layout) const {
    if (index >= data->cells.size() || source.size() > 1024 * 1024)
        throw std::invalid_argument("Invalid cell or source size");
    Utf8(source); // Reject malformed UTF-16 before it becomes canonical source.
    auto replacement = std::make_shared<Data>(*data);
    auto& cell = replacement->cells[index];
    auto content = std::make_shared<CellText>(*cell.text);
    if (content->source != source) {
        cell.sourceChanged = true;
        replacement->revision = NextCellId();
        cell.markdown.reset();
    }
    content->source = source;
    cell.text = std::move(content);
    cell.sourceMode = true;
    cell.sourceHeight = layout.sourceHeight;
    cell.outputHeight = layout.outputHeight;
    cell.contentWidth = layout.contentWidth;
    if(layout.sourceWidth)cell.sourceWidth=layout.sourceWidth;
    Reflow(*replacement);
    return Notebook(title, std::move(replacement));
}

Notebook Notebook::Insert(size_t index, const std::wstring& kind, const CellLayout& layout) const {
    if (index > data->cells.size() || data->cells.size() >= 10000 || layout.outputHeight!=0 || (kind != L"code" && kind != L"markdown"))
        throw std::invalid_argument("Invalid new cell");
    auto replacement = std::make_shared<Data>(*data);
    Cell cell;
    auto content = std::make_shared<CellText>();
    content->kind = kind;
    if (kind == L"code") content->prompt = L"In [ ]:";
    cell.text = std::move(content);
    std::string persistentId;
    cell.sourceMode = true;
    bool collision;
    do {
        cell.id = NextCellId();
        persistentId = "winip-" + std::to_string(cell.id);
        collision = std::any_of(data->cells.begin(), data->cells.end(), [&](const Cell& other) {
            return other.original->value("id", nlohmann::json()) == persistentId;
        });
    } while (collision);
    nlohmann::json original = {{"cell_type", Utf8(kind)}, {"source", ""}, {"metadata", nlohmann::json::object()}, {"id", persistentId}};
    if (kind == L"code") { original["outputs"] = nlohmann::json::array(); original["execution_count"] = nullptr; }
    cell.original = std::make_shared<const nlohmann::json>(std::move(original));
    cell.sourceHeight = layout.sourceHeight;
    cell.contentWidth = layout.contentWidth;
    if(layout.sourceWidth)cell.sourceWidth=layout.sourceWidth;
    replacement->cells.insert(replacement->cells.begin() + index, std::move(cell));
    replacement->revision = NextCellId();
    Reflow(*replacement);
    return Notebook(title, std::move(replacement));
}

Notebook Notebook::WithPresentation(size_t index, const CellLayout& layout, bool sourceMode) const {
    auto replacement=std::make_shared<Data>(*data);
    auto& cell=replacement->cells.at(index);
    cell.sourceHeight=layout.sourceHeight;cell.outputHeight=layout.outputHeight;cell.contentWidth=layout.contentWidth;
    if(layout.sourceWidth)cell.sourceWidth=layout.sourceWidth;
    cell.sourceMode=sourceMode;
    if(layout.markdown)cell.markdown=layout.markdown;
    Reflow(*replacement);
    return Notebook(title,std::move(replacement));
}

namespace {
nlohmann::json SerializeCell(const Cell& cell) {
    auto value = *cell.original;
    if (cell.sourceChanged) value["source"] = Utf8(cell.text->source);
    if (cell.execution) {
        value["outputs"] = cell.execution->at("outputs");
        value["execution_count"] = cell.execution->at("execution_count");
    }
    return value;
}
}
nlohmann::json Notebook::Serialize() const {
    auto result = nlohmann::json::object();
    for (const auto& field : Original().items())
        if (field.key() != "cells") result[field.key()] = field.value();
    auto cells = nlohmann::json::array();
    for (const auto& cell : data->cells) {
        cells.push_back(SerializeCell(cell));
    }
    result["cells"] = std::move(cells);
    return result;
}
void Notebook::Write(std::ostream& stream) const {
    stream << "{\n";
    for (const auto& field : Original().items()) {
        if (field.key() == "cells") continue;
        stream << nlohmann::json(field.key()) << ':' << std::setw(1) << field.value() << ",\n";
    }
    stream << "\"cells\":[\n";
    bool first = true;
    for (const auto& cell : data->cells) {
        if (!first) stream << ",\n";
        first = false;
        if (!cell.sourceChanged && !cell.execution) stream << std::setw(1) << *cell.original;
        else stream << std::setw(1) << SerializeCell(cell);
    }
    stream << "\n]}\n";
}
bool Notebook::SameContent(const Notebook& other) const {
    if (Original().size() != other.Original().size() || Cells().size() != other.Cells().size()) return false;
    for (const auto& field : Original().items()) {
        if (field.key() == "cells") continue;
        auto found = other.Original().find(field.key());
        if (found == other.Original().end() || field.value() != *found) return false;
    }
    for (size_t i = 0; i < Cells().size(); ++i) {
        const auto& a = Cells()[i];
        const auto& b = other.Cells()[i];
        if (!a.sourceChanged && !a.execution && !b.sourceChanged && !b.execution) {
            if (*a.original != *b.original) return false;
        } else if (SerializeCell(a) != SerializeCell(b)) return false;
    }
    return true;
}
Notebook Notebook::Delete(size_t index) const {
    if(index>=data->cells.size())throw std::invalid_argument("No cell selected");
    auto replacement=std::make_shared<Data>(*data);
    replacement->cells.erase(replacement->cells.begin()+index);
    replacement->revision=NextCellId();Reflow(*replacement);
    return Notebook(title,std::move(replacement));
}
Notebook Notebook::WithKind(size_t index,const std::wstring& kind) const {
    if(index>=data->cells.size() || (kind!=L"code" && kind!=L"markdown"))throw std::invalid_argument("Invalid cell type");
    if(data->cells[index].text->kind==kind)return *this;
    auto replacement=std::make_shared<Data>(*data);
    auto& cell=replacement->cells[index];
    auto content=std::make_shared<CellText>(*cell.text);
    auto original=*cell.original;
    original["cell_type"]=Utf8(kind);
    if(kind==L"code") {original["outputs"]=nlohmann::json::array();original["execution_count"]=nullptr;content->prompt=L"In [ ]:";}
    else {original.erase("outputs");original.erase("execution_count");content->prompt.clear();}
    content->kind=kind;content->output.clear();
    cell.text=std::move(content);cell.original=std::make_shared<const nlohmann::json>(std::move(original));
    cell.outputHeight=0;cell.output.reset();cell.markdown.reset();cell.execution.reset();cell.sourceMode=true;
    replacement->revision=NextCellId();Reflow(*replacement);
    return Notebook(title,std::move(replacement));
}

Notebook Notebook::WithExecution(uint64_t cellId,const Cell& prepared) const {
    auto found=std::find_if(data->cells.begin(),data->cells.end(),[&](const Cell& cell){return cell.id==cellId;});
    if(found==data->cells.end() || found->text->kind!=L"code" || found->text->source!=prepared.text->source)return *this;
    auto replacement=std::make_shared<Data>(*data);
    auto& cell=replacement->cells[found-data->cells.begin()];
    cell.text=prepared.text;cell.output=prepared.output;cell.execution=prepared.original;
    cell.outputHeight=prepared.outputHeight;cell.contentWidth=std::max(cell.sourceWidth,prepared.contentWidth);
    replacement->revision=NextCellId();Reflow(*replacement);
    return Notebook(title,std::move(replacement));
}
