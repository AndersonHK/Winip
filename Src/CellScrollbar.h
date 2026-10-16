#pragma once
#include <cstdint>

// Drag state uses cell identity; thumb geometry is shared by painting/input.
class CellScrollbar {
public:
    static int ThumbWidth(int page, int content);
    static int ThumbLeft(int page, int content, int scroll);
    void Begin(uint64_t cell, int pointer, int page, int content, int scroll);
    int Drag(int pointer, int page, int content) const;
    void End();
    uint64_t CellId() const;
private:
    uint64_t cellId=0;
    int offset=0;
};
