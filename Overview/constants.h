#pragma once

#define BUFSIZE 4096

#define WINDOW_CLONE_MAXIMUM_SCALE 1.0
#define LAYOUT_SCALE_WEIGHT 1.0
#define LAYOUT_SPACE_WEIGHT 0.1

#define _columnSpacing 20;
#define _rowSpacing 20;

#define ANIMATION_DURATION_MS 200
#define FADE_DURATION_MS 200
#define FPS 120

#define BKCOL RGB(0, 0, 0) // 255, 0, 0 for transparency using layered style

#define DEBUG
#undef DEBUG

#define WM_THREAD_DONE (WM_USER + 0x0001)

#define HIDE_TASKBAR_ICON
#undef HIDE_TASKBAR_ICON

const wchar_t CLASS_NAME[] = TEXT("ActivitiesDesktopWindow");
