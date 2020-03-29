#include <iostream>
#include <conio.h>
#include <Windows.h>
#include <shlwapi.h>
#include <Psapi.h>
#include <tlhelp32.h>
#include <assert.h>
#include <sddl.h>
#pragma comment(lib, "Shlwapi.lib")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    FILE* conout;
    wchar_t szHostPath[_MAX_PATH];
    wchar_t szLibPath[_MAX_PATH];
    HANDLE hThread = NULL;
    void* pLibRemote = NULL;
    DWORD hLibModule = 0;
    HMODULE hKernel32 = NULL;
    BOOL bResult = FALSE;
    FARPROC hAdrLoadLibrary = NULL;
    HANDLE hJob;
    JOBOBJECT_BASIC_LIMIT_INFORMATION jobInfoBasic;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo;
    HMODULE hMods[1024];
    DWORD cbNeeded;
    unsigned int i;
    HMODULE hInjectionDll;
    FARPROC hInjectionMainFunc;

    /*
    if (!AllocConsole())
    {
    }
    if (freopen_s(&conout, "CONOUT$", "w", stdout))
    {
    }
    */

    bResult = SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        );

    GetSystemDirectory(
        szHostPath,
        _MAX_PATH
        );
    lstrcat(
        szHostPath,
        L"\\RuntimeBroker.exe"
        );

    STARTUPINFO info = { sizeof(info) };
    PROCESS_INFORMATION processInfo;
    bResult = CreateProcess(
        //hChildToken,
        NULL,
        szHostPath,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &info,
        &processInfo
        );
    assert(bResult != 0);

    assert(processInfo.hProcess != NULL);

    hJob = CreateJobObject(
        NULL,
        NULL
        );
    jobInfoBasic.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    jobInfo.BasicLimitInformation = jobInfoBasic;
    SetInformationJobObject(
        hJob,
        JobObjectExtendedLimitInformation, &jobInfo,
        sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION)
        );
    AssignProcessToJobObject(
        hJob,
        processInfo.hProcess
        );

    GetModuleFileName(
        GetModuleHandle(NULL),
        szLibPath,
        _MAX_PATH
        );
    PathRemoveFileSpec(szLibPath);
    lstrcat(
        szLibPath,
        L"\\WinOverview.dll"
        );
    hKernel32 = GetModuleHandle(L"Kernel32");
    assert(hKernel32 != NULL);

    hAdrLoadLibrary = GetProcAddress(
        hKernel32,
        "LoadLibraryW"
        );
    assert(hAdrLoadLibrary != NULL);

    pLibRemote = VirtualAllocEx(
        processInfo.hProcess,
        NULL,
        sizeof(szLibPath),
        MEM_COMMIT,
        PAGE_READWRITE
        );
    assert(pLibRemote != NULL);

    bResult = WriteProcessMemory(
        processInfo.hProcess,
        pLibRemote,
        (void*)szLibPath,
        sizeof(szLibPath),
        NULL
        );
    assert(bResult == TRUE);

    hThread = CreateRemoteThread(
        processInfo.hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)hAdrLoadLibrary,
        pLibRemote,
        0,
        NULL
        );
    assert(hThread != NULL);
    WaitForSingleObject(
        hThread,
        INFINITE
        );

    GetExitCodeThread(
        hThread,
        &hLibModule
        );
    //assert(hLibModule != NULL);

    wchar_t szTmpLibPath[_MAX_PATH];
    hInjectionDll = LoadLibrary(szLibPath);
    hInjectionMainFunc = GetProcAddress(
        hInjectionDll,
        "main"
        );
    CharLower(szLibPath);
    if (EnumProcessModules(processInfo.hProcess, hMods, sizeof(hMods), &cbNeeded))
    {
        for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
        {
            TCHAR szModName[MAX_PATH];
            GetModuleFileNameEx(processInfo.hProcess, hMods[i], szTmpLibPath, _MAX_PATH);
            CharLower(szTmpLibPath);
            if (!wcscmp(szTmpLibPath, szLibPath))
            {
                break;
            }
        }
    }
    hThread = CreateRemoteThread(
        processInfo.hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)((uint64_t)hMods[i] + (uint64_t)hInjectionMainFunc - (uint64_t)hInjectionDll),
        NULL,
        0,
        NULL
        );
    assert(hThread != NULL);
    WaitForSingleObject(
        hThread,
        INFINITE
        );
    GetExitCodeThread(
        hThread,
        &hLibModule
        );

    VirtualFreeEx(
        processInfo.hProcess,
        (LPVOID)pLibRemote,
        0,
        MEM_RELEASE
        );

    WaitForSingleObject(processInfo.hProcess, INFINITE);

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    return 0;
}