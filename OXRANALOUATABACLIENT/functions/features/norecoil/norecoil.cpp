#include "norecoil.h"
#include "../../core/globals.h"
#include "../../core/memory.h"
#include "../../../output/offsets.hpp"
#include "../../../output/client_dll.hpp"

void UpdateNoRecoilNoSpread(bool cs2Active)
{
	if (!cs2Active)
		return;
	if (!g_noRecoilEnabled && !g_noSpreadEnabled)
		return;

	using namespace cs2_dumper;
	using namespace cs2_dumper::schemas::client_dll;

	uintptr_t client = GetClientBase();
	if (!client)
		return;

	__try
	{
		uintptr_t localPawn = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn))
			return;
		if (!localPawn)
			return;

		int hp = 0;
		if (!TryRead<int>(localPawn + C_BaseEntity::m_iHealth, hp))
			return;
		if (hp <= 0)
			return;

		if (g_noRecoilEnabled)
		{
			int shotsFired = 0;
			(void)TryRead<int>(localPawn + C_CSPlayerPawn::m_iShotsFired, shotsFired);
			
			uintptr_t aimPunchSvc = 0;
			(void)TryRead<uintptr_t>(localPawn + C_CSPlayerPawn::m_pAimPunchServices, aimPunchSvc);
			if (aimPunchSvc) {
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 0x0, 0.0f);
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 0x4, 0.0f);
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 0x8, 0.0f);
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngleVel + 0x0, 0.0f);
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngleVel + 0x4, 0.0f);
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngleVel + 0x8, 0.0f);
			}
			
			uintptr_t camServices = 0;
			(void)TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pCameraServices, camServices);
			if (camServices)
			{
				(void)TryWrite<float>(camServices + CPlayer_CameraServices::m_vecCsViewPunchAngle + 0x0, 0.0f);
				(void)TryWrite<float>(camServices + CPlayer_CameraServices::m_vecCsViewPunchAngle + 0x4, 0.0f);
				(void)TryWrite<float>(camServices + CPlayer_CameraServices::m_vecCsViewPunchAngle + 0x8, 0.0f);
			}
		}

		if (g_noSpreadEnabled)
		{
			uintptr_t weaponServices = 0;
			if (!TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pWeaponServices, weaponServices))
				return;
			if (!weaponServices)
				return;
			uint32_t hActive = 0;
			if (!TryRead<uint32_t>(weaponServices + CPlayer_WeaponServices::m_hActiveWeapon, hActive))
				return;
			if (!hActive)
				return;
			uintptr_t entityList = 0;
			if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList))
				return;
			if (!entityList)
				return;

			int weIndex = hActive & 0x1FF;
			int weEntryIndex = (hActive & 0x7FFF) >> 9;
			uintptr_t weListEntry = 0;
			if (!TryRead<uintptr_t>(entityList + 0x8 * weEntryIndex + 0x10, weListEntry))
				return;
			if (!weListEntry)
				return;
			uintptr_t weaponEnt = 0;
			if (!TryRead<uintptr_t>(weListEntry + 0x70 * weIndex, weaponEnt))
				return;
			if (!weaponEnt)
				return;

			// Сбрасываем все известные модификаторы точности
			(void)TryWrite<float>(weaponEnt + C_CSWeaponBase::m_fAccuracyPenalty, 0.0f);
			(void)TryWrite<float>(weaponEnt + C_CSWeaponBase::m_flRecoilIndex, 0.0f);
			(void)TryWrite<float>(weaponEnt + C_CSWeaponBase::m_flTurningInaccuracy, 0.0f);
			(void)TryWrite<float>(weaponEnt + C_CSWeaponBase::m_flTurningInaccuracyDelta, 0.0f);
			(void)TryWrite<float>(weaponEnt + C_CSWeaponBase::m_fAccuracySmoothedForZoom, 0.0f);

			// ИСПРАВЛЕНИЕ БАГА: Блок VData ЗАКОММЕНТИРОВАН!
			// Запись в VData ломает кэш движка и вызывает пропадание рук/оружия/звуков при смене раунда.
			// Для NoSpread достаточно обнуления m_fAccuracyPenalty и m_flRecoilIndex выше.
			/*
			uintptr_t vData = 0;
			if (TryRead<uintptr_t>(weaponEnt + 0x380, vData) && vData)
			{
				(void)TryWrite<float>(vData + 0x228, 0.0f); // m_flMaxInaccuracy
				(void)TryWrite<float>(vData + 0x22C, 0.0f); // m_flInaccuracyJumpInitial
			}
			*/

			// Velocity compensation for better spread control while moving
			float* vecVel = reinterpret_cast<float*>(localPawn + C_BaseEntity::m_vecVelocity);
			if (vecVel) {
				// We don't zero velocity as that would break movement, 
				// but the game uses it to calculate spread. 
				// The offsets above should cover it, but some weapons have specific logic.
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

// Skin Changer - включен/выключен
bool g_skinChangerEnabled = true; // Включен по умолчанию

// Конфигурация скинов для оружия (defIndex -> paintKit)
std::map<int, int> g_skinConfig;

// Переменные для Skin Changer
std::map<uintptr_t, int> g_appliedSkins;           // Уже применённые скины
uint32_t g_lastActiveWeapon = 0;                   // Последнее активное оружие
ULONGLONG g_lastSkinUpdateTick = 0;                // Время последнего обновления
LONG g_skinIdCounter = 0;                          // Счётчик уникальных ID

// Триггер для реал-тайм обновления скинов
int g_skinUpdateCounter = 0;
int g_skinRevision = 1;
int g_lastSkinUpdateCounterSeen = 0;

