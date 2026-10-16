#pragma once
#include <cstdint>

class NotebookShortcuts {
public:
    enum class Action {None,EnterEdit,ExitEdit,InsertAbove,InsertBelow,DeleteCell,Undo,Redo,Markdown,Code,Previous,Next,RenderStay,RenderNext,RunAll,Help};
    NotebookShortcuts();
    ~NotebookShortcuts();
    Action Key(unsigned key, bool control, bool shift, bool editing, bool repeat, uint64_t nowMs);
    void Reset();
private:
    bool deletePending=false;
    uint64_t deleteAt=0;
};
