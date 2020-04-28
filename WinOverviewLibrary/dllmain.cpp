// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <Windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <sddl.h>
#include <tlhelp32.h>
#include <vector>
#include <algorithm>
#include <assert.h>
#pragma comment(lib, "gdi32.lib")

#include "constants.h"
#include "structs.h"
#include "workspace.h"
#include "helpers.h"

HMODULE hpath = NULL;
CreateWindowInBand pCreateWindowInBand = NULL;
GetWindowBand pGetWindowBand = NULL;
SetWindowBand pSetWindowBand = NULL;
auto parentWindowClass = NULL;
auto simpleWindowClass = NULL;
auto bkgWindowClass = NULL;
HWND previous = NULL;

HWINEVENTHOOK fwndHook = NULL;
HWND mainHwnd = NULL;
HWND bkgHwnd = NULL;

std::vector<MonitorInfo> monitors;

void DetectSearchDismiss(
	HWINEVENTHOOK hWinEventHook,
	DWORD event,
	HWND hwnd,
	LONG idObject,
	LONG idChild,
	DWORD idEventThread,
	DWORD dwmsEventTime
	)
{
	TCHAR name[100];
	GetClassName(hwnd, name, 100);
	if (!wcsstr(name, TEXT("HwndWrapper[DefaultDomain;;")) && !wcsstr(name, TEXT("Windows.UI.Core.CoreW")) && wcscmp(name, CLASS_NAME_BKG)) {
		UnhookWinEvent(
			fwndHook
			);
		fwndHook = NULL;

		ShowWindow(mainHwnd, SW_SHOW);
		PostMessage(mainHwnd, WM_CLOSE, 1, 0);
		PostMessage(bkgHwnd, WM_CLOSE, 0, 0);

		mainHwnd = NULL;
		bkgHwnd = NULL;
	}
}

DWORD WINAPI CheckSearch(
	_Inout_ LPVOID lpParam
	)
{
	Sleep(400);

	MonitorInfo* monitorInfo = reinterpret_cast<MonitorInfo*>(lpParam);

	HWND fWnd = GetForegroundWindow();

	if (monitorInfo->hWnd != fWnd && monitorInfo->hSearchHWnd != fWnd)
	{
		for (UINT z = 0; z < monitors.size(); ++z)
		{
			PostMessage(monitors.at(z).hWnd, WM_CLOSE, 2, 0);
		}
	}

	return 0;
}

LRESULT CALLBACK SimpleWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MonitorInfo* info;
	if (uMsg == WM_CREATE)
	{
		CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		info = reinterpret_cast<MonitorInfo*>(pCreate->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)info);
		HINSTANCE hInstance = pCreate->hInstance;
	}
	else
	{
		LONG_PTR ptr = GetWindowLongPtr(hWnd, GWLP_USERDATA);
		info = reinterpret_cast<MonitorInfo*>(ptr);
	}
	switch (uMsg)
	{
	case WM_ACTIVATE:
		SetForegroundWindow(info->hWnd);
		break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WindowProcBkg(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MonitorInfo* info;
	if (uMsg == WM_CREATE)
	{
		CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		info = reinterpret_cast<MonitorInfo*>(pCreate->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)info);
		HINSTANCE hInstance = pCreate->hInstance;
	}
	else
	{
		LONG_PTR ptr = GetWindowLongPtr(hWnd, GWLP_USERDATA);
		info = reinterpret_cast<MonitorInfo*>(ptr);
	}
	switch (uMsg)
	{
	case WM_ACTIVATE:
	{
		if (info->hSearchHWnd && wParam == WA_ACTIVE)
		{
			BOOL bRet;
			if (BeginAnimateUpdate(info))
			{
				ShowWindow(info->hWnd, SW_SHOW);
				PostMessage(info->hWnd, WM_CLOSE, 0, 0);
				PostMessage(hWnd, WM_CLOSE, 0, 0);

				bRet = DoAnimate(TRUE, ANIMTYPE_PREVIEW_FADE, info, TRUE);

				info->hSearchHWnd = NULL;
			}
			return bRet;
		}
		break;
	}
	case WM_LBUTTONUP:
	{
		ShowWindow(info->hWnd, SW_SHOW);
		PostMessage(info->hWnd, WM_CLOSE, 0, 0);
		PostMessage(hWnd, WM_CLOSE, 0, 0);
		break;
	}
	case WM_NCHITTEST:
	{
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		RECT rect = GetSearchRect(info);
		if (xPos >= rect.left && xPos <= rect.right && yPos >= rect.top && yPos <= rect.bottom) {
			//return HTTRANSPARENT;
		}
		else
		{
			//return HTCLIENT;
		}
		break;
	}
	case WM_ERASEBKGND:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC wallpaperHdc = GetDC(info->hWallpaperWnd);

		BitBlt(
			hdc,
			0,
			0,
			info->realArea.right - info->realArea.left,
			info->realArea.bottom - info->realArea.top,
			wallpaperHdc,
			info->realArea.left,
			info->realArea.top,
			SRCCOPY
			);

		EndPaint(hWnd, &ps);
		return 1;
		break;
	}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MonitorInfo* info;
	if (uMsg == WM_CREATE)
	{
		CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		info = reinterpret_cast<MonitorInfo*>(pCreate->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)info);
		HINSTANCE hInstance = pCreate->hInstance;
	}
	else
	{
		LONG_PTR ptr = GetWindowLongPtr(hWnd, GWLP_USERDATA);
		info = reinterpret_cast<MonitorInfo*>(ptr);
	}

	switch (uMsg)
	{
	/*
	case WM_ACTIVATE:
	{
		if (wParam == WA_INACTIVE)
		{
			BOOL isOneInSearch = FALSE;
			BOOL isOurFocused = FALSE;
			HWND fWnd = GetForegroundWindow();
			for (UINT z = 0; z < monitors.size(); ++z)
			{
				if (monitors.at(z).hSearchHWnd != NULL)
				{
					isOneInSearch = TRUE;
				}
				if (monitors.at(z).hWnd == fWnd)
				{
					isOurFocused = TRUE;
				}
			}
			if (!isOneInSearch && !isOurFocused)
			{
				for (UINT z = 0; z < monitors.size(); ++z)
				{
					DoAnimate(FALSE, ANIMTYPE_PREVIEW, &monitors.at(z), FALSE);
				}
			}
		}
	}
	*/
	case WM_NCHITTEST:
	{
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		if (xPos < 10 && yPos < 10)
		{
			return HTCAPTION;
		}
		break;
	}
	case WM_CLOSE:
		if (wParam == 99)
		{
			HANDLE myself;
			myself = OpenProcess(PROCESS_ALL_ACCESS, false, GetCurrentProcessId());
			TerminateProcess(myself, 0);
			return TRUE;
		}
		if (info->hSearchHWnd == NULL)
		{
			BOOL isOneInSearch = FALSE;
			for (UINT z = 0; z < monitors.size(); ++z)
			{
				if (monitors.at(z).hSearchHWnd != NULL)
				{
					isOneInSearch = TRUE;
				}
			}
			if (!isOneInSearch && wParam != 3)
			{
				SetForegroundWindow(previous);
			}
			if (wParam == 3)
			{
				return DoAnimate(FALSE, ANIMTYPE_MAIN_FADE, info, FALSE);
			}
			if (info->focusHWnd == hWnd || wParam == 2 || !isOneInSearch)
			{
				return DoAnimate(FALSE, ANIMTYPE_PREVIEW, info, FALSE);
			}
			return 0;
		}
		else
		{
			LONG styles = GetWindowLong(info->hWnd, GWL_EXSTYLE);
			styles &= ~WS_EX_TRANSPARENT;
			SetWindowLong(info->hWnd, GWL_EXSTYLE, styles);
			SetWindowPos(info->hWnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

			if (!wParam)
			{
				// Esc or mouse pressed when searching
				BOOL bRet;
				if (BeginAnimateUpdate(info))
				{
					//DwmUnregisterThumbnail(info->hSearchThumb);
					//info->hSearchThumb = NULL;
					bRet = DoAnimate(TRUE, ANIMTYPE_PREVIEW_FADE, info, TRUE);

					SetForegroundWindow(hWnd);

					PostMessage(info->hSearchHWnd, WM_CLOSE, 0, 0);
					SetWindowPos(
						info->hSearchHWnd,
						info->hSearchHWnd,
						info->rcSearch.left,
						info->rcSearch.top,
						0,
						0,
						SWP_NOZORDER | SWP_NOSIZE
						);

					info->hSearchHWnd = NULL;

				}
				return bRet;
			}
			else
			{
				// Return pressed when searching
				BOOL bRet = DoAnimate(FALSE, ANIMTYPE_MAIN_FADE, info, FALSE);
				for (UINT z = 0; z < monitors.size(); ++z)
				{
					// semaphore protects us
					DoAnimate(FALSE, ANIMTYPE_PREVIEW, &monitors.at(z), FALSE);
				}
				return bRet;
			}

		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_WINDOWPOSCHANGING:
		return 0;
	
	case WM_ERASEBKGND:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC wallpaperHdc = GetDC(info->hWallpaperWnd);

		BitBlt(
			hdc,
			0,
			0,
			info->realArea.right - info->realArea.left,
			info->realArea.bottom - info->realArea.top,
			wallpaperHdc,
			info->realArea.left - info->wallpaperOffset->cx,
			info->realArea.top - info->wallpaperOffset->cy,
			SRCCOPY
			);

		EndPaint(hWnd, &ps);
		return 1;
		break;
	}
	
	case WM_THREAD_DONE:
		CloseHandle(info->hAnimThread);
		info->hAnimThread = NULL;
		if (!info->animation.isOpening)
		{
			switch (info->animation.type)
			{
			case ANIMTYPE_MAIN_FADE:
			case ANIMTYPE_PREVIEW:
			{
				EndAnimateUpdate(info);
				HANDLE myself;
				myself = OpenProcess(PROCESS_ALL_ACCESS, false, GetCurrentProcessId());
				TerminateProcess(myself, 0);
				return TRUE;
				break;
			}
			case ANIMTYPE_PREVIEW_FADE:
			{

				ShowWindow(info->hWnd, SW_HIDE);
				//PostMessage(h, WM_CLOSE, 0, 0);
				//RECT rect = GetSearchRect(info);

				//info->hSearchThumb = RegisterLiveThumbnail(
				//	hWnd, info->hSearchHWnd, rect
				//	);

				EndAnimateUpdate(info);
				return TRUE;
				break;
			}
			}
		}
		else
		{
			// fix for window not getting back foreground from Search or when opening
			switch (info->animation.type) 
			{
			case ANIMTYPE_PREVIEW:
			case ANIMTYPE_PREVIEW_FADE:
				if (GetForegroundWindow() != hWnd && hWnd == info->focusHWnd)
				{
					auto h = CreateWindowEx(
						WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
						CLASS_NAME_SIMPLE,
						NULL,
						WS_POPUP,
						info->area.left, info->area.top, 10, 10,
						NULL,
						NULL,
						NULL,
						info
						);
					ShowWindow(h, SW_NORMAL);
					PostMessage(h, WM_CLOSE, 0, 0);
				}
				break;
			}
		}
		EndAnimateUpdate(info);
		printf("%d\n", GetForegroundWindow());
		break;
	case WM_KEYUP:
	{
		if (wParam == VK_ESCAPE)
		{
			PostMessage(hWnd, WM_CLOSE, 0, 0);
		}
		break;
	}
	case WM_KEYDOWN:
	{
		if (((wParam >= 'a' && wParam <= 'z') ||
			(wParam >= 'A' && wParam <= 'Z') ||
			(wParam >= '0' && wParam <= '9') ||
			(wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) || wParam == VK_SPACE) && fwndHook == NULL)
		{
			if (BeginAnimateUpdate(info))
			{
				mainHwnd = hWnd;
				fwndHook = SetWinEventHook(
					EVENT_SYSTEM_FOREGROUND,
					EVENT_SYSTEM_FOREGROUND,
					NULL,
					DetectSearchDismiss,
					0,
					0,
					WINEVENT_OUTOFCONTEXT
					);
				bkgHwnd = CreateWindowEx(
					WS_EX_TOOLWINDOW,
					CLASS_NAME_BKG,
					NULL,
					WS_POPUP,
					info->realArea.left, info->realArea.top, info->realArea.right - info->realArea.left, info->realArea.bottom - info->realArea.top,
					NULL,
					NULL,
					NULL,
					info
					);
				ShowWindow(bkgHwnd, SW_NORMAL);
				ShowSearch(info, wParam);
			}
		}
		break;
	}

	case WM_SHOW_SEARCH:
	{
		if (BeginAnimateUpdate(info))
		{
			ShowSearch(info, wParam);
		}
		break;
	}

	case WM_IS_SEARCH:
	{
		return info->hSearchHWnd != NULL;
	}

	case WM_ASK_MOUSE:
	{
		RECT rect = GetSearchRect(info);

		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		if (info->focusHWnd == hWnd)
		{
			BOOL isInside = FALSE;
			for (UINT z = 0; z < monitors.size(); ++z)
			{
				if (xPos >= monitors.at(z).realArea.left && xPos <= monitors.at(z).realArea.right && yPos >= monitors.at(z).realArea.top && yPos <= monitors.at(z).realArea.bottom)
				{
					isInside = TRUE;
				}
			}
			if (!isInside)
			{
				for (UINT z = 0; z < monitors.size(); ++z)
				{
					monitors.at(z).hSearchHWnd = NULL;
					PostMessage(monitors.at(z).hWnd, WM_CLOSE, 3, 0);
				}
			}
		}

		if (info->hSearchHWnd != NULL) // && xPos >= rect.left && xPos <= rect.right && yPos >= rect.top && yPos <= rect.bottom)
		{
			//printf("-> %d\n", wParam);
			if (xPos >= rect.left && xPos <= rect.right && yPos >= rect.top && yPos <= rect.bottom)
			{
				if (wParam && info->focusHWnd == hWnd)
				{
					HANDLE thread = CreateThread(
						NULL,
						0,
						CheckSearch,
						reinterpret_cast<LPVOID>(info),
						0,
						NULL
						);
					CloseHandle(thread);
				}
			}
			else
			{
				LONG styles = GetWindowLong(hWnd, GWL_EXSTYLE);
				LONG new_styles = styles & ~WS_EX_TRANSPARENT;
				SetWindowLong(hWnd, GWL_EXSTYLE, new_styles);
				SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
				PostMessage(hWnd, WM_CLOSE, 0, 0);
			}
			return TRUE;
		}
		return FALSE;
		break;
	}

	case WM_LBUTTONUP:
	{
		if (!wParam)
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			if (info->hSearchHWnd == NULL)
			{
				if (BeginAnimateUpdate(info))
				{
					BOOL bClickOk = FALSE;
					for (int i = 0; i < info->animation.animations.size(); ++i) {
						Animation* animation = &(info->animation.animations.at(i));

						RECT rect = { 0,0,0,0 };
						rect.left = animation->end.x - (animation->w * animation->scale) / 2;
						rect.top = animation->end.y - (animation->h * animation->scale) / 2;
						rect.right = (animation->w * animation->scale) + rect.left;
						rect.bottom = (animation->h * animation->scale) + rect.top;

						if (xPos >= rect.left && xPos <= rect.right && yPos >= rect.top && yPos <= rect.bottom)
						{
							DWORD myBand;
							pGetWindowBand(animation->hWnd, &myBand);
							DwmUnregisterThumbnail(animation->thumb);
							animation->thumb = RegisterLiveThumbnail(hWnd, animation->hWnd, rect);
							for (int j = 0; j < info->animation.animations.size(); ++j) {
								if (i != j)
								{
									DWORD band;
									pGetWindowBand(info->animation.animations.at(j).hWnd, &band);
									if (band > myBand)
									{
										DwmUnregisterThumbnail(info->animation.animations.at(j).thumb);
										info->animation.animations.at(j).thumb = RegisterLiveThumbnail(hWnd, info->animation.animations.at(j).hWnd, rect);
									}
								}
							}
							SetForegroundWindow(animation->hWnd);
							DoAnimate(TRUE, ANIMTYPE_PREVIEW, info, FALSE);
							for (UINT z = 0; z < monitors.size(); ++z)
							{
								// we are protected by the semaphore
								DoAnimate(FALSE, ANIMTYPE_PREVIEW, &monitors.at(z), FALSE);
							}
							bClickOk = TRUE;
							break;
						}
					}
					if (!bClickOk)
					{
						SetForegroundWindow(previous);
						DoAnimate(TRUE, ANIMTYPE_PREVIEW, info, FALSE);
						for (UINT z = 0; z < monitors.size(); ++z)
						{
							DoAnimate(FALSE, ANIMTYPE_PREVIEW, &monitors.at(z), FALSE);
						}
					}
				}
			}
			else
			{
				RECT rect = GetSearchRect(info);
				if (!(xPos >= rect.left && xPos <= rect.right && yPos >= rect.top && yPos <= rect.bottom))
				{
					if (BeginAnimateUpdate(info))
					{
						//DwmUnregisterThumbnail(info->hSearchThumb);
						//info->hSearchThumb = NULL;
						DoAnimate(TRUE, ANIMTYPE_PREVIEW_FADE, info, TRUE);
						
						SetForegroundWindow(hWnd);

						PostMessage(info->hSearchHWnd, WM_CLOSE, 0, 0);
						SetWindowPos(
							info->hSearchHWnd,
							info->hSearchHWnd,
							info->rcSearch.left,
							info->rcSearch.top,
							0,
							0,
							SWP_NOZORDER | SWP_NOSIZE
							);

						info->hSearchHWnd = NULL;
					}
				}
			}
		}
		break;
	}

	default:
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

BOOL running = FALSE;

HWND CreateWin(RECT rect, LPVOID info)
{
	if (hpath == NULL)
	{
		hpath = LoadLibrary(L"user32.dll");
		pCreateWindowInBand =  CreateWindowInBand(GetProcAddress(hpath, "CreateWindowInBand"));
		pGetWindowBand = GetWindowBand(GetProcAddress(hpath, "GetWindowBand"));
		pSetWindowBand = SetWindowBand(GetProcAddress(hpath, "SetWindowBand"));
	}

	auto hwndParent = pCreateWindowInBand(
		WS_EX_TOPMOST | WS_EX_LAYERED,// | WS_EX_NOREDIRECTIONBITMAP,
		parentWindowClass,
		NULL,
		WS_POPUP,
		rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		NULL,
		NULL,
		NULL,
		info,
		ZBID_LOCK
		);

	SetLayeredWindowAttributes(hwndParent, 0, 255, LWA_ALPHA);

	const pfnSetWindowCompositionAttribute SetWindowCompositionAttribute = (pfnSetWindowCompositionAttribute)GetProcAddress(hpath, "SetWindowCompositionAttribute");
	if (SetWindowCompositionAttribute)
	{
		ACCENT_POLICY accent = { ACCENT_ENABLE_BLURBEHIND, 0, 0xCC000000, 0 };
		WINDOWCOMPOSITIONATTRIBDATA data;
		data.Attrib = WCA_ACCENT_POLICY;
		data.pvData = &accent;
		data.cbData = sizeof(accent);
		//SetWindowCompositionAttribute(hwndParent, &data);
	}

	return hwndParent;
}

__declspec(dllexport) DWORD WINAPI main(LPVOID lpParam)
{
	if (running) {
		return 0;
	}

	running = TRUE;
	previous = GetForegroundWindow();
	/*
	FILE* conout;
	if (!AllocConsole())
	{
	}
	if (freopen_s(&conout, "CONOUT$", "w", stdout))
	{
	}
	*/

	WNDCLASSEX wndParentClass = {};
	wndParentClass.cbSize = sizeof(WNDCLASSEX);
	wndParentClass.cbClsExtra = 0;
	wndParentClass.hIcon = NULL;
	wndParentClass.lpszMenuName = NULL;
	wndParentClass.hIconSm = NULL;
	wndParentClass.lpfnWndProc = WindowProc;
	wndParentClass.hInstance = NULL;
	wndParentClass.hCursor = LoadCursor(0, IDC_ARROW);
	wndParentClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndParentClass.lpszClassName = CLASS_NAME;
	parentWindowClass = RegisterClassEx(&wndParentClass);

	WNDCLASSEX wndParentClass2 = {};
	wndParentClass2.cbSize = sizeof(WNDCLASSEX);
	wndParentClass2.cbClsExtra = 0;
	wndParentClass2.hIcon = NULL;
	wndParentClass2.lpszMenuName = NULL;
	wndParentClass2.hIconSm = NULL;
	wndParentClass2.lpfnWndProc = SimpleWindowProc;
	wndParentClass2.hInstance = NULL;
	wndParentClass2.hCursor = LoadCursor(0, IDC_ARROW);
	wndParentClass2.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndParentClass2.lpszClassName = CLASS_NAME_SIMPLE;
	simpleWindowClass = RegisterClassEx(&wndParentClass2);

	WNDCLASSEX wndParentClass3 = {};
	wndParentClass3.cbSize = sizeof(WNDCLASSEX);
	wndParentClass3.cbClsExtra = 0;
	wndParentClass3.hIcon = NULL;
	wndParentClass3.lpszMenuName = NULL;
	wndParentClass3.hIconSm = NULL;
	wndParentClass3.lpfnWndProc = WindowProcBkg;
	wndParentClass3.hInstance = NULL;
	wndParentClass3.hCursor = LoadCursor(0, IDC_ARROW);
	wndParentClass3.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndParentClass3.lpszClassName = CLASS_NAME_BKG;
	bkgWindowClass = RegisterClassEx(&wndParentClass3);

	SIZE_T monitorNo, slotNo;
	BOOL atLeastOneWindow = FALSE;

	GetMonitors(&monitors);
	for (monitorNo = 0; monitorNo < monitors.size(); ++monitorNo) {
		monitors.at(monitorNo).realArea = monitors.at(monitorNo).rcWork;
		monitors.at(monitorNo).area = monitors.at(monitorNo).rcWork;
		// normalize area - move its origin to (0, 0) as we use these for displaying inside the window,
		// whose origin is always at (0, 0) no matter its physical coordinates (where it is on the displays)
		monitors.at(monitorNo).area.right -= monitors.at(monitorNo).area.left + AREA_BORDER * 2;
		monitors.at(monitorNo).area.bottom -= monitors.at(monitorNo).area.top + AREA_BORDER * 2;
		monitors.at(monitorNo).area.left = 0 + AREA_BORDER;
		monitors.at(monitorNo).area.top = 0 + AREA_BORDER;

		std::vector<WindowInfo> windows;
		GetWindowsOnMonitor(monitors.at(monitorNo), &windows);

		if (windows.size() == 0)
		{
			RECT r;
			r.left = 0;
			r.right = 0;
			r.right = 0;
			r.bottom = 0;
			monitors.at(monitorNo).realArea = r;
			continue;
		}
		atLeastOneWindow = TRUE;

		std::vector<Slot> slots;
		GetWindowSlots(&windows, monitors.at(monitorNo).area, &slots);
		sort(slots.begin(), slots.end(), SortSlotsByHwndZOrder);

		monitors.at(monitorNo).hWnd = CreateWin(
			monitors.at(monitorNo).realArea,
			&monitors.at(monitorNo)
			);

		InitializeAnimation(
			&monitors.at(monitorNo)
			);

		if (BeginAnimateUpdate(&monitors.at(monitorNo)))
		{
			for (slotNo = 0; slotNo < slots.size(); ++slotNo)
			{
				RECT initialRect;
				RECT finalRect;
				POINT initialCentre;
				POINT finalCentre;

				ComputeSlotCoordinates(
					slots.at(slotNo).window.hwnd,
					slots.at(slotNo).x,
					slots.at(slotNo).y,
					slots.at(slotNo).scale,
					monitors.at(monitorNo).realArea,
					monitors.at(monitorNo).area,
					&initialRect,
					&finalRect,
					&initialCentre,
					&finalCentre
					);

				AddAnimation(
					monitors.at(monitorNo).animation,
					initialCentre,
					finalCentre,
					initialRect,
					finalRect,
					slots.at(slotNo).scale,
					slots.at(slotNo).window.hwnd,
					RegisterLiveThumbnail(
						monitors.at(monitorNo).hWnd,
						slots.at(slotNo).window.hwnd,
						initialRect
						)
					);

			}
			EndAnimateUpdate(&monitors.at(monitorNo));
		}

		
		printf("%d, windows size: %d\n", monitorNo, windows.size());
		/*
		monitors.at(monitorNo).bkgHWnd = pCreateWindowInBand(
			WS_EX_TOPMOST | WS_EX_LAYERED,
			bkgWindowClass,
			NULL,
			WS_POPUP,
			monitors.at(monitorNo).area.left, monitors.at(monitorNo).area.top, monitors.at(monitorNo).area.right - monitors.at(monitorNo).area.left, monitors.at(monitorNo).area.bottom - monitors.at(monitorNo).area.top,
			NULL,
			NULL,
			NULL,
			&monitors.at(monitorNo),
			ZBID_IMMERSIVE_APPCHROME
			);
		SetLayeredWindowAttributes(monitors.at(monitorNo).bkgHWnd, 0, 255, LWA_ALPHA);
		ShowWindow(monitors.at(monitorNo).bkgHWnd, SW_SHOWNORMAL);
		*/


	}

	UINT uIndex;
	POINT pCur;
	GetCursorPos(&pCur);
	HMONITOR hMon = MonitorFromPoint(pCur, MONITOR_DEFAULTTONULL);
	for (UINT z = 0; z < monitors.size(); ++z)
	{
		if (monitors.at(z).hMonitor == hMon)
		{
			uIndex = z;
		}
		else
		{
			if (monitors.at(z).hWnd)
			{
				DoAnimate(FALSE, ANIMTYPE_PREVIEW, &monitors.at(z), TRUE);
				ShowWindow(monitors.at(z).hWnd, SW_SHOW);
			}
		}
	}
	monitors.at(uIndex).focusHWnd = monitors.at(uIndex).hWnd;
	if (monitors.at(uIndex).hWnd)
	{
		DoAnimate(FALSE, ANIMTYPE_PREVIEW, &monitors.at(uIndex), TRUE);
		ShowWindow(monitors.at(uIndex).hWnd, SW_SHOW);
		SetForegroundWindow(monitors.at(uIndex).hWnd);
	}
	for (UINT z = 0; z < monitors.size(); ++z)
	{
		monitors.at(z).focusHWnd = monitors.at(uIndex).hWnd;
	}
	if (!atLeastOneWindow)
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

		exit(0);
	}

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

BOOL APIENTRY DllMain( HINSTANCE hDllHandle,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hDllHandle);
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

