#pragma once

#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_4.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace D3D12 {
	struct FrameContext {
		ID3D12CommandAllocator* commandAllocator = nullptr;
		ID3D12Resource* main_render_target_resource = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE main_render_target_descriptor;
	};

	enum class Status {
		UnknownError = -1,
		NotSupportedError = -2,
		ModuleNotFoundError = -3,

		AlreadyInitializedError = -4,
		NotInitializedError = -5,

		Success = 0,
	};

	Status Init();

	Status InstallHooks();
	Status RemoveHooks();

	bool CreateDeviceD3D(IDXGISwapChain3* pSwapChain);
	void CleanupDeviceD3D();
	void CreateRenderTarget();
	void WaitForLastSubmittedFrame();
	void CleanupRenderTarget();
	FrameContext* WaitForNextFrameResources();
	void ResizeSwapChain(HWND hWnd, int width, int height);

	extern DWORD PID;
	extern HWND Window;

}
