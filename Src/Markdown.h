#pragma once
#include "framework.h"
#include <dwrite.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include "NotebookImage.h"
#include "../ThirdParty/nlohmann/json.hpp"
struct MarkdownImage { std::shared_ptr<const NotebookImage> image; float x=0,y=0,width=0,height=0; };
using ExpandedImages = std::set<std::shared_ptr<const NotebookImage>>;
struct MarkdownHeading {std::wstring text; float y;};

// Parsed and shaped once per source revision; immutable across UI/render owners.
class Markdown : public std::enable_shared_from_this<Markdown> {
public:
    static std::shared_ptr<const Markdown> Prepare(IDWriteFactory* factory, IDWriteFontCollection* fonts, const std::wstring& source, const nlohmann::json* attachments=nullptr, const std::wstring& directory=L"", bool resolveImages=true);
    std::shared_ptr<const Markdown> Reflow(float width, float height, const ExpandedImages& expanded) const;
    const std::vector<MarkdownImage>& Images() const;
    bool NeedsImages() const;
    ~Markdown();
    Markdown(const Markdown&)=delete;
    Markdown& operator=(const Markdown&)=delete;
    IDWriteTextLayout* Layout() const;
    const std::wstring& Text() const;
    int Width() const;
    int Height() const;
    const std::vector<MarkdownHeading>& Headings() const;
private:
    float layoutWidth=0;
    struct Content;
    std::shared_ptr<const Content> content;
    static std::shared_ptr<const Markdown> Shape(std::shared_ptr<const Content> content, float width, float height, const ExpandedImages& expanded);
    std::vector<MarkdownImage> images;
    Markdown(Microsoft::WRL::ComPtr<IDWriteTextLayout> layout, std::wstring text, int width, int height);
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    std::wstring text;
    int width, height;
    std::vector<MarkdownHeading> headings;
};
