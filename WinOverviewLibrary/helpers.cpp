#include "pch.h"
#include <Windows.h>
#include <dwmapi.h>
#include <algorithm>
#include <conio.h>
#pragma comment(lib, "Dwmapi.lib")

#include "constants.h"
#include "structs.h"
#include "helpers.h"
#include "workspace.h"

void GetMonitors(
    _In_ std::vector<MonitorInfo> *monitors
    )
{
	GetMonitorsParams monitorParams;
	monitorParams.monitors = monitors;
	EnumDisplayMonitors(
		NULL,
		NULL,
		EnumDisplayMonitorsCallback,
		reinterpret_cast<LPARAM>(&monitorParams)
		);

    // get monitor wallpaper
    GetWallpaperWindows(monitors);
}

BOOL CALLBACK EnumDisplayMonitorsCallback(
    _In_ HMONITOR hMonitor, 
    _In_ HDC hdcMonitor, 
    _In_ LPRECT lprcMonitor, 
    _Out_ LPARAM dwData
    )
{
    GetMonitorsParams* params = reinterpret_cast<GetMonitorsParams*>(dwData);

    // get monitor coordinates
    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(
        hMonitor,
        &monitorInfo
        );

    MonitorInfo info;
    info.rcWork = monitorInfo.rcWork;
    info.rcMonitor = monitorInfo.rcMonitor;
    info.hMonitor = hMonitor;
    info.hWallpaperWnd = NULL;
    info.hAnimMutex = CreateSemaphore(
        NULL,
        1,
        1,
        NULL
        );
    info.hAnimThread = NULL;
    info.hSearchThumb = NULL;
    info.hSearchHWnd = NULL;

    params->monitors->push_back(info);
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
    //ti.cbSize = sizeof(ti);
    //GetTitleBarInfo(hwnd, &ti);
    //if (ti.rgstate[0] & STATE_SYSTEM_INVISIBLE)
    //    return FALSE;

    // Tool windows should not be displayed either, these do not appear in the
    // task bar.
    if (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
        return FALSE;

    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(
    _In_ HWND   hwnd,
    _Out_ LPARAM lParam
    )
{
    EnumWindowProcParam* params = reinterpret_cast<EnumWindowProcParam*>(lParam);

    // exclude Modern Apps that are suspended
    // taken from: https://stackoverflow.com/questions/43927156/enumwindows-returns-closed-windows-store-applications
    BOOL isCloacked;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloacked, sizeof BOOL);

    if (IsAltTabWindow(hwnd) && !isCloacked && !IsIconic(hwnd) && MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL) == params->monitor.hMonitor) {
        WindowInfo info;
        info.hwnd = hwnd;
        GetWindowRect(hwnd, &info.rect);
        if (info.rect.right - info.rect.left != 0 || info.rect.bottom - info.rect.top != 0) {
            // normalize coordinates
            info.rect.right -= (params->monitor.realArea.left);
            info.rect.bottom -= (params->monitor.realArea.top);
            info.rect.left -= (params->monitor.realArea.left);
            info.rect.top -= (params->monitor.realArea.top);
            params->windows->push_back(info);
        }
    }
    return TRUE;
}

void GetWindowsOnMonitor(
    _In_ MonitorInfo monitor, 
    _Out_ std::vector<WindowInfo> *windows
    )
{
    EnumWindowProcParam params;
    params.windows = windows;
    params.monitor = monitor;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&params));
}

// based on: https://stackoverflow.com/questions/56132584/draw-on-windows-10-wallpaper-in-c
BOOL CALLBACK GetWallpaperHwnd(
    _In_ HWND hwnd, 
    _Out_ LPARAM lParam
    )
{
    std::vector<MonitorInfo>* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(lParam);

    SIZE_T monitorNo;

    HWND p = FindWindowEx(hwnd, NULL, L"SHELLDLL_DefView", NULL);
    if (p)
    {
        HWND t = FindWindowEx(NULL, hwnd, L"WorkerW", NULL);
        if (t)
        {
            RECT rect;
            GetWindowRect(hwnd, &rect);
            for (monitorNo = 0; monitorNo < monitors->size(); ++monitorNo)
            {
                monitors->at(monitorNo).hWallpaperWnd = t;
            }
        }
    }
    return TRUE;
}

void GetWallpaperWindows(
    _Inout_ std::vector<MonitorInfo>* monitors
    )
{
    HWND progman = FindWindow(L"Progman", NULL);
    SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);
    EnumWindows(GetWallpaperHwnd, reinterpret_cast<LPARAM>(monitors));
}

void GetWindowSlots(
    _In_ std::vector<WindowInfo>* windows,
    _In_ RECT area,
    _Out_ std::vector<Slot>* slots
    )
{
    Layout lastLayout;
    lastLayout.scale = -1;
    lastLayout.numRows = 0;
    lastLayout.numColumns = 0;
    for (int numRows = 1; ; numRows++) {
        int numColumns = ceil(windows->size() / numRows);
        if (numColumns == lastLayout.numColumns)
            break;
        Layout layout;
        layout.numRows = numRows;
        layout.numColumns = numColumns;
        computeLayout(windows, &layout, area);
        computeScaleAndSpace(&layout, area);

        if (!isBetterLayout(&lastLayout, &layout))
            break;

        lastLayout = layout;
    }

    // compute slot where each live preview has to arrive
    computeWindowSlots(&lastLayout, area, slots);
}

HTHUMBNAIL RegisterLiveThumbnail(
    _In_ HWND dest, 
    _In_ HWND src, 
    _In_ RECT rect
    )
{
    HTHUMBNAIL thumb;
    int ret = DwmRegisterThumbnail(dest, src, &thumb);
    if (!ret) {
        DWM_THUMBNAIL_PROPERTIES props;
        props.dwFlags = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_OPACITY;
        props.opacity = 255;
        props.fVisible = true;
        props.rcDestination = rect;
        props.fSourceClientAreaOnly = FALSE;

        ret = DwmUpdateThumbnailProperties(thumb, &props);
    }
    return thumb;
}

DWORD WINAPI animate(
    _Inout_ LPVOID lpParam
    )
{
    MonitorInfo* monitorInfo = reinterpret_cast<MonitorInfo*>(lpParam);
    AnimationInfo* threadArgs = &monitorInfo->animation;
    std::vector<Animation>& anim = threadArgs->animations;

    register int noValues;

    register int size = anim.size();
    register int j = 0, k = 0;
    std::vector<RECT> rects;
    std::vector<double> opacities;
    switch (threadArgs->type)
    {
    case ANIMTYPE_PREVIEW:
        noValues = ANIMATION_DURATION_MS / threadArgs->delay_ms;
        rects.reserve(noValues * size);
        break;
    case ANIMTYPE_PREVIEW_FADE:
        noValues = FADE_DURATION_MS / threadArgs->delay_ms;
        opacities.reserve(noValues * size);
        break;
    case ANIMTYPE_MAIN_FADE:
        noValues = ANIMATION_DURATION_MS / threadArgs->delay_ms;
        opacities.reserve(noValues);
    }
    while (j <= noValues) {
        register int i;
        switch (threadArgs->type)
        {
        case ANIMTYPE_PREVIEW:
        {
            for (i = 0; i < size; ++i) {
                Animation& info = anim.at(i);

                POINT start = threadArgs->isOpening ? info.start : info.end;
                POINT end = threadArgs->isOpening ? info.end : info.start;
                double scale_start = threadArgs->isOpening ? 1.0 : info.scale;
                double scale_end = threadArgs->isOpening ? info.scale : 1.0;

                double x = linear(easeOutQuad((threadArgs->delay_ms * j) / ANIMATION_DURATION_MS * 1.0), start.x, end.x, ANIMATION_DURATION_MS);
                double y = linear(easeOutQuad((threadArgs->delay_ms * j) / ANIMATION_DURATION_MS * 1.0), start.y, end.y, ANIMATION_DURATION_MS);
                double s = linear(easeOutQuad((threadArgs->delay_ms * j) / ANIMATION_DURATION_MS * 1.0), scale_start, scale_end, ANIMATION_DURATION_MS);
                double w = info.w * s;
                double h = info.h * s;

                RECT rect;
                rect.left = x - w / 2;
                rect.top = y - h / 2;
                rect.bottom = h + rect.top;
                rect.right = w + rect.left;

                rects.push_back(rect);
            }
            break;
        }
        case ANIMTYPE_PREVIEW_FADE:
        {
            for (i = 0; i < size; ++i) {
                int start = threadArgs->isOpening ? 0 : 255;
                int end = threadArgs->isOpening ? 255 : 0;

                double opacity = linear(easeOutQuad((threadArgs->delay_ms * j) / FADE_DURATION_MS * 1.0), start, end, FADE_DURATION_MS);

                opacities.push_back(opacity);
            }
            break;
        }
        case ANIMTYPE_MAIN_FADE:
        {
            int start = threadArgs->isOpening ? 0 : 100;
            int end = threadArgs->isOpening ? 100 : 0;

            double opacity = (255.0 * linear(easeOutQuad((threadArgs->delay_ms * j) / ANIMATION_DURATION_MS * 1.0), start, end, ANIMATION_DURATION_MS)) / 100.0;
            opacities.push_back(opacity);
            break;
        }
        }
        j++;
    }

    HANDLE timer;   /* Timer handle */
    LARGE_INTEGER li;   /* Time defintion */
    /* Create timer */
    if (!(timer = CreateWaitableTimer(NULL, TRUE, NULL)))
        return FALSE;

    j = 0;
    while (k <= noValues) {

        ULONGLONG startTime = GetTickCount64();

        int i;
        
        switch (threadArgs->type)
        {
        case ANIMTYPE_PREVIEW:
        {
            for (i = 0; i < size; ++i) {
                DWM_THUMBNAIL_PROPERTIES props;
                props.dwFlags = DWM_TNP_RECTDESTINATION;
                props.rcDestination = rects.at(j + i);

                DwmUpdateThumbnailProperties(anim.at(i).thumb, &props);
            }
            break;
        }
        case ANIMTYPE_PREVIEW_FADE:
        {
            for (i = 0; i < size; ++i) {
                DWM_THUMBNAIL_PROPERTIES props;
                props.dwFlags = DWM_TNP_OPACITY;
                props.opacity = opacities.at(j + i);

                DwmUpdateThumbnailProperties(anim.at(i).thumb, &props);
            }
            break;
        }
        case ANIMTYPE_MAIN_FADE:
        {
            SetLayeredWindowAttributes(monitorInfo->hWnd, 0, opacities.at(k), LWA_ALPHA);
        }
        }

        k++;
        j += size;

        ULONGLONG elapsed = GetTickCount64() - startTime;
        double sleep = threadArgs->delay_ms - elapsed;
        //printf("%d ", GetTickCount64());
        if (sleep > 0)
        {
            li.QuadPart = -sleep * 1000LL;
            //printf("%llu ", -li.QuadPart);
            if (!SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE)) {
                CloseHandle(timer);
                return FALSE;
            }
            /* Start & wait for timer */
            WaitForSingleObject(timer, INFINITE);
        }
    }

    CloseHandle(timer);

    SendMessage(monitorInfo->hWnd, WM_THREAD_DONE, 0, 0);

    return 0;
}

BOOL BeginAnimateUpdate(
    _In_ MonitorInfo* info
    )
{
    DWORD dwWaitResult = WaitForSingleObject(
        info->hAnimMutex,
        0);
    if (dwWaitResult == WAIT_OBJECT_0)
    {
        return TRUE;
    }
    return FALSE;
}

void EndAnimateUpdate(
    _In_ MonitorInfo* info
    )
{
    ReleaseSemaphore(info->hAnimMutex, 1, NULL);
}

BOOL DoAnimate(
    _In_ BOOL alreadyAcquired,
    _In_ UINT type,
    _In_ MonitorInfo* info,
    _In_ BOOL isOpening
    )
{
    BOOL bAcquired = TRUE;
    if (!alreadyAcquired)
    {
        bAcquired = BeginAnimateUpdate(info);
    }
    if (bAcquired)
    {
        info->animation.isOpening = isOpening;
        info->animation.type = type;
        info->hAnimThread = CreateThread(
            NULL,
            0,
            animate,
            reinterpret_cast<LPVOID>(info),
            0,
            NULL
            );
    }
    return bAcquired;
}

bool SortHwndByZorder(
    _In_ HWND hWnd1, 
    _In_ HWND hWnd2
    )
{
    BOOL start = TRUE;
    HWND hwnd = NULL;
    while (TRUE) {
        if (start) hwnd = GetTopWindow(NULL);
        else hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
        if (hWnd1 == hwnd)
        {
            return false;
        }
        else if (hWnd2 == hwnd)
        {
            return true;
        }
        start = FALSE;
    }
}

bool SortAnimationsByHwndZorder(
    _In_ Animation a1, 
    _In_ Animation a2
    )
{
    return SortHwndByZorder(a1.hWnd, a2.hWnd);
}

RECT GetSearchRect(
    _In_ MonitorInfo* info
    )
{
    RECT rect;
    GetWindowRect(info->hSearchHWnd, &rect);

    RECT targetRect;
    targetRect.left = (info->area.right - info->area.left) / 2 - (rect.right - rect.left) / 2;
    targetRect.top = (info->area.bottom - info->area.top) / 2 - (rect.bottom - rect.top) / 2;
    targetRect.right = targetRect.left + rect.right - rect.left;
    targetRect.bottom = targetRect.top + rect.bottom - rect.top;

    return targetRect;
}

void ShowSearch(
    _In_ MonitorInfo* info, 
    _In_ WPARAM wParam
    )
{
    if (DoAnimate(FALSE, ANIMTYPE_PREVIEW_FADE, info, FALSE))
    {
        INPUT ip;
        ip.type = INPUT_KEYBOARD;
        ip.ki.wScan = 0;
        ip.ki.time = 0;
        ip.ki.dwExtraInfo = 0;

        ip.ki.wVk = VK_LWIN;
        ip.ki.dwFlags = 0;
        SendInput(1, &ip, sizeof(INPUT));

        ip.ki.wVk = 0x51;
        ip.ki.dwFlags = 0;
        SendInput(1, &ip, sizeof(INPUT));

        ip.ki.wVk = 0x51;
        ip.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ip, sizeof(INPUT));

        ip.ki.wVk = VK_LWIN;
        ip.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ip, sizeof(INPUT));

        TCHAR name[100];
        HWND fWnd;
        do {
            fWnd = GetForegroundWindow();
            GetClassName(fWnd, name, 100);
        } while (!wcsstr(name, TEXT("Windows.UI.Core.CoreW")));

        info->hSearchHWnd = fWnd;

        RECT rect;
        GetWindowRect(fWnd, &rect);
        info->rcSearch = rect;
        SetWindowPos(
            fWnd,
            fWnd,
            info->area.left + (info->area.right - info->area.left) / 2 - (rect.right - rect.left) / 2,
            info->area.top + (info->area.bottom - info->area.top) / 2 - (rect.bottom - rect.top) / 2,
            0,
            0,
            SWP_NOZORDER | SWP_NOSIZE
            );

        POINT pt;
        GetCursorPos(&pt);
        //if (pt.x >= rect.left && pt.x <= rect.right && pt.y >= rect.top && pt.y <= rect.bottom)
        //{
            LONG styles = GetWindowLong(info->hWnd, GWL_EXSTYLE);
            SetWindowLong(info->hWnd, GWL_EXSTYLE, styles | WS_EX_TRANSPARENT);
            SetWindowPos(info->hWnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
        //}

        if (wParam != NULL)
        {
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
        }
    }
}