#pragma once
#include <cstdint>
class Renderer;
class Notebook;

struct NavigatorGeometry {
    float x=0,y=0,width=22,height=0,thumbY=0,thumbHeight=0,thumbInset=3;
    int maximum=0,windowWidth=0;
    bool fitsMargin=false;
};
// Copyable view state; the renderer receives an immutable frame snapshot.
class NotebookNavigator {
public:
    static NavigatorGeometry Geometry(int width,int height,int documentHeight,int scroll);
    static bool Hit(float x,float y,const NavigatorGeometry& geometry);
    void Begin(float pointer,const NavigatorGeometry& geometry,uint64_t now);
    int Drag(float pointer,const NavigatorGeometry& geometry) const;
    void End(uint64_t now);
    bool Held() const;
    bool Hover(float x,float y,const NavigatorGeometry& geometry);
    bool Visible(const NavigatorGeometry& geometry) const;
    float Expansion(uint64_t now) const;
    bool Animating(uint64_t now) const;
    void Draw(Renderer& renderer,const Notebook& document,const NavigatorGeometry& geometry,uint64_t now) const;
private:
    void Transition(float target,uint64_t now);
    bool held=false;
    float pointerX=-1,pointerY=-1;
    float offset=0,from=0,to=0;
    uint64_t started=0;
};
