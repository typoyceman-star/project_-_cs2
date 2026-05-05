#include "style.h"
#include "../core/globals.h"
#include "imgui.h"

void ApplyClientStyle()
{
	ImGuiStyle& s = ImGui::GetStyle();
	
	// Современная геометрия
	s.WindowPadding = ImVec2(16, 16);
	s.FramePadding = ImVec2(10, 6);
	s.ItemSpacing = ImVec2(10, 10);
	s.ItemInnerSpacing = ImVec2(8, 6);
	
	// Скругления (Стиль macOS/Modern UI)
	s.WindowRounding = 10.0f; 
	s.ChildRounding = 8.0f;
	s.FrameRounding = 8.0f;
	s.PopupRounding = 8.0f;
	s.ScrollbarRounding = 12.0f;
	s.GrabMinSize = 10.0f; // Тонкий ползунок слайдера
	s.GrabRounding = 8.0f; // Круглый ползунок
	
	// Бордеры (Минимализм)
	s.WindowBorderSize = 0.0f;
	s.ChildBorderSize = 1.0f;
	s.FrameBorderSize = 0.0f;
	s.PopupBorderSize = 1.0f;

	ImVec4* c = s.Colors;
	
	// Deep Dark Theme с фиолетово-синим акцентом
	c[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 0.97f);
	c[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.09f, 0.60f);
	c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);
	
	c[ImGuiCol_Border] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
	c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	
	c[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
	c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);

	// Элементы управления
	c[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
	c[ImGuiCol_ButtonHovered] = ImVec4(0.16f, 0.16f, 0.19f, 1.00f);
	c[ImGuiCol_ButtonActive] = g_guiColor;

	// Инпуты (темнее фона для глубины)
	c[ImGuiCol_FrameBg] = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);
	c[ImGuiCol_FrameBgHovered] = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
	c[ImGuiCol_FrameBgActive] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

	// Акценты
	c[ImGuiCol_CheckMark] = g_guiColor;
	c[ImGuiCol_SliderGrab] = g_guiColor;
	c[ImGuiCol_SliderGrabActive] = ImVec4(g_guiColor.x + 0.1f, g_guiColor.y + 0.1f, g_guiColor.z + 0.1f, 1.0f);
	
	// Хедеры
	c[ImGuiCol_Header] = ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.30f);
	c[ImGuiCol_HeaderHovered] = ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.60f);
	c[ImGuiCol_HeaderActive] = g_guiColor;
	
	// Сепараторы
	c[ImGuiCol_Separator] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
	c[ImGuiCol_SeparatorHovered] = g_guiColor;
	c[ImGuiCol_SeparatorActive] = g_guiColor;
	c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.75f);
	
	// Табы
	s.TabRounding = 6.0f;
	s.TabBorderSize = 0.0f;
	c[ImGuiCol_Tab]                = ImVec4(0.10f, 0.10f, 0.13f, 1.0f);
	c[ImGuiCol_TabHovered]         = ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.5f);
	c[ImGuiCol_TabActive]          = ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.9f);
	c[ImGuiCol_TitleBg]            = ImVec4(0.06f, 0.06f, 0.08f, 1.0f);
	c[ImGuiCol_TitleBgActive]      = ImVec4(g_guiColor.x*0.5f, g_guiColor.y*0.4f, g_guiColor.z*0.9f, 1.0f);
	s.ItemSpacing                  = ImVec2(8, 7);
	s.IndentSpacing                = 16.0f;
}

