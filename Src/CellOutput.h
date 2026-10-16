#pragma once
#include "NotebookImage.h"
#include "../ThirdParty/nlohmann/json.hpp"
#include <memory>
#include <string>
#include <vector>
#include <set>
struct OutputBlock {
    std::wstring text;
    std::shared_ptr<const NotebookImage> image;
    float top=0,width=0,height=0;
    float naturalWidth=0,naturalHeight=0;
};
class CellOutput {
public:
    explicit CellOutput(const nlohmann::json& outputs);
    const std::vector<OutputBlock>& Blocks() const;
    std::shared_ptr<const CellOutput> Fit(float width,float height,const std::set<std::shared_ptr<const NotebookImage>>& expanded) const;
    const std::wstring& PlainText() const;
    int Width() const;
    int Height() const;
private:
    std::vector<OutputBlock> blocks;
    std::wstring plainText;
    int width=0,height=0;
};
