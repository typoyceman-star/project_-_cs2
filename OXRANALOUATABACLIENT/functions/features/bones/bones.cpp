#include "bones.h"
#include "../../core/globals.h"
#include "../../core/memory.h"
#include "../../core/math.h"
#include "../aimbot/aimbot.h"
#include "../../../output/offsets.hpp"
#include "../../../output/client_dll.hpp"
#include "imgui.h"

void UpdateBonesCache(int width, int height)
{
	uintptr_t client = GetClientBase();
	if (!client) return;
	using namespace cs2_dumper;
	using namespace cs2_dumper::schemas::client_dll;
	static const int neededBones[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 };

	// Временный массив для защиты от краша (используется ВНЕ блока __try)
	BoneCacheEntryWorld tempBones[64] = {};
	int boneCount = 0;

	__try
	{
		for (const EspTargetWorld& e : g_espTargetsWorld)
		{
			if (boneCount >= 64) break;
			uintptr_t pawn = e.pawn;
			if (!pawn) continue;

			uintptr_t gameScene = 0;
			if (!TryRead<uintptr_t>(pawn + C_BaseEntity::m_pGameSceneNode, gameScene) || !gameScene) continue;

			uintptr_t boneMatrix = 0;
			if (!TryRead<uintptr_t>(gameScene + CSkeletonInstance::m_modelState + 0x80, boneMatrix) || !boneMatrix) continue;

			BoneCacheEntryWorld entry{};
			entry.pawn = pawn;
			for (auto& cb : entry.b) cb.valid = false;

			for (int boneId : neededBones)
			{
				Vec3 p3{};
				if (TryRead<float>(boneMatrix + boneId * 0x20 + 0x0, p3.x) &&
					TryRead<float>(boneMatrix + boneId * 0x20 + 0x4, p3.y) &&
					TryRead<float>(boneMatrix + boneId * 0x20 + 0x8, p3.z))
				{
					entry.b[boneId].world = p3;
					entry.b[boneId].valid = true;
				}
			}
			tempBones[boneCount++] = entry;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}

	// ИСПРАВЛЕНИЕ: std::vector используется ВНЕ блока __try!
	g_boneCache.clear();
	for (int i = 0; i < boneCount; ++i) {
		g_boneCache.push_back(tempBones[i]);
	}
}


void DrawBonesImGui(const float view[16], int width, int height)
{
	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	const ImU32 col = ImGui::ColorConvertFloat4ToU32(g_boneColor);

	// Draw-only: must not read memory here.
	// Reduced chains: spine/head/arms/legs
	static const int spineChain[] = { 6, 5, 4, 2, 0 };
	static const int leftArmChain[] = { 4, 7, 8, 9, 10 };
	static const int rightArmChain[] = { 4, 12, 13, 14, 15 };
	static const int leftLegChain[] = { 0, 22, 23, 24 };
	static const int rightLegChain[] = { 0, 25, 26, 27 };

	auto DrawChain = [&](const BoneCacheEntryWorld& entry, const int* chain, int count)
	{
		ImVec2 pts[8]{};
		int n = 0;
		for (int i = 0; i < count; ++i)
		{
			int id = chain[i];
			if (id < 0 || id >= 28) continue;
			if (!entry.b[id].valid) continue;
			Vec2 p2{};
			if (!WorldToScreen(view, entry.b[id].world, width, height, p2))
				continue;
			pts[n++] = ImVec2(p2.x, p2.y);
		}
		if (n >= 2)
			drawList->AddPolyline(pts, n, col, 0, 1.5f);
	};

	for (const BoneCacheEntryWorld& entry : g_boneCache)
	{
		if (!g_boneHideLines)
		{
			DrawChain(entry, spineChain, (int)(sizeof(spineChain) / sizeof(spineChain[0])));
			DrawChain(entry, leftArmChain, (int)(sizeof(leftArmChain) / sizeof(leftArmChain[0])));
			DrawChain(entry, rightArmChain, (int)(sizeof(rightArmChain) / sizeof(rightArmChain[0])));
			DrawChain(entry, leftLegChain, (int)(sizeof(leftLegChain) / sizeof(leftLegChain[0])));
			DrawChain(entry, rightLegChain, (int)(sizeof(rightLegChain) / sizeof(rightLegChain[0])));
		}

		if (g_boneDebugIds)
		{
			static const int knownBones[] = { 0, 4, 5, 6, 7, 8, 10, 12, 13, 15, 22, 24, 25, 27 };
			static const int extraBones[] = { 1, 2, 3, 9, 11, 14, 23, 26 };
			for (int id : knownBones)
			{
				if (!entry.b[id].valid) continue;
				Vec2 p2{};
				if (!WorldToScreen(view, entry.b[id].world, width, height, p2)) continue;
				char buf[8];
				sprintf_s(buf, "%d", id);
				drawList->AddCircleFilled(ImVec2(p2.x, p2.y), 3.0f, IM_COL32(255, 255, 0, 255));
				drawList->AddText(ImVec2(p2.x + 5, p2.y - 6), IM_COL32(255, 255, 0, 255), buf);
			}
			for (int id : extraBones)
			{
				if (!entry.b[id].valid) continue;
				Vec2 p2{};
				if (!WorldToScreen(view, entry.b[id].world, width, height, p2)) continue;
				char buf[8];
				sprintf_s(buf, "%d", id);
				drawList->AddCircleFilled(ImVec2(p2.x, p2.y), 4.0f, IM_COL32(0, 255, 0, 255));
				drawList->AddText(ImVec2(p2.x + 5, p2.y - 6), IM_COL32(0, 255, 0, 255), buf);
			}
		}
	}
}
