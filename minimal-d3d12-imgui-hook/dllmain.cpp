#include <Windows.h>
#include "D3D12Hook.h"
#include <utility>


DWORD WINAPI InitializeThread(LPVOID lParam) {
    if (D3D12::Init() == D3D12::Status::Success) {
        D3D12::InstallHooks();
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
        CreateThread(nullptr, 0, &InitializeThread, static_cast<LPVOID>(hModule), 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

