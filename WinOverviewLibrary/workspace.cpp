#include "pch.h"
#include <iostream>
#include <Windows.h>
#include <algorithm>

#include "constants.h"
#include "structs.h"
#include "workspace.h"
#include "helpers.h"

// Window layout logic converted from the official GNOME Activities JavaScript code
// Available at: https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/master/js/ui/workspace.js

static inline double interpolate(double start, double end, double step) {
    return start + (end - start) * step;
}

static inline double computeWindowScale(WindowInfo window, RECT area) {
    double ratio = (window.rect.bottom - window.rect.top) / ((area.bottom - area.top) * 1.0);
    double res = interpolate(2.5, 1, ratio);
    return res;
}

static void computeRowSizes(Layout* layout)
{
    for (int i = 0; i < layout->rows.size(); ++i) {
        Row& row = layout->rows.at(i);
        row.width = row.fullWidth * layout->scale + (row.windows.size() - 1) * _columnSpacing;
        row.height = row.fullHeight * layout->scale;
    }
}

void computeWindowSlots(Layout* layout, RECT area, std::vector<Slot>* slots) {
    computeRowSizes(layout);

    std::vector<Row>& rows = layout->rows;
    double& scale = layout->scale;

    // Do this in three parts.
    int heightWithoutSpacing = 0;
    for (int i = 0; i < rows.size(); i++) {
        Row& row = rows.at(i);
        heightWithoutSpacing += row.height;
    }

    int verticalSpacing = (rows.size() - 1) * _rowSpacing;
    double additionalVerticalScale = min(1, ((area.bottom - area.top) - verticalSpacing) / heightWithoutSpacing);

    // keep track how much smaller the grid becomes due to scaling
    // so it can be centered again
    double compensation = 0;
    double y = 0;

    for (int i = 0; i < rows.size(); ++i) {
        Row& row = rows.at(i);

        // If this window layout row doesn't fit in the actual
        // geometry, then apply an additional scale to it.
        int horizontalSpacing = (row.windows.size() - 1) * _columnSpacing;
        int widthWithoutSpacing = row.width - horizontalSpacing;
        double additionalHorizontalScale = min(1, ((area.right - area.left) - horizontalSpacing) / widthWithoutSpacing);

        if (additionalHorizontalScale < additionalVerticalScale) {
            row.additionalScale = additionalHorizontalScale;
            // Only consider the scaling in addition to the vertical scaling for centering.
            compensation += (additionalVerticalScale - additionalHorizontalScale) * row.height;
        }
        else {
            row.additionalScale = additionalVerticalScale;
            // No compensation when scaling vertically since centering based on a too large
            // height would undo what vertical scaling is trying to achieve.
        }

        row.x = area.left + (max((area.right - area.left) - (widthWithoutSpacing * row.additionalScale + horizontalSpacing), 0) / 2);
        row.y = area.top + (max((area.bottom - area.top) - (heightWithoutSpacing + verticalSpacing), 0) / 2) + y;
        y += row.height * row.additionalScale + _rowSpacing;
    }

    compensation /= 2;

    for (int i = 0; i < rows.size(); ++i) {
        Row& row = rows.at(i);
        double x = row.x;
        for (int j = 0; j < row.windows.size(); ++j) {
            WindowInfo& window = row.windows.at(j);

            double s = scale * computeWindowScale(window, area) * row.additionalScale;
            double cellWidth = (window.rect.right - window.rect.left) * s;
            double cellHeight = (window.rect.bottom - window.rect.top) * s;

            s = min(s, WINDOW_CLONE_MAXIMUM_SCALE);
            double cloneWidth = (window.rect.right - window.rect.left) * s;

            double cloneX = x + (cellWidth - cloneWidth) / 2;
            double cloneY = row.y + row.height * row.additionalScale - cellHeight + compensation;

            // Align with the pixel grid to prevent blurry windows at scale = 1
            cloneX = floor(cloneX);
            cloneY = floor(cloneY);

            Slot slot;
            slot.x = cloneX;
            slot.y = cloneY;
            slot.scale = s;
            slot.window = window;
            slots->push_back(slot);

            x += cellWidth + _columnSpacing;
        }
    }
}

bool isBetterLayout(Layout* oldLayout, Layout* newLayout) {
    if (oldLayout->scale == -1)
        return true;

    double spacePower = (newLayout->space - oldLayout->space) * LAYOUT_SPACE_WEIGHT;
    double scalePower = (newLayout->scale - oldLayout->scale) * LAYOUT_SCALE_WEIGHT;

    if (newLayout->scale > oldLayout->scale && newLayout->space > oldLayout->space) {
        // Win win -- better scale and better space
        return true;
    }
    else if (newLayout->scale > oldLayout->scale && newLayout->space <= oldLayout->space) {
        // Keep new layout only if scale gain outweighs aspect space loss
        return scalePower > spacePower;
    }
    else if (newLayout->scale <= oldLayout->scale && newLayout->space > oldLayout->space) {
        // Keep new layout only if aspect space gain outweighs scale loss
        return spacePower > scalePower;
    }
    else {
        // Lose -- worse scale and space
        return false;
    }
}


void computeScaleAndSpace(Layout* layout, RECT area)
{
    double hspacing = (layout->maxColumns - 1) * _columnSpacing;
    double vspacing = (layout->numRows - 1) * _rowSpacing;

    double spacedWidth = (area.right - area.left) - hspacing;
    double spacedHeight = (area.bottom - area.top) - vspacing;

    double horizontalScale = spacedWidth / layout->gridWidth;
    double verticalScale = spacedHeight / layout->gridHeight;

    // Thumbnails should be less than 70% of the original size
    double scale = min(horizontalScale, verticalScale, 1.0);

    double scaledLayoutWidth = layout->gridWidth * scale + hspacing;
    double scaledLayoutHeight = layout->gridHeight * scale + vspacing;
    double space = (scaledLayoutWidth * scaledLayoutHeight) / ((area.right - area.left) * (area.bottom - area.top));

    layout->scale = scale;
    layout->space = space;
}

static inline bool keepSameRow(Row row, WindowInfo window, double width, double idealRowWidth) {
    if (row.fullWidth + width <= idealRowWidth)
        return true;

    double oldRatio = 1.0 * row.fullWidth / idealRowWidth;
    double newRatio = 1.0 * (row.fullWidth + width) / idealRowWidth;

    if (abs(1.0 - newRatio) < abs(1.0 - oldRatio))
        return true;

    return false;
}

static bool windowInfoSortVerticallyComparator(WindowInfo w1, WindowInfo w2) {
    int w1x = (w1.rect.right - w1.rect.left) / 2 + w1.rect.left;
    int w1y = (w1.rect.bottom - w1.rect.top) / 2 + w1.rect.top;
    int w2x = (w2.rect.right - w2.rect.left) / 2 + w2.rect.left;
    int w2y = (w2.rect.bottom - w2.rect.top) / 2 + w2.rect.top;
    return w1y < w2y;
}

static bool windowInfoSortHorizontallyComparator(WindowInfo w1, WindowInfo w2) {
    int w1x = (w1.rect.right - w1.rect.left) / 2 + w1.rect.left;
    int w1y = (w1.rect.bottom - w1.rect.top) / 2 + w1.rect.top;
    int w2x = (w2.rect.right - w2.rect.left) / 2 + w2.rect.left;
    int w2y = (w2.rect.bottom - w2.rect.top) / 2 + w2.rect.top;
    return w1x < w2x;
}

bool SortSlotsByHwndZOrder(Slot s1, Slot s2) {
    return SortHwndByZorder(s1.window.hwnd, s2.window.hwnd);
}


void computeLayout(std::vector<WindowInfo>* windows, Layout* layout, RECT area) {
    Row nullRow;
    nullRow.exists = false;

    int numRows = layout->numRows;

    std::vector<Row>& rows = layout->rows;
    double totalWidth = 0;
    for (int i = 0; i < windows->size(); ++i) {
        WindowInfo window = windows->at(i);
        double s = computeWindowScale(window, area);
        totalWidth += (window.rect.right - window.rect.left) * s;
    }

    double idealRowWidth = totalWidth / numRows;

    // Sort windows vertically to minimize travel distance.
    // This affects what rows the windows get placed in.
    std::vector<WindowInfo> sortedWindows(*windows);
    std::sort(sortedWindows.begin(), sortedWindows.end(), windowInfoSortVerticallyComparator);

    int windowIdx = 0;
    for (int i = 0; i < numRows; ++i) {
        Row _row;
        _row.exists = true;
        _row.fullHeight = 0;
        _row.fullWidth = 0;
        _row.width = 0;
        _row.height = 0;
        rows.push_back(_row);
        Row& row = rows.at(rows.size() - 1);

        for (; windowIdx < sortedWindows.size(); ++windowIdx) {
            WindowInfo window = sortedWindows.at(windowIdx);
            double s = computeWindowScale(window, area);
            double width = (window.rect.right - window.rect.left) * s;
            double height = (window.rect.bottom - window.rect.top) * s;
            row.fullHeight = max(row.fullHeight, height);

            if (keepSameRow(row, window, width, idealRowWidth) || (i == numRows - 1))
            {
                row.windows.push_back(window);
                row.fullWidth += width;
            }
            else
            {
                break;
            }
        }
    }

    int gridHeight = 0;
    Row& maxRow = nullRow;
    for (int i = 0; i < numRows; ++i) {
        Row& row = rows.at(i);
        sort(row.windows.begin(), row.windows.end(), windowInfoSortHorizontallyComparator);

        if (!maxRow.exists || row.fullWidth > maxRow.fullWidth)
            maxRow = row;
        gridHeight += row.fullHeight;
    }

    layout->maxColumns = maxRow.windows.size();
    layout->gridWidth = maxRow.fullWidth;
    layout->gridHeight = gridHeight;
}