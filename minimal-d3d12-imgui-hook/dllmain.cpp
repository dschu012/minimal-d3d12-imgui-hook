#include <Windows.h>
#include "D3D12Hook.h"
#include <utility>
#include <fstream>

DWORD WINAPI AttachThread(LPVOID lParam) {
    if (D3D12::Init() == D3D12::Status::Success) {
        D3D12::InstallHooks();
        //Sleep(5000);
        //D3D12::RemoveHooks();
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
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, &AttachThread, static_cast<LPVOID>(hModule), 0, nullptr);
        break;
    }
    case DLL_PROCESS_DETACH: {
        D3D12::RemoveHooks();
        break;
    }
    }
    return TRUE;
}

