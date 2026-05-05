#include "watermark.h"
#include "../core/globals.h"
#include "../core/memory.h"
#include "imgui.h"

void DrawWatermarkImGui()
{
	static int cs2Fps = 0;
	static ULONGLONG lastFpsTime = GetTickCount64();
	static int lastFrameCount = 0;
	
	uintptr_t client = GetClientBase();
	int currentPing = 0;
	char playerName[64] = "Unknown";
	
	if (client) {
		uintptr_t globalVars = 0;
		if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwGlobalVars, globalVars) && globalVars) {
			int frameCount = 0;
			if (TryRead<int>(globalVars + 0x4, frameCount)) { // 0x4 - это m_real_frametime/framecount в CS2
				ULONGLONG now = GetTickCount64();
				if (now - lastFpsTime >= 1000) {
					cs2Fps = frameCount - lastFrameCount;
					lastFrameCount = frameCount;
					lastFpsTime = now;
				}
			}
		}
		
		uintptr_t localCtrl = 0;
		if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerController, localCtrl) && localCtrl) {
			TryRead<int>(localCtrl + 0x740, currentPing); // 0x740 - это m_iPing
			
			StringBuf64 nameRaw = {0};
			if (TryRead<StringBuf64>(localCtrl + g_offsetsRuntime.m_iszPlayerName, nameRaw)) {
				memcpy(playerName, nameRaw.data, 64);
				playerName[63] = '\0';
			}
		}
	}
	
	if (cs2Fps <= 0 || cs2Fps > 2000) cs2Fps = (int)ImGui::GetIO().Framerate; // Fallback

	ImGuiWindowFlags wmFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;
	if (!g_menuOpen) wmFlags |= ImGuiWindowFlags_NoInputs; // Пропускать клики, если меню закрыто!
	
	ImGui::SetNextWindowBgAlpha(0.7f);
	ImGui::Begin("Watermark", nullptr, wmFlags);
	ImGui::TextColored(g_guiColor, "OXRANA LOUTABA");
	ImGui::SameLine();
	ImGui::Text("| User: %s | FPS: %d | Ping: %d ms", playerName, cs2Fps, currentPing);
	ImGui::End();
}
