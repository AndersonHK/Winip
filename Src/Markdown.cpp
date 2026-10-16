#include "Markdown.h"
#include "LoadMetrics.h"
#include "Notebook.h"
#include "MarkdownInline.h"
#include <filesystem>
#include <functional>
#include "../ThirdParty/md4c/md4c.h"
extern "C" {
#include "../ThirdParty/md4c/entity.h"
}
#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>

namespace {
void CheckMarkdown(HRESULT value) {if(FAILED(value))throw std::runtime_error("Cannot shape Markdown preview");}
std::string DecodeImagePath(std::string_view uri) {
    std::string decoded;
    auto hex=[](char ch)->int {
        if(ch>='0' && ch<='9')return ch-'0';
        if(ch>='a' && ch<='f')return ch-'a'+10;
        if(ch>='A' && ch<='F')return ch-'A'+10;
        return -1;
    };
    for(size_t i=0;i<uri.size();++i) {
        if(uri[i]=='%' && i+2<uri.size() && hex(uri[i+1])>=0 && hex(uri[i+2])>=0) {
            decoded+=char(hex(uri[i+1])*16+hex(uri[i+2]));i+=2;
        } else decoded+=uri[i];
    }
    return decoded;
}
struct Style {float size=18; bool bold=false,italic=false,mono=false,underline=false,strike=false;};
struct Run {UINT32 start,length;Style style;};
struct ImageRun { UINT32 position; std::shared_ptr<const NotebookImage> image; };
struct Parser {
    std::function<std::shared_ptr<const NotebookImage>(std::string_view)> resolve;
    std::vector<ImageRun> images;
    bool inImage=false, deferredImages=false, resolveImages=true;
    std::shared_ptr<const NotebookImage> pendingImage;
    std::wstring imageAlt, imageError;
    std::wstring text;
    std::vector<Run> runs;
    std::vector<std::pair<bool,unsigned>> lists;
    std::vector<Style> spans;
    Style style;
    int quote=0,items=0;
    bool code=false;
    size_t headingStart=0;
    std::vector<std::pair<size_t,size_t>> headings;
    std::exception_ptr error;
    template<class Action> static int Guard(void* user,Action action) {
        auto& parser=*static_cast<Parser*>(user);
        try {action(parser);return 0;}catch(...){parser.error=std::current_exception();return 1;}
    }
    void Append(const std::wstring& value) {
        if(value.empty())return;
        runs.push_back({static_cast<UINT32>(text.size()),static_cast<UINT32>(value.size()),style});text+=value;
    }
    void Break(int count) {
        if(text.empty())return;
        size_t trailing=0;for(auto it=text.rbegin();it!=text.rend() && *it==L'\n';++it)++trailing;
        if(trailing<static_cast<size_t>(count))Append(std::wstring(count-trailing,L'\n'));
    }
    static int EnterBlock(MD_BLOCKTYPE type,void* detail,void* user) {
        return Guard(user,[&](Parser& p){
            if(type==MD_BLOCK_UL || type==MD_BLOCK_OL) {
                p.Break(1);p.lists.push_back({type==MD_BLOCK_OL,type==MD_BLOCK_OL?static_cast<MD_BLOCK_OL_DETAIL*>(detail)->start:0});
            } else if(type==MD_BLOCK_LI) {
                p.Break(1);++p.items;
                p.Append(std::wstring((p.lists.size()-1)*4,L' '));
                const auto task=static_cast<MD_BLOCK_LI_DETAIL*>(detail);
                if(task->is_task)p.Append(task->task_mark==' '?L"☐ ":L"☑ ");
                else if(!p.lists.empty() && p.lists.back().first)p.Append(std::to_wstring(p.lists.back().second++)+L". ");
                else p.Append(L"• ");
            } else if(type==MD_BLOCK_QUOTE) {p.Break(2);++p.quote;}
            else if(type==MD_BLOCK_H) {
                p.Break(2);p.headingStart=p.text.size();auto level=static_cast<MD_BLOCK_H_DETAIL*>(detail)->level;
                const float sizes[]={27,24,22,20,19,18};p.style.size=sizes[std::clamp(level,1u,6u)-1];p.style.bold=true;
            } else if(type==MD_BLOCK_CODE) {p.Break(2);p.code=true;p.style.mono=true;p.style.size=17;}
            else if(type==MD_BLOCK_P) {if(!p.items)p.Break(2);if(p.quote)p.Append(L"│ ");}
            else if(type==MD_BLOCK_HR) {p.Break(2);p.Append(L"────────────────────────");p.Break(2);}
        });
    }
    static int LeaveBlock(MD_BLOCKTYPE type,void*,void* user) {
        return Guard(user,[&](Parser& p){
            if(type==MD_BLOCK_H)p.headings.push_back({p.headingStart,p.text.size()-p.headingStart});
            if(type==MD_BLOCK_H || type==MD_BLOCK_CODE) {p.Break(2);p.style={};p.code=false;}
            else if(type==MD_BLOCK_P)p.Break(p.items?1:2);
            else if(type==MD_BLOCK_LI) {p.Break(1);--p.items;}
            else if(type==MD_BLOCK_UL || type==MD_BLOCK_OL) {p.lists.pop_back();p.Break(p.lists.empty()?2:1);}
            else if(type==MD_BLOCK_QUOTE) {--p.quote;p.Break(2);}
        });
    }
    static int EnterSpan(MD_SPANTYPE type,void* detail,void* user) {
        return Guard(user,[&](Parser& p){
            p.spans.push_back(p.style);
            if(type==MD_SPAN_EM)p.style.italic=true;
            else if(type==MD_SPAN_STRONG)p.style.bold=true;
            else if(type==MD_SPAN_CODE)p.style.mono=true;
            else if(type==MD_SPAN_A)p.style.underline=true;
            else if(type==MD_SPAN_DEL)p.style.strike=true;
            else if(type==MD_SPAN_IMG) {
                p.Break(2);p.inImage=true;if(!p.resolveImages)p.deferredImages=true;p.imageAlt.clear();p.imageError.clear();p.pendingImage.reset();
                const auto& src=static_cast<MD_SPAN_IMG_DETAIL*>(detail)->src;
                try {p.pendingImage=p.resolve(std::string_view(src.text,src.size));}
                catch(const std::exception&) {p.imageError=p.resolveImages?L"Image unavailable":L"Loading image";}
            }
        });
    }
    static int LeaveSpan(MD_SPANTYPE type,void*,void* user) {
        return Guard(user,[&](Parser& p){
            if(type==MD_SPAN_IMG) {
                p.inImage=false;
                if(p.pendingImage) {p.images.push_back({static_cast<UINT32>(p.text.size()),std::move(p.pendingImage)});p.Append(L"\ufffc");}
                else p.Append(L"["+p.imageError+L": "+p.imageAlt+L"]");
                p.Break(2);
            }
            p.style=p.spans.back();p.spans.pop_back();
        });
    }
    void Codepoint(unsigned value) {
        if(!value || value>0x10ffff || (value>=0xd800 && value<=0xdfff))value=0xfffd;
        if(value<=0xffff)Append(std::wstring(1,static_cast<wchar_t>(value)));
        else {value-=0x10000;Append({static_cast<wchar_t>(0xd800+(value>>10)),static_cast<wchar_t>(0xdc00+(value&1023))});}
    }
    static int Text(MD_TEXTTYPE type,const MD_CHAR* data,MD_SIZE size,void* user) {
        return Guard(user,[&](Parser& p){
            if(p.inImage) {p.imageAlt+=Utf16(std::string(data,size));return;}
            if(type==MD_TEXT_BR)p.Append(L"\n");
            else if(type==MD_TEXT_SOFTBR)p.Append(L" ");
            else if(type==MD_TEXT_NULLCHAR)p.Append(L"�");
            else if(type==MD_TEXT_ENTITY) {
                if(size>3 && data[1]=='#') {
                    std::string entity(data+2,size-3);unsigned value=0;
                    try {value=static_cast<unsigned>(std::stoul(entity,nullptr,(entity[0]=='x' || entity[0]=='X')?16:10));}
                    catch(...) {
                        if(entity[0]=='x' || entity[0]=='X')try{value=static_cast<unsigned>(std::stoul(entity.substr(1),nullptr,16));}catch(...){value=0xfffd;}
                        else value=0xfffd;
                    }
                    p.Codepoint(value);
                } else if(auto entity=entity_lookup(data,size)) {p.Codepoint(entity->codepoints[0]);if(entity->codepoints[1])p.Codepoint(entity->codepoints[1]);}
                else p.Append(Utf16(std::string(data,size)));
            } else p.Append(Utf16(std::string(data,size)));
        });
    }
};
}
Markdown::Markdown(Microsoft::WRL::ComPtr<IDWriteTextLayout> shaped,std::wstring value,int w,int h)
    :layout(std::move(shaped)),text(std::move(value)),width(w),height(h) {}
Markdown::~Markdown()=default;
IDWriteTextLayout* Markdown::Layout() const{return layout.Get();}
const std::wstring& Markdown::Text() const{return text;}
int Markdown::Width() const{return width;}
int Markdown::Height() const{return height;}
const std::vector<MarkdownHeading>& Markdown::Headings() const{return headings;}
struct Markdown::Content {
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fonts;
    std::wstring text;
    std::vector<Run> runs;
    std::vector<ImageRun> images;
    std::vector<std::pair<size_t,size_t>> headings;
    bool deferredImages=false;
    float naturalWidth=0;
};
bool Markdown::NeedsImages() const {return content->deferredImages;}
const std::vector<MarkdownImage>& Markdown::Images() const {return images;}
std::shared_ptr<const Markdown> Markdown::Reflow(float w,float h,const ExpandedImages& expanded) const {
    // A viewport wider than the fully shaped text cannot change its line breaks.
    // Reuse the same layout/raster identity instead of invalidating short headings.
    if(content->images.empty())w=std::min(w,std::max(1.f,content->naturalWidth));
    bool unchanged=w==layoutWidth;
    for(const auto& placed:images) {
        const float factor=expanded.count(placed.image)?1.f:std::min({1.f,w/placed.image->Width(),h/placed.image->Height()});
        unchanged=unchanged && placed.width==placed.image->Width()*factor && placed.height==placed.image->Height()*factor;
    }
    if(unchanged)return shared_from_this();
    return Shape(content,w,h,expanded);
}
std::shared_ptr<const Markdown> Markdown::Prepare(IDWriteFactory* factory,IDWriteFontCollection* fonts,const std::wstring& source, const nlohmann::json* attachments, const std::wstring& directory, bool resolveImages) {
    LoadMetrics::Scope timing(LoadMetrics::MarkdownPrepare);
    Parser parsed;parsed.resolveImages=resolveImages;
    parsed.resolve=[&](std::string_view uri)->std::shared_ptr<const NotebookImage> {
        if(!resolveImages)throw std::runtime_error("Image loading queued");
        if(uri.rfind("data:image/",0)==0) {
            const auto comma=uri.find(',');
            if(comma==std::string::npos || uri.substr(0,comma).find(";base64")==std::string::npos)
                throw std::runtime_error("Only base64 image data URLs are supported");
            return std::make_shared<const NotebookImage>(uri.substr(comma+1));
        }
        if(uri.rfind("attachment:",0)==0) {
            if(!attachments)throw std::runtime_error("Missing image attachment");
            const auto& data=attachments->at(DecodeImagePath(uri.substr(11)));
            const auto& encoded=data.at(data.contains("image/png")?"image/png":"image/jpeg");
            std::string bytes;
            if(encoded.is_string())return std::make_shared<const NotebookImage>(encoded.get_ref<const std::string&>());
            else for(const auto& line:encoded)bytes+=line.get<std::string>();
            return std::make_shared<const NotebookImage>(bytes);
        }
        if(uri.find(':')!=std::string::npos || uri.rfind("//",0)==0 || directory.empty())
            throw std::runtime_error("Remote image URLs are not loaded");
        const auto decoded=DecodeImagePath(uri);
        const auto relative=std::filesystem::path(Utf16(decoded));
        if(relative.has_root_path() || decoded.find(':')!=std::string::npos || decoded.find('\0')!=std::string::npos)throw std::runtime_error("Expected a relative image path");
        return NotebookImage::Load((std::filesystem::path(directory)/relative).wstring());
    };
    MD_PARSER parser{};parser.flags=MD_FLAG_PERMISSIVEATXHEADERS|MD_FLAG_NOHTML|MD_FLAG_STRIKETHROUGH|MD_FLAG_TASKLISTS;
    parser.enter_block=Parser::EnterBlock;parser.leave_block=Parser::LeaveBlock;parser.enter_span=Parser::EnterSpan;parser.leave_span=Parser::LeaveSpan;parser.text=Parser::Text;
    auto utf8=Utf8(source);int parseResult=md_parse(utf8.data(),static_cast<MD_SIZE>(utf8.size()),&parser,&parsed);
    if(parsed.error)std::rethrow_exception(parsed.error);
    if(parseResult)throw std::runtime_error("Cannot parse Markdown");
    while(!parsed.text.empty() && parsed.text.back()==L'\n')parsed.text.pop_back();
    auto stored=std::make_shared<Content>();
    stored->deferredImages=parsed.deferredImages;
    stored->factory=factory;stored->fonts=fonts;stored->text=std::move(parsed.text);
    stored->runs=std::move(parsed.runs);stored->images=std::move(parsed.images);stored->headings=std::move(parsed.headings);
    auto result=Shape(stored,16777216,16777216,{});
    stored->naturalWidth=float(result->Width()-4);
    return result;
}
std::shared_ptr<const Markdown> Markdown::Shape(std::shared_ptr<const Content> stored,float availableWidth,float availableHeight,const ExpandedImages& expanded) {
    LoadMetrics::Scope timing(LoadMetrics::TextLayout);
    const auto& parsed=*stored;
    auto factory=stored->factory.Get();auto fonts=stored->fonts.Get();
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    CheckMarkdown(factory->CreateTextFormat(L"Noto Sans",fonts,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,18,L"en-us",&format));
    CheckMarkdown(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    CheckMarkdown(factory->CreateTextLayout(parsed.text.data(),static_cast<UINT32>(parsed.text.size()),format.Get(),availableWidth,16777216,&layout));
    for(const auto& run:parsed.runs) {
        if(run.start>=parsed.text.size())break;
        DWRITE_TEXT_RANGE range{run.start,std::min(run.length,static_cast<UINT32>(parsed.text.size()-run.start))};
        CheckMarkdown(layout->SetFontSize(run.style.size,range));
        CheckMarkdown(layout->SetFontWeight(run.style.bold?DWRITE_FONT_WEIGHT_BOLD:DWRITE_FONT_WEIGHT_NORMAL,range));
        CheckMarkdown(layout->SetFontStyle(run.style.italic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,range));
        if(run.style.mono)CheckMarkdown(layout->SetFontFamilyName(L"Noto Sans Mono",range));
        if(run.style.underline)CheckMarkdown(layout->SetUnderline(TRUE,range));
        if(run.style.strike)CheckMarkdown(layout->SetStrikethrough(TRUE,range));
    }
    std::vector<MarkdownImage> placed;
    for(const auto& item:parsed.images) {
        const float naturalWidth=float(item.image->Width()),naturalHeight=float(item.image->Height());
        const float scale=expanded.count(item.image)?1.f:std::min({1.f,availableWidth/naturalWidth,availableHeight/naturalHeight});
        Microsoft::WRL::ComPtr<IDWriteInlineObject> object;
        object.Attach(new MarkdownInline(naturalWidth*scale,naturalHeight*scale));
        CheckMarkdown(layout->SetInlineObject(object.Get(),{item.position,1}));
        placed.push_back({item.image,0,0,naturalWidth*scale,naturalHeight*scale});
    }
    DWRITE_TEXT_METRICS metrics{};CheckMarkdown(layout->GetMetrics(&metrics));
    auto result=std::shared_ptr<Markdown>(new Markdown(layout,parsed.text,static_cast<int>(std::ceil(metrics.widthIncludingTrailingWhitespace))+4,static_cast<int>(std::ceil(metrics.height))));
    result->content=std::move(stored);
    result->layoutWidth=availableWidth;
    for(size_t i=0;i<placed.size();++i) {
        DWRITE_HIT_TEST_METRICS hit{};
        CheckMarkdown(layout->HitTestTextPosition(parsed.images[i].position,FALSE,&placed[i].x,&placed[i].y,&hit));
    }
    result->images=std::move(placed);
    for(const auto& heading:parsed.headings) {
        FLOAT x=0,y=0;DWRITE_HIT_TEST_METRICS hit{};
        CheckMarkdown(layout->HitTestTextPosition(static_cast<UINT32>(heading.first),FALSE,&x,&y,&hit));
        auto label=result->text.substr(heading.first,std::min<size_t>(heading.second,160));
        if(heading.second>160) {if(!label.empty() && label.back()>=0xd800 && label.back()<=0xdbff)label.pop_back();label+=L"…";}
        result->headings.push_back({std::move(label),y});
    }
    return result;
}
