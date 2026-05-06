#include "mem_alloc.h"
#include <Windows.h>

namespace
{
	using AllocFn = void* (__fastcall*)(std::size_t);
	using FreeFn  = void  (__fastcall*)(void*);

	AllocFn g_alloc = nullptr;
	FreeFn  g_free  = nullptr;
	bool    g_inited = false;
}

bool InitGameAlloc()
{
	if (g_inited && g_alloc && g_free) return true;

	HMODULE tier0 = GetModuleHandleA("tier0.dll");
	if (!tier0) {
		// tier0.dll должен быть подгружен к моменту инжекта в client.dll.
		return false;
	}

	g_alloc = reinterpret_cast<AllocFn>(GetProcAddress(tier0, "MemAlloc_AllocFunc"));
	g_free  = reinterpret_cast<FreeFn>(GetProcAddress(tier0, "MemAlloc_FreeFunc"));

	g_inited = (g_alloc != nullptr && g_free != nullptr);
	return g_inited;
}

void* GameAlloc(std::size_t size)
{
	if (!g_alloc) (void)InitGameAlloc();
	return g_alloc ? g_alloc(size) : nullptr;
}

void GameFree(void* ptr)
{
	if (!ptr) return;
	if (!g_free) (void)InitGameAlloc();
	if (g_free) g_free(ptr);
}
