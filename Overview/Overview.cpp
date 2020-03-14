#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h> 

#ifdef _DEBUG
    #define NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
    // Replace _NORMAL_BLOCK with _CLIENT_BLOCK if you want the
    // allocations to be of _CLIENT_BLOCK type
#else
    #define NEW new
#endif

#include <iostream>
#include <vector>
#include <algorithm> 
#include <conio.h>

#include <Windows.h>
#include <dwmapi.h>
#include <assert.h>
#include <Shobjidl.h>
#include <wrl/client.h>
#include <Windowsx.h>

#include "constants.h"
#include "structs.h"
#include "utils.h"
#include "resource.h"

#pragma comment(lib, "Dwmapi.lib")

LRESULT CALLBACK WindowProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
);

BOOL IsAltTabWindow(
    _In_ HWND hwnd
);

BOOL CALLBACK EnumWindowsProc(
    _In_ HWND   hwnd,
    _In_ LPARAM lParam
);

BOOL CALLBACK CheckRunningInstance(
    _In_ HWND   hwnd,
    _In_ LPARAM lParam
);

BOOL CALLBACK GetMonitors(
    HMONITOR hMonitor,
    HDC hdcMonitor,
    LPRECT lprcMonitor,
    LPARAM dwData
);

DWORD WINAPI animateWindows(
    LPVOID lpParam
);

void computeLayout(std::vector<WindowInfo>* windows, Layout* layout, RECT area);

void computeScaleAndSpace(Layout* layout, RECT area);

void computeWindowSlots(Layout* layout, RECT area, std::vector<Slot> &slots);

bool isBetterLayout(Layout* oldLayout, Layout* newLayout);

bool SortSlotsByHwndZOrder(Slot s1, Slot s2);

std::vector<AnimationThreadArguments*>* animations = NULL;

#ifdef HIDE_TASKBAR_ICON
Microsoft::WRL::ComPtr<ITaskbarList3> taskbar;
#endif

BOOL enteredSearch = FALSE;
BOOL clicked = FALSE;
BOOL isClosing = FALSE;

// based on: https://stackoverflow.com/questions/56132584/draw-on-windows-10-wallpaper-in-c
BOOL CALLBACK GetWallpaperHwnd(HWND hwnd, LPARAM lParam)
{
    HWND p = FindWindowEx(hwnd, NULL, L"SHELLDLL_DefView", NULL);
    HWND* ret = (HWND*)lParam;
    if (p)
    {
        *ret = FindWindowEx(NULL, hwnd, L"WorkerW", NULL);
    }
    return true;
}
HWND get_wallpaper_window()
{
    HWND progman = FindWindow(L"Progman", NULL);
    SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);
    HWND wallpaper_hwnd = nullptr;
    EnumWindows(GetWallpaperHwnd, (LPARAM)&wallpaper_hwnd);
    return wallpaper_hwnd;
}

void overview(HINSTANCE hInstance) {
    int i, j;

    // get list of monitors
    std::vector<HMONITOR> monitors;
    GetMonitorsParams monitorParams;
    monitorParams.monitors = &monitors;
    EnumDisplayMonitors(
        NULL,
        NULL,
        GetMonitors,
        reinterpret_cast<LPARAM>(&monitorParams)
    );

    // this vector stores information about the animation on each "desktop" window
    animations = NEW std::vector<AnimationThreadArguments*>;

    // we need this so that we do not spawn a button in the taskbar
#ifdef HIDE_TASKBAR_ICON
    CoInitialize(NULL);
    HRESULT result = CoCreateInstance(
        CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar));
    if (result == S_OK) {
        taskbar->HrInit();
    }
#endif

    // spawn a "desktop" window onto which we draw the live previews for each monitor
    for (int monitorNo = 0; monitorNo < monitors.size(); ++monitorNo) {
        MONITORINFOEX monitorInfo;
        monitorInfo.cbSize = sizeof(MONITORINFOEX);
        GetMonitorInfo(
            monitors.at(monitorNo),
            &monitorInfo
        );
        // we use only work area, so the taskbar will still be visible
        RECT area = (RECT)monitorInfo.rcWork;

        // find desktop window - this does not work, I believe, for multiple monitors at the moment
        HWND wallpaper = get_wallpaper_window();
        WindowProcInfo* winProcInfo = NEW WindowProcInfo;
        winProcInfo->wallpaper = wallpaper;
        winProcInfo->area = area;
        winProcInfo->monitorNo = monitorNo;

        // open a "desktop" for each monitor
        HWND hWnd = CreateWindowEx(
            0,                      // Optional window styles
            CLASS_NAME,          // Window class
            TEXT(""),                    // Window text
            WS_POPUP,    // Window style
            // Size and position
            area.left,
            area.top,
            area.right - area.left,
            area.bottom - area.top,
            NULL,       // Parent window    
            NULL,       // Menu
            hInstance,  // Instance handle
            winProcInfo      // Additional application data
        );
        // hide taskbar button
#ifdef HIDE_TASKBAR_ICON
        if (result == S_OK) {
            taskbar->DeleteTab(hWnd);
        }
#endif

        // make this top most
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        // enum top level windows and create preview window for each
        std::vector<WindowInfo> windows;
        EnumWindowProcParam params;
        params.windows = &windows;
        params.monitor = monitors.at(monitorNo);
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&params));

        // compute positions for windows
        Layout lastLayout;
        lastLayout.scale = -1;
        lastLayout.numRows = 0;
        lastLayout.numColumns = 0;
        for (int numRows = 1; ; numRows++) {
            int numColumns = ceil(windows.size() / numRows);
            if (numColumns == lastLayout.numColumns)
                break;
            Layout layout;
            layout.numRows = numRows;
            layout.numColumns = numColumns;
            computeLayout(&windows, &layout, area);
            computeScaleAndSpace(&layout, area);

            if (!isBetterLayout(&lastLayout, &layout))
                break;

            lastLayout = layout;
        }
        wprintf(TEXT("Layout\n"));
        wprintf(
            TEXT("gridHeight = %d\ngridWidth = %d\nnumRows = %d\nnumColumns = %d\nmaxColumns = %d\n"),
            lastLayout.gridHeight,
            lastLayout.gridWidth,
            lastLayout.numRows,
            lastLayout.numColumns,
            lastLayout.maxColumns
        );
        for (i = 0; i < lastLayout.rows.size(); ++i) {
            for (j = 0; j < lastLayout.rows.at(i).windows.size(); ++j) {
                TCHAR buffer[BUFSIZE];
                GetWindowText(lastLayout.rows.at(i).windows.at(j).hwnd, buffer, BUFSIZE - 1);
                wprintf(
                    TEXT("(%d, %d) -> %s\n"),
                    i,
                    j,
                    buffer
                );
            }
        }

        // set up animation thread
        AnimationThreadArguments* animationThreadArgs = NEW AnimationThreadArguments;
        animationThreadArgs->frame = 0;
        animationThreadArgs->delay_ms = 1000.0 / FPS;
        animationThreadArgs->isOpening = TRUE;
        animationThreadArgs->hwnd = hWnd;
        animationThreadArgs->primary = (monitorNo == 0);
        animationThreadArgs->toActivate = 0;
        animations->push_back(animationThreadArgs);

        // compute slot where each live preview has to arrive
        std::vector<Slot> slots;
        computeWindowSlots(&lastLayout, area, slots);
        sort(slots.begin(), slots.end(), SortSlotsByHwndZOrder);
        for (i = 0; i < slots.size(); ++i) {
            RECT initialRect;
            GetWindowRect(slots.at(i).window.hwnd, &initialRect);

            RECT finalRect;
            finalRect.left = area.left + slots.at(i).x;
            finalRect.top = area.top + slots.at(i).y;
            finalRect.right = ((initialRect.right - initialRect.left) * slots.at(i).scale) + finalRect.left;
            finalRect.bottom = ((initialRect.bottom - initialRect.top) * slots.at(i).scale) + finalRect.top;

            POINT initialCentre;
            initialCentre.x = (initialRect.right - initialRect.left) / 2 + initialRect.left;
            initialCentre.y = (initialRect.bottom - initialRect.top) / 2 + initialRect.top;

            POINT finalCentre;
            finalCentre.x = (finalRect.right - finalRect.left) / 2 + finalRect.left;
            finalCentre.y = (finalRect.bottom - finalRect.top) / 2 + finalRect.top;

            AnimationInfo* info = NEW AnimationInfo;
            info->frame = 0;
            info->start = initialCentre;
            info->end = finalCentre;
            info->w = (initialRect.right - initialRect.left);
            info->h = (initialRect.bottom - initialRect.top);
            info->scale = slots.at(i).scale;
            info->hWnd = slots.at(i).window.hwnd;
            animationThreadArgs->anim.push_back(info);

            TCHAR buffer[BUFSIZE];
            GetWindowText(slots.at(i).window.hwnd, buffer, BUFSIZE - 1);

            wprintf(
                TEXT("x = %d; y = %d ; scale = %f: %s\n"),
                slots.at(i).x,
                slots.at(i).y,
                slots.at(i).scale,
                buffer
            );

            // register live preview with system
            HTHUMBNAIL thumb;
            int ret = DwmRegisterThumbnail(hWnd, slots.at(i).window.hwnd, &thumb);
            if (!ret) {
                info->thumb = thumb;
                //SIZE size;
                //DwmQueryThumbnailSourceSize(thumb, &size);

                DWM_THUMBNAIL_PROPERTIES props;
                props.dwFlags = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_OPACITY;
                props.opacity = 255;
                props.fVisible = true;
                props.rcDestination = initialRect;

                DwmUpdateThumbnailProperties(thumb, &props);
            }
        }

        // start thread that will perform the animation for the live preview
        DWORD threadId;
        winProcInfo->thread = CreateThread(
            NULL,
            0,
            animateWindows,
            reinterpret_cast<LPVOID>(animationThreadArgs),
            0,
            &threadId
        );

        ShowWindow(hWnd, SW_SHOW);
    }
}

void close_overview(HWND activate) {
    // closing the overview means opening a thread that will undo the animation
    // and then destroying all resources
    int i;
    for (i = 0; i < animations->size(); ++i) {
        animations->at(i)->delay_ms = 1000.0 / FPS;
        animations->at(i)->isOpening = FALSE;
        animations->at(i)->frame = 0;
        animations->at(i)->toActivate = activate;
        DWORD threadId;
        HANDLE thread = CreateThread(
            NULL,
            0,
            animateWindows,
            reinterpret_cast<LPVOID>(animations->at(i)),
            0,
            &threadId
        );
    }
}

int wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd
)
{
    // if already running, request old instance to close and exit
    EnumWindows(CheckRunningInstance, NULL);

    // print memory leak information on exit
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    // set DPI aware v2 in order to get real screen coordinates and avoid blurry scaling
    SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    );

    // allocate a console window (for debug)
#ifdef DEBUG
    FILE* freopenr;
    AllocConsole();
    freopen_s(&freopenr, "CONOUT$", "w", stdout);
#endif

    // register the preview window class
    WNDCLASS wc = { };
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hbrBackground = CreateSolidBrush(BKCOL);
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    // open overview
    overview(hInstance);

    // run the message loop
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

bool SortSlotsByHwndZOrder(Slot s1, Slot s2) {
    BOOL start = TRUE;
    HWND hwnd = NULL;
    while (TRUE) {
        if (start) hwnd = GetTopWindow(NULL);
        else hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
        if (s1.window.hwnd == hwnd)
        {
            return false;
        }
        else if (s2.window.hwnd == hwnd)
        {
            return true;
        }
        start = FALSE;
    }
}

DWORD WINAPI animateWindows(
    LPVOID lpParam
)
{
    AnimationThreadArguments *threadArgs = reinterpret_cast<AnimationThreadArguments*>(lpParam);
    std::vector<AnimationInfo*> &anim = threadArgs->anim;

    register int noValues = ANIMATION_DURATION_MS / threadArgs->delay_ms;

    while (threadArgs->frame <= noValues) {

        ULONGLONG startTime = GetTickCount64();

        int i;
        for (i = 0; i < anim.size(); ++i) {
            AnimationInfo* info = anim.at(i);

            POINT start = threadArgs->isOpening ? info->start : info->end;
            POINT end = threadArgs->isOpening ? info->end : info->start;
            double scale_start = threadArgs->isOpening ? 1.0 : info->scale;
            double scale_end = threadArgs->isOpening ? info->scale : 1.0;

            double x = linear(easeOutQuad((threadArgs->delay_ms * threadArgs->frame) / ANIMATION_DURATION_MS * 1.0), (threadArgs->delay_ms * threadArgs->frame), start.x, end.x, ANIMATION_DURATION_MS);
            double y = linear(easeOutQuad((threadArgs->delay_ms * threadArgs->frame) / ANIMATION_DURATION_MS * 1.0), (threadArgs->delay_ms * info->frame), start.y, end.y, ANIMATION_DURATION_MS);
            double s = linear(easeOutQuad((threadArgs->delay_ms * threadArgs->frame) / ANIMATION_DURATION_MS * 1.0), (threadArgs->delay_ms * info->frame), scale_start, scale_end, ANIMATION_DURATION_MS);
            double w = info->w * s;
            double h = info->h * s;

            RECT rect;
            rect.left = x - w / 2;
            rect.top = y - h / 2;
            rect.bottom = h + rect.top;
            rect.right = w + rect.left;

            DWM_THUMBNAIL_PROPERTIES props;
            props.dwFlags = DWM_TNP_RECTDESTINATION;
            props.rcDestination = rect;

            DwmUpdateThumbnailProperties(info->thumb, &props);
        }

        threadArgs->frame++;

        ULONGLONG elapsed = GetTickCount64() - startTime;
        double sleep = threadArgs->delay_ms - elapsed;
        if (sleep > 0) Sleep(sleep);
    }

    if (!threadArgs->isOpening) {
        SendMessage(threadArgs->hwnd, WM_THREAD_DONE, TRUE, threadArgs->primary);
    }
    else SendMessage(threadArgs->hwnd, WM_THREAD_DONE, NULL, NULL);

    return 0;
}

DWORD WINAPI fadeThumbnail(
    LPVOID lpParam
)
{
    FadeThumbnailParams* threadArgs = reinterpret_cast<FadeThumbnailParams*>(lpParam);
    std::vector<HTHUMBNAIL>& thumbnails = threadArgs->thumbnails;

    register int noValues = FADE_DURATION_MS / threadArgs->delay_ms;

    while (threadArgs->frame <= noValues) {
        if (clicked) {
            int i;
            for (i = 0; i < thumbnails.size(); ++i) {
                DWM_THUMBNAIL_PROPERTIES props;
                props.dwFlags = DWM_TNP_OPACITY;
                props.opacity = 255;

                DwmUpdateThumbnailProperties(thumbnails.at(i), &props);
            }
            clicked = FALSE;
            return 0;
        }

        ULONGLONG startTime = GetTickCount64();

        int i;
        for (i = 0; i < thumbnails.size(); ++i) {
            int start = threadArgs->isOpening ? 0 : 255;
            int end = threadArgs->isOpening ? 255 : 0;

            double opacity = linear(easeOutQuad((threadArgs->delay_ms * threadArgs->frame) / FADE_DURATION_MS * 1.0), (threadArgs->delay_ms * threadArgs->frame), start, end, FADE_DURATION_MS);

            DWM_THUMBNAIL_PROPERTIES props;
            props.dwFlags = DWM_TNP_OPACITY;
            props.opacity = opacity;

            DwmUpdateThumbnailProperties(thumbnails.at(i), &props);
        }

        threadArgs->frame++;

        ULONGLONG elapsed = GetTickCount64() - startTime;
        double sleep = threadArgs->delay_ms - elapsed;
        if (sleep > 0) Sleep(sleep);
    }

    if (!threadArgs->isOpening) {
        int i;
        for (i = 0; i < thumbnails.size(); ++i) {
            DwmUnregisterThumbnail(thumbnails.at(i));
        }
    }

    delete threadArgs;

    return 0;
}

inline WindowProcInfo* GetAppState(HWND hwnd)
{
    LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
    WindowProcInfo* pState = reinterpret_cast<WindowProcInfo*>(ptr);
    return pState;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WindowProcInfo* info;
    if (uMsg == WM_CREATE)
    {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        info = reinterpret_cast<WindowProcInfo*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)info);
        HINSTANCE hInstance = pCreate->hInstance;
        HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
        SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }
    else
    {
        info = GetAppState(hWnd);
    }

    switch (uMsg) {
    case WM_NCCALCSIZE:
    {
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        printf("clicked\n");
        clicked = TRUE;
        int xPos = GET_X_LPARAM(lParam);
        int yPos = GET_Y_LPARAM(lParam);

        if (!enteredSearch) {
            for (int i = 0; i < animations->at(info->monitorNo)->anim.size(); ++i) {
                AnimationInfo* animation = animations->at(info->monitorNo)->anim.at(i);

                RECT rect = { 0,0,0,0 };
                rect.left = animation->end.x - (animation->w * animation->scale) / 2;
                rect.top = animation->end.y - (animation->h * animation->scale) / 2;
                rect.right = (animation->w * animation->scale) + rect.left;
                rect.bottom = (animation->h * animation->scale) + rect.top;

                if (xPos >= rect.left && xPos <= rect.right && yPos >= rect.top && yPos <= rect.bottom && !info->thread)
                {
                    DwmUnregisterThumbnail(animation->thumb);

                    HTHUMBNAIL thumb;
                    int ret = DwmRegisterThumbnail(animations->at(info->monitorNo)->hwnd, animation->hWnd, &thumb);
                    if (!ret) {
                        animation->thumb = thumb;

                        DWM_THUMBNAIL_PROPERTIES props;
                        props.dwFlags = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_OPACITY;
                        props.opacity = 255;
                        props.fVisible = true;
                        props.rcDestination = rect;

                        DwmUpdateThumbnailProperties(thumb, &props);
                    }
                    BringWindowToTop(animation->hWnd);
                    isClosing = TRUE;
                    close_overview(animation->hWnd);
                    return DefWindowProc(hWnd, uMsg, wParam, lParam);
                }
            }
        }
        if (!info->thread) {
            isClosing = TRUE;
            close_overview(0);
        }

        break;
    }
    case WM_ERASEBKGND:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        HDC wallpaperHdc = GetDC(info->wallpaper);

        BitBlt(
            hdc,
            0,
            0,
            info->area.right - info->area.left,
            info->area.bottom - info->area.top,
            wallpaperHdc,
            0,
            0,
            SRCCOPY
        );

        EndPaint(hWnd, &ps);
        return 1;
    }
    case WM_ACTIVATE:
    {
        printf("activate\n");

        if (enteredSearch && wParam != WA_INACTIVE && !isClosing) {
            FadeThumbnailParams* params = NEW FadeThumbnailParams;
            params->delay_ms = 1000.0 / FPS;
            params->isOpening = TRUE;
            params->frame = 0;
            for (int i = 0; i < animations->at(info->monitorNo)->anim.size(); ++i) {
                AnimationInfo* animation = animations->at(info->monitorNo)->anim.at(i);

                RECT rect = { 0,0,0,0 };
                rect.left = animation->end.x - (animation->w * animation->scale) / 2;
                rect.top = animation->end.y - (animation->h * animation->scale) / 2;
                rect.right = (animation->w * animation->scale) + rect.left;
                rect.bottom = (animation->h * animation->scale) + rect.top;

                HTHUMBNAIL thumb;
                int ret = DwmRegisterThumbnail(animations->at(info->monitorNo)->hwnd, animation->hWnd, &thumb);
                if (!ret) {
                    animation->thumb = thumb;

                    DWM_THUMBNAIL_PROPERTIES props;
                    props.dwFlags = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_OPACITY;
                    props.opacity = 0;
                    props.fVisible = true;
                    props.rcDestination = rect;

                    DwmUpdateThumbnailProperties(thumb, &props);

                    params->thumbnails.push_back(thumb);
                }
            }
            clicked = FALSE;
            DWORD threadId;
            HANDLE thread = CreateThread(
                NULL,
                0,
                fadeThumbnail,
                reinterpret_cast<LPVOID>(params),
                0,
                &threadId
            );
        }
        else if (wParam == WA_INACTIVE && !isClosing) {
            if (!info->thread) {
                HWND fWnd = GetForegroundWindow();
                TCHAR name[100];
                GetClassName(fWnd, name, 100);
                name[21] = 0;
                if (!wcsstr(name, TEXT("HwndWrapper[Wox.exe;;"))) {
                    close_overview(0);
                }
            }
        }
        return 0;
    }
    case WM_KEYDOWN: 
    {
        printf("key\n");

        if (wParam == VK_ESCAPE)
        {
            isClosing = TRUE;
            if (!info->thread) {
                close_overview(0);
            }
        } 
        else if ((wParam >= 'a' && wParam <= 'z') ||
            (wParam >= 'A' && wParam <= 'Z') || 
            (wParam >= '0' && wParam <= '9') || 
            (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) && !isClosing)
        {
            enteredSearch = TRUE;

            // based on: https://batchloaf.wordpress.com/2012/04/17/simulating-a-keystroke-in-win32-c-or-c-using-sendinput/
            INPUT ip;
            ip.type = INPUT_KEYBOARD;
            ip.ki.wScan = 0;
            ip.ki.time = 0;
            ip.ki.dwExtraInfo = 0;

            ip.ki.wVk = VK_MENU;
            ip.ki.dwFlags = 0;
            SendInput(1, &ip, sizeof(INPUT));

            ip.ki.wVk = VK_SPACE;
            ip.ki.dwFlags = 0;
            SendInput(1, &ip, sizeof(INPUT));

            ip.ki.wVk = VK_SPACE;
            ip.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &ip, sizeof(INPUT));

            ip.ki.wVk = VK_MENU;
            ip.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &ip, sizeof(INPUT));

            BOOL wasShift = GetAsyncKeyState(VK_SHIFT);
            if (wasShift < 0) {
                ip.ki.wVk = VK_SHIFT;
                ip.ki.dwFlags = 0;
                SendInput(1, &ip, sizeof(INPUT));
            }

            ip.ki.wVk = wParam;
            ip.ki.dwFlags = 0;
            SendInput(1, &ip, sizeof(INPUT));

            ip.ki.wVk = wParam;
            ip.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &ip, sizeof(INPUT));

            if (wasShift < 0) {
                ip.ki.wVk = VK_SHIFT;
                ip.ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(1, &ip, sizeof(INPUT));
            }

            Sleep(10);

            FadeThumbnailParams* params = NEW FadeThumbnailParams;
            params->delay_ms = 1000.0 / FPS;
            params->isOpening = FALSE;
            params->frame = 0;
            for (int i = 0; i < animations->at(info->monitorNo)->anim.size(); ++i) {
                AnimationInfo* animation = animations->at(info->monitorNo)->anim.at(i);

                params->thumbnails.push_back(animation->thumb);

                animation->thumb = 0;
            }
            DWORD threadId;
            HANDLE thread = CreateThread(
                NULL,
                0,
                fadeThumbnail,
                reinterpret_cast<LPVOID>(params),
                0,
                &threadId
            );
        }
        break;
    }
    case WM_CLOSE:
    {
        printf("close\n");

        if (!info->thread) {
            if (!enteredSearch) {
                close_overview(0);
            }
            else {
                INPUT ip;
                ip.type = INPUT_KEYBOARD;
                ip.ki.wScan = 0;
                ip.ki.time = 0;
                ip.ki.dwExtraInfo = 0;

                ip.ki.wVk = VK_ESCAPE;
                ip.ki.dwFlags = 0; // 0 for key press
                SendInput(1, &ip, sizeof(INPUT));

                ip.ki.wVk = VK_ESCAPE;
                ip.ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(1, &ip, sizeof(INPUT));
            }
        }
        return 0;
    }
    case WM_DESTROY:
    {
        printf("destroy\n");

        delete info;
        exit(0);
        break;
    }
    case WM_THREAD_DONE:
    {
        printf("thread_done\n");

        if (wParam == NULL) 
        {
            CloseHandle(info->thread);
            info->thread = 0;
        }
        else 
        {
            if (animations->at(info->monitorNo)->toActivate) {
                BringWindowToTop(animations->at(info->monitorNo)->toActivate);
                ShowWindow(animations->at(info->monitorNo)->hwnd, SW_HIDE);
            }
            std::vector<HWND> hwnds;
            CloseHandle(info->thread);
            info->thread = 0;
            if (lParam != 0) {
                int i, j;
                for (i = 0; i < animations->size(); ++i) {
                    for (j = 0; j < animations->at(i)->anim.size(); ++j) {
                        delete animations->at(i)->anim.at(j);
                    }
                    hwnds.push_back(animations->at(i)->hwnd);
                    delete animations->at(i);
                }
                delete animations;
                for (i = 0; i < hwnds.size(); ++i) {
                    DestroyWindow(hwnds.at(i));
                }
            }
#ifdef HIDE_TASKBAR_ICON
            taskbar->Release();
            CoUninitialize();
#endif
            animations = NULL;
        }
        break;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

BOOL CALLBACK GetMonitors(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    GetMonitorsParams* params = reinterpret_cast<GetMonitorsParams*>(dwData);
    params->monitors->push_back(hMonitor);
    return TRUE;
}

// taken from: https://devblogs.microsoft.com/oldnewthing/20071008-00/?p=24863
BOOL IsAltTabWindow(
    _In_ HWND hwnd
)
{
    TITLEBARINFO ti;
    HWND hwndTry, hwndWalk = NULL;

    if (!IsWindowVisible(hwnd))
        return FALSE;

    hwndTry = GetAncestor(hwnd, GA_ROOTOWNER);
    while (hwndTry != hwndWalk)
    {
        hwndWalk = hwndTry;
        hwndTry = GetLastActivePopup(hwndWalk);
        if (IsWindowVisible(hwndTry))
            break;
    }
    if (hwndWalk != hwnd)
        return FALSE;

    // the following removes some task tray programs and "Program Manager"
    ti.cbSize = sizeof(ti);
    GetTitleBarInfo(hwnd, &ti);
    if (ti.rgstate[0] & STATE_SYSTEM_INVISIBLE)
        return FALSE;

    // Tool windows should not be displayed either, these do not appear in the
    // task bar.
    if (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
        return FALSE;

    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(
    _In_ HWND   hwnd,
    _In_ LPARAM lParam
)
{
    EnumWindowProcParam* params = reinterpret_cast<EnumWindowProcParam*>(lParam);

    // exclude Modern Apps that are suspended
    // taken from: https://stackoverflow.com/questions/43927156/enumwindows-returns-closed-windows-store-applications
    BOOL isCloacked;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloacked, sizeof BOOL);

    if (IsAltTabWindow(hwnd) && !isCloacked && !IsIconic(hwnd) && MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL) == params->monitor) {
        WindowInfo info;
        info.hwnd = hwnd;
        GetWindowRect(hwnd, &info.rect);
        params->windows->push_back(info);
    }
    return TRUE;
}

BOOL CALLBACK CheckRunningInstance(
    _In_ HWND   hwnd,
    _In_ LPARAM lParam
)
{
    TCHAR name[100];
    GetClassName(hwnd, name, 100);
    if (wcsstr(name, CLASS_NAME)) {
        SendMessage(hwnd, WM_CLOSE, NULL, NULL);
        exit(0);
    }
    return TRUE;
}