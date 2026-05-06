#include "hooks.h"
#include "../core/globals.h"
#include "../core/memory.h"
#include "../features/skinchanger/skin_changer.hpp"
#include "../features/skinchanger/glove_changer.hpp"
#include "../../MinHook.h"
#include "../../resource.h"
#include "../../main.hpp"

// ==================== НОВЫЕ ФУНКЦИИ (INTERNAL HOOKS) ====================

void __fastcall hkFrameStageNotify(void* rcx, int curStage) {
	// Обновляем указатель на локального игрока для нового скинчейнджера
	if (g_interfaces && g_interfaces->m_entity_system) {
		g_ctx->m_local_pawn = g_interfaces->m_entity_system->get_local_pawn();
		g_ctx->m_local_controller = g_interfaces->m_entity_system->get_local_controller();
	}

	// Стадия 7 (FRAME_RENDER_END) — запись финальная, не перезаписывается сервером
	if (curStage == 7) {
		g_skin_changer->run(curStage);
		g_glove_changer->run(curStage);
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

// UpdateSkinChanger - УДАЛЕНА (заменена на g_skin_changer->run() / g_glove_changer->run())
