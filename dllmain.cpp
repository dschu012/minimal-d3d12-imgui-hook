#include <Windows.h>
#include "D3D12Hook.h"
#include <utility>


DWORD WINAPI InitializeThread(LPVOID lParam) {
    if (D3D12::Init() == D3D12::Status::Success) {
        D3D12::InstallHooks();
    }
    return 0;
}

//https://stackoverflow.com/a/39290139/597419
HWND FindTopWindow(DWORD pid) {
    std::pair<HWND, DWORD> params = { 0, pid };
    BOOL bResult = EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto pParams = (std::pair<HWND, DWORD>*)(lParam);
        DWORD processId;
        if (GetWindowThreadProcessId(hwnd, &processId) && processId == pParams->second) {
            SetLastError(-1);
            pParams->first = hwnd;
            return FALSE;
        }
        return TRUE;
        }, (LPARAM)&params);
    if (!bResult && GetLastError() == -1 && params.first) {
        return params.first;
    }
    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        D3D12::PID = GetCurrentProcessId();
        D3D12::Window = FindTopWindow(D3D12::PID);
        CreateThread(nullptr, 0, &InitializeThread, static_cast<LPVOID>(hModule), 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

