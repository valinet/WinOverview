#pragma once

#define BUFSIZE 4096

#define _columnSpacing 20;
#define _rowSpacing 20;
#define WINDOW_CLONE_MAXIMUM_SCALE 1.0
#define LAYOUT_SCALE_WEIGHT 1.0
#define LAYOUT_SPACE_WEIGHT 0.1

#define ANIMATION_DURATION_MS 150
#define FADE_DURATION_MS 120
#define FPS 120

#define AREA_BORDER 30

#define WM_THREAD_DONE (WM_USER + 0x0001)
#define WM_ASK_MOUSE (WM_USER + 0x0002)
#define WM_SHOW_SEARCH (WM_USER + 0x0003)
#define WM_IS_SEARCH (WM_USER + 0x0004)


#define CLASS_NAME L"ActivitiesOverviewWindowClassFull"
#define CLASS_NAME_SIMPLE L"ActivitiesOverviewWindowClassSimple"
#define CLASS_NAME_BKG L"ActivitiesOverviewWindowClassBkg"
