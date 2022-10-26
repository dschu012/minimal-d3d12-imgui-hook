#pragma once
#include <Windows.h>
#include <cstdint>

// https://www.unknowncheats.me/forum/general-programming-and-reversing/502738-ida-style-pattern-scanner.html
// Use the next two creating globals header/class files.
#define FUNC_TYPEDEF(ret, conv, name, args) \
	typedef ret conv name##_t##args##;

#define FUNC_DEF(name) \
	name##_t* name##;

#define EXTERN_FUNC_DEF(ret, conv, name, args) \
	FUNC_TYPEDEF(ret, conv, name, args) \
	extern FUNC_DEF(name)

// Use this if you want it declared in only this class.
#define LOCAL_FUNC_DEF(ret, conv, name, args) \
	FUNC_TYPEDEF(ret, conv, name, args) \
	FUNC_DEF(name)

#define FUNC_PATTERN(name, mod, signature) \
	name = reinterpret_cast<name##_t*>(Pattern::Scan(mod, signature));

#define FUNC_PATTERNREF(name, mod, signature, opCodeByteOffset) \
	name = reinterpret_cast<name##_t*>(Pattern::ScanRef(mod, signature, opCodeByteOffset));

// Use the next two creating globals header/class files.
#define VAR_DEF(type, name) \
	type* name##;

#define EXTERN_VAR_DEF(type, name) \
	extern VAR_DEF(type, name)

// Use this if you want it declared in only this class.
#define LOCAL_VAR_DEF(type, name) \
	VAR_DEF(type, name)

#define VAR_PATTERN(name, type, mod, signature) \
	name = reinterpret_cast<type*>(Pattern::Scan(mod, signature));

#define VAR_PATTERNREF(name, type, mod, signature, opCodeByteOffset) \
	name = reinterpret_cast<type*>(Pattern::ScanRef(mod, signature, opCodeByteOffset));

/*
Sample Usage:
Sample.h

typedef int64_t __fastcall MyFunction_t(void* pArg1, uint32_t nArg2);
extern MyFunction_t* MyFunction;

typedef int64_t __fastcall MyFunctionFromCallSig_t(void* pArg1, uint32_t nArg2);
extern MyFunctionFromCallSig_t* MyFunctionFromCallSig;

OR

EXTERN_FUNC_DEF(int64_t, __fastcall, MyFunction, (void* pArg1, uint32_t nArg2));
EXTERN_FUNC_DEF(int64_t, __fastcall, MyFunctionFromCallSig, (void* pArg1, uint32_t nArg2));

Sample.cpp

FUNC_DEF(MyFunction);
FUNC_DEF(MyFunctionFromCallSig);

MyPlugin::MyPlugin() {
	MyFunction = reinterpret_cast<MyFunction_t*>(Pattern::Scan(NULL, "48 83 EC 28 45 0F B7 C8 48 85 C9 74 42 48 8B 89 ? ? ? ? 48 85 C9 74 2F"));
	MyFunctionFromCallSig =  reinterpret_cast<MyFunctionFromCallSig_t*>(Pattern::ScanRef(NULL, "E8 ? ? ? ? 8B 4F 05", 1));

	OR

	FUNC_PATTERN(MyFunction, NULL, "48 83 EC 28 45 0F B7 C8 48 85 C9 74 42 48 8B 89 ? ? ? ? 48 85 C9 74 2F");
	FUNC_PATTERNREF(MyFunctionFromCallSig, NULL,  "E8 ? ? ? ? 8B 4F 05", 1);
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

