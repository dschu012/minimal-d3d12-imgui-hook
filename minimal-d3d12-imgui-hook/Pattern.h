#pragma once
#include <Windows.h>
#include <cstdint>
#include <span>
#include "Logging.h"
#include "stb.h"

// Scanner impl
// https://www.unknowncheats.me/forum/general-programming-and-reversing/502738-ida-style-pattern-scanner.html
#define FUNC_DEF(ret, conv, name, args) \
	typedef ret conv name##_t##args##; \
	extern __declspec(selectany) name##_t* name##;

#define FUNC_PATTERN(name, mod, signature) \
	{ \
		constexpr auto _patternBytes = stb::compiletime_string_to_byte_array_data::getter<##signature##>::value; \
		name = reinterpret_cast<name##_t*>(Pattern::Scan(mod,  _patternBytes)); \
		LOG("Found " #name " at {:#08x}", reinterpret_cast<uint64_t>(name) - Pattern::BaseAddress(mod)); \
	}

#define FUNC_PATTERNREF(name, mod, signature, opCodeByteOffset) \
	{ \
		constexpr auto _patternBytes = stb::compiletime_string_to_byte_array_data::getter<##signature##>::value; \
		name = reinterpret_cast<name##_t*>(Pattern::ScanRef(mod,  _patternBytes, opCodeByteOffset)); \
		LOG("Found " #name " at {:#08x}", reinterpret_cast<uint64_t>(name) - Pattern::BaseAddress(mod)); \
	}

#define VAR_DEF(type, name) \
	extern __declspec(selectany) type* name##;

#define VAR_PATTERN(name, mod, signature) \
	{ \
		constexpr auto _patternBytes = stb::compiletime_string_to_byte_array_data::getter<##signature##>::value; \
		name = reinterpret_cast<decltype(##name##)>(Pattern::Scan(mod, _patternBytes); \
		LOG("Found " #name " at {:#08x}", reinterpret_cast<uint64_t>(name) - Pattern::BaseAddress(mod)); \
	}

#define VAR_PATTERNREF(name, mod, signature, opCodeByteOffset) \
	{ \
		constexpr auto _patternBytes = stb::compiletime_string_to_byte_array_data::getter<##signature##>::value; \
		name = reinterpret_cast<decltype(##name##)>(Pattern::ScanRef(mod, _patternBytes, opCodeByteOffset)); \
		LOG("Found " #name " at {:#08x}", reinterpret_cast<uint64_t>(name) - Pattern::BaseAddress(mod)); \
	} 

/*
Sample Usage:
Ptrs.h

#include "Pattern.h"

FUNC_DEF(int64_t, __fastcall, Function1, (void* a1, uint32_t a2, uint16_t a3));
FUNC_DEF(D2UnitStrc*, __fastcall, Function2, (uint32_t a1, uint32_t a2));
VAR_DEF(void, Variable1);

class Ptrs {
public:
	static void Initialize() {
		TIMER_START;
		FUNC_PATTERN(Function1, NULL, "48 83 EC 28 45 0F B7 C8 48 85 C9 74 42 48 8B 89 ? ? ? ? 48 85 C9 74 2F");
		FUNC_PATTERNREF(Function2, NULL, "E8 ? ? ? ? 8B 4F 05", 1);
		VAR_PATTERNREF(Variable1, NULL, "48 8D 3D ? ? ? ? BB ? ? ? ? 48 8B CF E8 ? ? ? ? 48 83 C7 10 ", 3);
		TIMER_END;
	}
};
*/


class Pattern
{
public:
	static DWORD64 BaseAddress(const wchar_t* szModule);
	static DWORD64 Scan(const wchar_t* szModule, const std::span<const int> sPattern);
	/*
	Scans for a pattern that is address is referenced in an opcode. i.e. `call sub_7FF7615FD0D0`, direct reference: [actual address in first opcode] E8 ? ? ? ? 8B 4F 05
	*/
	static DWORD64 ScanRef(const wchar_t* szModule, const std::span<const int> sPattern, int32_t nOpCodeByteOffset = 0);
};

