#include "widgets.h"
#include "../core/globals.h"
#include "../config/config_io.h"
#include "imgui.h"
#include "imgui_internal.h"

void DrawOutlinedText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text)
{
	ImU32 outlineCol = IM_COL32(0, 0, 0, 255);
	dl->AddText(font, size, ImVec2(pos.x - 1, pos.y - 1), outlineCol, text);
	dl->AddText(font, size, ImVec2(pos.x + 1, pos.y - 1), outlineCol, text);
	dl->AddText(font, size, ImVec2(pos.x - 1, pos.y + 1), outlineCol, text);
	dl->AddText(font, size, ImVec2(pos.x + 1, pos.y + 1), outlineCol, text);
	dl->AddText(font, size, pos, color, text);
}


bool PremiumToggle(const char* label, bool* v)
{
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	
	float height = 20.0f;
	float width = 38.0f;
	float radius = height * 0.5f;

	ImGui::InvisibleButton(label, ImVec2(width, height));
	bool clicked = ImGui::IsItemClicked();
	if (clicked) *v = !*v;
	bool hovered = ImGui::IsItemHovered();

	// Анимация 
	ImGuiID id = ImGui::GetID(label);
	float* anim_ptr = ImGui::GetStateStorage()->GetFloatRef(id, *v ? 1.0f : 0.0f);
	float& anim = *anim_ptr;
	
	// Скорость анимации (12.0f = плавно, но быстро)
	anim += (*v ? 1.0f : -1.0f) * ImGui::GetIO().DeltaTime * 12.0f;
	if (anim < 0.0f) anim = 0.0f;
	if (anim > 1.0f) anim = 1.0f;

	ImVec4 col_off = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
	ImVec4 col_on = g_guiColor;
	
	// Интерполяция цвета от серого к цвету темы
	ImVec4 current_col = ImVec4(
		col_off.x + (col_on.x - col_off.x) * anim,
		col_off.y + (col_on.y - col_off.y) * anim,
		col_off.z + (col_on.z - col_off.z) * anim,
		1.0f
	);
	
	if (hovered) {
		current_col.x = (current_col.x + 0.1f > 1.0f) ? 1.0f : current_col.x + 0.1f;
		current_col.y = (current_col.y + 0.1f > 1.0f) ? 1.0f : current_col.y + 0.1f;
		current_col.z = (current_col.z + 0.1f > 1.0f) ? 1.0f : current_col.z + 0.1f;
	}

	// Фон свитча
	draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), ImGui::ColorConvertFloat4ToU32(current_col), radius);
	
	// Плавный сдвиг кружка
	float circle_x = p.x + radius + (width - radius * 2.0f) * anim;
	draw_list->AddCircleFilled(ImVec2(circle_x, p.y + radius), radius - 2.0f, IM_COL32(255, 255, 255, 255));
	
	// Мягкое свечение кружка
	if (anim > 0.0f) {
		draw_list->AddCircleFilled(ImVec2(circle_x, p.y + radius), radius + 2.0f, ImGui::ColorConvertFloat4ToU32(ImVec4(current_col.x, current_col.y, current_col.z, 0.4f * anim)));
	}

	// Плавное изменение цвета текста
	ImVec4 text_col = ImVec4(
		0.5f + (current_col.x - 0.5f) * anim,
		0.5f + (current_col.y - 0.5f) * anim,
		0.5f + (current_col.z - 0.5f) * anim,
		1.0f
	);
	
	ImGui::SameLine();
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (height - ImGui::GetTextLineHeight()) * 0.5f);
	
	ImGui::PushStyleColor(ImGuiCol_Text, text_col);
	ImGui::TextUnformatted(label);
	ImGui::PopStyleColor();
	
	return clicked;
}

void BeginPremiumChild(const char* str_id, const char* title, ImVec2 size) 
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.09f, 0.85f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 14));
	
	ImGui::BeginChild(str_id, size, true, ImGuiWindowFlags_NoScrollbar);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 w_pos = ImGui::GetWindowPos();
	float w_width = ImGui::GetWindowWidth();
	
	// Отрисовка красивого заголовка панели
	ImU32 headBgL = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x * 0.2f, g_guiColor.y * 0.2f, g_guiColor.z * 0.3f, 0.7f));
	ImU32 headBgR = ImGui::ColorConvertFloat4ToU32(ImVec4(0.08f, 0.08f, 0.09f, 0.0f));
	
	// Градиент в шапке
	draw_list->AddRectFilledMultiColor(w_pos, ImVec2(w_pos.x + w_width, w_pos.y + 35.0f), headBgL, headBgR, headBgR, headBgL);
	
	// Акцентная линия снизу заголовка
	draw_list->AddLine(ImVec2(w_pos.x, w_pos.y + 35.0f), ImVec2(w_pos.x + w_width, w_pos.y + 35.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.5f)), 1.0f);

	// Текст заголовка
	ImGui::SetCursorPos(ImVec2(14, 10));
	ImGui::TextColored(g_guiColor, title);
	
	// Сдвигаем курсор для контента (чтобы не рисовалось на шапке)
	ImGui::SetCursorPos(ImVec2(14, 45)); 
}

void EndPremiumChild() 
{
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(2);
}


bool PremiumSettingsButton(const char* id)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::InvisibleButton(id, ImVec2(24, 24));
	bool hovered = ImGui::IsItemHovered();
	
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImU32 col = ImGui::GetColorU32(hovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
	
	ImVec2 c(pos.x + 12, pos.y + 12);
	
	// Отрисовка идеальной шестеренки (Gear) математикой
	dl->AddCircle(c, 4.5f, col, 12, 2.0f); // Основное кольцо
	for (int i = 0; i < 6; i++) {
		float angle = i * (3.1415926f / 3.0f);
		dl->AddLine(
			ImVec2(c.x + cosf(angle) * 4.5f, c.y + sinf(angle) * 4.5f),
			ImVec2(c.x + cosf(angle) * 7.0f, c.y + sinf(angle) * 7.0f), 
			col, 2.5f
		); // Зубья
	}
	
	// Центральное отверстие (цвет фона)
	dl->AddCircleFilled(c, 2.0f, ImGui::ColorConvertFloat4ToU32(ImVec4(0.06f, 0.06f, 0.07f, 1.0f))); 
	
	return clicked;
}

bool KeybindWidget(const char* id, int& vk)
{
	ImGui::PushID(id);
	static ImGuiID s_capturing = 0;
	ImGuiID my = ImGui::GetID("keybind");

	bool changed = false;
	const bool capturing = (s_capturing == my);
	
	// Визуализация
	char buf[32]{};
	if (capturing)
		lstrcpyA(buf, "Press any key...");
	else if (vk == 0)
		lstrcpyA(buf, "[ None ]");
	else
		wsprintfA(buf, "[ %s ]", VkToStringA(vk));

	ImVec4 col = capturing ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, col);
	
	if (ImGui::Button(buf, ImVec2(110.0f, 0.0f)))
	{
		s_capturing = my; // Начинаем слушать ввод по клику
	}
	ImGui::PopStyleColor();

	// Обработка ввода если виджет активен
	if (capturing)
	{
		// Проверяем ESC для сброса
		if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
		{
			vk = 0;
			s_capturing = 0;
			changed = true;
			SaveConfig(); // Автосохранение
			
			// Ждем пока отпустят кнопку чтобы не мигало
			while(GetAsyncKeyState(VK_ESCAPE) & 0x8000) Sleep(1);
		}
		// Проверяем Shift для сброса (как ты просил)
		else if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
		{
			// Если нажали Shift во время бинда - считаем это сбросом? 
			// Или можно биндить Shift. Сделаем как просил: Shift очищает.
			vk = 0;
			s_capturing = 0;
			changed = true;
			SaveConfig();
		}
		else
		{
			// Перебор всех клавиш
			for (int k = 1; k < 255; ++k)
			{
				if (k == VK_ESCAPE) continue;
				
				// Пропускаем клик ЛКМ, которым мы активировали кнопку (ждем пока отпустят)
				if (k == VK_LBUTTON && ImGui::IsMouseDown(0)) continue;

				// Проверка нажатия (async state)
				if (GetAsyncKeyState(k) & 0x8000)
				{
					vk = k;
					s_capturing = 0; // Перестаем слушать
					changed = true;
					SaveConfig(); // Автосохранение
					break;
				}
			}
		}
	}

	ImGui::PopID();
	return changed;
}

const char* VkToStringA(int vk)
{
	static char buf[64];
	buf[0] = '\0';
	if (vk == 0) return "[none]";

	UINT scan = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
	LONG lParam = (LONG)(scan << 16);
	if (vk == VK_LEFT || vk == VK_UP || vk == VK_RIGHT || vk == VK_DOWN || vk == VK_PRIOR || vk == VK_NEXT || vk == VK_END || vk == VK_HOME || vk == VK_INSERT || vk == VK_DELETE)
		lParam |= 1 << 24;

	int n = GetKeyNameTextA(lParam, buf, (int)sizeof(buf));
	if (n > 0) return buf;
	wsprintfA(buf, "VK_%d", vk);
	return buf;
}
