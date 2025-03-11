#include "mem.h"

uintptr_t mem::FindDMAAddy(uintptr_t ptr, std::vector<unsigned int> offsets) {
	uintptr_t addr = ptr;

	for (int i = 0; i < offsets.size(); i++) {
		addr = *(uintptr_t*)addr;
		addr += offsets[i];
	}

	return addr;
}

uintptr_t mem::PatternScan(HMODULE module, const unsigned char* pattern, const char* mask) {
	MODULEINFO modInfo = {};
	GetModuleInformation(GetCurrentProcess(), module, &modInfo, sizeof(modInfo));

	size_t pos = 0;
	int masklength = strlen(mask) - 1;
	uintptr_t start = reinterpret_cast<uintptr_t>(module);
	size_t length = modInfo.SizeOfImage;

	for (auto i = start; i < start + length; i++) {
		if (*reinterpret_cast<unsigned char*>(i) == pattern[pos] || mask[pos] == '?') {
			if (mask[pos + 1] == '\0') {
				return i - masklength;
			}
			
			pos++;
		}
		else {
			pos = 0;
		}
	}
	return 0;
}

void mem::Patch(BYTE* dst, BYTE* newBytes, size_t length) {
	DWORD oldProtect;
	VirtualProtect(dst, length, PAGE_EXECUTE_READWRITE, &oldProtect);

	memcpy(dst, newBytes, length);
	VirtualProtect(dst, length, oldProtect, &oldProtect);
}

void mem::Nop(BYTE* dst, size_t length) {
	DWORD oldProtect;
	VirtualProtect(dst, length, PAGE_EXECUTE_READWRITE, &oldProtect);

	memset(dst, 0x90, length);
	VirtualProtect(dst, length, oldProtect, &oldProtect);
}

bool mem::Hook(BYTE* src, BYTE* dst, size_t length) {
	if (length < 5) return false;

	DWORD oldProtect;
	VirtualProtect(src, length, PAGE_EXECUTE_READWRITE, &oldProtect);

	memset(src, 0x90, length);

	src[0] = 0xE9;
	*(uintptr_t*)(src + 1) = ((uintptr_t)dst - (uintptr_t)src) - 5;

	VirtualProtect(src, length, oldProtect, &oldProtect);
	return true;
}

BYTE* mem::TrampHook(BYTE* src, BYTE* dst, size_t length) {
	if (length < 5) return 0;

	BYTE* gateway = (BYTE*)VirtualAlloc(0, length + 14, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	memcpy_s(gateway, length, src, length);

	*(gateway + length) = 0xE9;
	*(uintptr_t*)((uintptr_t)gateway + length + 1) = (uintptr_t)src - (uintptr_t)gateway - 5;

	mem::Hook(src, dst, length);
	return gateway;
}