#include "Pattern.h"
#include <Psapi.h>
#include <vector>

static auto pattern_to_byte = [](const char* pattern)
{
	auto bytes = std::vector<char>{};
	auto start = const_cast<char*>(pattern);
	auto end = const_cast<char*>(pattern) + strlen(pattern);

	for (auto current = start; current < end; ++current)
	{
		if (*current == '?')
		{
			++current;
			if (*current == '?')
				++current;
			bytes.push_back('\?');
		}
		else
		{
			bytes.push_back(static_cast<uint8_t>(strtoul(current, &current, 16)));
		}
	}
	return bytes;
};

DWORD64 Pattern::Scan(const wchar_t* szModule, const char* signature)
{
	HANDLE hProcess = GetCurrentProcess();
	HMODULE hModule = GetModuleHandle(szModule);
	if (hModule == 0 || hProcess == 0) {
		return NULL;
	}
	MODULEINFO mInfo = {};
	GetModuleInformation(hProcess, hModule, &mInfo, sizeof(MODULEINFO));
	DWORD64 base = (DWORD64)mInfo.lpBaseOfDll;
	DWORD64 sizeOfImage = (DWORD64)mInfo.SizeOfImage;
	auto patternBytes = pattern_to_byte(signature);

	DWORD64 patternLength = patternBytes.size();
	auto data = patternBytes.data();

	for (DWORD64 i = 0; i < sizeOfImage - patternLength; i++)
	{
		bool found = true;
		for (DWORD64 j = 0; j < patternLength; j++)
		{
			char a = '\?';
			char b = *(char*)(base + i + j);
			found &= data[j] == a || data[j] == b;
		}
		if (found)
		{
			return base + i;
		}
	}
	return NULL;
}

DWORD64 Pattern::ScanOp(const wchar_t* szModule, const char* signature, int32_t nOpCodeByteOffset)
{
	HANDLE hProcess = GetCurrentProcess();
	HMODULE hModule = GetModuleHandle(szModule);
	if (hModule == 0 || hProcess == 0) {
		return NULL;
	}
	MODULEINFO mInfo = {};
	GetModuleInformation(hProcess, hModule, &mInfo, sizeof(MODULEINFO));
	DWORD64 base = (DWORD64)mInfo.lpBaseOfDll;
	DWORD64 sizeOfImage = (DWORD64)mInfo.SizeOfImage;
	auto patternBytes = pattern_to_byte(signature);

	DWORD64 patternLength = patternBytes.size();
	auto data = patternBytes.data();

	for (DWORD64 i = 0; i < sizeOfImage - patternLength; i++)
	{
		bool found = true;
		for (DWORD64 j = 0; j < patternLength; j++)
		{
			char a = '\?';
			char b = *(char*)(base + i + j);
			found &= data[j] == a || data[j] == b;
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