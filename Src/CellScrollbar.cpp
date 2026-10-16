#include "CellScrollbar.h"
#include <algorithm>

int CellScrollbar::ThumbWidth(int page,int content) {
    if(page<=0 || content<=page)return std::max(0,page);
    return std::min(page,std::max(24,static_cast<int>(int64_t(page)*page/content)));
}
int CellScrollbar::ThumbLeft(int page,int content,int scroll) {
    const int maximum=std::max(0,content-page),travel=page-ThumbWidth(page,content);
    return maximum?static_cast<int>(int64_t(travel)*std::clamp(scroll,0,maximum)/maximum):0;
}
void CellScrollbar::Begin(uint64_t cell,int pointer,int page,int content,int scroll) {
    cellId=cell;
    const int thumb=ThumbWidth(page,content),left=ThumbLeft(page,content,scroll);
    offset=pointer>=left && pointer<left+thumb?pointer-left:thumb/2;
}
int CellScrollbar::Drag(int pointer,int page,int content) const {
    const int travel=page-ThumbWidth(page,content),maximum=std::max(0,content-page);
    return travel>0?static_cast<int>(int64_t(std::clamp(pointer-offset,0,travel))*maximum/travel):0;
}
void CellScrollbar::End(){cellId=0;offset=0;}
uint64_t CellScrollbar::CellId() const{return cellId;}
