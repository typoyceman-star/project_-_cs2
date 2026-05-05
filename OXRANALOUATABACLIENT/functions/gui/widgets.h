#pragma once
// Общие ImGui виджеты, используемые меню и фичами.

#include "imgui.h"

void DrawOutlinedText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text);

bool PremiumToggle(const char* label, bool* v);
void BeginPremiumChild(const char* str_id, const char* title, ImVec2 size);
void EndPremiumChild();
bool PremiumSettingsButton(const char* id);

bool KeybindWidget(const char* id, int& vk);
const char* VkToStringA(int vk);
