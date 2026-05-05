#include "hooks.h"
#include "../core/globals.h"
#include "../core/memory.h"
#include "../features/skinchanger/skinchanger.h"
#include "../../MinHook.h"
#include "../../resource.h"

// ==================== НОВЫЕ ФУНКЦИИ (INTERNAL HOOKS) ====================

void __fastcall hkFrameStageNotify(void* rcx, int curStage) {
	// ВОЗВРАЩАЕМ FRAME_RENDER_START (6)
	// Стадия 4 не работает на текущем патче CS2 для записи в Weapons (перезаписывается сервером).
	// Стадия 6 (5 или 6) - это момент отрисовки, тут наше изменение будет финальным.
	
	// Проверка ревизии для обновления при смене в меню
	if (g_skinUpdateCounter != g_lastSkinUpdateCounterSeen) {
		g_lastSkinUpdateCounterSeen = g_skinUpdateCounter;
		++g_skinRevision;
		if (g_skinRevision <= 0) g_skinRevision = 1;
	}
	
	if ((curStage == 6) && g_skinChangerEnabled) 
	{
		UpdateSkinChangerHooked();
	}
	oFrameStageNotify(rcx, curStage);
}

void InitHooks() {
	// --- ПРЕДЗАГРУЗКА ЗВУКА В КЭШ ДЛЯ МОМЕНТАЛЬНОГО ВОСПРОИЗВЕДЕНИЯ ---
	HRSRC hRes = FindResourceW(g_hModule, MAKEINTRESOURCEW(IDR_WAV_HITSOUND), RT_RCDATA);
	if (hRes) {
		HGLOBAL hMem = LoadResource(g_hModule, hRes);
		if (hMem) {
			g_hitSoundBuffer = LockResource(hMem);
		}
	}

	MH_Initialize(); // Инициализируем MinHook

	// Ставим VTable хук на скинченджер
	void* pSource2Client = GetInterface("client.dll", "Source2Client002");
	if (pSource2Client) {
		g_pSource2ClientVTable = *(void***)pSource2Client;
		DWORD oldProtect;
		VirtualProtect(&g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
		oFrameStageNotify = (FrameStageNotify_t)g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX];
		g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX] = (void*)hkFrameStageNotify;
		VirtualProtect(&g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX], sizeof(void*), oldProtect, &oldProtect);
	}
}

void RemoveHooks() {
	if (g_pSource2ClientVTable && oFrameStageNotify)
	{
		// ИСПРАВЛЕНО: Индекс изменен с 31 на 35
		DWORD oldProtect;
		VirtualProtect(&g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
		
		g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX] = (void*)oFrameStageNotify;
		
		VirtualProtect(&g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX], sizeof(void*), oldProtect, &oldProtect);
	}
}

// UpdateSkinChanger - УДАЛЕНА (устаревшая, заменена на UpdateSkinChangerHooked)
