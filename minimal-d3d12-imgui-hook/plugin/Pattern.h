#pragma once
#include <Windows.h>
#include <cstdint>

// https://www.unknowncheats.me/forum/general-programming-and-reversing/502738-ida-style-pattern-scanner.html
/*
Sample Usage:
typedef int64_t __fastcall MyFunction_t(void* pArg1, uint32_t nArg2);
MyFunction_t* MyFunction;

typedef int64_t __fastcall MyFunctionFromCallSig_t(void* pArg1, uint32_t nArg2);
MyFunctionFromCallSig_t* MyFunctionFromCallSig;

MyPlugin::MyPlugin() {
	MyFunction = reinterpret_cast<MyFunction_t*>(Pattern::Scan(NULL, "48 83 EC 28 45 0F B7 C8 48 85 C9 74 42 48 8B 89 ? ? ? ? 48 85 C9 74 2F"));
	MyFunctionFromCallSig =  reinterpret_cast<MyFunctionFromCallSig_t*>(Pattern::ScanRef(NULL, "E8 ? ? ? ? 8B 4F 05", 1));
}

*/

class Pattern
{
public:
	static DWORD64 Scan(const wchar_t* szModule, const char* signature);
	/*
	Scans for a pattern that is address is referenced in an opcode. i.e. `call sub_7FF7615FD0D0`, direct reference: [actual address in first opcode] E8 ? ? ? ? 8B 4F 05
	*/
	static DWORD64 ScanRef(const wchar_t* szModule, const char* signature, int32_t nOpCodeByteOffset = 0);
};

