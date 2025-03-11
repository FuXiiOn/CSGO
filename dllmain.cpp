#include <iostream>
#include <Windows.h>
#include "modules.h"
#include "config.h"
#include "directx.h"

BOOL WINAPI HackThread(HMODULE hModule) {
    AllocConsole();
    FILE* file;
    freopen_s(&file, "CONOUT$", "w", stdout);

    initHooks();

    Sleep(100);

    DirectX::initD3D();

    while (!GetAsyncKeyState(VK_END) & 1) {

        if (GetAsyncKeyState(VK_INSERT) & 1) {
            Config::showMenu = !Config::showMenu;
        }

        executeModules();

        Sleep(1);
    }

    disableHooks();

    FreeConsole();
    fclose(file);
    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain( HMODULE hModule,DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)HackThread, hModule, 0, 0);
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

