#include "radar.h"
#include "../../core/globals.h"
#include "../../core/memory.h"
#include "../../../output/offsets.hpp"
#include "../../../output/client_dll.hpp"
#include "imgui.h"

void DrawRadarImGui()
{
	if (!g_radarHackEnabled || g_espTargetsWorld.empty()) return;

	uintptr_t client = GetClientBase();
	if (!client) return;

	QAngle* vpPtr = reinterpret_cast<QAngle*>(client + g_offsetsRuntime.dwViewAngles);
	QAngle va{};
	if (!TryRead<QAngle>((uintptr_t)vpPtr, va)) return;

	uintptr_t localPawn = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn) || !localPawn) return;
	uintptr_t localScene = 0;
	if (!TryRead<uintptr_t>(localPawn + g_offsetsRuntime.m_pGameSceneNode, localScene) || !localScene) return;
	Vec3 myPos{};
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 0x0, myPos.x);
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 0x4, myPos.y);
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 0x8, myPos.z);

	ImVec2 ds = ImGui::GetIO().DisplaySize;

	// Радар: позиция и размер
	static float radarX = 20.0f, radarY = 20.0f;
	float radarSize = g_radarSize;
	float radarScale = g_radarScale;
	ImVec2 radarCenter(radarX + radarSize * 0.5f, radarY + radarSize * 0.5f);

	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	// Фон
	dl->AddCircleFilled(radarCenter, radarSize * 0.5f, IM_COL32(0, 0, 0, 160), 64);
	dl->AddCircle(radarCenter, radarSize * 0.5f, IM_COL32(80, 80, 80, 255), 64, 1.5f);
	// Кресты
	dl->AddLine(ImVec2(radarCenter.x - radarSize*0.5f, radarCenter.y), ImVec2(radarCenter.x + radarSize*0.5f, radarCenter.y), IM_COL32(60,60,60,150), 0.5f);
	dl->AddLine(ImVec2(radarCenter.x, radarCenter.y - radarSize*0.5f), ImVec2(radarCenter.x, radarCenter.y + radarSize*0.5f), IM_COL32(60,60,60,150), 0.5f);
	// Игрок (белая точка в центре)
	dl->AddCircleFilled(radarCenter, 4.0f, IM_COL32(255, 255, 255, 255));

	float yawRad = va.y * (M_PI / 180.0f);
	float cosYaw = std::cos(-yawRad);
	float sinYaw = std::sin(-yawRad);

	for (const EspTargetWorld& t : g_espTargetsWorld)
	{
		float dx = t.origin.x - myPos.x;
		float dy = t.origin.y - myPos.y;

		// Поворачиваем относительно взгляда
		float rx = dx * cosYaw - dy * sinYaw;
		float ry = dx * sinYaw + dy * cosYaw;

		// Масштабируем
		float px = radarCenter.x + rx / radarScale;
		float py = radarCenter.y - ry / radarScale; // Y инвертирован

		// Ограничиваем по кругу
		float relX = px - radarCenter.x;
		float relY = py - radarCenter.y;
		float dist = std::sqrt(relX * relX + relY * relY);
		float maxR = radarSize * 0.5f - 5.0f;
		if (dist > maxR) { px = radarCenter.x + relX * maxR / dist; py = radarCenter.y + relY * maxR / dist; }

		// Цвет: красный для CT, желтый для T
		ImU32 dotCol = IM_COL32(255, 80, 80, 255);
		dl->AddCircleFilled(ImVec2(px, py), 4.0f, dotCol);
		dl->AddCircle(ImVec2(px, py), 4.5f, IM_COL32(0,0,0,180), 8, 1.0f);
	}

	// Бомба на радаре
	if (g_bombEspEnabled && g_bombData.found)
	{
		float bdx = g_bombData.pos.x - myPos.x;
		float bdy = g_bombData.pos.y - myPos.y;
		float brx = bdx * cosYaw - bdy * sinYaw;
		float bry = bdx * sinYaw + bdy * cosYaw;
		float bpx = radarCenter.x + brx / radarScale;
		float bpy = radarCenter.y - bry / radarScale;
		float relBX = bpx - radarCenter.x, relBY = bpy - radarCenter.y;
		float bd = std::sqrt(relBX*relBX + relBY*relBY);
		float maxR2 = radarSize * 0.5f - 5.0f;
		if (bd > maxR2) { bpx = radarCenter.x + relBX*maxR2/bd; bpy = radarCenter.y + relBY*maxR2/bd; }
		dl->AddRectFilled(ImVec2(bpx-4, bpy-4), ImVec2(bpx+4, bpy+4), IM_COL32(255, 220, 0, 255));
	}
}

