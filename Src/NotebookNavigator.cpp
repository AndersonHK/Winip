#include "NotebookNavigator.h"
#include "Notebook.h"
#include "NotebookViewport.h"
#include "Markdown.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#include <vector>

NavigatorGeometry NotebookNavigator::Geometry(int width,int height,int documentHeight,int scroll) {
    NavigatorGeometry g;g.windowWidth=width;
    g.x=float(width)-42;g.y=190;g.height=std::max(0.f,float(height)-244);
    // Leave eight DIPs between the paper's three-DIP shadow and the rail.
    const int paperRight=width-NotebookViewport::SideMargin(width);
    g.fitsMargin=g.x>=paperRight+3+8;
    const int page=std::max(22,height-232);
    g.maximum=std::max(0,documentHeight-page);
    g.thumbHeight=g.maximum?std::min(g.height,std::max(40.f,g.height*page/std::max(1,documentHeight))):g.height;
    g.thumbY=g.y+(g.maximum?(g.height-g.thumbHeight)*std::clamp(scroll,0,g.maximum)/g.maximum:0);
    return g;
}
bool NotebookNavigator::Hit(float x,float y,const NavigatorGeometry& g) {
    return g.maximum>0 && g.height>0 && x>=g.x-5 && x<=g.x+g.width+5 && y>=g.y && y<=g.y+g.height;
}
void NotebookNavigator::Transition(float target,uint64_t now) {from=Expansion(now);to=target;started=now;}
void NotebookNavigator::Begin(float pointer,const NavigatorGeometry& g,uint64_t now) {
    held=true;offset=pointer>=g.thumbY && pointer<=g.thumbY+g.thumbHeight?pointer-g.thumbY:g.thumbHeight/2;
    Transition(1,now);
}
int NotebookNavigator::Drag(float pointer,const NavigatorGeometry& g) const {
    const float travel=g.height-g.thumbHeight;
    return travel>0?int(std::lround(std::clamp((pointer-offset-g.y)/travel,0.f,1.f)*g.maximum)):0;
}
void NotebookNavigator::End(uint64_t now) {held=false;Transition(0,now);}
bool NotebookNavigator::Held() const{return held;}
bool NotebookNavigator::Hover(float x,float y,const NavigatorGeometry& g) {
    bool before=Visible(g);pointerX=x;pointerY=y;return before!=Visible(g);
}
bool NotebookNavigator::Visible(const NavigatorGeometry& g) const {
    return g.fitsMargin || held || (g.maximum>0 && pointerX>=g.x-130 && pointerX<=g.windowWidth && pointerY>=g.y-14 && pointerY<=g.y+g.height+14);
}
float NotebookNavigator::Expansion(uint64_t now) const {
    float t=std::min(1.f,float(now-started)/180.f);t=t*t*(3-2*t);
    return from+(to-from)*t;
}
bool NotebookNavigator::Animating(uint64_t now) const {return from!=to && now-started<180;}
void NotebookNavigator::Draw(Renderer& renderer,const Notebook& doc,const NavigatorGeometry& g,uint64_t now) const {
    if(!g.maximum || g.height<=0 || (!Visible(g) && !Animating(now)))return;
    const float expansion=Expansion(now),focus=g.thumbY+g.thumbHeight/2;
    if(expansion>0) {
        renderer.PushClip(8,g.y-22,g.x+g.width+4,g.y+g.height+24);
        const float radius=std::max(65.f,g.height*.24f);
        auto proximity=[&](float y){float d=(y-focus)/radius;return std::exp(-d*d*2);};
        // Marks at the same DIP row naturally coincide. Combine those marks
        // once, preserving their density in length/ink instead of issuing
        // thousands of identical draw calls for very dense notebooks.
        int last=-1,count=0;
        auto mark=[&] {
            if(last<0)return;
            float weight=proximity(float(last));
            float length=(18+88*weight+std::min(12,count-1))*expansion;
            renderer.RoundedBox(g.x-12-length,float(last),length,2,1,RGB(130,130,130),expansion*.85f);
        };
        struct Label {const std::wstring* text;float y,weight;};
        std::vector<Label> labels;
        for(const auto& cell:doc.Cells()) {
            int row=int(g.y+g.height*(cell.top+cell.height*.5f)/std::max(1,doc.Height()));
            if(row!=last){mark();last=row;count=0;}++count;
            if(cell.markdown)for(const auto& heading:cell.markdown->Headings()) {
                float y=g.y+g.height*(cell.top+heading.y)/std::max(1,doc.Height());
                labels.push_back({&heading.text,y,proximity(y)});
            }
        }
        mark();
        std::sort(labels.begin(),labels.end(),[](const Label& a,const Label& b){return a.weight>b.weight;});
        std::vector<std::pair<float,float>> occupied;
        for(const auto& label:labels) {
            float size=11+7*label.weight*expansion;
            float y=label.y-size*.65f;
            // Keep labels readable in dense regions; labels nearest the thumb
            // get first placement, and change as the thumb moves.
            bool collision=false;
            for(const auto& range:occupied)if(y<range.second+4 && y+size+5>range.first-4){collision=true;break;}
            if(collision)continue;
            occupied.push_back({y,y+size+5});
            float width=std::min(240.f,g.x-30),distance=12+(34+110*label.weight)*expansion;
            float labelWidth=renderer.MeasureText(*label.text,width,size,true,true);
            float x=std::max(12.f,g.x-labelWidth-distance);
            int gray=int(255-(255-80)*expansion);
            renderer.Text(*label.text,x,y,width,size+7,size,RGB(gray,gray,gray),false,true,true,true);
        }
        renderer.PopClip();
    }
    renderer.RoundedBox(g.x,g.y,g.width,g.height,11,RGB(147,147,147),.8f);
    renderer.RoundedBox(g.x+2,g.y+2,g.width-4,g.height-4,9,RGB(255,255,255),.96f);
    renderer.RoundedBox(g.x+3,g.thumbY+2,g.width-6,std::max(0.f,g.thumbHeight-4),8,held?RGB(153,153,153):RGB(187,187,187));
}
