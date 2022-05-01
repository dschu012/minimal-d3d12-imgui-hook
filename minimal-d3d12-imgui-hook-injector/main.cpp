#include <iostream>
#include <Windows.h>
#include <tlhelp32.h>
#include <vector>
#include <format>
#include <filesystem>
#include <wtsapi32.h>
#include <Psapi.h>

#pragma comment(lib, "Wtsapi32.lib")

std::vector<DWORD> GetPIDs(std::wstring processName);
std::wstring ExePath();
void EjectDLL(const int& pid, const std::wstring& path);
void InjectDLL(const int& pid, const std::wstring& path);

/*
* Simple basic injector as a proof of concept. May or may not work on your game. Launch using 
* ./minimal-d3d12-imgui-hook-injector.exe "MyGame.exe" or if running from Visual Studio's add
* the process executable name as a Debug > Command Argument
*/
int main(int argc, char* argv[])
{
	if (argc != 2) {
		std::wcerr << L"Provide the process name as an argument or add it in Debug > Command Argument: ./minimal-d3d12-imgui-hook-injector.exe \"MyGame.exe\"" << std::endl;
		exit(-1);
	}
	wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	std::wcout << L"[+]Looking for to inject into \"" << wargv[1] << "\"" << std::endl;
	std::vector<DWORD> pids = GetPIDs(wargv[1]);
	for (auto& pid : pids) {
		std::wstring dllName = L"overlay.dll";
		std::wstring dllPath = std::format(L"{}\\{}", ExePath(), dllName);
		std::wcout << L"[+]Injecting into " << pid << std::endl;
		EjectDLL(pid, dllName);
		InjectDLL(pid, dllPath);
	}
	exit(0);
	//system("pause");
}

std::wstring ExePath() {
	TCHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
	return std::wstring(buffer).substr(0, pos);
}

std::vector<DWORD> GetPIDs(std::wstring processName) {
	std::vector<DWORD> pids;
	WTS_PROCESS_INFO* pWPIs = NULL;
	DWORD dwProcCount = 0;
	if (WTSEnumerateProcessesW(WTS_CURRENT_SERVER_HANDLE, NULL, 1, &pWPIs, &dwProcCount))
	{
		//Go through all processes retrieved
		for (DWORD i = 0; i < dwProcCount; i++)
		{
			if (!wcscmp(pWPIs[i].pProcessName, processName.c_str())) {
				pids.push_back(pWPIs[i].ProcessId);
			}
		}
	}

	//Free memory
	if (pWPIs)
	{
		WTSFreeMemory(pWPIs);
		pWPIs = NULL;
	}
	return pids;
}

void EjectDLL(const int& pid, const std::wstring& path) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
	LPVOID dwModuleBaseAddress = 0;
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		std::wcerr << L"[!]Fail to create tool help snapshot!" << std::endl;
		exit(-1);
	}

	MODULEENTRY32 ModuleEntry32 = { 0 };
	ModuleEntry32.dwSize = sizeof(MODULEENTRY32);
	bool found = false;
	if (Module32First(hSnapshot, &ModuleEntry32))
	{
		do
		{
			if (wcscmp(ModuleEntry32.szModule, path.c_str()) == 0)
			{
				found = true;
				break;
			}
		} while (Module32Next(hSnapshot, &ModuleEntry32));
	}
	CloseHandle(hSnapshot);

	if (found) {
		HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

		std::wcout << L"[+]DLL already injected. Ejecting first." << std::endl;
		HMODULE hKernel32 = LoadLibrary(L"kernel32");
		LPVOID lpStartAddress = GetProcAddress(hKernel32, "FreeLibrary");
		HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(lpStartAddress), ModuleEntry32.modBaseAddr, 0, NULL);
		if (hThread == NULL) {
			std::wcerr << L"[!]Fail to create Remote Thread" << std::endl;
			exit(-1);
		}
		WaitForSingleObject(hThread, INFINITE);

		//FreeLibrary(hKernel32);
		CloseHandle(hProc);
		CloseHandle(hThread);
	}
}

void InjectDLL(const int& pid, const std::wstring& path) {
	if (!std::filesystem::exists(path)) {
		std::wcerr << L"[!]Couldn't find DLL!" << std::endl;
		exit(-1);
	}

	long dll_size = path.length() * sizeof(wchar_t) + 1;
	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

	if (hProc == NULL) {
		std::wcerr << L"[!]Fail to open target process!" << std::endl;
		exit(-1);
	}
	std::wcout << L"[+]Opening Target Process..." << std::endl;

	LPVOID lpAlloc = VirtualAllocEx(hProc, NULL, dll_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (lpAlloc == NULL) {
		std::wcerr << L"[!]Fail to allocate memory in Target Process." << std::endl;
		exit(-1);
	}

	std::wcout << L"[+]Allocating memory in Target Process." << std::endl;
	if (WriteProcessMemory(hProc, lpAlloc, path.c_str(), dll_size, 0) == 0) {
		std::wcerr << L"[!]Fail to write in Target Process memory." << std::endl;
		exit(-1);
	}
	std::wcout << L"[+]Creating Remote Thread in Target Process" << std::endl;

	HMODULE hKernel32 = LoadLibrary(L"kernel32");
	LPVOID lpStartAddress = GetProcAddress(hKernel32, "LoadLibraryW");
	HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(lpStartAddress), lpAlloc, 0, NULL);
	if (hThread == NULL) {
		std::wcerr << L"[!]Fail to create Remote Thread" << std::endl;
		exit(-1);
	}
	WaitForSingleObject(hThread, INFINITE);

	//VirtualFreeEx(hProc, lpAlloc, 0, MEM_RELEASE);
	//FreeLibrary(hKernel32);
	CloseHandle(hProc);
	CloseHandle(hThread);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
