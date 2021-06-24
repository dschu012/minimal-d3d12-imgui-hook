#include "D3D12Hook.h"
#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <stdio.h>
#include <string>
#include <chrono>
#include <thread>
#include "plugin/PluginManager.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace D3D12 {

	template<typename T>
	static void SafeRelease(T*& res) {
		if (res)
			res->Release();
		res = NULL;
	}

	//https://github.com/ocornut/imgui/blob/master/examples/example_win32_directx12/main.cpp

	// Data
	static FrameContext* g_frameContext;
	static UINT							g_frameBufferCount = 1;

	static ID3D12Device* g_pd3dDevice = NULL;
	static ID3D12DescriptorHeap* g_pd3dRtvDescHeap = NULL;
	static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;
	static ID3D12CommandQueue* g_pd3dCommandQueue = NULL;
	static ID3D12GraphicsCommandList* g_pd3dCommandList = NULL;


	typedef long(__fastcall* Present) (IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);
	static Present OriginalPresent;

	typedef void(*ExecuteCommandLists)(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* ppCommandLists);
	static ExecuteCommandLists OriginalExecuteCommandLists;

	typedef void(__fastcall* DrawInstanced)(ID3D12GraphicsCommandList* dCommandList, UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation);
	static DrawInstanced OriginalDrawInstanced;

	typedef void(__fastcall* DrawIndexedInstanced)(ID3D12GraphicsCommandList* dCommandList, UINT IndexCount, UINT InstanceCount, UINT StartIndex, INT BaseVertex);
	static DrawIndexedInstanced  OriginalDrawIndexedInstanced;

	typedef HRESULT(*Signal)(ID3D12CommandQueue* queue, ID3D12Fence* pFence, UINT64 pValue);
	static Signal  OriginalSignal;

	WNDPROC	OriginalWndProc;

	DWORD PID;
	HWND Window = nullptr;

	static uint64_t* g_methodsTable = NULL;

	static PluginManager* g_PluginManager;

	/**
	* We aren't really creating a device here, but I wanted to follow IMGUI's
	* naming conventions.
	**/
	bool CreateDeviceD3D(IDXGISwapChain3* pSwapChain) {
		if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&g_pd3dDevice))) {
			return false;
		}

		{
			DXGI_SWAP_CHAIN_DESC desc;
			pSwapChain->GetDesc(&desc);
			desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			desc.OutputWindow = Window;
			desc.Windowed = ((GetWindowLongPtr(Window, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

			g_frameBufferCount = desc.BufferCount;
			g_frameContext = new FrameContext[g_frameBufferCount];
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = g_frameBufferCount;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
				return false;
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			desc.NumDescriptors = g_frameBufferCount;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask = 1;

			if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
				return false;

			const auto rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();

			for (size_t i = 0; i < g_frameBufferCount; i++) {
				ID3D12Resource* pBackBuffer = nullptr;

				g_frameContext[i].main_render_target_descriptor = rtvHandle;
				pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
				g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
				g_frameContext[i].main_render_target_resource = pBackBuffer;
				rtvHandle.ptr += rtvDescriptorSize;
			}

		}

		{
			ID3D12CommandAllocator* allocator;
			if (g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)) != S_OK)
				return false;

			for (size_t i = 0; i < g_frameBufferCount; i++) {
				g_frameContext[i].commandAllocator = allocator;
			}

			if (g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, IID_PPV_ARGS(&g_pd3dCommandList)) != S_OK || g_pd3dCommandList->Close() != S_OK)
				return false;
		}
		return true;
	}

	long __fastcall HookPresent(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags) {
		if (g_pd3dDevice == nullptr) {
			CreateDeviceD3D(pSwapChain);
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO(); (void)io;
			io.IniFilename = nullptr;
			//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
			//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

			// Setup Dear ImGui style
			ImGui::StyleColorsDark();
			//ImGui::StyleColorsClassic();

			// Setup Platform/Renderer backends
			ImGui_ImplWin32_Init(Window);
			ImGui_ImplDX12_Init(g_pd3dDevice, g_frameBufferCount,
				DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap,
				g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
				g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

			g_PluginManager = new PluginManager();
		}

		if (g_pd3dCommandQueue == nullptr) {
			return OriginalPresent(pSwapChain, SyncInterval, Flags);
		}

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		g_PluginManager->Present();

		FrameContext& currentFrameContext = g_frameContext[pSwapChain->GetCurrentBackBufferIndex()];
		currentFrameContext.commandAllocator->Reset();

		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = currentFrameContext.main_render_target_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		g_pd3dCommandList->Reset(currentFrameContext.commandAllocator, nullptr);
		g_pd3dCommandList->ResourceBarrier(1, &barrier);
		g_pd3dCommandList->OMSetRenderTargets(1, &currentFrameContext.main_render_target_descriptor, FALSE, nullptr);
		g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		g_pd3dCommandList->ResourceBarrier(1, &barrier);
		g_pd3dCommandList->Close();

		g_pd3dCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&g_pd3dCommandList));

		return OriginalPresent(pSwapChain, SyncInterval, Flags);
	}

	void HookExecuteCommandLists(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* ppCommandLists) {
		if (!g_pd3dCommandQueue && NumCommandLists == 1)
			g_pd3dCommandQueue = queue;

		OriginalExecuteCommandLists(queue, NumCommandLists, ppCommandLists);
	}

	void __fastcall HookDrawInstanced(ID3D12GraphicsCommandList* dCommandList, UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) {
		OriginalDrawInstanced(dCommandList, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	}

	void __fastcall HookDrawIndexedInstanced(ID3D12GraphicsCommandList* dCommandList, UINT IndexCount, UINT InstanceCount, UINT StartIndex, INT BaseVertex) {
		OriginalDrawIndexedInstanced(dCommandList, IndexCount, InstanceCount, StartIndex, BaseVertex);
	}

	HRESULT HookSignal(ID3D12CommandQueue* queue, ID3D12Fence* pFence, UINT64 Value) {
		return OriginalSignal(queue, pFence, Value);
	}


	Status Init() {
		WNDCLASSEX windowClass;
		windowClass.cbSize = sizeof(WNDCLASSEX);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = DefWindowProc;
		windowClass.cbClsExtra = 0;
		windowClass.cbWndExtra = 0;
		windowClass.hInstance = GetModuleHandle(NULL);
		windowClass.hIcon = NULL;
		windowClass.hCursor = NULL;
		windowClass.hbrBackground = NULL;
		windowClass.lpszMenuName = NULL;
		windowClass.lpszClassName = L"Fake Window";
		windowClass.hIconSm = NULL;

		::RegisterClassEx(&windowClass);

		HWND window = ::CreateWindow(windowClass.lpszClassName, L"Fake DirectX Window", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);

		MH_Initialize();

		HMODULE libDXGI;
		HMODULE libD3D12;

		if ((libDXGI = ::GetModuleHandle(L"dxgi.dll")) == NULL) {
			::DestroyWindow(window);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
			return Status::ModuleNotFoundError;
		}

		if ((libD3D12 = ::GetModuleHandle(L"d3d12.dll")) == NULL) {
			::DestroyWindow(window);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
			return Status::ModuleNotFoundError;
		}

		void* CreateDXGIFactory;
		if ((CreateDXGIFactory = ::GetProcAddress(libDXGI, "CreateDXGIFactory")) == NULL) {
			::DestroyWindow(window);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
			return Status::UnknownError;
		}

		IDXGIFactory* factory;
		if (((long(__stdcall*)(const IID&, void**))(CreateDXGIFactory))(__uuidof(IDXGIFactory), (void**)&factory) < 0) {
			::DestroyWindow(window);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
			return Status::UnknownError;
		}

		IDXGIAdapter* adapter;
		if (factory->EnumAdapters(0, &adapter) == DXGI_ERROR_NOT_FOUND) {
			::DestroyWindow(window);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
			return Status::UnknownError;
		}

		void* D3D12CreateDevice;
		if ((D3D12CreateDevice = ::GetProcAddress(libD3D12, "D3D12CreateDevice")) == NULL) {
			::DestroyWindow(window);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
			return Status::UnknownError;
		}

		ID3D12Device* device;
		if (((long(__stdcall*)(IUnknown*, D3D_FEATURE_LEVEL, const IID&, void**))(D3D12CreateDevice))(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&device) < 0) {
			::DestroyWindow(window);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
			return Status::UnknownError;
		}

		D3D12_COMMAND_QUEUE_DESC queueDesc;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Priority = 0;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 0;

		ID3D12CommandQueue* commandQueue;
		if (device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&commandQueue) < 0) {
			::DestroyWindow(window);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
			return Status::UnknownError;
		}

		ID3D12CommandAllocator* commandAllocator;
		if (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&commandAllocator) < 0) {
			::DestroyWindow(window);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
			return Status::UnknownError;
		}

		ID3D12GraphicsCommandList* commandList;
		if (device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), (void**)&commandList) < 0) {
			::DestroyWindow(window);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
			return Status::UnknownError;
		}

		DXGI_RATIONAL refreshRate;
		refreshRate.Numerator = 60;
		refreshRate.Denominator = 1;

		DXGI_MODE_DESC bufferDesc;
		bufferDesc.Width = 100;
		bufferDesc.Height = 100;
		bufferDesc.RefreshRate = refreshRate;
		bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		DXGI_SAMPLE_DESC sampleDesc;
		sampleDesc.Count = 1;
		sampleDesc.Quality = 0;

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferDesc = bufferDesc;
		swapChainDesc.SampleDesc = sampleDesc;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;
		swapChainDesc.OutputWindow = window;
		swapChainDesc.Windowed = 1;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		IDXGISwapChain* swapChain;
		if (factory->CreateSwapChain(commandQueue, &swapChainDesc, &swapChain) < 0) {
			::DestroyWindow(window);
			::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
			return Status::UnknownError;
		}

		g_methodsTable = (uint64_t*)::calloc(150, sizeof(uint64_t));
		::memcpy(g_methodsTable, *(uint64_t**)device, 44 * sizeof(uint64_t));
		::memcpy(g_methodsTable + 44, *(uint64_t**)commandQueue, 19 * sizeof(uint64_t));
		::memcpy(g_methodsTable + 44 + 19, *(uint64_t**)commandAllocator, 9 * sizeof(uint64_t));
		::memcpy(g_methodsTable + 44 + 19 + 9, *(uint64_t**)commandList, 60 * sizeof(uint64_t));
		::memcpy(g_methodsTable + 44 + 19 + 9 + 60, *(uint64_t**)swapChain, 18 * sizeof(uint64_t));

		SafeRelease(device);
		SafeRelease(commandQueue);
		SafeRelease(commandAllocator);
		SafeRelease(commandList);
		SafeRelease(swapChain);

		::DestroyWindow(window);
		::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);

		return Status::Success;
	}

	Status Hook(uint16_t _index, void** _original, void* _function) {
		void* target = (void*)g_methodsTable[_index];
		if (MH_CreateHook(target, _function, _original) != MH_OK || MH_EnableHook(target) != MH_OK) {
			return Status::UnknownError;
		}
		return Status::Success;
	}

	LRESULT APIENTRY WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		ImGuiIO& io = ImGui::GetIO();
		g_PluginManager->WndProc(hWnd, msg, wParam, lParam);
		switch (msg) {
		case WM_SIZE:
			if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
				ImGui_ImplDX12_InvalidateDeviceObjects();
				ImGui_ImplWin32_Shutdown();
				ImGui::DestroyContext();
				SafeRelease(g_pd3dDevice);
				SafeRelease(g_pd3dRtvDescHeap);
				SafeRelease(g_pd3dSrvDescHeap);
				SafeRelease(g_pd3dCommandQueue);
				SafeRelease(g_pd3dCommandList);
				auto ret = CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
			}
			break;
		case WM_LBUTTONDOWN:
			io.MouseDown[0] = true;
			return io.WantCaptureMouse ? 0 : CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
		case WM_LBUTTONUP:
			io.MouseDown[0] = false;
			return io.WantCaptureMouse ? 0 : CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
		case WM_RBUTTONDOWN:
			io.MouseDown[1] = true;
			return io.WantCaptureMouse ? 0 : CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
		case WM_RBUTTONUP:
			io.MouseDown[1] = false;
			return io.WantCaptureMouse ? 0 : CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
		case WM_MBUTTONDOWN:
			io.MouseDown[2] = true;
			return io.WantCaptureMouse ? 0 : CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
		case WM_MBUTTONUP:
			io.MouseDown[2] = false;
			return io.WantCaptureMouse ? 0 : CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
		case WM_MOUSEWHEEL:
			io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
			return io.WantCaptureMouse ? 0 : CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
		case WM_MOUSEMOVE:
			io.MousePos.x = (signed short)(lParam);
			io.MousePos.y = (signed short)(lParam >> 16);
			return io.WantCaptureMouse ? 0 : CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
		case WM_KEYDOWN:
			if (wParam < 256)
				io.KeysDown[wParam] = 1;
			return io.WantCaptureKeyboard ? 0 : CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
		case WM_KEYUP:
			if (wParam < 256)
				io.KeysDown[wParam] = 0;
			return io.WantCaptureKeyboard ? 0 : CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
		case WM_CHAR:
			// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
			if (wParam > 0 && wParam < 0x10000)
				io.AddInputCharacter((unsigned short)wParam);
			return io.WantCaptureKeyboard ? 0 : CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
		}
		return CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
	}

	Status InstallHooks() {
		Hook(54, (void**)&OriginalExecuteCommandLists, HookExecuteCommandLists);
		Hook(140, (void**)&OriginalPresent, HookPresent);

		//Hook(58, (void**)&OriginalSignal, HookSignal);
		//Hook(84, (void**)&OriginalDrawInstanced, HookDrawInstanced);
		//Hook(85, (void**)&OriginalDrawIndexedInstanced, HookDrawIndexedInstanced);

		OriginalWndProc = (WNDPROC)SetWindowLongPtr(Window, GWLP_WNDPROC, (__int3264)(LONG_PTR)WndProc);

		return Status::Success;
	}

	Status RemoveHooks() {
		MH_DisableHook((void*)g_methodsTable[54]);
		MH_DisableHook((void*)g_methodsTable[140]);

		//MH_DisableHook((void*)g_methodsTable[58]);
		//MH_DisableHook((void*)g_methodsTable[84]);
		//MH_DisableHook((void*)g_methodsTable[85]);
		return Status::Success;
	}

}