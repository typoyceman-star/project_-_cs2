#include "snaplines.h"
#include "../../core/globals.h"
#include "imgui.h"

void DrawSnaplinesImGui()
{
	if (!g_snaplinesEnabled && !g_distanceEnabled) return;
	if (g_espBoxes.empty()) return;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	ImVec2 ds = ImGui::GetIO().DisplaySize;
	ImVec2 bottom(ds.x * 0.5f, ds.y);

	uintptr_t client = GetClientBase();
	if (!client) return;
	uintptr_t localPawn = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn) || !localPawn) return;
	uintptr_t localScene = 0;
	if (!TryRead<uintptr_t>(localPawn + g_offsetsRuntime.m_pGameSceneNode, localScene) || !localScene) return;
	Vec3 myPos{};
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin, myPos.x);
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 4, myPos.y);
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 8, myPos.z);

	int idx = 0;
	for (const auto& tgt : g_espTargetsWorld)
	{
		if (idx >= (int)g_espBoxes.size()) break;
		const EspBox& eb = g_espBoxes[idx++];

		if (g_snaplinesEnabled)
		{
			ImVec2 boxBottom((eb.box.left + eb.box.right) * 0.5f, eb.box.bottom);
			dl->AddLine(bottom, boxBottom, IM_COL32(255, 200, 80, 130), 1.0f);
		}

		if (g_distanceEnabled)
		{
			float dx = tgt.origin.x - myPos.x;
			float dy = tgt.origin.y - myPos.y;
			float dz = tgt.origin.z - myPos.z;
			float dist = std::sqrt(dx*dx + dy*dy + dz*dz) * 0.01905f; // units→metres
			char distBuf[16];
			sprintf_s(distBuf, sizeof(distBuf), "%.0fm", dist);
			ImVec2 distPos((eb.box.left + eb.box.right) * 0.5f - 12.0f, eb.box.top - 28.0f);
			dl->AddText(distPos, IM_COL32(200, 200, 200, 200), distBuf);
		}
	}
}
