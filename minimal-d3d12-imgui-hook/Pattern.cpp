#include "Pattern.h"
#include <Psapi.h>
#include <vector>
#include <unordered_map>

static HANDLE hProcess = GetCurrentProcess();
static std::unordered_map<const wchar_t*, LPMODULEINFO> mModuleInfoMap = {};

static LPMODULEINFO GetModuleInfo(const wchar_t* szModule) {
	if (mModuleInfoMap[szModule]) {
		return mModuleInfoMap[szModule];
	}
	HMODULE hModule = GetModuleHandle(szModule);
	mModuleInfoMap[szModule] = new MODULEINFO();
	GetModuleInformation(hProcess, hModule, mModuleInfoMap[szModule], sizeof(MODULEINFO));
	return mModuleInfoMap[szModule];
}

DWORD64 Pattern::BaseAddress(const wchar_t* szModule)
{
	auto lpmInfo = GetModuleInfo(szModule);
	return (DWORD64)lpmInfo->lpBaseOfDll;
}


DWORD64 Pattern::Scan(const wchar_t* szModule, const std::span<const int> sPattern)
{
	auto lpmInfo = GetModuleInfo(szModule);
	DWORD64 base = (DWORD64)lpmInfo->lpBaseOfDll;
	DWORD64 sizeOfImage = (DWORD64)lpmInfo->SizeOfImage;
	
	unsigned int patternLength = sPattern.size();
	for (DWORD64 i = 0; i < sizeOfImage - patternLength; i++)
	{
		bool found = true;
		for (DWORD64 j = 0; j < patternLength && found; j++)
		{
			char b = *(char*)(base + i + j);
			found &= sPattern[j] == -1 || static_cast<char>(sPattern[j]) == b;
		}
		if (found)
		{
			return base + i;
		}
	}
	return NULL;
}

DWORD64 Pattern::ScanRef(const wchar_t* szModule, const std::span<const int> sPattern, int32_t nOpCodeByteOffset)
{
	auto lpmInfo = GetModuleInfo(szModule);
	DWORD64 base = (DWORD64)lpmInfo->lpBaseOfDll;
	DWORD64 sizeOfImage = (DWORD64)lpmInfo->SizeOfImage;

	unsigned int patternLength = sPattern.size();

	for (DWORD64 i = 0; i < sizeOfImage - patternLength; i++)
	{
		bool found = true;
		for (DWORD64 j = 0; j < patternLength && found; j++)
		{
			char b = *(char*)(base + i + j);
			found &= sPattern[j] == -1 || static_cast<char>(sPattern[j]) == b;
		}
		if (found)
		{
			//generally the size of what your looking for is a dword. relative addr to a func/variable.
			int32_t relativeAddress = *(int32_t*)(base + i + nOpCodeByteOffset);
			return base + i + relativeAddress + nOpCodeByteOffset + sizeof(int32_t);
		}
	}
	return NULL;
}