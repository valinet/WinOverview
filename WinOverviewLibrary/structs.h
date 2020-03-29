#pragma once
#include <Windows.h>
#include <dwmapi.h>
#include <vector>

struct Animation {
    HWND hWnd;
    POINT start;
    POINT end;
    int w;
    int h;
    double scale;
    HTHUMBNAIL thumb;
};

struct AnimationInfo {
    std::vector<Animation> animations;
    UINT type;
    double delay_ms;
    BOOL isOpening;
};

struct MonitorInfo {
    HMONITOR hMonitor;
    RECT rcWork;
    RECT rcMonitor;
    RECT area;
    RECT realArea;
    RECT rcSearch;
    HWND hWallpaperWnd;
    HWND hWnd;
    HWND bkgHWnd;
    HWND focusHWnd;
    AnimationInfo animation;
    HANDLE hAnimMutex;
    HANDLE hAnimThread;
    HTHUMBNAIL hSearchThumb;
    HWND hSearchHWnd;
};

struct WindowInfo {
    HWND hwnd;
    RECT rect;
};