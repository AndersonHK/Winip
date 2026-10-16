#include "NotebookViewport.h"
#include <algorithm>

int NotebookViewport::SideMargin(int width) {
    return std::max(8, (width - 1180) / 2);
}
