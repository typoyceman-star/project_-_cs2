#include "memory.h"
#include "globals.h"
#include "../../output/offsets.hpp"
#include "../../output/client_dll.hpp"

typedef void* (*tCreateInterface)(const char* name, int* returnCode);

void* GetInterface(const char* dllName, const char* interfaceName) {
	HMODULE hMod = GetModuleHandleA(dllName);
	if (!hMod) return nullptr;
	tCreateInterface createInterface = (tCreateInterface)GetProcAddress(hMod, "CreateInterface");
	if (!createInterface) return nullptr;
	return createInterface(interfaceName, nullptr);
}

bool IsCs2Active()
{
	HWND hwnd = GetForegroundWindow();
	if (!hwnd) return false;

	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	return pid == GetCurrentProcessId();
}

uintptr_t GetClientBase()
{
	HMODULE hClient = GetModuleHandleW(L"client.dll");
	if (!hClient) return 0;
	return reinterpret_cast<uintptr_t>(hClient);
}
