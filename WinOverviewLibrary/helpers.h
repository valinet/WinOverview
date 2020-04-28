#pragma once
#include <Windows.h>
#include <dwmapi.h>
#include "workspace.h"

enum ANIMTYPE
{
	ANIMTYPE_PREVIEW = 0,
	ANIMTYPE_PREVIEW_FADE = 1,
	ANIMTYPE_MAIN_FADE = 2,
};

enum ZBID
{
	ZBID_DEFAULT = 0,
	ZBID_DESKTOP = 1,
	ZBID_UIACCESS = 2,
	ZBID_IMMERSIVE_IHM = 3,
	ZBID_IMMERSIVE_NOTIFICATION = 4,
	ZBID_IMMERSIVE_APPCHROME = 5,
	ZBID_IMMERSIVE_MOGO = 6,
	ZBID_IMMERSIVE_EDGY = 7,
	ZBID_IMMERSIVE_INACTIVEMOBODY = 8,
	ZBID_IMMERSIVE_INACTIVEDOCK = 9,
	ZBID_IMMERSIVE_ACTIVEMOBODY = 10,
	ZBID_IMMERSIVE_ACTIVEDOCK = 11,
	ZBID_IMMERSIVE_BACKGROUND = 12,
	ZBID_IMMERSIVE_SEARCH = 13,
	ZBID_GENUINE_WINDOWS = 14,
	ZBID_IMMERSIVE_RESTRICTED = 15,
	ZBID_SYSTEM_TOOLS = 16,
	ZBID_LOCK = 17,
	ZBID_ABOVELOCK_UX = 18,
};

typedef HWND(WINAPI* CreateWindowInBand)(
	_In_ DWORD dwExStyle,
	_In_opt_ ATOM atom,
	_In_opt_ LPCWSTR lpWindowName,
	_In_ DWORD dwStyle,
	_In_ int X,
	_In_ int Y,
	_In_ int nWidth,
	_In_ int nHeight,
	_In_opt_ HWND hWndParent,
	_In_opt_ HMENU hMenu,
	_In_opt_ HINSTANCE hInstance,
	_In_opt_ LPVOID lpParam, DWORD band
	);

typedef BOOL(WINAPI* GetWindowBand)(
	_In_ HWND hWnd,
	_Out_ PDWORD pdwBand
	);

typedef BOOL(WINAPI* SetWindowBand)(
	_In_ HWND hWnd,
	_In_ HWND hWndInsertAfter,
	_In_ DWORD dwBand
	);

typedef enum _WINDOWCOMPOSITIONATTRIB
{
	WCA_UNDEFINED = 0,
	WCA_NCRENDERING_ENABLED = 1,
	WCA_NCRENDERING_POLICY = 2,
	WCA_TRANSITIONS_FORCEDISABLED = 3,
	WCA_ALLOW_NCPAINT = 4,
	WCA_CAPTION_BUTTON_BOUNDS = 5,
	WCA_NONCLIENT_RTL_LAYOUT = 6,
	WCA_FORCE_ICONIC_REPRESENTATION = 7,
	WCA_EXTENDED_FRAME_BOUNDS = 8,
	WCA_HAS_ICONIC_BITMAP = 9,
	WCA_THEME_ATTRIBUTES = 10,
	WCA_NCRENDERING_EXILED = 11,
	WCA_NCADORNMENTINFO = 12,
	WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
	WCA_VIDEO_OVERLAY_ACTIVE = 14,
	WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
	WCA_DISALLOW_PEEK = 16,
	WCA_CLOAK = 17,
	WCA_CLOAKED = 18,
	WCA_ACCENT_POLICY = 19,
	WCA_FREEZE_REPRESENTATION = 20,
	WCA_EVER_UNCLOAKED = 21,
	WCA_VISUAL_OWNER = 22,
	WCA_HOLOGRAPHIC = 23,
	WCA_EXCLUDED_FROM_DDA = 24,
	WCA_PASSIVEUPDATEMODE = 25,
	WCA_USEDARKMODECOLORS = 26,
	WCA_LAST = 27
} WINDOWCOMPOSITIONATTRIB;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA
{
	WINDOWCOMPOSITIONATTRIB Attrib;
	PVOID pvData;
	SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

typedef enum _ACCENT_STATE
{
	ACCENT_DISABLED = 0,
	ACCENT_ENABLE_GRADIENT = 1,
	ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
	ACCENT_ENABLE_BLURBEHIND = 3,
	ACCENT_ENABLE_ACRYLICBLURBEHIND = 4, // RS4 1803
	ACCENT_ENABLE_HOSTBACKDROP = 5, // RS5 1809
	ACCENT_INVALID_STATE = 6
} ACCENT_STATE;

typedef struct _ACCENT_POLICY
{
	ACCENT_STATE AccentState;
	DWORD AccentFlags;
	DWORD GradientColor;
	DWORD AnimationId;
} ACCENT_POLICY;

typedef BOOL(WINAPI* pfnGetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

typedef BOOL(WINAPI* pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

struct GetMonitorsParams {
	std::vector<MonitorInfo>* monitors;
	SIZE* offset;
};

struct EnumWindowProcParam {
	std::vector<WindowInfo>* windows;
	MonitorInfo monitor;
};

void GetMonitors(
	_Out_ std::vector<MonitorInfo>* monitors
	);

BOOL CALLBACK EnumDisplayMonitorsCallback(
	_In_ HMONITOR hMonitor, 
	_In_ HDC hdcMonitor, 
	_In_ LPRECT lprcMonitor, 
	_Out_ LPARAM dwData
	);

BOOL IsAltTabWindow(
	_In_ HWND hwnd
	);

BOOL CALLBACK EnumWindowsProc(
	_In_ HWND   hwnd,
	_Out_ LPARAM lParam
	);

void GetWindowsOnMonitor(
	_In_ MonitorInfo monitor,
	_Out_ std::vector<WindowInfo>* windows
	);

BOOL CALLBACK GetWallpaperHwnd(
	_In_ HWND hwnd,
	_Out_ LPARAM lParam
	);

void GetWallpaperWindows(
	_Inout_ std::vector<MonitorInfo>* monitors
	);

void GetWindowSlots(
	_In_ std::vector<WindowInfo>* windows,
	_In_ RECT area,
	_Out_ std::vector<Slot>* slots
	);

inline void ComputeSlotCoordinates(
	_In_ HWND hWnd,
	_In_ int x,
	_In_ int y,
	_In_ double scale,
	_In_ RECT realArea,
	_In_ RECT area,
	_Out_ RECT* initialRect,
	_Out_ RECT* finalRect,
	_Out_ POINT* initialCentre,
	_Out_ POINT* finalCentre
	)
{
	GetWindowRect(hWnd, initialRect);
	initialRect->right -= (realArea.left);
	initialRect->bottom -= (realArea.top);
	initialRect->left -= (realArea.left);
	initialRect->top -= (realArea.top);

	finalRect->left = area.left + x;
	finalRect->top = area.top + y;
	finalRect->right = ((initialRect->right - initialRect->left) * scale) + finalRect->left;
	finalRect->bottom = ((initialRect->bottom - initialRect->top) * scale) + finalRect->top;

	initialCentre->x = (initialRect->right - initialRect->left) / 2 + initialRect->left;
	initialCentre->y = (initialRect->bottom - initialRect->top) / 2 + initialRect->top;

	finalCentre->x = (finalRect->right - finalRect->left) / 2 + finalRect->left;
	finalCentre->y = (finalRect->bottom - finalRect->top) / 2 + finalRect->top;
}

HTHUMBNAIL RegisterLiveThumbnail(
	_In_ HWND dest,
	_In_ HWND src,
	_In_ RECT rect
	);

inline double linear(
	_In_ double percent, 
	_In_ double start, 
	_In_ double end, 
	_In_ double total
	)
{
	return start + (end - start) * percent;
}

// p = t / d
inline double easeOutQuad(
	_In_ double p
	) 
{
	double m = p - 1; return 1 - m * m;
};

DWORD WINAPI animate(
	_Inout_ LPVOID lpParam
	);

inline void InitializeAnimation(
	_Inout_ MonitorInfo* monitorInfo
	)
{
	monitorInfo->animation.delay_ms = 1000.0 / FPS;
}

inline void AddAnimation(
	_Inout_ AnimationInfo &animations,
	_In_ POINT initialCentre,
	_In_ POINT finalCentre,
	_In_ RECT initialRect,
	_In_ RECT finalRect,
	_In_ double scale,
	_In_ HWND hWnd,
	_In_ HTHUMBNAIL thumbnail
	)
{
	Animation animation;
	animation.start = initialCentre;
	animation.end = finalCentre;
	animation.w = (initialRect.right - initialRect.left);
	animation.h = (initialRect.bottom - initialRect.top);
	animation.scale = scale;
	animation.hWnd = hWnd;
	animation.thumb = thumbnail;
	animations.animations.push_back(animation);
}

BOOL BeginAnimateUpdate(
	_In_ MonitorInfo* info
	);

void EndAnimateUpdate(
	_In_ MonitorInfo* info
	);

BOOL DoAnimate(
	_In_ BOOL alreadyAcquired,
	_In_ UINT type,
	_In_ MonitorInfo* info,
	_In_ BOOL isOpening
	);

bool SortHwndByZorder(
	_In_ HWND hWnd1,
	_In_ HWND hWnd2
	);

bool SortAnimationsByHwndZorder(
	_In_ Animation a1,
	_In_ Animation a2
	);

RECT GetSearchRect(
	_In_ MonitorInfo* info
	);

void ShowSearch(
	_In_ MonitorInfo* info,
	_In_ WPARAM wParam
	);