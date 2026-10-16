#include "CellOutput.h"
#include "Notebook.h"
#include "FontSet.h"
#include <dwrite.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
using Microsoft::WRL::ComPtr;
namespace {
std::string Lines(const nlohmann::json& value) {
    if(value.is_string())return value.get<std::string>();
    std::string result;for(const auto& line:value)result+=line.get<std::string>();return result;
}
void CheckOutput(HRESULT hr) {if(FAILED(hr))throw std::runtime_error("Cannot prepare image output text");}
}
CellOutput::CellOutput(const nlohmann::json& outputs) {
    ComPtr<IDWriteFactory> factory;CheckOutput(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.GetAddressOf())));
    FontSet fonts(factory.Get());ComPtr<IDWriteTextFormat> format;
    CheckOutput(factory->CreateTextFormat(L"Noto Sans Mono",fonts.Collection(),DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,17,L"en-us",&format));
    CheckOutput(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    for(const auto& output:outputs) {
        OutputBlock block;
        static const auto emptyData=nlohmann::json::object();
        const auto& data=output.contains("data")?output.at("data"):emptyData;
        auto type=output.value("output_type",std::string());
        const char* mime=data.contains("image/png")?"image/png":data.contains("image/jpeg")?"image/jpeg":nullptr;
        if(mime) {
            try {
                const auto& encoded=data.at(mime);
                block.image=encoded.is_string()
                    ?std::make_shared<const NotebookImage>(encoded.get_ref<const std::string&>())
                    :std::make_shared<const NotebookImage>(Lines(encoded));
                block.width=float(block.image->Width());block.height=float(block.image->Height());
                auto metadata=output.value("metadata",nlohmann::json::object()).value(mime,nlohmann::json::object());
                auto dimension=[&](const char* key,float fallback) {
                    if(!metadata.contains(key))return fallback;
                    float value=metadata.at(key).get<float>();
                    if(!std::isfinite(value) || value<=0 || value>16384)throw std::runtime_error("Invalid image display dimension");
                    return value;
                };
                float requestedWidth=dimension("width",block.width),requestedHeight=dimension("height",block.height);
                if(metadata.contains("width") && !metadata.contains("height"))requestedHeight=block.height*requestedWidth/block.width;
                if(metadata.contains("height") && !metadata.contains("width"))requestedWidth=block.width*requestedHeight/block.height;
                if(requestedWidth>16384 || requestedHeight>16384)throw std::runtime_error("Image aspect ratio exceeds display dimension limit");
                block.width=requestedWidth;block.height=requestedHeight;
                plainText+=L"[Image]\n";
            } catch(const std::exception& e) {block.image.reset();block.text=L"[Image unavailable: "+Utf16(e.what())+L"]\n";}
        } else if(type=="stream")block.text=Utf16(Lines(output.at("text")));
        else if(type=="error")block.text=Utf16(output.value("ename","Error")+": "+output.value("evalue",""))+L"\n";
        else if(data.contains("text/plain"))block.text=Utf16(Lines(data.at("text/plain")))+L"\n";
        else block.text=L"[Rich output is preserved; preview is not available yet.]\n";
        if(!block.image) {
            plainText+=block.text;
            ComPtr<IDWriteTextLayout> layout;
            CheckOutput(factory->CreateTextLayout(block.text.data(),static_cast<UINT32>(block.text.size()),format.Get(),16777216,16777216,&layout));
            DWRITE_TEXT_METRICS metrics{};CheckOutput(layout->GetMetrics(&metrics));
            block.width=metrics.widthIncludingTrailingWhitespace;block.height=metrics.height;
        }
        block.naturalWidth=block.width;block.naturalHeight=block.height;
        block.top=float(height);height+=int(std::ceil(block.height))+8;width=std::max(width,int(std::ceil(block.width))+4);
        blocks.push_back(std::move(block));
    }
}
const std::vector<OutputBlock>& CellOutput::Blocks() const{return blocks;}
const std::wstring& CellOutput::PlainText() const{return plainText;}
int CellOutput::Width() const{return width;}
int CellOutput::Height() const{return height;}

std::shared_ptr<const CellOutput> CellOutput::Fit(float availableWidth,float availableHeight,const std::set<std::shared_ptr<const NotebookImage>>& expanded) const {
    auto result=std::make_shared<CellOutput>(*this);
    result->width=0;result->height=0;
    for(auto& block:result->blocks) {
        float factor=block.image && !expanded.count(block.image)?std::min({1.f,availableWidth/block.naturalWidth,availableHeight/block.naturalHeight}):1.f;
        block.width=block.naturalWidth*factor;block.height=block.naturalHeight*factor;
        block.top=float(result->height);result->height+=int(std::ceil(block.height))+8;
        result->width=std::max(result->width,int(std::ceil(block.width))+4);
    }
    return result;
}
