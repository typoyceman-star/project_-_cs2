#include "aimbot.h"
#include "../../core/globals.h"
#include "../../core/memory.h"
#include "../../core/math.h"
#include "../../../output/offsets.hpp"
#include "../../../output/client_dll.hpp"

// Глобальные переменные для контроля автовыстрела
static ULONGLONG g_lastShotTime = 0;

Vec3 GetBonePos(uintptr_t pawn, int boneIndex)
{
	Vec3 pos = { 0, 0, 0 };
	if (!pawn) return pos;

	// Получаем GameSceneNode
	uintptr_t gameScene = 0;
	if (!TryRead<uintptr_t>(pawn + g_offsetsRuntime.m_pGameSceneNode, gameScene) || !gameScene)
		return pos;

	// Получаем BoneArray (ModelState + 0x80)
	uintptr_t boneMatrix = 0;
	if (!TryRead<uintptr_t>(gameScene + g_offsetsRuntime.m_modelState + 0x80, boneMatrix) || !boneMatrix)
		return pos;

	// Читаем координаты напрямую из памяти
	float x = 0.0f, y = 0.0f, z = 0.0f;
	if (TryRead<float>(boneMatrix + boneIndex * 0x20 + 0x0, x) &&
		TryRead<float>(boneMatrix + boneIndex * 0x20 + 0x4, y) &&
		TryRead<float>(boneMatrix + boneIndex * 0x20 + 0x8, z))
	{
		pos.x = x;
		pos.y = y;
		pos.z = z;
	}
	return pos;
}

void RunAimbot(bool cs2Active)
{
	if (!cs2Active || !g_aimbotEnabled) {
		// Сброс автовыстрела при отпускании кнопки
		if (g_autoFireActive) {
			uintptr_t client = GetClientBase();
			if (client) (void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
			g_autoFireActive = false;
		}
		return;
	}

	if (g_aimbotKey != 0 && !(GetAsyncKeyState(g_aimbotKey) & 0x8000)) {
		if (g_autoFireActive) {
			uintptr_t client = GetClientBase();
			if (client) (void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
			g_autoFireActive = false;
		}
		return;
	}

	uintptr_t client = GetClientBase();
	if (!client) return;

	using namespace cs2_dumper::schemas::client_dll;
	using namespace cs2_dumper;

	__try
	{
		uintptr_t localPawn = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn))
			return;
		if (!localPawn) return;

		int localTeam = 0;
		(void)TryRead<int>(localPawn + C_BaseEntity::m_iTeamNum, localTeam);

		// Получаем углы и позицию
		QAngle* viewAnglesPtr = reinterpret_cast<QAngle*>(client + g_offsetsRuntime.dwViewAngles);
		QAngle currentAngles{};
		if (!TryRead<QAngle>((uintptr_t)viewAnglesPtr, currentAngles))
			return;

		// ИСПРАВЛЕНИЕ: Используем актуальную позицию из GameSceneNode вместо m_vOldOrigin
		uintptr_t localGameScene = 0;
		if (!TryRead<uintptr_t>(localPawn + C_BaseEntity::m_pGameSceneNode, localGameScene))
			return;
		if (!localGameScene) return;

		Vec3 localOrigin{};
		(void)TryRead<float>(localGameScene + CGameSceneNode::m_vecAbsOrigin + 0x0, localOrigin.x);
		(void)TryRead<float>(localGameScene + CGameSceneNode::m_vecAbsOrigin + 0x4, localOrigin.y);
		(void)TryRead<float>(localGameScene + CGameSceneNode::m_vecAbsOrigin + 0x8, localOrigin.z);
		
		Vec3 viewOffset{};
		(void)TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x0, viewOffset.x);
		(void)TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x4, viewOffset.y);
		(void)TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x8, viewOffset.z);
		Vec3 localEyePos = { localOrigin.x + viewOffset.x, localOrigin.y + viewOffset.y, localOrigin.z + viewOffset.z };
		
		// Проверяем, в прицеле ли мы (для снайперок)
		bool isScoped = false;
		(void)TryRead<bool>(localPawn + C_CSPlayerPawn::m_bIsScoped, isScoped);

		// Читаем AimPunch (отдачу) через m_pAimPunchServices
		QAngle aimPunch = {0,0,0};
		uintptr_t aimPunchSvc = 0;
		(void)TryRead<uintptr_t>(localPawn + C_CSPlayerPawn::m_pAimPunchServices, aimPunchSvc);
		if (aimPunchSvc) {
			(void)TryRead<QAngle>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle, aimPunch);
		}

		// ДОБАВЛЕНО: Читаем количество произведенных выстрелов
		int shotsFired = 0;
		(void)TryRead<int>(localPawn + C_CSPlayerPawn::m_iShotsFired, shotsFired);

		// ИСПРАВЛЕНИЕ: Читаем ViewMatrix и размеры экрана для Screen-Space расчетов
		const float* viewMatrix = reinterpret_cast<const float*>(client + g_offsetsRuntime.dwViewMatrix);
		if (!viewMatrix) return;
		
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		int width = (int)ds.x;
		int height = (int)ds.y;
		
		// Вычисляем радиус FOV в пикселях (та же формула, что в DrawEspImGui)
		float fovRadius = std::tan(g_aimbotFov * M_PI / 180.0f / 2.0f) * (height * 0.5f) / std::tan(90.0f * M_PI / 180.0f / 2.0f);

		// Логика поиска цели (SCREEN-SPACE для легита)
		float bestFovPixels = fovRadius; // Для Legit
		Vec3 bestTargetPos = {0,0,0};
		bool foundTarget = false;
		uintptr_t bestPawn = 0;

		uintptr_t entityList = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList))
			return;
		if (!entityList) return;

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

		for (int id = 0; id < 64; ++id)
		{
			uintptr_t listEntry = 0;
			if (!TryRead<uintptr_t>(entityList + 0x8 * ((id & 0x7FFF) >> 9) + 0x10, listEntry))
				continue;
			if (!listEntry) continue;

			uintptr_t controller = 0;
			if (!TryRead<uintptr_t>(listEntry + 0x70 * (id & 0x1FF), controller))
				continue;
			if (!controller) continue;

			uint32_t pawnHandle = 0;
			if (!TryRead<uint32_t>(controller + CCSPlayerController::m_hPlayerPawn, pawnHandle))
				continue;
			if (!pawnHandle) continue;

			int pawnIndex = pawnHandle & 0x1FF;
			int pawnEntryIndex = (pawnHandle & 0x7FFF) >> 9;

			uintptr_t pawnListEntry = 0;
			if (!TryRead<uintptr_t>(entityList + 0x8 * pawnEntryIndex + 0x10, pawnListEntry))
				continue;
			if (!pawnListEntry) continue;

			uintptr_t pawn = 0;
			if (!TryRead<uintptr_t>(pawnListEntry + 0x70 * pawnIndex, pawn))
				continue;
			if (!pawn) continue;
			if (localPawn && pawn == localPawn) continue;

			int tHp = 0;
			if (!TryRead<int>(pawn + C_BaseEntity::m_iHealth, tHp))
				continue;
			if (tHp <= 0 || tHp > 200) continue;

			int tTeam = 0;
			if (!TryRead<int>(pawn + C_BaseEntity::m_iTeamNum, tTeam))
				continue;
			if (tTeam < 2 || tTeam > 3) continue;
			if (g_aimbotTeamCheck && tTeam == localTeam) continue;

			// 1. ПРОВЕРКА ВИДИМОСТИ
			bool isVisible = false;
			if (localPlayerId != -1)
			{
				uint64_t spottedMask = 0;
				if (TryRead<uint64_t>(pawn + C_CSPlayerPawn::m_entitySpottedState + 0xC, spottedMask))
				{
					isVisible = (spottedMask & (1ULL << (localPlayerId - 1))) != 0;
				}
			}

			// Если включена проверка стен, скипаем тех, кого не видно
			if (g_aimbotEnabled && g_aimbotVisCheck)
			{
				if (!isVisible) continue;
			}

			// ИСПРАВЛЕНИЕ: Получаем АКТУАЛЬНУЮ позицию кости каждый кадр
			Vec3 targetBone = GetBonePos(pawn, g_aimbotBone);

			// Fallback если кость не найдена - используем более точный расчет
			if (targetBone.x == 0 && targetBone.y == 0 && targetBone.z == 0) {
				uintptr_t gameScene = 0;
				if (!TryRead<uintptr_t>(pawn + C_BaseEntity::m_pGameSceneNode, gameScene))
					continue;
				if (!gameScene) continue;

				Vec3 absOrigin{};
				(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x0, absOrigin.x);
				(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x4, absOrigin.y);
				(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x8, absOrigin.z);

				Vec3 targetViewOffset{};
				(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x0, targetViewOffset.x);
				(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x4, targetViewOffset.y);
				(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x8, targetViewOffset.z);

				// ИСПРАВЛЕНИЕ: Используем точные координаты без смещения по X/Y
				targetBone.x = absOrigin.x;
				targetBone.y = absOrigin.y;

				// ИСПРАВЛЕНИЕ: Высота в зависимости от выбранной кости (без viewOffset по X/Y)
				if (g_aimbotBone == 6) {
					// Голова - используем высоту глаз (это и есть центр головы)
					targetBone.z = absOrigin.z + targetViewOffset.z;
				}
				else if (g_aimbotBone == 5) {
					// Шея - чуть ниже головы
					targetBone.z = absOrigin.z + targetViewOffset.z * 0.85f;
				}
				else if (g_aimbotBone == 4) {
					// Грудь - середина тела
					targetBone.z = absOrigin.z + targetViewOffset.z * 0.5f;
				}
				else if (g_aimbotBone == 2) {
					// Живот - ниже середины
					targetBone.z = absOrigin.z + targetViewOffset.z * 0.3f;
				}
				else {
					// По умолчанию - голова
					targetBone.z = absOrigin.z + targetViewOffset.z;
				}
			}

			// 2. ЛОГИКА ВЫБОРА ЦЕЛИ
			// LEGITBOT: Обычный поиск по радиусу FOV возле прицела
			Vec2 screenPos;
			if (!WorldToScreen(viewMatrix, targetBone, width, height, screenPos))
				continue;

			float dx = screenPos.x - (width / 2.0f);
			float dy = screenPos.y - (height / 2.0f);
			float fovPixels = std::sqrt(dx * dx + dy * dy);

			if (fovPixels < bestFovPixels && fovPixels <= fovRadius)
			{
				bestFovPixels = fovPixels;
				bestTargetPos = targetBone;
				foundTarget = true;
				bestPawn = pawn;
			}
		}

		if (foundTarget) {
			// ИСПРАВЛЕНИЕ: Перечитываем позицию кости ПРЯМО ПЕРЕД наведением
			Vec3 freshTargetPos = GetBonePos(bestPawn, g_aimbotBone);
			
			// Если кость найдена, используем её напрямую без корректировок
			if (freshTargetPos.x != 0 || freshTargetPos.y != 0 || freshTargetPos.z != 0) {
				bestTargetPos = freshTargetPos;
			}
			// Если кость не найдена, используем fallback с точным расчетом
			else {
				uintptr_t gameScene = 0;
				if (TryRead<uintptr_t>(bestPawn + C_BaseEntity::m_pGameSceneNode, gameScene) && gameScene) {
					Vec3 absOrigin{};
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x0, absOrigin.x);
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x4, absOrigin.y);
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x8, absOrigin.z);
					
					Vec3 targetViewOffset{};
					(void)TryRead<float>(bestPawn + C_BaseModelEntity::m_vecViewOffset + 0x0, targetViewOffset.x);
					(void)TryRead<float>(bestPawn + C_BaseModelEntity::m_vecViewOffset + 0x4, targetViewOffset.y);
					(void)TryRead<float>(bestPawn + C_BaseModelEntity::m_vecViewOffset + 0x8, targetViewOffset.z);
					
					// Используем центр модели без смещения по X/Y
					bestTargetPos.x = absOrigin.x;
					bestTargetPos.y = absOrigin.y;
					bestTargetPos.z = absOrigin.z + targetViewOffset.z; // Высота глаз = центр головы
				}
			}
			
			// === VELOCITY PREDICTION (упреждение под движение) ===
			{
				Vec3 enemyVel{};
				TryRead<float>(bestPawn + C_BaseEntity::m_vecVelocity + 0x0, enemyVel.x);
				TryRead<float>(bestPawn + C_BaseEntity::m_vecVelocity + 0x4, enemyVel.y);
				TryRead<float>(bestPawn + C_BaseEntity::m_vecVelocity + 0x8, enemyVel.z);

				float enemySpeed = std::sqrt(enemyVel.x * enemyVel.x + enemyVel.y * enemyVel.y);

				// Применяем упреждение только если враг реально движется (> 10 юнитов/с)
				if (enemySpeed > 10.0f)
				{
					// Дистанция до врага в юнитах
					float dx = bestTargetPos.x - localEyePos.x;
					float dy = bestTargetPos.y - localEyePos.y;
					float dz = bestTargetPos.z - localEyePos.z;
					float distToTarget = std::sqrt(dx*dx + dy*dy + dz*dz);

					// Скорость пули CS2 примерно 50000 юн/с (зависит от оружия, но среднее)
					// Время полёта пули до цели
					constexpr float bulletSpeed = 50000.0f;
					float bulletTravelTime = distToTarget / bulletSpeed;

					// Масштабируем упреждение: чем дальше цель, тем больше упреждение
					// Clamp чтобы на очень большой дистанции не было безумного смещения
					float predScale = bulletTravelTime;
					if (predScale > 0.15f) predScale = 0.15f; // максимум 150мс упреждения

					bestTargetPos.x += enemyVel.x * predScale;
					bestTargetPos.y += enemyVel.y * predScale;
					// Z не трогаем — вертикальное упреждение обычно не нужно и даёт промахи
				}
			}
			// === END VELOCITY PREDICTION ===
			
			QAngle targetAngles = CalcAngle(localEyePos, bestTargetPos);

			// ВАЖНО: Компенсация отдачи (RCS)
			// ИСПРАВЛЕНИЕ: Вычитаем отдачу ТОЛЬКО во время стрельбы (shotsFired > 0).
			// Иначе прицел "съезжает" с головы от тряски камеры при ходьбе и прыжках.
			if (!isScoped && shotsFired > 0) {
				targetAngles.x -= aimPunch.x * 2.0f;
				targetAngles.y -= aimPunch.y * 2.0f;
			}

			ClampAngles(targetAngles); // ОБЯЗАТЕЛЬНО нормализуем ДО смуса!

			// Smooth
			if (g_aimbotSmooth > 1.0f) {
				float deltaX = targetAngles.x - currentAngles.x;
				float deltaY = targetAngles.y - currentAngles.y;
				if (deltaY > 180.0f) deltaY -= 360.0f;
				if (deltaY < -180.0f) deltaY += 360.0f;

				targetAngles.x = currentAngles.x + deltaX / g_aimbotSmooth;
				targetAngles.y = currentAngles.y + deltaY / g_aimbotSmooth;
			}

			ClampAngles(targetAngles);
			(void)TryWrite<QAngle>((uintptr_t)viewAnglesPtr, targetAngles);

			// --- ИСПРАВЛЕННЫЙ AUTOFIRE (Точный расчет без учета Smooth) ---
			if (g_aimbotAutoFire) {
				// 1. Считаем ИДЕАЛЬНЫЙ угол до кости прямо сейчас (без Smooth)
				QAngle idealAngles = CalcAngle(localEyePos, bestTargetPos);
				
				// 2. Учитываем отдачу для автовыстрела
				if (!isScoped && shotsFired > 0) {
					idealAngles.x -= aimPunch.x * 2.0f;
					idealAngles.y -= aimPunch.y * 2.0f;
				}
				ClampAngles(idealAngles);
				
				// 3. Считаем реальную погрешность между ТЕКУЩИМ прицелом и ИДЕАЛЬНЫМ углом
				float errorX = idealAngles.x - currentAngles.x;
				float errorY = idealAngles.y - currentAngles.y;
				while (errorY > 180.0f) errorY -= 360.0f;
				while (errorY < -180.0f) errorY += 360.0f;
				
				// Переводим в градусы отклонения
				float errorDegrees = std::sqrt(errorX * errorX + errorY * errorY);
				
				// Если прицел в радиусе 1.2 градуса от идеальной точки кости — стреляем!
				bool onTarget = (errorDegrees < 1.2f);

				// Определяем semi-auto оружие (пистолеты, снайперки)
				bool isSemiAuto = false;
				{
					uintptr_t ws2 = 0;
					if (TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pWeaponServices, ws2) && ws2) {
						uint32_t ha2 = 0;
						if (TryRead<uint32_t>(ws2 + CPlayer_WeaponServices::m_hActiveWeapon, ha2) && ha2) {
							int wi2 = ha2 & 0x1FF, we2 = (ha2 & 0x7FFF) >> 9;
							uintptr_t wle2 = 0;
							if (TryRead<uintptr_t>(entityList + 0x8 * we2 + 0x10, wle2) && wle2) {
								uintptr_t went2 = 0;
								if (TryRead<uintptr_t>(wle2 + 0x70 * wi2, went2) && went2) {
									uint16_t di2 = 0;
									TryRead<uint16_t>(went2 + C_EconEntity::m_AttributeManager +
										C_AttributeContainer::m_Item +
										C_EconItemView::m_iItemDefinitionIndex, di2);
									// Пистолеты и снайперки — semi-auto
									isSemiAuto = (di2==1||di2==2||di2==3||di2==4||di2==9||di2==30||di2==32||di2==36||di2==40||di2==61||di2==63||di2==64);
								}
							}
						}
					}
				}

				static bool  s_firePressed   = false;
				static ULONGLONG s_pressTime = 0;
				static ULONGLONG s_cooldownStart = 0;
				ULONGLONG nowFire = GetTickCount64();

				const ULONGLONG HOLD_MS     = 25;   // время удержания кнопки
				// ИСПРАВЛЕНИЕ: Добавили 40мс кулдауна для автоматов, чтобы разброс успевал остывать, и немного увеличили для полуавтоматов
				const ULONGLONG COOLDOWN_MS = isSemiAuto ? 220 : 40;

				bool inCooldown = (nowFire - s_cooldownStart < COOLDOWN_MS);

				if (onTarget && !inCooldown) {
					if (!s_firePressed) {
						(void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 65537);
						s_firePressed = true;
						s_pressTime = nowFire;
						g_autoFireActive = true;
					} else if (nowFire - s_pressTime >= HOLD_MS) {
						(void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
						s_firePressed = false;
						s_cooldownStart = nowFire; // начинаем паузу
						g_autoFireActive = false;
					}
				} else if (!onTarget) {
					if (s_firePressed) {
						(void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
						s_firePressed = false;
						g_autoFireActive = false;
					}
				}
			}
		} else {
			// Цель вообще не найдена в цикле
			if (g_autoFireActive) {
				(void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
				g_autoFireActive = false;
			}
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		if (g_autoFireActive) {
			uintptr_t client = GetClientBase();
			if (client) (void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
			g_autoFireActive = false;
		}
	}
}

