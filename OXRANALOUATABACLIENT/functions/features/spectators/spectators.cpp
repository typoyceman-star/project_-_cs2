#include "spectators.h"
#include "../../core/globals.h"
#include "imgui.h"

void DrawSpectatorListImGui()
{
	// Функция включена? Если нет — вообще не рендерим
	if (!g_spectatorListEnabled) return;

	ImGui::SetNextWindowBgAlpha(0.85f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.6f));
	
	// Логика взаимодействия: если меню закрыто, окно прозрачно для кликов и зафиксировано
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar;
	if (!g_menuOpen) {
		windowFlags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
	}

	// Задаем минимальный размер, чтобы окно не "схлопывалось" до микро-размера, когда никого нет
	ImGui::SetNextWindowSizeConstraints(ImVec2(150, 0), ImVec2(300, 500));
	
	ImGui::Begin("Spectators", nullptr, windowFlags);
	
	ImGui::PushFont(ImGui::GetFont());
	ImGui::TextColored(g_guiColor, "SPECTATORS");
	ImGui::PopFont();
	ImGui::Separator();
	
	if (g_spectators.empty()) 
	{
		// Состояние "Пусто"
		ImGui::TextDisabled("No Spectators");
	} 
	else 
	{
		// Динамическое наполнение
		for (const auto& spec : g_spectators) {
			ImGui::Text("👀 %s", spec.c_str());
		}
	}
	
	ImGui::End();
	
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

