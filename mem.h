#pragma once
#include <Windows.h>
#include <iostream>
#include <vector>
#include <Psapi.h>

namespace mem {
	uintptr_t FindDMAAddy(uintptr_t ptr, std::vector<unsigned int> offsets);
	uintptr_t PatternScan(HMODULE module, const unsigned char* pattern, const char* mask);
	void Patch(BYTE* dst, BYTE* newBytes, size_t length);
	void Nop(BYTE* dst, size_t length);
	bool Hook(BYTE* src, BYTE* dst, size_t length);
	BYTE* TrampHook(BYTE* src, BYTE* dst, size_t length);
}