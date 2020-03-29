#pragma once
#include <vector>
#include "structs.h"

struct Row {
    bool exists;
    int fullHeight;
    int fullWidth;
    std::vector<WindowInfo> windows;
    int width = 0;
    int height = 0;
    double additionalScale;
    int x;
    int y;
};

struct Layout {
    int numRows;
    int numColumns;
    std::vector<Row> rows;
    int maxColumns;
    int gridWidth;
    int gridHeight;
    double scale;
    double space;
};

struct Slot {
    int x;
    int y;
    double scale;
    WindowInfo window;
};

void computeLayout(
    std::vector<WindowInfo>* windows, 
    Layout* layout, 
    RECT area
    );

void computeScaleAndSpace(
    Layout* layout,
    RECT area
    );

bool isBetterLayout(
    Layout* oldLayout, 
    Layout* newLayout
    );

void computeWindowSlots(
    Layout* layout,
    RECT area,
    std::vector<Slot>* slots
    );

bool SortSlotsByHwndZOrder(
    Slot s1, 
    Slot s2
    );