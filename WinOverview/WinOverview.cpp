#include <iostream>
#include <conio.h>
#include <Windows.h>
#include <shlwapi.h>
#include <Psapi.h>
#include <tlhelp32.h>
#include <assert.h>
#include <sddl.h>
#pragma comment(lib, "Shlwapi.lib")

#include "../WinOverviewLibrary/constants.h"
#include "NtUserBuildHwndList.h"

HMODULE hKernel32 = NULL;
FARPROC hAdrWinExec = NULL;
BOOL running = FALSE;
DWORD lastKey = NULL;
HHOOK keyboardHook = NULL;
HHOOK mouseHook = NULL;
NtUserBuildHwndList pNtUserBuildHwndList = NULL;

DWORD WINAPI run(LPVOID lpParam)
{
    HANDLE hThread = NULL;
    BOOL bResult = FALSE;
    void* pLibRemote = NULL;
    DWORD hLibModule = 0;
    char szHostPath[_MAX_PATH + 2 * sizeof(char)];

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    if (Process32First(snapshot, &entry) == TRUE)
    {
        while (Process32Next(snapshot, &entry) == TRUE)
        {
            if (!wcscmp(entry.szExeFile, L"explorer.exe"))
            {
                HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);

                if (hProcess)
                {
                    GetModuleFileNameA(
                        GetModuleHandle(NULL),
                        szHostPath + sizeof(char),
                        _MAX_PATH
                        );
                    PathRemoveFileSpecA(szHostPath + sizeof(char));
                    szHostPath[0] = '\"';
                    strcat_s(
                        szHostPath,
                        "\\WinOverviewLauncher.exe\""
                        );

                    pLibRemote = VirtualAllocEx(
                        hProcess,
                        NULL,
                        sizeof(szHostPath),
                        MEM_COMMIT,
                        PAGE_READWRITE
                        );
                    assert(pLibRemote != NULL);

                    bResult = WriteProcessMemory(
                        hProcess,
                        pLibRemote,
                        (void*)szHostPath,
                        sizeof(szHostPath),
                        NULL
                        );
                    assert(bResult == TRUE);

                    hThread = CreateRemoteThread(
                        hProcess,
                        NULL,
                        0,
                        (LPTHREAD_START_ROUTINE)hAdrWinExec,
                        pLibRemote,
                        CREATE_SUSPENDED,
                        NULL
                        );
                    assert(hThread != NULL);

                    ResumeThread(
                        hThread
                        );

                    WaitForSingleObject(
                        hThread,
                        INFINITE
                        );

                    GetExitCodeThread(
                        hThread,
                        &hLibModule
                        );

                    UnhookWindowsHookEx(mouseHook);
                    mouseHook = NULL;
                    //printf("WinExec: %d\n", hLibModule);

                    VirtualFreeEx(
                        hProcess,
                        (LPVOID)pLibRemote,
                        0,
                        MEM_RELEASE
                        );

                    CloseHandle(hProcess);

                    break;
                }
            }
        }
    }

    CloseHandle(snapshot);

    running = FALSE;

    return 0;
}

BOOL CALLBACK EnumWindowsSendMessage(
    _In_ HWND   hwnd,
    _In_ LPARAM lParam
    )
{
    TCHAR name[200];
    GetClassName(hwnd, name, 200);
    if (!wcscmp(name, CLASS_NAME))
    {
        SendMessage(hwnd, WM_CLOSE, lParam, 0);
    }
    return TRUE;
}

DWORD WINAPI EnumWindowsSendMessageThread(
    _In_ LPVOID lpParam
    )
{
    Gui_RealEnumWindows(pNtUserBuildHwndList, EnumWindowsSendMessage, reinterpret_cast<LPARAM>(lpParam));
    return 0;
}

BOOL CALLBACK EnumWindowsAskMouse(
    _In_ HWND   hwnd,
    _In_ LPARAM lParam
    )
{
    TCHAR name[200];
    GetClassName(hwnd, name, 200);
    if (!wcscmp(name, CLASS_NAME))
    {
        MSLLHOOKSTRUCT* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        SendMessage(hwnd, WM_ASK_MOUSE, TRUE, MAKELPARAM(info->pt.x, info->pt.y));
    }
    return TRUE;
}

DWORD WINAPI EnumWindowsAskMouseThread(
    _In_ LPVOID lpParam
    )
{
    Gui_RealEnumWindows(pNtUserBuildHwndList, EnumWindowsAskMouse, reinterpret_cast<LPARAM>(lpParam));
    return 0;
}

LRESULT CALLBACK LowLevelMouseProc(
    _In_ int    nCode,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    if (running)
    {
        if (wParam == WM_LBUTTONUP || wParam == WM_RBUTTONUP)
        {
            CreateThread(NULL, 0, EnumWindowsAskMouseThread, reinterpret_cast<LPVOID>(lParam), 0, NULL);
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(
    _In_ int    nCode,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    if (nCode == HC_ACTION)
    {
        if (wParam == WM_KEYDOWN)
        {
            KBDLLHOOKSTRUCT* state = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            printf("%d\n", state->vkCode);
            if (state->vkCode == VK_LWIN || state->vkCode == VK_RWIN || state->vkCode == VK_ESCAPE)
            {
                printf("keydown ");
                printf("running: %d\n", running);
                lastKey = VK_LWIN;
            }
            else
            {
                lastKey = NULL;
            }
        }
        if (wParam == WM_KEYUP)
        {
            KBDLLHOOKSTRUCT* state = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            if ((state->vkCode == VK_RETURN) && running)
            {
                CreateThread(NULL, 0, EnumWindowsSendMessageThread, reinterpret_cast<LPVOID>(1), 0, NULL);
            }
            if ((state->vkCode == VK_LWIN || state->vkCode == VK_RWIN || state->vkCode == VK_ESCAPE) && lastKey == VK_LWIN)
            {
                printf("keyup ");
                printf("%d ", GetTickCount64());
                printf("running: %d\n", running);
                lastKey = NULL;
                if (!running)
                {
                    if (state->vkCode != VK_ESCAPE)
                    {
                        running = TRUE;
                        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)run, NULL, 0, 0);
                        printf("CreateThread running: %d\n", running);
                        mouseHook = SetWindowsHookEx(
                            WH_MOUSE_LL,
                            LowLevelMouseProc,
                            NULL,
                            0
                            );
                    }
                }
                else
                {
                    CreateThread(NULL, 0, EnumWindowsSendMessageThread, reinterpret_cast<LPVOID>(0), 0, NULL);
                }
                if (state->vkCode != VK_ESCAPE)
                {
                    INPUT ip;
                    ip.type = INPUT_KEYBOARD;
                    ip.ki.wScan = 0;
                    ip.ki.time = 0;
                    ip.ki.dwExtraInfo = 0;

                    ip.ki.wVk = VK_CONTROL;
                    ip.ki.dwFlags = 0;
                    SendInput(1, &ip, sizeof(INPUT));

                    ip.ki.wVk = VK_CONTROL;
                    ip.ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(1, &ip, sizeof(INPUT));
                }
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    /*
    FILE* conout;
    if (!AllocConsole())
    {
    }
    if (freopen_s(&conout, "CONOUT$", "w", stdout))
    {
    }
    */
    HMODULE hpath;
    hpath = LoadLibrary(L"win32u.dll");
    pNtUserBuildHwndList = NtUserBuildHwndList(GetProcAddress(hpath, "NtUserBuildHwndList"));

    hKernel32 = GetModuleHandle(L"Kernel32");
    assert(hKernel32 != NULL);

    hAdrWinExec = GetProcAddress(
        hKernel32,
        "WinExec"
        );
    assert(hAdrWinExec != NULL);

    int argc;
    LocalFree(CommandLineToArgvW(GetCommandLineW(), &argc));
    if (argc > 1)
    {
        run(NULL);
        exit(0);
    }

    keyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        NULL,
        0
        );

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }    

    return 0;
}