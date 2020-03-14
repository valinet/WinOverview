#pragma once
#include <vector>
#include <Windows.h>
#include <dwmapi.h>

struct WindowInfo {
    HWND hwnd;
    RECT rect;
};

struct EnumWindowProcParam {
    std::vector<WindowInfo>* windows;
    HMONITOR monitor;
};

struct GetMonitorsParams {
    std::vector<HMONITOR>* monitors;
};

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

struct AnimationInfo {
    HWND hWnd;
    int frame;
    POINT start;
    POINT end;
    int w;
    int h;
    double scale;
    HTHUMBNAIL thumb;
};

struct AnimationThreadArguments {
    std::vector<AnimationInfo*> anim;
    int frame;
    double delay_ms;
    BOOL isOpening;
    HWND hwnd;
    BOOL primary;
    HWND toActivate;
};

struct FadeThumbnailParams {
    std::vector<HTHUMBNAIL> thumbnails;
    int frame;
    double delay_ms;
    BOOL isOpening;
};

struct WindowProcInfo {
    HANDLE thread;
    HWND wallpaper;
    RECT area;
    int monitorNo;
};
