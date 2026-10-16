#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <iosfwd>
#include "../ThirdParty/nlohmann/json.hpp"
class Markdown;
class CellOutput;

struct CellText {
    std::wstring kind, source, output, prompt;
};
struct Cell {
    std::shared_ptr<const CellText> text;
    std::shared_ptr<const nlohmann::json> original;
    uint64_t id = 0;
    std::shared_ptr<const nlohmann::json> execution;
    bool sourceChanged = false;
    bool sourceMode = false;
    std::shared_ptr<const Markdown> markdown;
    std::shared_ptr<const CellOutput> output;
    int top = 0, height = 0;
    int sourceHeight = 0, outputHeight = 0, contentWidth = 0, sourceWidth = 0;
};
struct CellLayout {
    int sourceHeight = 0;
    int outputHeight = 0;
    int contentWidth = 0;
    std::shared_ptr<const Markdown> markdown;
    std::shared_ptr<const CellOutput> output;
    int sourceWidth = 0;
};

// Value snapshot: all observable content is immutable and copies share storage.
// Layout replacements are validated completely before a new snapshot is returned.
class Notebook {
public:
    Notebook();
    ~Notebook();
    Notebook(const Notebook& other);
    Notebook& operator=(Notebook other);

    static Notebook Load(const std::wstring& path, std::function<bool()> cancelled = {});
    static Notebook FromJson(nlohmann::json original, std::wstring title, std::wstring directory=L"");
    const nlohmann::json& Original() const;
    const std::vector<Cell>& Cells() const;
    const std::wstring& Title() const;
    const std::wstring& Directory() const;
    int Height() const;
    int ContentWidth() const;
    bool HasMeasuredLayout() const;
    uint64_t ContentRevision() const;
    Notebook WithTitle(std::wstring title) const;
    Notebook WithLayout(const std::vector<CellLayout>& layout) const;
    Notebook WithSource(size_t index, const std::wstring& source, const CellLayout& layout) const;
    Notebook WithPresentation(size_t index, const CellLayout& layout, bool sourceMode) const;
    Notebook Insert(size_t index, const std::wstring& kind, const CellLayout& layout) const;
    Notebook Delete(size_t index) const;
    Notebook WithKind(size_t index, const std::wstring& kind) const;
    Notebook WithExecution(uint64_t cellId, const Cell& prepared) const;
    nlohmann::json Serialize() const;
    void Write(std::ostream& stream) const;
    bool SameContent(const Notebook& other) const;

private:
    struct Data;
    Notebook(std::wstring title, std::shared_ptr<const Data> data);
    void Swap(Notebook& other) noexcept;
    static void Reflow(Data& data);

    std::wstring title;
    std::shared_ptr<const Data> data;
};
std::wstring Utf16(const std::string& text);
std::string Utf8(const std::wstring& text);
Notebook LoadNotebook(const std::wstring& path);
bool ValidNotebookTitle(const std::wstring& title);
// Same-directory rename, never replaces an existing destination.
std::wstring RenameNotebookFile(const std::wstring& path,const std::wstring& title);
