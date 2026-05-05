#include "triggerbot.h"
#include "../../core/globals.h"
#include "../../core/memory.h"
#include "../../core/math.h"
#include "../aimbot/aimbot.h"
#include "../../../output/offsets.hpp"
#include "../../../output/client_dll.hpp"
#include "imgui.h"

void RunCombat(bool cs2Active)
{
	// --- СОСТОЯНИЕ СТРЕЛЬБЫ ---
	static int s_triggerState = 0; // 0=Idle, 1=Delay, 2=Shooting, 3=Cooldown, 4=WaitingForAccuracy
	static ULONGLONG s_stateTime = 0;
	static int s_currentWeaponDefIndex = 0;
	static int s_shotsFired = 0;

	if (!cs2Active) {
		s_triggerState = 0;
		s_shotsFired = 0;
		return;
	}

	uintptr_t client = GetClientBase();
	if (!client) return;

	using namespace cs2_dumper::schemas::client_dll;
	using namespace cs2_dumper;

	uintptr_t attackPtr = client + g_offsetsRuntime.dwForceAttack;
	if (!attackPtr) return;

	__try
	{

	// ИСПРАВЛЕНИЕ: Строгая проверка валидности локального игрока
	uintptr_t localPawn = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn))
		return;
	
	// 1. Проверка на nullptr (мы в меню)
	if (!localPawn) {
		g_espTargetsWorld.clear();
		g_espBoxes.clear();
		return;
	}
	
	// 2. Проверка здоровья (при смене карты часто бывает мусорное значение)
	int localHp = 0;
	if (!TryRead<int>(localPawn + C_BaseEntity::m_iHealth, localHp))
		return;
	if (localHp <= 0 || localHp > 10000) {
		return;
	}
	
	uintptr_t entityList = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList))
		return;
	if (!entityList) return; // Также проверяем EntityList
	
	int localTeam = 0;
	if (!TryRead<int>(localPawn + C_BaseEntity::m_iTeamNum, localTeam))
		return;

	// Читаем Punch (отдачу) для коррекции триггера через m_pAimPunchServices
	Vec3 aimPunch = {0,0,0};
	if (localPawn) {
		uintptr_t aimPunchSvc = 0;
		(void)TryRead<uintptr_t>(localPawn + C_CSPlayerPawn::m_pAimPunchServices, aimPunchSvc);
		if (aimPunchSvc) {
			(void)TryRead<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 0, aimPunch.x);
			(void)TryRead<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 4, aimPunch.y);
			(void)TryRead<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 8, aimPunch.z);
		}
	}

	// 2. Углы и позиция
	QAngle* viewAnglesPtr = reinterpret_cast<QAngle*>(client + g_offsetsRuntime.dwViewAngles);
	if (!viewAnglesPtr) return;
	QAngle currentAngles{};
	if (!TryRead<QAngle>((uintptr_t)viewAnglesPtr, currentAngles))
		return;
	
	// Эффективный угол пули = Куда смотрим + (Отдача * 2)
	QAngle bulletAngles = currentAngles;
	if (g_noRecoilEnabled) {
		// Если RCS включен, пули летят в центр
	} else {
		bulletAngles.x += aimPunch.x * 2.0f;
		bulletAngles.y += aimPunch.y * 2.0f;
	}

	Vec3 localOrigin{};
	if (!TryRead<float>(localPawn + C_BasePlayerPawn::m_vOldOrigin + 0x0, localOrigin.x)) return;
	if (!TryRead<float>(localPawn + C_BasePlayerPawn::m_vOldOrigin + 0x4, localOrigin.y)) return;
	if (!TryRead<float>(localPawn + C_BasePlayerPawn::m_vOldOrigin + 0x8, localOrigin.z)) return;
	Vec3 viewOffset{};
	if (!TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x0, viewOffset.x)) return;
	if (!TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x4, viewOffset.y)) return;
	if (!TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x8, viewOffset.z)) return;
	Vec3 localEyePos = { localOrigin.x + viewOffset.x, localOrigin.y + viewOffset.y, localOrigin.z + viewOffset.z };

	// Проверка скорости (для точности)
	Vec3 velocity{};
	if (!TryRead<float>(localPawn + C_BaseEntity::m_vecVelocity + 0x0, velocity.x)) return;
	if (!TryRead<float>(localPawn + C_BaseEntity::m_vecVelocity + 0x4, velocity.y)) return;
	if (!TryRead<float>(localPawn + C_BaseEntity::m_vecVelocity + 0x8, velocity.z)) return;
	float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

	// 3. Оружие
	bool isSniper = false;
	bool isPistol = false;
	uintptr_t weaponServices = 0;
	(void)TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pWeaponServices, weaponServices);
	if (weaponServices) {
		uint32_t hActive = 0;
		(void)TryRead<uint32_t>(weaponServices + CPlayer_WeaponServices::m_hActiveWeapon, hActive);
		if (hActive) {
			uintptr_t entityList = 0;
			(void)TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList);
			int weIdx = hActive & 0x1FF;
			int weEntryIdx = (hActive & 0x7FFF) >> 9;
			uintptr_t weList = 0;
			if (entityList)
				(void)TryRead<uintptr_t>(entityList + 0x8 * weEntryIdx + 0x10, weList);
			if (weList) {
				uintptr_t weaponEnt = 0;
				(void)TryRead<uintptr_t>(weList + 0x70 * weIdx, weaponEnt);
				if (weaponEnt) {
					uintptr_t itemView = weaponEnt + C_EconEntity::m_AttributeManager + C_AttributeContainer::m_Item;
					uint16_t defIndex = 0;
					if (TryRead<uint16_t>(itemView + C_EconItemView::m_iItemDefinitionIndex, defIndex))
						s_currentWeaponDefIndex = defIndex;
					
					if (s_currentWeaponDefIndex == 9 || s_currentWeaponDefIndex == 40) isSniper = true;
					if (s_currentWeaponDefIndex == 1 || s_currentWeaponDefIndex == 4 || s_currentWeaponDefIndex == 61 || s_currentWeaponDefIndex == 32 || s_currentWeaponDefIndex == 36) isPistol = true;
				}
			}
		}
	}

	ULONGLONG now = GetTickCount64();
	bool targetInSight = false;
	float distance = 0.0f;

	// Проверка нажатия кнопки
	bool triggerKeyHeld = (g_triggerKey != 0) && ((GetAsyncKeyState(g_triggerKey) & 0x8000) != 0);
	if (g_triggerEnabled && g_triggerKey == 0) triggerKeyHeld = true;
	
	if (g_triggerEnabled && triggerKeyHeld)
	{
		// Если мы бежим с винтовкой - не стрелять (разброс дикий)
		float maxSpeed = (isPistol || s_currentWeaponDefIndex == 17) ? 80.0f : 5.0f;
		if (speed <= maxSpeed || g_noSpreadEnabled)
		{
			uintptr_t entityList = 0;
			if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList) || !entityList)
				return;
			
			// Находим ID (индекс) локального игрока для проверки видимости
			int localPlayerId = -1;
			for (int i = 1; i <= 64; ++i) { // ВАЖНО: Итерация от 1 до 64
				uintptr_t lEntry = 0; 
				if (!TryRead<uintptr_t>(entityList + 0x8 * ((i & 0x7FFF) >> 9) + 0x10, lEntry)) continue;
				if (!lEntry) continue;
				uintptr_t ctrl = 0; 
				if (!TryRead<uintptr_t>(lEntry + 0x70 * (i & 0x1FF), ctrl)) continue;
				if (!ctrl) continue;
				uint32_t pHandle = 0; 
				if (!TryRead<uint32_t>(ctrl + CCSPlayerController::m_hPlayerPawn, pHandle)) continue;
				if (!pHandle) continue;
				uintptr_t pList = 0; 
				if (!TryRead<uintptr_t>(entityList + 0x8 * ((pHandle & 0x7FFF) >> 9) + 0x10, pList)) continue;
				if (!pList) continue;
				uintptr_t pwn = 0; 
				if (!TryRead<uintptr_t>(pList + 0x70 * (pHandle & 0x1FF), pwn)) continue;
				if (pwn == localPawn) { localPlayerId = i; break; }
			}
			
			__try
			{
				for (int id = 0; id < 64; ++id)
				{
					uintptr_t listEntry = 0;
					if (!TryRead<uintptr_t>(entityList + 0x8 * ((id & 0x7FFF) >> 9) + 0x10, listEntry) || !listEntry) continue;
					uintptr_t controller = 0;
					if (!TryRead<uintptr_t>(listEntry + 0x70 * (id & 0x1FF), controller) || !controller) continue;

					uint32_t pawnHandle = 0;
					if (!TryRead<uint32_t>(controller + CCSPlayerController::m_hPlayerPawn, pawnHandle) || !pawnHandle) continue;
					int pawnIndex = pawnHandle & 0x1FF;
					int pawnEntryIndex = (pawnHandle & 0x7FFF) >> 9;
					uintptr_t pawnListEntry = 0;
					if (!TryRead<uintptr_t>(entityList + 0x8 * pawnEntryIndex + 0x10, pawnListEntry) || !pawnListEntry) continue;
					uintptr_t pawn = 0;
					if (!TryRead<uintptr_t>(pawnListEntry + 0x70 * pawnIndex, pawn) || !pawn) continue;
					if (localPawn && pawn == localPawn) continue;

					int tHp = 0;
					if (!TryRead<int>(pawn + C_BaseEntity::m_iHealth, tHp) || tHp <= 0 || tHp > 200) continue;
					int tTeam = 0;
					if (!TryRead<int>(pawn + C_BaseEntity::m_iTeamNum, tTeam) || tTeam < 2 || tTeam > 3) continue;
					if (g_triggerTeamCheck && tTeam == localTeam) continue;

					// Проверка видимости (Отдельная от радархака, использует 0xC)
					if (g_triggerVisCheck && localPlayerId != -1) {
						uint64_t spottedMask = 0;
						if (!TryRead<uint64_t>(pawn + C_CSPlayerPawn::m_entitySpottedState + 0xC, spottedMask))
							continue;
						
						// ИСПРАВЛЕНИЕ: Сдвиг бита на (ID - 1)
						if (!(spottedMask & (1ULL << (localPlayerId - 1)))) continue;
					}

					int bonesToCheck[4]; int boneCount = 0;
					if (g_triggerHeadOnly) { bonesToCheck[0] = 6; boneCount = 1; }
					else { bonesToCheck[0]=6; bonesToCheck[1]=5; bonesToCheck[2]=4; bonesToCheck[3]=2; boneCount=4; }

					bool aimOnTarget = false;
					for (int i = 0; i < boneCount && !aimOnTarget; i++) {
						Vec3 bonePos = GetBonePos(pawn, bonesToCheck[i]);
						if (bonePos.x == 0 && bonePos.y == 0 && bonePos.z == 0) {
							uintptr_t gs = 0;
							if (!TryRead<uintptr_t>(pawn + C_BaseEntity::m_pGameSceneNode, gs) || !gs) continue;
							Vec3 ao{}, tvo{};
							(void)TryRead<float>(gs + CGameSceneNode::m_vecAbsOrigin + 0x0, ao.x);
							(void)TryRead<float>(gs + CGameSceneNode::m_vecAbsOrigin + 0x4, ao.y);
							(void)TryRead<float>(gs + CGameSceneNode::m_vecAbsOrigin + 0x8, ao.z);
							(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x0, tvo.x);
							(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x4, tvo.y);
							(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x8, tvo.z);
							bonePos = { ao.x, ao.y, ao.z + (bonesToCheck[i]==6?tvo.z : bonesToCheck[i]==5?tvo.z*0.85f : bonesToCheck[i]==4?tvo.z*0.5f : tvo.z*0.3f) };
						}
						QAngle angToBone = CalcAngle(localEyePos, bonePos);
						float fov = GetFov(bulletAngles, angToBone);
						if (fov < 4.0f) aimOnTarget = true; // Увеличили с 2.5 до 4.0
					}

					if (aimOnTarget) {
						bool scopeOk = true;
						if (g_triggerScopeOnly && isSniper) {
							bool sc = false;
							TryRead<bool>(localPawn + C_CSPlayerPawn::m_bIsScoped, sc);
							scopeOk = sc;
						}
						if (scopeOk) { targetInSight = true; break; }
					}
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				targetInSight = false;
			}
		}
	}

	// Настройка задержек с контролем отдачи
	ULONGLONG delayBeforeShot = 0;
	if (isSniper) delayBeforeShot = 30;
	else if (distance > 1000.0f) delayBeforeShot = 40;
	else delayBeforeShot = 0;

	// Небольшая дополнительная задержка для не-снайперских оружий,
	// чтобы дать разбросу пуль чуть упасть перед выстрелом при зажатом триггере
	if (!isSniper)
	{
		delayBeforeShot += 5; // ~1-2 мс в игровых тиках, визуально не ощущается, но уменьшает промахи от разброса
	}

	ULONGLONG holdTime = 0;
	ULONGLONG cooldownTime = 0;
	ULONGLONG accuracyWaitTime = 0;

	// Конфиг оружия с контролем отдачи
	if (s_currentWeaponDefIndex == 7) { // AK-47
		holdTime = 90; 
		cooldownTime = 130;
		accuracyWaitTime = 150; // Ждем пока разброс уйдет
	}
	else if (isPistol) { 
		holdTime = 20; 
		cooldownTime = 40;
		accuracyWaitTime = 80; // Для пистолетов меньше
	}
	else if (isSniper) { 
		holdTime = 50; 
		cooldownTime = 1000;
		accuracyWaitTime = 0; // Снайперки не нуждаются в контроле отдачи
	}
	else { // Другие винтовки
		holdTime = 40; 
		cooldownTime = 100;
		accuracyWaitTime = 120;
	}

	// STATE MACHINE (ИСПРАВЛЕНО для One Tap)
	if (s_triggerState == 0) // Ждем цель
	{
		if (targetInSight) {
			(void)TryWrite<int>((uintptr_t)attackPtr, 65537); // Нажали (+attack)
			s_stateTime = now;
			s_triggerState = 1;
		}
	}
	else if (s_triggerState == 1) // Ждем отпускания
	{
		// Держим нажатым всего 15-30 мс (1-2 тика)
		if (now - s_stateTime >= 30) {
			(void)TryWrite<int>((uintptr_t)attackPtr, 256); // Отпустили (-attack)
			s_stateTime = now;
			s_triggerState = 2; // Переходим в кулдаун
		}
	}
	else if (s_triggerState == 2) // Пауза между выстрелами
	{
		(void)TryWrite<int>((uintptr_t)attackPtr, 256); // Гарантируем, что кнопка отпущена
		// Ждем 150-200мс перед следующим выстрелом
		if (now - s_stateTime >= 200) {
			s_triggerState = 0; // Готовы стрелять снова
		}
	}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		if (g_autoFireActive) {
			uintptr_t client2 = GetClientBase();
			if (client2) (void)TryWrite<int>(client2 + g_offsetsRuntime.dwForceAttack, 256);
			g_autoFireActive = false;
		}
	}
}

