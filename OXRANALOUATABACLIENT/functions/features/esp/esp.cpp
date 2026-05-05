#include "esp.h"
#include "../../core/globals.h"
#include "../../core/memory.h"
#include "../../core/math.h"
#include "../../../output/offsets.hpp"
#include "../../../output/client_dll.hpp"
#include "imgui.h"
#include "imgui_internal.h"

float GetEstimatedGameTimeSeconds()
{
	if (g_gameTimeBaseTickMs <= 0.0)
		return 0.0f;

	double nowMs = (double)GetTickCount64();
	double dt = (nowMs - g_gameTimeBaseTickMs) / 1000.0;
	return (float)(g_gameTimeBaseSeconds + dt);
}

const char* GetWeaponName(int defIndex)
{
	switch(defIndex) {
		case 1: return "DEAGLE";
		case 2: return "ELITE";
		case 3: return "FIVESEVEN";
		case 4: return "GLOCK";
		case 7: return "AK-47";
		case 8: return "AUG";
		case 9: return "AWP";
		case 10: return "FAMAS";
		case 11: return "G3SG1";
		case 13: return "GALIL";
		case 14: return "M249";
		case 16: return "M4A4";
		case 17: return "MAC10";
		case 19: return "P90";
		case 23: return "MP5";
		case 24: return "UMP45";
		case 25: return "XM1014";
		case 26: return "BIZON";
		case 27: return "MAG7";
		case 28: return "NEGEV";
		case 29: return "SAWEDOFF";
		case 30: return "TEC9";
		case 31: return "ZEUS";
		case 32: return "P2000";
		case 33: return "MP7";
		case 34: return "MP9";
		case 35: return "NOVA";
		case 36: return "P250";
		case 38: return "SCAR20";
		case 39: return "SG556";
		case 40: return "SCOUT";
		case 41: return "KNIFE";
		case 42: return "KNIFE";
		case 43: return "FLASH";
		case 44: return "HE";
		case 45: return "SMOKE";
		case 46: return "MOLOTOV";
		case 47: return "DECOY";
		case 48: return "INCENDIARY";
		case 49: return "C4";
		case 59: return "KNIFE";
		case 60: return "M4A1-S";
		case 61: return "USP-S";
		case 63: return "CZ75";
		case 64: return "REVOLVER";
		case 500: return "BAYONET";
		case 503: return "CLASSIC";
		case 505: return "FLIP";
		case 506: return "GUT";
		case 507: return "KARAMBIT";
		case 508: return "M9";
		case 509: return "HUNTSMAN";
		case 512: return "FALCHION";
		case 514: return "BOWIE";
		case 515: return "BUTTERFLY";
		case 516: return "DAGGERS";
		default: return "WEAPON";
	}
}

// ИСПРАВЛЕНИЕ C2712: Структура для временного хранения зрителей
struct SpectatorTemp {
	char name[64];
	bool valid;
};


void UpdateESPInternal_TryBlock(
	uintptr_t client,
	EspTargetWorld* tempTargets,
	int* targetCount,
	SpectatorTemp* tempSpectators,
	int* spectatorCount
)
{
	__try
	{
		using namespace cs2_dumper;
		using namespace cs2_dumper::schemas::client_dll;

		// ИСПРАВЛЕНИЕ: Строгая проверка валидности локального игрока
		uintptr_t localPawn = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn))
			return;
		
		// 1. Проверка на nullptr (мы в меню)
		if (!localPawn) {
			return;
		}
		
		// 2. ПРОВЕРКА ОТ КРАША: Проверяем GameSceneNode у себя. Если его нет - мир не загружен.
		uintptr_t localScene = 0;
		if (!TryRead<uintptr_t>(localPawn + C_BaseEntity::m_pGameSceneNode, localScene))
			return;
		if (!localScene) {
			return;
		}
		
		// 3. ИСПРАВЛЕНИЕ: НЕ проверяем HP локального игрока!
		// При слежке за другим игроком (spectator mode) наш HP = 0, но ESP должен работать.
		// Проверка здоровья нужна только для защиты от мусорных значений при смене карты.
		int localHp = 0;
		if (!TryRead<int>(localPawn + C_BaseEntity::m_iHealth, localHp))
			return;
		// Разрешаем HP от -100 до 10000 (включая 0 для режима наблюдателя)
		if (localHp < -100 || localHp > 10000) {
			return;
		}
		
		// ОПТИМИЗАЦИЯ: Проверяем снайперку здесь, а не в рендере
		g_isLocalSniperScoped = false;
		uintptr_t weaponServices = 0;
		if (TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pWeaponServices, weaponServices) && weaponServices)
		{
			uint32_t hActive = 0;
			if (TryRead<uint32_t>(weaponServices + CPlayer_WeaponServices::m_hActiveWeapon, hActive) && hActive)
			{
				uintptr_t entityList = 0;
				if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList) && entityList)
				{
					int weIndex = hActive & 0x1FF;
					int weEntryIndex = (hActive & 0x7FFF) >> 9;
					uintptr_t weList = 0;
					if (TryRead<uintptr_t>(entityList + 0x8 * weEntryIndex + 0x10, weList) && weList)
					{
						uintptr_t weaponEnt = 0;
						if (TryRead<uintptr_t>(weList + 0x70 * weIndex, weaponEnt) && weaponEnt)
						{
							uintptr_t itemView = weaponEnt + C_EconEntity::m_AttributeManager + C_AttributeContainer::m_Item;
							uint16_t defIndex = 0;
							if (TryRead<uint16_t>(itemView + C_EconItemView::m_iItemDefinitionIndex, defIndex))
							{
								// 9 = AWP, 40 = SSG08 (Scout)
								g_isLocalSniperScoped = (defIndex == 9 || defIndex == 40);
							}
						}
					}
				}
			}
		}

		uintptr_t entityList = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList))
			return;
		if (!entityList) return;
		
		// ИСПРАВЛЕНИЕ: При слежке (HP <= 0) не проверяем команду, показываем всех
		bool isSpectating = (localHp <= 0);
		
		int localTeam = 0;
		bool hasLocalTeam = false;
		if (localPawn && !isSpectating)
		{
			if (!TryRead<int>(localPawn + C_BaseEntity::m_iTeamNum, localTeam))
				localTeam = 0;
			hasLocalTeam = (localTeam >= 2 && localTeam <= 3);
		}

		// Импульс отключения радара (одиночный сброс флага при выключении)
		static bool s_wasRadarHackEnabled = false;
		bool radarDisablePulse = false;
		if (s_wasRadarHackEnabled && !g_radarHackEnabled) {
			radarDisablePulse = true;
		}
		s_wasRadarHackEnabled = g_radarHackEnabled;

		// Находим наш локальный Pawn Handle для Spectator List
		uint32_t localPawnHandle = 0;
		uintptr_t localCtrl = 0;
		if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerController, localCtrl) && localCtrl) {
			TryRead<uint32_t>(localCtrl + g_offsetsRuntime.m_hPlayerPawn, localPawnHandle);
		}

		for (int id = 1; id <= 64; ++id)
		{
			uintptr_t listEntry = 0;
			if (!TryRead<uintptr_t>(entityList + 0x8 * ((id & 0x7FFF) >> 9) + 0x10, listEntry) || !listEntry) continue;

			uintptr_t controller = 0;
			if (!TryRead<uintptr_t>(listEntry + 0x70 * (id & 0x1FF), controller) || !controller) continue;

			// --- ИДЕАЛЬНЫЙ SPECTATOR CHECK (работает даже когда мертвы) ---
			uint32_t pawnHandle = 0, obsPawnHandle = 0;
			TryRead<uint32_t>(controller + g_offsetsRuntime.m_hPlayerPawn, pawnHandle);
			TryRead<uint32_t>(controller + g_offsetsRuntime.m_hObserverPawn, obsPawnHandle);

			bool isAlive = false;
			uintptr_t activePawnToCheck = 0;

			// 1. Проверяем основную пешку игрока (жив ли он?)
			if (pawnHandle) {
				int pIdx = pawnHandle & 0x1FF;
				int pEntry = (pawnHandle & 0x7FFF) >> 9;
				uintptr_t pList = 0;
				if (TryRead<uintptr_t>(entityList + 0x8 * pEntry + 0x10, pList) && pList) {
					uintptr_t actualPawn = 0;
					if (TryRead<uintptr_t>(pList + 0x70 * pIdx, actualPawn) && actualPawn) {
						int actualHp = 0;
						TryRead<int>(actualPawn + g_offsetsRuntime.m_iHealth, actualHp);
						if (actualHp > 0) {
							isAlive = true; // Игрок возродился, пропускаем проверку зрителя
						} else {
							activePawnToCheck = actualPawn; // Игрок мертв, запоминаем его пешку
						}
					}
				}
			}

			// 2. Если игрок мертв, но у него есть активная пешка обсервера, отдаем ей приоритет
			if (!isAlive && obsPawnHandle) {
				int pIdx = obsPawnHandle & 0x1FF;
				int pEntry = (obsPawnHandle & 0x7FFF) >> 9;
				uintptr_t pList = 0;
				if (TryRead<uintptr_t>(entityList + 0x8 * pEntry + 0x10, pList) && pList) {
					uintptr_t obsPawn = 0;
					if (TryRead<uintptr_t>(pList + 0x70 * pIdx, obsPawn) && obsPawn) {
						activePawnToCheck = obsPawn;
					}
				}
			}

			// 3. Только если игрок мертв (isAlive == false), проверяем, за кем он следит
			if (!isAlive && activePawnToCheck && localPawnHandle && *spectatorCount < 64 && controller != localCtrl) {
				uintptr_t obsServices = 0;
				if (TryRead<uintptr_t>(activePawnToCheck + g_offsetsRuntime.m_pObserverServices, obsServices) && obsServices) {
					uint32_t obsTarget = 0;
					if (TryRead<uint32_t>(obsServices + g_offsetsRuntime.m_hObserverTarget, obsTarget) && obsTarget) {
						// Если цель обсервера - МЫ, добавляем в список
						if (obsTarget == localPawnHandle && localHp > 0) {
							StringBuf64 specName = {0};
							if (TryRead<StringBuf64>(controller + g_offsetsRuntime.m_iszPlayerName, specName)) {
								specName.data[63] = '\0';
								// Исключаем пустые имена
								if (specName.data[0] != '\0') {
									memcpy(tempSpectators[*spectatorCount].name, specName.data, 64);
									tempSpectators[*spectatorCount].valid = true;
									(*spectatorCount)++;
								}
							}
						}
					}
				}
			}
			// --- КОНЕЦ SPECTATOR CHECK ---

			// --- PAWN LOGIC ДЛЯ ESP И HITSOUND ---
			if (!pawnHandle) continue;
			int pawnIndex = pawnHandle & 0x1FF;
			int pawnEntryIndex = (pawnHandle & 0x7FFF) >> 9;
			uintptr_t pawnListEntry = 0;
			if (!TryRead<uintptr_t>(entityList + 0x8 * pawnEntryIndex + 0x10, pawnListEntry) || !pawnListEntry) continue;
			uintptr_t pawn = 0;
			if (!TryRead<uintptr_t>(pawnListEntry + 0x70 * pawnIndex, pawn) || !pawn) continue;
			if (localPawn && pawn == localPawn) continue;

			// СНАЧАЛА проверяем команду (чтобы не реагировать на урон по своим)
			int team = 0;
			if (!TryRead<int>(pawn + g_offsetsRuntime.m_iTeamNum, team)) continue;
			if (team < 2 || team > 3) continue;
			if (!isSpectating && hasLocalTeam && team == localTeam) continue;

			int hp = 0;
			if (!TryRead<int>(pawn + g_offsetsRuntime.m_iHealth, hp)) continue;
			if (hp <= 0 || hp > 200) continue; // Мертвецов не рисуем в ESP

			// --- GLOW (свечение сквозь стены) ---
				if (g_chamsEnabled)
				{
					ImVec4 c = (team == 2) ? g_chamsColorT : g_chamsColorCT;
					uintptr_t pGlow = pawn + C_BaseModelEntity::m_Glow;

					(void)TryWrite<bool>(pGlow + 0x51, true);   // m_bGlowing
					(void)TryWrite<int> (pGlow + 0x30, 3);      // Type 3 = только контур (outline)
					
					struct ColorRGBA { uint8_t r, g, b, a; };
					ColorRGBA glowColor = {
						(uint8_t)(c.x * 255.0f), (uint8_t)(c.y * 255.0f), (uint8_t)(c.z * 255.0f), (uint8_t)(c.w * 255.0f)
					};
					(void)TryWrite<ColorRGBA>(pGlow + 0x40, glowColor);

					// ИСПРАВЛЕНИЕ: true = рендерить сквозь стены!
					(void)TryWrite<bool>(pGlow + 0x52, true);   // m_bRenderWhenOccluded
					(void)TryWrite<bool>(pGlow + 0x53, true);   // m_bRenderWhenUnoccluded
				}
				else
				{
					// Сброс при выключении
					uintptr_t pGlow = pawn + C_BaseModelEntity::m_Glow;
					(void)TryWrite<bool>(pGlow + 0x51, false);
				}

				// Читаем origin, mins, maxs для ESP бокса
				Vec3 origin{}, mins{}, maxs{};
				uintptr_t gameScene = 0;
				if (TryRead<uintptr_t>(pawn + C_BaseEntity::m_pGameSceneNode, gameScene) && gameScene) {
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x0, origin.x);
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x4, origin.y);
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x8, origin.z);
					
					// Collision bounds для 2D бокса
					uintptr_t collision = 0;
					if (TryRead<uintptr_t>(pawn + C_BaseEntity::m_pCollision, collision) && collision) {
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMins + 0x0, mins.x);
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMins + 0x4, mins.y);
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMins + 0x8, mins.z);
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMaxs + 0x0, maxs.x);
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMaxs + 0x4, maxs.y);
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMaxs + 0x8, maxs.z);
					}
				}

				EspTargetWorld tgt{};
				tgt.pawn = pawn;
				tgt.hp = hp;
				tgt.origin = origin;
				tgt.mins = mins;
				tgt.maxs = maxs;
				tgt.weaponIconIndex = -1;
				tgt.flashed = false;
				{
					// ОПТИМИЗАЦИЯ: Читаем имя блоком за 1 вызов TryRead
					StringBuf64 nameRaw = {0};
					if (TryRead<StringBuf64>(controller + CBasePlayerController::m_iszPlayerName, nameRaw)) {
						memcpy(tgt.name, nameRaw.data, 64);
						tgt.name[63] = '\0'; // Гарантируем закрытие строки
					} else {
						tgt.name[0] = '\0';
					}
				}
				{
					uintptr_t weaponServices = 0;
					if (!TryRead<uintptr_t>(pawn + C_BasePlayerPawn::m_pWeaponServices, weaponServices))
						weaponServices = 0;
					if (weaponServices)
					{
						uint32_t hActive = 0;
						if (!TryRead<uint32_t>(weaponServices + CPlayer_WeaponServices::m_hActiveWeapon, hActive))
							hActive = 0;
						if (hActive)
						{
							int weIndex = hActive & 0x1FF;
							int weEntryIndex = (hActive & 0x7FFF) >> 9;
							uintptr_t weListEntry = 0;
							if (!TryRead<uintptr_t>(entityList + 0x8 * weEntryIndex + 0x10, weListEntry))
								weListEntry = 0;
							if (weListEntry)
							{
								uintptr_t weaponEnt = 0;
								if (!TryRead<uintptr_t>(weListEntry + 0x70 * weIndex, weaponEnt))
									weaponEnt = 0;
								if (weaponEnt)
								{
									uintptr_t econEntity = weaponEnt;
									uintptr_t itemView = econEntity + C_EconEntity::m_AttributeManager + C_AttributeContainer::m_Item;
									uint16_t defIndex = 0;
									if (!TryRead<uint16_t>(itemView + C_EconItemView::m_iItemDefinitionIndex, defIndex))
										defIndex = 0;
									if (defIndex != 0)
									{
										// ИСПРАВЛЕНИЕ: Используем текстовое имя вместо иконок
										const char* weaponName = GetWeaponName((int)defIndex);
										strncpy_s(tgt.weapon, sizeof(tgt.weapon), weaponName, _TRUNCATE);
									}
								}
							}
						}
					}
				}

				float flashDur = 0.0f;
				if (!TryRead<float>(pawn + C_CSPlayerPawnBase::m_flFlashDuration, flashDur))
					flashDur = 0.0f;
				float flashEndTime = 0.0f;
				(void)TryRead<float>(pawn + C_CSPlayerPawnBase::m_flFlashBangTime, flashEndTime);

				// Синхронизация локального времени с "game time" по первому валидному значению.
				if (flashEndTime > 0.0f && g_gameTimeBaseTickMs <= 0.0) {
					g_gameTimeBaseTickMs = (double)GetTickCount64();
					// flashBangTime в игре обычно является "absolute game time" конца ослепления.
					// Поэтому базу ставим чуть раньше на flashDur.
					g_gameTimeBaseSeconds = (double)flashEndTime - (double)flashDur;
				}

				tgt.flashEndTime = flashEndTime;
				tgt.flashed = (flashDur > 0.0f);
				tgt.flashDur = flashDur; // Добавляем длительность флешки
				
				// RADAR HACK (Native) - Раздельная независимая проверка (использует 0x8)
				if (g_radarHackEnabled) {
					(void)TryWrite<bool>(pawn + C_CSPlayerPawn::m_entitySpottedState + 0x8, true);
				} else if (radarDisablePulse) {
					(void)TryWrite<bool>(pawn + C_CSPlayerPawn::m_entitySpottedState + 0x8, false);
				}
				
				// ИСПРАВЛЕНИЕ C2712: Записываем в POD-буфер вместо vector::push_back
				if (*targetCount < 64) {
					tempTargets[*targetCount] = tgt;
					(*targetCount)++;
				}
			}
		
		// --- Поиск установленной C4 (работает даже если игрок мертв) ---
		g_bombData.found = false;
		if (g_offsetsRuntime.dwPlantedC4)
		{
			uintptr_t c4Ptr = 0;
			if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwPlantedC4, c4Ptr) && c4Ptr)
			{
				uintptr_t c4Node = 0;
				if (TryRead<uintptr_t>(c4Ptr, c4Node) && c4Node)
				{
					bool isTicking = false;
					bool isDefused = false;
					(void)TryRead<bool>(c4Node + g_offsetsRuntime.m_bBombTicking, isTicking);
					(void)TryRead<bool>(c4Node + g_offsetsRuntime.m_bBombDefused, isDefused);
					
					if (isTicking && !isDefused) {
						uintptr_t gameScene = 0;
						if (TryRead<uintptr_t>(c4Node + g_offsetsRuntime.m_pGameSceneNode, gameScene) && gameScene)
						{
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x0, g_bombData.pos.x);
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x4, g_bombData.pos.y);
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x8, g_bombData.pos.z);
							
							if (g_bombData.pos.x != 0.0f || g_bombData.pos.y != 0.0f) {
								TryRead<float>(c4Node + g_offsetsRuntime.m_flC4Blow, g_bombData.blowTime);
								TryRead<bool>(c4Node + g_offsetsRuntime.m_bBeingDefused, g_bombData.isDefusing);
								g_bombData.found = true;
							}
						}
					}
				}
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		// Вектор уже очищен в начале UpdateESPInternal
	}
}

// Основная функция UpdateESPInternal
void UpdateESPInternal(int width, int height)
{
	// ИСПРАВЛЕНИЕ C2712: Операции с std::vector ПЕРЕД вызовом helper-функции
	g_espTargetsWorld.clear();
	g_espBoxes.clear();
	g_spectators.clear(); // Принудительно очищаем в начале каждого тика
	
	// Сброс данных бомбы каждый кадр
	g_bombData.found = false;

	uintptr_t client = GetClientBase();
	if (!client) return;

	// ИСПРАВЛЕНИЕ C2712: Временные POD-буферы
	SpectatorTemp tempSpectators[64] = {};
	int spectatorCount = 0;
	
	EspTargetWorld tempTargets[64] = {};
	int targetCount = 0;

	// Вызываем helper-функцию с __try/__except
	UpdateESPInternal_TryBlock(client, tempTargets, &targetCount, tempSpectators, &spectatorCount);

	// ИСПРАВЛЕНИЕ C2712: Переносим ESP цели из POD-буфера в вектор (вне __try блока)
	g_espTargetsWorld.reserve(targetCount);
	for (int i = 0; i < targetCount; ++i) {
		g_espTargetsWorld.push_back(tempTargets[i]);
	}

	// Переносим зрителей из временного буфера в вектор (вне __try блока)
	g_spectators.clear();
	for (int i = 0; i < spectatorCount; ++i) {
		if (tempSpectators[i].valid) {
			g_spectators.push_back(tempSpectators[i].name);
		}
	}
}

void ProjectESPBoxes(const float view[16], int width, int height)
{
	g_espBoxes.clear();
	g_espBoxes.reserve(g_espTargetsWorld.size());

	for (const EspTargetWorld& t : g_espTargetsWorld)
	{
		Box2D box{};
		if (!GetEntityBox2DFromWorldAABB(t.origin, t.mins, t.maxs, view, width, height, box))
			continue;
		EspBox e{};
		e.box = box;
		e.hp = t.hp;
		e.pawn = t.pawn;
		// copy cached name/weapon for draw phase
		for (int i = 0; i < 64; ++i)
		{
			e.name[i] = t.name[i];
			if (!t.name[i]) break;
		}
		for (int i = 0; i < 64; ++i)
		{
			e.weapon[i] = t.weapon[i];
			if (!t.weapon[i]) break;
		}
		e.flashed = t.flashed;
		e.flashDur = t.flashDur; // Добавляем длительность флешки
		e.flashEndTime = t.flashEndTime;
		e.weaponIconIndex = t.weaponIconIndex;
		g_espBoxes.push_back(e);
	}
}

void UpdateHitInfo(uintptr_t client)
{
	if (!g_hitSoundEnabled && !g_damageIndicatorsEnabled) return;
	if (!client) return;

	uintptr_t entityList = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList) || !entityList) return;

	uintptr_t localPawn = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn) || !localPawn) return;
	
	int localTeam = 0;
	if (!TryRead<int>(localPawn + g_offsetsRuntime.m_iTeamNum, localTeam)) return;

	// Iterate all players (1-64)
	for (int id = 1; id <= 64; ++id)
	{
		uintptr_t listEntry = 0;
		if (!TryRead<uintptr_t>(entityList + 0x8 * ((id & 0x7FFF) >> 9) + 0x10, listEntry) || !listEntry) continue;

		uintptr_t controller = 0;
		if (!TryRead<uintptr_t>(listEntry + 0x70 * (id & 0x1FF), controller) || !controller) continue;

		uint32_t pawnHandle = 0;
		if (!TryRead<uint32_t>(controller + g_offsetsRuntime.m_hPlayerPawn, pawnHandle) || !pawnHandle) continue;

		int pawnIndex = pawnHandle & 0x1FF;
		int pawnEntryIndex = (pawnHandle & 0x7FFF) >> 9;
		uintptr_t pawnListEntry = 0;
		if (!TryRead<uintptr_t>(entityList + 0x8 * pawnEntryIndex + 0x10, pawnListEntry) || !pawnListEntry) continue;

		uintptr_t pawn = 0;
		if (!TryRead<uintptr_t>(pawnListEntry + 0x70 * pawnIndex, pawn) || !pawn) continue;
		if (pawn == localPawn) continue;

		int team = 0;
		if (!TryRead<int>(pawn + g_offsetsRuntime.m_iTeamNum, team)) continue;
		if (team == localTeam) continue; // Skip teammates

		int hp = 0;
		if (!TryRead<int>(pawn + g_offsetsRuntime.m_iHealth, hp)) continue;

		auto it = g_lastEnemyHp.find(pawn);
		if (it != g_lastEnemyHp.end())
		{
			int prevHp = it->second;
			if (hp < prevHp && prevHp > 0 && hp >= 0)
			{
				// Check if we shot recently
				bool isShooting = (GetTickCount64() - g_lastLocalShotTime < 600) || (GetAsyncKeyState(VK_LBUTTON) & 0x8000) || g_autoFireActive;
				
				if (isShooting)
				{
					int damage = prevHp - hp;
					g_hitmarkerAlpha = 1.0f;
					g_hitmarkerTime = GetTickCount64();

					if (g_hitSoundEnabled && g_hitSoundBuffer)
					{
						PlaySoundA((LPCSTR)g_hitSoundBuffer, NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
					}

					if (g_damageIndicatorsEnabled && damage > 0)
					{
						DamageText dt;
						dt.targetPawn = pawn;
						dt.damage = damage;
						dt.spawnTime = GetTickCount64();
						dt.alpha = 1.0f;
						dt.currentYOffset = 0.0f;
						dt.targetYOffset = 0.0f;
						
						uintptr_t gameScene = 0;
						if (TryRead<uintptr_t>(pawn + g_offsetsRuntime.m_pGameSceneNode, gameScene) && gameScene)
						{
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x0, dt.worldPos.x);
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x4, dt.worldPos.y);
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x8, dt.worldPos.z);
							dt.worldPos.z += 40.0f;
						}
						g_damageTexts.push_back(dt);
					}
				}
			}
		}
		g_lastEnemyHp[pawn] = hp;
	}
}


void DrawBombEsp(ImDrawList* drawList)
{
	if (!g_bombEspEnabled || !g_bombData.found)
		return;

	uintptr_t client = GetClientBase();
	if (!client)
		return;

	__try
	{
		const float* viewMatrix = reinterpret_cast<const float*>(client + g_offsetsRuntime.dwViewMatrix);
		if (viewMatrix)
		{
			ImVec2 ds = ImGui::GetIO().DisplaySize;
			int width = (int)ds.x;
			int height = (int)ds.y;

			Vec2 sPos;
			if (WorldToScreen(viewMatrix, g_bombData.pos, width, height, sPos))
			{
				// Рисуем красный квадрат вокруг бомбы
				drawList->AddRect(ImVec2(sPos.x - 15, sPos.y - 15), ImVec2(sPos.x + 15, sPos.y + 15), IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);

				// Таймер с fallback
				uintptr_t globalVars = 0;
				float timeLeft = -1.0f;
				bool hasTime = false;

				if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwGlobalVars, globalVars) && globalVars)
				{
					float curTime = 0.0f;
					// ИСПРАВЛЕНИЕ: Оффсет времени изменен с 0x2C на 0x30 (на некоторых версиях игры это 0x34)
					if (TryRead<float>(globalVars + 0x30, curTime) && curTime > 0.0f) {
						timeLeft = g_bombData.blowTime - curTime;
						hasTime = true;
					}
				}

				// ИСПРАВЛЕНИЕ: Отрисовываем только если время адекватное (отсекаем старые раунды)
				char buf[64];
				if (hasTime && timeLeft > 0.0f && timeLeft <= 45.0f) {
					sprintf_s(buf, sizeof(buf), "C4: %.1f", timeLeft);
					if (g_bombData.isDefusing) lstrcatA(buf, " [DEF]");
					drawList->AddText(ImVec2(sPos.x - 20, sPos.y + 16), IM_COL32(255, 255, 0, 255), buf);
				} else if (hasTime && timeLeft <= 0.0f && timeLeft > -5.0f) {
					// Бомба только что взорвалась
					drawList->AddText(ImVec2(sPos.x - 20, sPos.y + 16), IM_COL32(255, 0, 0, 255), "BOOM");
				} else if (!hasTime) {
					// Fallback: если время не читается
					lstrcpyA(buf, "C4");
					if (g_bombData.isDefusing) lstrcatA(buf, " [DEF]");
					drawList->AddText(ImVec2(sPos.x - 20, sPos.y + 16), IM_COL32(255, 200, 0, 255), buf);
				}

				// DEBUG: Логируем данные бомбы
				#ifdef _DEBUG
				char bombDbg[256];
				sprintf_s(bombDbg, "[BOMB] found=%d pos=%.1f,%.1f,%.1f blowTime=%.2f timeLeft=%.2f\n",
					g_bombData.found, g_bombData.pos.x, g_bombData.pos.y, g_bombData.pos.z,
					g_bombData.blowTime, timeLeft);
				OutputDebugStringA(bombDbg);
				#endif
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
}


bool TryReadViewMatrix(uintptr_t client, float* viewMatrix)
{
	if (!client) return false;
	__try {
		for (int i = 0; i < 16; ++i) {
			viewMatrix[i] = *(float*)(client + g_offsetsRuntime.dwViewMatrix + i * sizeof(float));
		}
		return true;
	} __except(1) {}
	return false;
}

void DrawEspImGui()
{
	// Если ничего не включено, не рисуем (но hitmarker должен рисоваться!)
	if (!g_whEnabled && !g_bonesEnabled && !g_nameEspEnabled && !g_gunEspEnabled && !g_hpBarEnabled && !(g_aimbotEnabled && g_aimbotDrawFov) && !g_customCrosshair && g_bombEspEnabled == false && g_hitmarkerAlpha <= 0.0f) return;

	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	const ImU32 staticBoxCol = ImGui::ColorConvertFloat4ToU32(g_boxColor);

	// --- ОТРИСОВКА ESP (БОКСЫ, ИМЕНА, ОРУЖИЕ) ---
	for (const EspBox& e : g_espBoxes)
	{
		const Box2D& box = e.box;
		
		// Dynamic box color based on HP
		ImU32 boxCol = staticBoxCol;
		if (g_dynamicBoxColor) {
			float hpPct = e.hp / 100.0f;
			if (hpPct < 0.0f) hpPct = 0.0f;
			if (hpPct > 1.0f) hpPct = 1.0f;
			// Green (100%) -> Yellow (50%) -> Red (0%)
			float r = (hpPct > 0.5f) ? (1.0f - (hpPct - 0.5f) * 2.0f) : 1.0f;
			float g = (hpPct > 0.5f) ? 1.0f : (hpPct * 2.0f);
			boxCol = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, 0.1f, 1.0f));
		}
		
		// Боксы (Corner Box style с черной аутлайн-тенью)
		if (g_whEnabled)
		{
			float bW = box.right - box.left;
			float bH = box.bottom - box.top;
			float lX = bW * 0.25f; // Длина уголка (25% от ширины)
			float lY = bW * 0.25f; // Длина уголка по вертикали
			float thick = g_boxThickness;

			ImU32 outlineCol = IM_COL32(0, 0, 0, 200); // Черная обводка для читаемости

			auto drawCorner = [&](float x1, float y1, float x2, float y2, float x3, float y3) {
				// Тень (Outline)
				drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), outlineCol, thick + 2.0f);
				drawList->AddLine(ImVec2(x2, y2), ImVec2(x3, y3), outlineCol, thick + 2.0f);
				// Основная линия
				drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), boxCol, thick);
				drawList->AddLine(ImVec2(x2, y2), ImVec2(x3, y3), boxCol, thick);
			};

			// Top Left
			drawCorner(box.left, box.top + lY, box.left, box.top, box.left + lX, box.top);
			// Top Right
			drawCorner(box.right - lX, box.top, box.right, box.top, box.right, box.top + lY);
			// Bottom Left
			drawCorner(box.left, box.bottom - lY, box.left, box.bottom, box.left + lX, box.bottom);
			// Bottom Right
			drawCorner(box.right - lX, box.bottom, box.right, box.bottom, box.right, box.bottom - lY);
		}

		// HP Bar с плавной анимацией
		if (g_hpBarEnabled)
		{
			// Статическая мапа для хранения "текущего" анимированного HP каждого игрока
			static std::map<uintptr_t, float> lerpedHpMap;
			
			float targetHp = (float)e.hp;
			if (lerpedHpMap.find(e.pawn) == lerpedHpMap.end()) lerpedHpMap[e.pawn] = targetHp;
			
			// Плавное приближение (Lerp) к реальному HP, скорость зависит от FPS (DeltaTime)
			lerpedHpMap[e.pawn] += (targetHp - lerpedHpMap[e.pawn]) * 10.0f * ImGui::GetIO().DeltaTime;
			float displayHp = lerpedHpMap[e.pawn];

			const float hp01 = displayHp / 100.0f;
			float r = (hp01 > 0.5f) ? (1.0f - (hp01 - 0.5f) * 2.0f) : 1.0f;
			float g = (hp01 > 0.5f) ? 1.0f : (hp01 * 2.0f);
			
			// Добавим легкое свечение (Glow) для полоски здоровья
			ImU32 hpCol = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, 0.2f, 1.0f));
			ImU32 hpGlow = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, 0.2f, 0.4f)); 

			const float barW = g_hpBarWidth;
			const float barPad = g_hpBarOffset;
			const float barX0 = box.left - barPad - barW;
			const float barY0 = box.top;
			const float barX1 = box.left - barPad;
			const float barY1 = box.bottom;
			const float barH = (barY1 - barY0);
			const float fillH = barH * hp01;

			// Тень/Фон бара
			drawList->AddRectFilled(ImVec2(barX0, barY0), ImVec2(barX1, barY1), IM_COL32(0, 0, 0, 180), 2.0f);
			// Сама полоска с закруглениями
			drawList->AddRectFilled(ImVec2(barX0 + 1.0f, barY1 - fillH + 1.0f), ImVec2(barX1 - 1.0f, barY1 - 1.0f), hpCol, 1.5f);
			// Glow эффект вокруг полоски
			drawList->AddRect(ImVec2(barX0 + 1.0f, barY1 - fillH + 1.0f), ImVec2(barX1 - 1.0f, barY1 - 1.0f), hpGlow, 1.5f, 0, 2.0f);
			// Обводка бара
			drawList->AddRect(ImVec2(barX0, barY0), ImVec2(barX1, barY1), IM_COL32(0, 0, 0, 255), 2.0f, 0, 1.0f);
		}

		// Name ESP
		if (g_nameEspEnabled && e.pawn)
		{
			if (e.name[0] != '\0')
			{
				ImU32 nameColU32 = ImGui::ColorConvertFloat4ToU32(g_nameColor);
				ImVec2 textSize = ImGui::CalcTextSize(e.name);
				float xCenter = (box.left + box.right) * 0.5f;
				ImVec2 pos(xCenter - textSize.x * 0.5f, box.top - g_nameOffsetY - textSize.y);
				drawList->AddText(pos, nameColU32, e.name);
			}
		}
		
		// Gun ESP (ИСПРАВЛЕНО: Убраны скобки [], цвет теперь берется из правильной переменной)
		if (g_gunEspEnabled && e.pawn && e.weapon[0] != '\0')
		{
			char wepText[64];
			// Убрали квадратные скобки
			wsprintfA(wepText, "%s", e.weapon);
			
			// Используем g_weaponIconColor, который меняется в меню
			ImU32 gunColU32 = ImGui::ColorConvertFloat4ToU32(g_weaponIconColor);
			
			ImVec2 textSize = ImGui::CalcTextSize(wepText);
			float xCenter = (box.left + box.right) * 0.5f;
			ImVec2 pos(xCenter - textSize.x * 0.5f, box.bottom + g_gunOffsetY);
			drawList->AddText(pos, gunColU32, wepText);
		}

		// Flash Bar (вместо текста)
		if (e.flashed && (e.flashDur > 0.0f || e.flashEndTime > 0.0f))
		{
			// Максимальная длительность флешки около 5 секунд
			float flashMax = 5.0f;
			float flashPct = 0.0f;

			float nowGameTime = GetEstimatedGameTimeSeconds();
			if (e.flashEndTime > 0.0f && nowGameTime > 0.0f) {
				float remaining = e.flashEndTime - nowGameTime;
				if (remaining < 0.0f) remaining = 0.0f;
				flashPct = remaining / flashMax;
			} else {
				// Fallback: если не можем оценить game time, используем duration как раньше
				flashPct = e.flashDur / flashMax;
			}
			if (flashPct > 1.0f) flashPct = 1.0f;
			if (flashPct < 0.0f) flashPct = 0.0f;

			// Рисуем бар справа от бокса
			float barW = 3.0f;
			float barH = (box.bottom - box.top) * flashPct;

			float x = box.right + 4.0f;
			float y = box.bottom - barH;

			// Фон бара (темный)
			drawList->AddRectFilled(ImVec2(x, box.top), ImVec2(x + barW, box.bottom), IM_COL32(0, 0, 0, 150));
			// Белый бар (показывает длительность)
			drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + barW, box.bottom), IM_COL32(255, 255, 255, 255));
		}
	}

	// --- ОТРИСОВКА ПРИЦЕЛА СНАЙПЕРКИ (AWP/SCOUT) ---
	// ИСПРАВЛЕНИЕ: Добавлена проверка g_customCrosshair
	if (g_customCrosshair && g_isLocalSniperScoped)
	{
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		ImVec2 c(ds.x * 0.5f, ds.y * 0.5f);
		const float len = 10.0f;
		const float gap = 6.0f;
		const float thick = 1.5f;
		const ImU32 col = IM_COL32(255, 255, 255, 220);

		drawList->AddLine(ImVec2(c.x - gap - len, c.y), ImVec2(c.x - gap, c.y), col, thick);
		drawList->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + gap + len, c.y), col, thick);
		drawList->AddLine(ImVec2(c.x, c.y - gap - len), ImVec2(c.x, c.y - gap), col, thick);
		drawList->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + gap + len), col, thick);
	}

	// --- HITMARKER (Анимированный с разлетом) ---
	if (g_hitmarkerAlpha > 0.0f) {
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		ImVec2 c(ds.x * 0.5f, ds.y * 0.5f);
		
		ULONGLONG now = GetTickCount64();
		float timeSinceHit = (float)(now - g_hitmarkerTime);
		
		// Анимация разлета (от 2px до 10px) - создает чувство удара
		float hitGap = 2.0f + (timeSinceHit * 0.03f); 
		if (hitGap > 10.0f) hitGap = 10.0f;
		
		const float hitLen = 8.0f; // Чуть компактнее
		const float hitThick = 2.0f;
		
		// Плавное затухание
		float fadeTime = 400.0f; 
		if (timeSinceHit > 100.0f) { 
			g_hitmarkerAlpha = 1.0f - ((timeSinceHit - 100.0f) / fadeTime);
			if (g_hitmarkerAlpha < 0.0f) g_hitmarkerAlpha = 0.0f;
		}
		
		ImU32 hitCol = IM_COL32(255, 255, 255, (int)(g_hitmarkerAlpha * 255));
		ImU32 shadowCol = IM_COL32(0, 0, 0, (int)(g_hitmarkerAlpha * 150));
		
		// Рисуем тень для лучшей видимости на светлых текстурах
		drawList->AddLine(ImVec2(c.x - hitGap - hitLen + 1, c.y - hitGap - hitLen + 1), ImVec2(c.x - hitGap + 1, c.y - hitGap + 1), shadowCol, hitThick + 1.0f);
		drawList->AddLine(ImVec2(c.x + hitGap + 1, c.y + hitGap + 1), ImVec2(c.x + hitGap + hitLen + 1, c.y + hitGap + hitLen + 1), shadowCol, hitThick + 1.0f);
		drawList->AddLine(ImVec2(c.x + hitGap + 1, c.y - hitGap - hitLen + 1), ImVec2(c.x + hitGap + hitLen + 1, c.y - hitGap + 1), shadowCol, hitThick + 1.0f);
		drawList->AddLine(ImVec2(c.x - hitGap - hitLen + 1, c.y + hitGap + 1), ImVec2(c.x - hitGap + 1, c.y + hitGap + hitLen + 1), shadowCol, hitThick + 1.0f);
		
		// Рисуем сам маркер
		drawList->AddLine(ImVec2(c.x - hitGap - hitLen, c.y - hitGap - hitLen), ImVec2(c.x - hitGap, c.y - hitGap), hitCol, hitThick);
		drawList->AddLine(ImVec2(c.x + hitGap, c.y + hitGap), ImVec2(c.x + hitGap + hitLen, c.y + hitGap + hitLen), hitCol, hitThick);
		drawList->AddLine(ImVec2(c.x + hitGap, c.y - hitGap - hitLen), ImVec2(c.x + hitGap + hitLen, c.y - hitGap), hitCol, hitThick);
		drawList->AddLine(ImVec2(c.x - hitGap - hitLen, c.y + hitGap), ImVec2(c.x - hitGap, c.y + hitGap + hitLen), hitCol, hitThick);
	}

	// --- ОТРИСОВКА FOV (ТЕПЕРЬ ВСЕГДА В КОНЦЕ) ---
	if (g_aimbotEnabled && g_aimbotDrawFov)
	{
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		ImVec2 center(ds.x * 0.5f, ds.y * 0.5f);
		
		// Конвертируем FOV в пиксели (примерная формула для 16:9)
		// Радиус = tan(FOV/2) * (ScreenHeight/2) / tan(CameraFOV/2)
		// Упрощенно:
		float fovRadius = std::tan(g_aimbotFov * M_PI / 180.0f / 2.0f) * (ds.y * 0.5f) / std::tan(90.0f * M_PI / 180.0f / 2.0f);
		
		ImU32 fovColor = ImGui::ColorConvertFloat4ToU32(g_aimbotFovColor);
		drawList->AddCircle(center, fovRadius, fovColor, 64, 1.0f);
	}

	// --- ОТРИСОВКА БОМБЫ (C4) ---
	DrawBombEsp(drawList);

	// --- OUT OF FOV ARROWS ---
	if (g_oofArrowsEnabled && !g_espTargetsWorld.empty())
	{
		uintptr_t client = GetClientBase();
		if (client) {
			QAngle* vpPtr = reinterpret_cast<QAngle*>(client + g_offsetsRuntime.dwViewAngles);
			QAngle va{};
			if (TryRead<QAngle>((uintptr_t)vpPtr, va)) {
				
				uintptr_t localPawn = 0;
				if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn) && localPawn) {
					uintptr_t localScene = 0;
					if (TryRead<uintptr_t>(localPawn + g_offsetsRuntime.m_pGameSceneNode, localScene) && localScene) {
						Vec3 myPos{};
						(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 0x0, myPos.x);
						(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 0x4, myPos.y);

						ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
						
						// Анимация пульсации прозрачности (дыхание)
						float oofPulse = (sinf((float)ImGui::GetTime() * 6.0f) * 0.4f) + 0.6f; // От 0.6 до 1.0
						ImVec4 animColor = g_oofArrowsColor;
						animColor.w *= oofPulse; // Применяем пульсацию к альфа-каналу
						ImU32 arrowCol = ImGui::ColorConvertFloat4ToU32(animColor);
						
						// Переводим текущий угол взгляда (yaw) в радианы
						float viewYaw = va.y * (M_PI / 180.0f);

						for (const EspTargetWorld& t : g_espTargetsWorld) {
							// Проверяем, на экране ли враг
							Vec2 screenPos;
							bool onScreen = WorldToScreen(reinterpret_cast<const float*>(client + g_offsetsRuntime.dwViewMatrix), t.origin, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y), screenPos);
							
							// Если враг вне экрана
							if (!onScreen) {
								float dx = t.origin.x - myPos.x;
								float dy = t.origin.y - myPos.y;
								
								// Угол на врага в мире
								float angleToEnemy = std::atan2(dy, dx);
								
								// Разница между нашим взглядом и углом на врага
								// В CS2 X/Y оси повернуты, поэтому вычитаем
								float relativeAngle = viewYaw - angleToEnemy - (M_PI / 2.0f);
								
								// Позиция стрелки на экране
								float arrowX = center.x + std::cos(relativeAngle) * g_oofArrowsRadius;
								float arrowY = center.y + std::sin(relativeAngle) * g_oofArrowsRadius;
								
								// Рисуем треугольник (стрелку), указывающую в эту сторону
								float size = g_oofArrowsSize;
								ImVec2 p1(arrowX + std::cos(relativeAngle) * size, arrowY + std::sin(relativeAngle) * size);
								ImVec2 p2(arrowX + std::cos(relativeAngle + 2.5f) * size * 0.8f, arrowY + std::sin(relativeAngle + 2.5f) * size * 0.8f);
								ImVec2 p3(arrowX + std::cos(relativeAngle - 2.5f) * size * 0.8f, arrowY + std::sin(relativeAngle - 2.5f) * size * 0.8f);
								
								drawList->AddTriangleFilled(p1, p2, p3, arrowCol);
								drawList->AddTriangle(p1, p2, p3, IM_COL32(0,0,0,200), 1.5f); // Обводка
							}
						}
					}
				}
			}
		}
	}

	// --- FLOATING DAMAGE INDICATORS (STACKED WITH ANIMATION) ---
	if (g_damageIndicatorsEnabled && !g_damageTexts.empty())
	{
		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		ULONGLONG now = GetTickCount64();
		
		uintptr_t client = GetClientBase();
		
		// Безопасное чтение матрицы
		float viewMatrix[16] = {0};
		bool hasViewMatrix = TryReadViewMatrix(client, viewMatrix);
		
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		float lifetimeMs = g_damageTextLifetime * 1000.0f;
		
		// Count stack per target for vertical offset
		std::map<uintptr_t, int> stackCount;
		
		// Iterate from end (newest first, at bottom)
		for (int i = (int)g_damageTexts.size() - 1; i >= 0; --i)
		{
			DamageText& dt = g_damageTexts[i];
			float ageMs = (float)(now - dt.spawnTime);
			
			// Remove old entries
			if (ageMs > lifetimeMs) {
				g_damageTexts.erase(g_damageTexts.begin() + i);
				continue;
			}
			
			// Stack logic: each new hit on same target goes 25px higher
			int index = stackCount[dt.targetPawn]++;
			dt.targetYOffset = index * 25.0f;
			
			// Smooth animation interpolation
			dt.currentYOffset += (dt.targetYOffset - dt.currentYOffset) * 0.15f;
			
			Vec2 screenPos;
			if (hasViewMatrix && WorldToScreen(viewMatrix, dt.worldPos, (int)ds.x, (int)ds.y, screenPos))
			{
				// Fade in last second of life
				if (ageMs > lifetimeMs - 1000.0f) {
					dt.alpha = (lifetimeMs - ageMs) / 1000.0f;
				} else {
					dt.alpha = 1.0f;
				}
				
				char buf[16];
				sprintf_s(buf, sizeof(buf), "-%d", dt.damage);
				
				ImU32 col = IM_COL32((int)(g_damageColor.x * 255), (int)(g_damageColor.y * 255), (int)(g_damageColor.z * 255), (int)(dt.alpha * 255));
				ImU32 outlineCol = IM_COL32(0, 0, 0, (int)(dt.alpha * 200));
				
				// Draw to the right of model, rising up
				ImVec2 textPos(screenPos.x + 30.0f, screenPos.y - 20.0f - dt.currentYOffset);
				
				// Premium outline (4 corners)
				ImFont* font = ImGui::GetFont();
				dl->AddText(font, g_damageTextSize, ImVec2(textPos.x + 1, textPos.y + 1), outlineCol, buf);
				dl->AddText(font, g_damageTextSize, ImVec2(textPos.x - 1, textPos.y - 1), outlineCol, buf);
				dl->AddText(font, g_damageTextSize, ImVec2(textPos.x + 1, textPos.y - 1), outlineCol, buf);
				dl->AddText(font, g_damageTextSize, ImVec2(textPos.x - 1, textPos.y + 1), outlineCol, buf);
				dl->AddText(font, g_damageTextSize, textPos, col, buf);
			}
		}
	}
}
