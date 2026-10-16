#include "NotebookShortcuts.h"
#include "framework.h"

NotebookShortcuts::NotebookShortcuts()=default;
NotebookShortcuts::~NotebookShortcuts()=default;
void NotebookShortcuts::Reset(){deletePending=false;}
NotebookShortcuts::Action NotebookShortcuts::Key(unsigned key,bool control,bool shift,bool editing,bool repeat,uint64_t nowMs) {
    if(key==VK_ESCAPE || (control && key=='M')) {Reset();return Action::ExitEdit;}
    if(key==VK_RETURN) {
        Reset();
        if(shift && control)return Action::RunAll;
        if(shift)return Action::RenderNext;
        if(control)return Action::RenderStay;
        return editing?Action::None:Action::EnterEdit;
    }
    if(editing || control) {Reset();return Action::None;}
    if(key=='D') {
        if(repeat)return Action::None;
        bool second=deletePending && nowMs-deleteAt<=1000;
        deletePending=!second;deleteAt=nowMs;
        return second?Action::DeleteCell:Action::None;
    }
    Reset();
    switch(key) {
    case 'A':return Action::InsertAbove;
    case 'B':return Action::InsertBelow;
    case 'M':return Action::Markdown;
    case 'Y':return Action::Code;
    case 'Z':return shift?Action::Redo:Action::Undo;
    case 'K':case VK_UP:return Action::Previous;
    case 'J':case VK_DOWN:return Action::Next;
    case 'H':return Action::Help;
    default:return Action::None;
    }
}
