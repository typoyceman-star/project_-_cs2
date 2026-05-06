#include "menu.h"
#include "widgets.h"
#include "style.h"
#include "../core/globals.h"
#include "../core/memory.h"
#include "../config/config_io.h"
#include "../config/paths.h"
#include "../features/skinchanger/skin_changer.hpp"
#include "../features/skinchanger/glove_changer.hpp"
#include "../features/skinchanger/item_schema.hpp"
#include "../../main.hpp"
#include "imgui.h"
#include "imgui_internal.h"

void DrawMenuImGui()
{
	if (!g_menuOpen) return;

	// Авто-сохранение: сохраняем через 800мс после последнего изменения
	static bool  s_cfgDirty = false;
	static ULONGLONG s_cfgDirtyTs = 0;
	auto MarkDirty = [&]() { s_cfgDirty = true; s_cfgDirtyTs = GetTickCount64(); };
	if (s_cfgDirty && (GetTickCount64() - s_cfgDirtyTs > 800)) { SaveConfig(); s_cfgDirty = false; }

	ImGui::SetNextWindowBgAlpha(0.96f);

	ImGui::SetNextWindowSize(ImVec2(650.0f, 420.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("##midnight_menu", &g_menuOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetWindowPos();
	ImVec2 s_win = ImGui::GetWindowSize();
	
	// --- ПЛАВАЮЩИЕ ЧАСТИЦЫ НА ФОНЕ МЕНЮ ---
	{
		struct Particle { ImVec2 pos; ImVec2 vel; };
		static std::vector<Particle> particles;
		static bool initParticles = false;
		if (!initParticles) {
			for (int i = 0; i < 40; i++) {
				particles.push_back({
					ImVec2(p.x + rand() % (int)s_win.x, p.y + rand() % (int)s_win.y),
					ImVec2((rand() % 100 - 50) / 100.0f, (rand() % 100 - 50) / 100.0f)
				});
			}
			initParticles = true;
		}

		ImU32 particleColor = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.3f));
		ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.15f));

		for (auto& part : particles) {
			// Движение
			part.pos.x += part.vel.x * ImGui::GetIO().DeltaTime * 60.0f;
			part.pos.y += part.vel.y * ImGui::GetIO().DeltaTime * 60.0f;

			// Отражение от краев окна
			if (part.pos.x < p.x || part.pos.x > p.x + s_win.x) part.vel.x *= -1.0f;
			if (part.pos.y < p.y || part.pos.y > p.y + s_win.y) part.vel.y *= -1.0f;

			// Отрисовка точек
			drawList->AddCircleFilled(part.pos, 1.5f, particleColor);

			// Соединение линиями
			for (auto& other : particles) {
				float dist = std::hypot(part.pos.x - other.pos.x, part.pos.y - other.pos.y);
				if (dist < 60.0f && dist > 0.0f) {
					drawList->AddLine(part.pos, other.pos, lineColor, 1.0f);
				}
			}
		}
	}
	// --------------------------------------
	
	// Улучшенный Ultra-Premium Glow (мягкое объемное свечение вокруг окна)
	for (int i = 0; i < 12; ++i) {
		float alpha = 0.05f * std::pow(0.7f, (float)i); // Экспоненциальное затухание для крутой тени
		drawList->AddRect(
			ImVec2(p.x - i, p.y - i), ImVec2(p.x + s_win.x + i, p.y + s_win.y + i), 
			ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, alpha)), 
			8.0f + i, 0, 2.0f
		);
	}

	// Полная шапка с градиентом
	ImU32 hdrL = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x*0.4f, g_guiColor.y*0.3f, g_guiColor.z*0.8f, 1.0f));
	ImU32 hdrR = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 1.0f));
	drawList->AddRectFilledMultiColor(p, ImVec2(p.x + s_win.x, p.y + 36.0f), hdrL, hdrR, hdrR, hdrL);

	// Иконка + название
	ImGui::SetCursorPos(ImVec2(14, 10));
	ImGui::TextColored(ImVec4(1,1,1,1), "OXRANA LOUTABA CLIENT");

	// Версия справа
	ImGui::SetCursorPos(ImVec2(s_win.x - 50, 10));
	ImGui::TextColored(ImVec4(1,1,1,0.5f), "v2.0");

	ImGui::SetCursorPos(ImVec2(0, 36));
	ImGui::Separator();
	ImGui::SetCursorPosY(46);

	enum Tab : int { TAB_COMBAT = 0, TAB_ESP = 1, TAB_PLAYER = 2, TAB_MOVEMENT = 3, TAB_SKINS = 4, TAB_CONFIGS = 5, TAB_OTHER = 6 };
	static int activeTab = TAB_ESP;
	static int prevTab = -1;
	static float tabFadeAlpha = 1.0f;
	static float tabFadeStart = 0.0f;
	
	// Tab fade-in animation (0.2 seconds)
	if (prevTab != activeTab) {
		tabFadeStart = (float)ImGui::GetTime();
		prevTab = activeTab;
	}
	float fadeElapsed = (float)ImGui::GetTime() - tabFadeStart;
	tabFadeAlpha = ImMin(1.0f, fadeElapsed / 0.2f);

	const float btnH = 30.0f;
	ImGuiStyle& s = ImGui::GetStyle();
	ImVec4 base = ImGui::GetStyleColorVec4(ImGuiCol_Button);
	ImVec4 baseH = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
	ImVec4 baseA = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);

	auto TabButton = [&](const char* label, int tabId)
	{
		const bool isActive = (activeTab == tabId);
		float t = (float)ImGui::GetTime();
		
		ImVec4 col = isActive ? baseA : base;
		
		ImGui::BeginGroup();
		
		// Анимация при наведении
		if (isActive) {
			float pulse = 0.8f + 0.2f * sinf(t * 4.0f);
			col.w = pulse;
		}
		
		ImGui::PushStyleColor(ImGuiCol_Button, col);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, isActive ? 1.0f : 0.0f);
		
		// Анимированная рамка для активной вкладки
		float borderGlow = 0.6f + 0.4f * sinf(t * 5.0f);
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(borderGlow, 0.3f, 0.9f, 1.0f));
		
		ImGui::PushID(tabId);
		if (ImGui::Button(label, ImVec2(-1.0f, btnH))) activeTab = tabId;
		
		// Эффект свечения при наведении
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) 
		{
			ImVec4 hoverCol = baseH;
			float hoverAnim = 0.5f + 0.5f * sinf(t * 8.0f);
			hoverCol.w = 0.8f + 0.2f * hoverAnim;
			
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 rmin = ImGui::GetItemRectMin();
			ImVec2 rmax = ImGui::GetItemRectMax();
			
			// Двойная рамка для эффекта свечения
			dl->AddRect(rmin, rmax, ImGui::ColorConvertFloat4ToU32(hoverCol), 6.0f, 0, 1.5f);
			ImVec2 rmin2 = ImVec2(rmin.x - 2, rmin.y - 2);
			ImVec2 rmax2 = ImVec2(rmax.x + 2, rmax.y + 2);
			dl->AddRect(rmin2, rmax2, ImGui::ColorConvertFloat4ToU32(ImVec4(hoverCol.x, hoverCol.y, hoverCol.z, hoverCol.w * 0.3f)), 6.0f, 0, 2.0f);
		}
		ImGui::PopID();

		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);
		
		ImGui::EndGroup();
	};

	// Три колонки: [Tabs 140px] | [Content flex] | [Preview 210px]
	// Ширина превью будет 0, если мы не во вкладке ESP
	float actualPreviewW = (activeTab == TAB_ESP) ? 210.0f : 0.0f;
	float contentW = ImGui::GetContentRegionAvail().x - 150.0f - actualPreviewW - 16.0f;

	ImGui::BeginChild("##left_tabs", ImVec2(140.0f, 0), true, ImGuiWindowFlags_None);
	TabButton("Combat", TAB_COMBAT);
	TabButton("Render", TAB_ESP);
	TabButton("Player", TAB_PLAYER);
	TabButton("Movement", TAB_MOVEMENT);
	TabButton("Skins", TAB_SKINS);
	TabButton("Configs", TAB_CONFIGS);
	TabButton("Other", TAB_OTHER);
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	ImGui::BeginChild("##content", ImVec2(contentW, 0), false, ImGuiWindowFlags_None);
	
	// Apply fade-in animation to tab content
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tabFadeAlpha);

	if (activeTab == TAB_COMBAT)
	{
		// Панель 1: Aimbot
		BeginPremiumChild("##aimbot_panel", "Aimbot Settings", ImVec2(contentW, 180));
		PremiumToggle("Enable Aimbot", &g_aimbotEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::SameLine();
		KeybindWidget("##aimbot_bind", g_aimbotKey);
		ImGui::SameLine();
		ImGui::SameLine();
		if (PremiumSettingsButton("##aimbot_gear_btn")) ImGui::OpenPopup("##aimbot_ctx");

		if (ImGui::BeginPopup("##aimbot_ctx"))
		{
			ImGui::TextUnformatted("Настройки Аимбота");
			ImGui::Separator();
			PremiumToggle("Проверка команды", &g_aimbotTeamCheck);
			if (ImGui::IsItemClicked()) MarkDirty();
			PremiumToggle("Проверка видимости", &g_aimbotVisCheck);
			if (ImGui::IsItemClicked()) MarkDirty();
			PremiumToggle("Автовыстрел", &g_aimbotAutoFire);
			if (ImGui::IsItemClicked()) MarkDirty();
			
			// АВТОСОХРАНЕНИЕ: сохраняем только после отпускания слайдера
			ImGui::SliderFloat("FOV", &g_aimbotFov, 1.0f, 20.0f, "%.1f");
			if (ImGui::IsItemDeactivatedAfterEdit()) { MarkDirty(); }
			
			// АВТОСОХРАНЕНИЕ: сохраняем только после отпускания слайдера
			ImGui::SliderFloat("Smooth", &g_aimbotSmooth, 1.0f, 10.0f, "%.1f");
			if (ImGui::IsItemDeactivatedAfterEdit()) { MarkDirty(); }
			
			const char* boneNames[] = { "Голова", "Шея", "Грудь", "Живот" };
			const int boneIds[] = { 6, 5, 4, 2 };
			int currentBoneIdx = 0;
			for (int i = 0; i < 4; i++) {
				if (g_aimbotBone == boneIds[i]) {
					currentBoneIdx = i;
					break;
				}
			}
			if (ImGui::Combo("Цель", &currentBoneIdx, boneNames, 4)) {
				g_aimbotBone = boneIds[currentBoneIdx];
				MarkDirty();
			}
			PremiumToggle("Показать FOV", &g_aimbotDrawFov);
			ImGui::ColorEdit4("Цвет FOV", (float*)&g_aimbotFovColor, ImGuiColorEditFlags_NoInputs);
			ImGui::EndPopup();
		}
		EndPremiumChild();

		ImGui::Spacing();

		// Панель 2: Triggerbot
		BeginPremiumChild("##trigger_panel", "Triggerbot", ImVec2(contentW, 140));
		PremiumToggle("Enable Triggerbot", &g_triggerEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::SameLine();
		KeybindWidget("##trigger_bind", g_triggerKey);
		ImGui::SameLine();
		if (PremiumSettingsButton("##trigger_gear_btn")) ImGui::OpenPopup("##trigger_ctx");

		if (ImGui::BeginPopup("##trigger_ctx"))
		{
			ImGui::TextUnformatted("Настройки Триггербота");
			ImGui::Separator();
			PremiumToggle("Только в голову", &g_triggerHeadOnly);
			if (ImGui::IsItemClicked()) MarkDirty();
			PremiumToggle("Проверка команды", &g_triggerTeamCheck);
			if (ImGui::IsItemClicked()) MarkDirty();
			PremiumToggle("Только с прицелом", &g_triggerScopeOnly);
			if (ImGui::IsItemClicked()) MarkDirty();
			PremiumToggle("Проверка видимости", &g_triggerVisCheck);
			if (ImGui::IsItemClicked()) MarkDirty();
			ImGui::EndPopup();
		}
		EndPremiumChild();

		ImGui::Spacing();

		// Панель 3: Accuracy
		BeginPremiumChild("##accuracy_panel", "Accuracy", ImVec2(contentW, 100));
		PremiumToggle("No Recoil", &g_noRecoilEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		PremiumToggle("No Spread", &g_noSpreadEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		EndPremiumChild();
		
		ImGui::Spacing();
	}
	else if (activeTab == TAB_ESP)
	{
		// Начинаем таблицу на 2 колонки
		if (ImGui::BeginTable("##esp_columns", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings))
		{
			ImGui::TableNextRow();
			
			// --- КОЛОНКА 1 ---
			ImGui::TableSetColumnIndex(0);
			
			PremiumToggle("Boxes", &g_whEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			ImGui::SameLine();
			if (PremiumSettingsButton("##box_gear_btn")) ImGui::OpenPopup("##box_ctx");

			if (ImGui::BeginPopup("##box_ctx")) {
				ImGui::TextUnformatted("Настройки боксов");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет", (float*)&g_boxColor, ImGuiColorEditFlags_NoInputs);
				PremiumToggle("Dynamic HP Color", &g_dynamicBoxColor);
				if (ImGui::IsItemClicked()) MarkDirty();
				ImGui::SliderFloat("Отступы", &g_boxPadding, 0.0f, 10.0f, "%.1f");
				ImGui::SliderFloat("Толщина", &g_boxThickness, 0.1f, 5.0f, "%.1f");
				ImGui::EndPopup();
			}

			ImGui::Separator();
			
			PremiumToggle("HP Bar", &g_hpBarEnabled);
			ImGui::SameLine();
			if (PremiumSettingsButton("##hp_gear_btn")) ImGui::OpenPopup("##hp_ctx");

			if (ImGui::BeginPopup("##hp_ctx")) {
				ImGui::TextUnformatted("Настройки HP Bar");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет", (float*)&g_hpBarColor, ImGuiColorEditFlags_NoInputs);
				ImGui::SliderFloat("Ширина", &g_hpBarWidth, 1.0f, 10.0f, "%.1f");
				ImGui::SliderFloat("Отступ", &g_hpBarOffset, 0.0f, 20.0f, "%.1f");
				ImGui::EndPopup();
			}

			ImGui::Separator();
			
			PremiumToggle("Skeletons", &g_bonesEnabled);
			ImGui::SameLine();
			if (PremiumSettingsButton("##bone_gear_btn")) ImGui::OpenPopup("##bone_ctx");

			if (ImGui::BeginPopup("##bone_ctx")) {
				ImGui::TextUnformatted("Настройки скелета");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет", (float*)&g_boneColor, ImGuiColorEditFlags_NoInputs);
				ImGui::Checkbox("Debug: bone IDs", &g_boneDebugIds);
				ImGui::Checkbox("Hide lines", &g_boneHideLines);
				ImGui::EndPopup();
			}

			ImGui::Separator();

			// --- КОЛОНКА 2 ---
			ImGui::TableSetColumnIndex(1);
			
			PremiumToggle("Names", &g_nameEspEnabled);
			ImGui::SameLine();
			if (PremiumSettingsButton("##name_gear_btn")) ImGui::OpenPopup("##name_ctx");

			if (ImGui::BeginPopup("##name_ctx")) {
				ImGui::TextUnformatted("Настройки имён");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет", (float*)&g_nameColor, ImGuiColorEditFlags_NoInputs);
				ImGui::SliderFloat("Смещение Y", &g_nameOffsetY, 0.0f, 20.0f, "%.1f");
				ImGui::EndPopup();
			}

			ImGui::Separator();
			
			PremiumToggle("Weapons", &g_gunEspEnabled);
			ImGui::SameLine();
			if (PremiumSettingsButton("##weapon_gear_btn")) ImGui::OpenPopup("##weapon_ctx");

			if (ImGui::BeginPopup("##weapon_ctx")) {
				ImGui::TextUnformatted("Настройки оружия (текст)");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет текста", (float*)&g_weaponIconColor, ImGuiColorEditFlags_NoInputs);
				ImGui::SliderFloat("Смещение X", &g_gunOffsetX, -30.0f, 30.0f, "%.1f");
				ImGui::SliderFloat("Смещение Y", &g_gunOffsetY, 0.0f, 30.0f, "%.1f");
				ImGui::EndPopup();
			}

			ImGui::Separator();
			
			PremiumToggle("Chams (Glow)", &g_chamsEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			ImGui::SameLine();
			if (PremiumSettingsButton("##chams_gear_btn")) ImGui::OpenPopup("##chams_ctx");

			if (ImGui::BeginPopup("##chams_ctx")) {
				ImGui::TextUnformatted("Настройки Chams");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет T", (float*)&g_chamsColorT, ImGuiColorEditFlags_NoInputs);
				ImGui::ColorEdit4("Цвет CT", (float*)&g_chamsColorCT, ImGuiColorEditFlags_NoInputs);
				ImGui::EndPopup();
			}

			// Продолжаем ПЕРВУЮ КОЛОНКУ (добавляем туда элементы)
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			
			PremiumToggle("Snaplines", &g_snaplinesEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			
			ImGui::Separator();
			PremiumToggle("Distance", &g_distanceEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();

			ImGui::Separator();
			PremiumToggle("Custom Crosshair", &g_customCrosshair);

			ImGui::Separator();
			PremiumToggle("Damage Indicators", &g_damageIndicatorsEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			ImGui::SameLine();
			if (PremiumSettingsButton("##dmg_gear_btn")) ImGui::OpenPopup("##dmg_ctx");
			if (ImGui::BeginPopup("##dmg_ctx")) {
				ImGui::ColorEdit4("Цвет урона", (float*)&g_damageColor, ImGuiColorEditFlags_NoInputs);
				ImGui::SliderFloat("Размер текста", &g_damageTextSize, 10.0f, 40.0f, "%.1f");
				ImGui::SliderFloat("Время (сек)", &g_damageTextLifetime, 1.0f, 10.0f, "%.1f");
				ImGui::EndPopup();
			}

			// --- ВТОРАЯ КОЛОНКА ---
			ImGui::TableSetColumnIndex(1);
			
			PremiumToggle("Radar Hack (Native)", &g_radarHackEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			
			ImGui::Separator();
			PremiumToggle("Out of FOV Arrows", &g_oofArrowsEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			ImGui::SameLine();
			if (PremiumSettingsButton("##oof_gear_btn")) ImGui::OpenPopup("##oof_ctx");
			if (ImGui::BeginPopup("##oof_ctx")) {
				ImGui::ColorEdit4("Цвет стрелок", (float*)&g_oofArrowsColor, ImGuiColorEditFlags_NoInputs);
				ImGui::SliderFloat("Радиус", &g_oofArrowsRadius, 50.0f, 500.0f, "%.0f");
				ImGui::SliderFloat("Размер", &g_oofArrowsSize, 10.0f, 30.0f, "%.1f");
				ImGui::EndPopup();
			}

			ImGui::Separator();
			PremiumToggle("Bomb ESP (Timer)", &g_bombEspEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();

			ImGui::Separator();
			PremiumToggle("Spectator List", &g_spectatorListEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();

			ImGui::EndTable(); // ТЕПЕРЬ ТАБЛИЦА ЗАКРЫВАЕТСЯ ЗДЕСЬ
		}

		// Эти можно оставить под таблицей
		ImGui::Spacing();
		PremiumToggle("Keybinds List", &g_keybindsListEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		
		ImGui::Separator();
		ImGui::Spacing();
	}
	else if (activeTab == TAB_PLAYER)
	{
		PremiumToggle("Anti-Flash", &g_antiFlashEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		
		PremiumToggle("No Smoke", &g_noSmokeEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::SameLine();
		ImGui::TextDisabled("(Removes smoke grenades)");
		
		PremiumToggle("Hit Sound & Hitmarker", &g_hitSoundEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::SameLine();
		ImGui::TextDisabled("(Plays sound on hit)");
		
		ImGui::Separator();
		
		if (PremiumToggle("FOV Override", &g_fovEnabled))
		{
			MarkDirty();
		}
		ImGui::SliderFloat("FOV Value", &g_fovValue, 60.0f, 140.0f, "%.1f");
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			MarkDirty();
		}
		
		ImGui::Separator();
		
		// Stream Proof / Anti-Capture
		if (PremiumToggle("Anti-Capture (OBS/Discord)", &g_antiCaptureEnabled))
		{
			MarkDirty();
			// Применяем сразу
			if (g_hOverlayWnd) {
				SetWindowDisplayAffinity(g_hOverlayWnd, g_antiCaptureEnabled ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
			}
		}
		ImGui::TextDisabled("Скрывает меню и ESP от захвата экрана");
	}
	else if (activeTab == TAB_MOVEMENT)
	{
		PremiumToggle("Bhop", &g_bhopEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::SameLine();
		KeybindWidget("##bhop_bind", g_bhopKey);
		PremiumToggle("Auto-Strafe (ADAD sync)", &g_autoStrafeEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::TextDisabled("Auto-Strafe работает совместно с Bhop");
	}
	else if (activeTab == TAB_SKINS)
	{
		// === KNIFE CHANGER ===
		ImGui::Text("Knife Changer:");
		ImGui::Spacing();
		ImGui::Checkbox("Enable Knife Changer", &g_cfg->knife_changer.m_enabled);
		if (ImGui::IsItemClicked()) MarkDirty();

		if (g_cfg->knife_changer.m_enabled) {
			if (g_item_schema->is_initialized() && !g_item_schema->knife_names_cstr.empty()) {
				if (ImGui::Combo("Knife Model", &g_cfg->knife_changer.m_knife,
					g_item_schema->knife_names_cstr.data(),
					(int)g_item_schema->knife_names_cstr.size())) {
					g_skin_changer->should_update = true;
					MarkDirty();
				}
			}

			uint16_t selected_knife = 0;
			if (g_item_schema->is_initialized() &&
				g_cfg->knife_changer.m_knife < (int)g_item_schema->knives.size()) {
				selected_knife = g_item_schema->knives[g_cfg->knife_changer.m_knife].definition_index;
			}

			if (g_item_schema->is_initialized()) {
				auto& knife_skins = g_item_schema->get_paint_kit_names_for_item(selected_knife);
				if (!knife_skins.empty()) {
					if (ImGui::Combo("Knife Skin", &g_cfg->knife_changer.m_paint_kit,
						knife_skins.data(), (int)knife_skins.size())) {
						g_skin_changer->should_update = true;
						MarkDirty();
					}
				}
			}

			if (ImGui::SliderFloat("Knife Wear", &g_cfg->knife_changer.m_wear, 0.0001f, 1.0f, "%.4f")) {
				g_skin_changer->should_update = true;
				MarkDirty();
			}
			if (ImGui::InputInt("Knife Seed", &g_cfg->knife_changer.m_seed)) {
				if (g_cfg->knife_changer.m_seed < 0) g_cfg->knife_changer.m_seed = 0;
				if (g_cfg->knife_changer.m_seed > 1000) g_cfg->knife_changer.m_seed = 1000;
				g_skin_changer->should_update = true;
				MarkDirty();
			}
			if (ImGui::InputText("Knife Name", g_cfg->knife_changer.m_custom_name, sizeof(g_cfg->knife_changer.m_custom_name))) {
				g_skin_changer->should_update = true;
				MarkDirty();
			}
		}

		ImGui::Spacing();
		ImGui::Separator();

		// === GLOVE CHANGER ===
		ImGui::Text("Glove Changer:");
		ImGui::Spacing();
		ImGui::Checkbox("Enable Glove Changer", &g_cfg->glove_changer.m_enabled);
		if (ImGui::IsItemClicked()) MarkDirty();

		if (g_cfg->glove_changer.m_enabled) {
			if (g_item_schema->is_initialized() && !g_item_schema->glove_names_cstr.empty()) {
				if (ImGui::Combo("Glove Model", &g_cfg->glove_changer.m_glove,
					g_item_schema->glove_names_cstr.data(),
					(int)g_item_schema->glove_names_cstr.size())) {
					g_glove_changer->should_update = true;
					MarkDirty();
				}
			}

			uint16_t selected_glove = 0;
			if (g_item_schema->is_initialized() &&
				g_cfg->glove_changer.m_glove < (int)g_item_schema->gloves.size()) {
				selected_glove = g_item_schema->gloves[g_cfg->glove_changer.m_glove].definition_index;
			}

			static int last_glove = -1;
			if (last_glove != g_cfg->glove_changer.m_glove) {
				auto& glove_skins = g_item_schema->get_paint_kit_names_for_item(selected_glove);
				g_cfg->glove_changer.m_paint_kit = (glove_skins.size() > 1) ? 1 : 0;
				last_glove = g_cfg->glove_changer.m_glove;
			}

			if (g_item_schema->is_initialized()) {
				auto& glove_skins = g_item_schema->get_paint_kit_names_for_item(selected_glove);
				if (!glove_skins.empty()) {
					if (ImGui::Combo("Glove Skin", &g_cfg->glove_changer.m_paint_kit,
						glove_skins.data(), (int)glove_skins.size())) {
						g_glove_changer->should_update = true;
						MarkDirty();
					}
				}
			}

			if (ImGui::SliderFloat("Glove Wear", &g_cfg->glove_changer.m_wear, 0.0001f, 1.0f, "%.4f")) {
				g_glove_changer->should_update = true;
				MarkDirty();
			}
			if (ImGui::InputInt("Glove Seed", &g_cfg->glove_changer.m_seed)) {
				if (g_cfg->glove_changer.m_seed < 0) g_cfg->glove_changer.m_seed = 0;
				if (g_cfg->glove_changer.m_seed > 1000) g_cfg->glove_changer.m_seed = 1000;
				g_glove_changer->should_update = true;
				MarkDirty();
			}
		}

		ImGui::Spacing();
		ImGui::Separator();

		// === SKIN CHANGER ===
		ImGui::Text("Skin Changer:");
		ImGui::Spacing();
		ImGui::Checkbox("Enable Skin Changer", &g_cfg->skin_changer.m_enabled);
		if (ImGui::IsItemClicked()) MarkDirty();

		if (g_cfg->skin_changer.m_enabled && g_item_schema->is_initialized()) {
			if (!g_item_schema->weapon_names_cstr.empty()) {
				if (ImGui::Combo("Weapon", &g_cfg->skin_changer.m_selected_weapon,
					g_item_schema->weapon_names_cstr.data(),
					(int)g_item_schema->weapon_names_cstr.size())) {
					MarkDirty();
				}
			}

			uint16_t selected_weapon_def = 0;
			if (g_cfg->skin_changer.m_selected_weapon < (int)g_item_schema->weapons.size()) {
				selected_weapon_def = g_item_schema->weapons[g_cfg->skin_changer.m_selected_weapon].definition_index;
			}

			if (selected_weapon_def > 0) {
				int config_index = c_config::skin_changer_t::get_config_index(selected_weapon_def);
				auto& weapon_skin = g_cfg->skin_changer.weapon_skins[config_index];

				auto& weapon_skins = g_item_schema->get_paint_kit_names_for_item(selected_weapon_def);
				if (!weapon_skins.empty()) {
					if (ImGui::Combo("Skin", &weapon_skin.paint_kit,
						weapon_skins.data(), (int)weapon_skins.size())) {
						g_skin_changer->should_update = true;
						MarkDirty();
					}
				}

				if (ImGui::SliderFloat("Wear", &weapon_skin.wear, 0.0001f, 1.0f, "%.4f")) {
					g_skin_changer->should_update = true;
					MarkDirty();
				}
				if (ImGui::InputInt("Seed", &weapon_skin.seed)) {
					if (weapon_skin.seed < 0) weapon_skin.seed = 0;
					if (weapon_skin.seed > 1000) weapon_skin.seed = 1000;
					g_skin_changer->should_update = true;
					MarkDirty();
				}
				if (ImGui::InputText("Custom Name", weapon_skin.custom_name, sizeof(weapon_skin.custom_name))) {
					g_skin_changer->should_update = true;
					MarkDirty();
				}
			}
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Скины подгружаются из item_schema движка — полный список.");
	}
	else if (activeTab == TAB_CONFIGS)
	{
		ImGui::TextUnformatted("Config Manager");
		ImGui::Separator();
		
		// Get DLL directory for configs
		static std::string s_dllDir;
		static std::vector<std::string> s_configFiles;
		static int s_selectedConfig = 0;
		static ULONGLONG s_lastRefresh = 0;
		
		// Refresh config list periodically
		ULONGLONG now = GetTickCount64();
		if (s_dllDir.empty() || now - s_lastRefresh > 2000) {
			if (GetDllDirA(s_dllDir)) {
				s_configFiles.clear();
				
				std::string searchPath = s_dllDir + "\\*.ini";
				WIN32_FIND_DATAA findData;
				HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
				if (hFind != INVALID_HANDLE_VALUE) {
					do {
						if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
							s_configFiles.push_back(findData.cFileName);
						}
					} while (FindNextFileA(hFind, &findData));
					FindClose(hFind);
				}
			}
			s_lastRefresh = now;
		}
		
		// Config list
		ImGui::Text("Available Configs:");
		if (ImGui::BeginListBox("##configs_list", ImVec2(-1, 200))) {
			for (int i = 0; i < (int)s_configFiles.size(); i++) {
				bool isSelected = (s_selectedConfig == i);
				if (ImGui::Selectable(s_configFiles[i].c_str(), isSelected)) {
					s_selectedConfig = i;
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndListBox();
		}
		
		ImGui::Spacing();
		
		// Action buttons
		if (ImGui::Button("Load Selected", ImVec2(120, 0))) {
			if (s_selectedConfig >= 0 && s_selectedConfig < (int)s_configFiles.size()) {
				// Copy selected config to config.ini
				std::string srcPath = s_dllDir + "\\" + s_configFiles[s_selectedConfig];
				std::string dstPath = s_dllDir + "\\config.ini";
				CopyFileA(srcPath.c_str(), dstPath.c_str(), FALSE);
				LoadConfig();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Save As...", ImVec2(120, 0))) {
			static char newConfigName[64] = "new_config.ini";
			ImGui::OpenPopup("Save Config As");
		}
		
		// Save As popup
		if (ImGui::BeginPopupModal("Save Config As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			static char newConfigName[64] = "new_config.ini";
			ImGui::InputText("Filename", newConfigName, sizeof(newConfigName));
			ImGui::Spacing();
			if (ImGui::Button("Save", ImVec2(100, 0))) {
				std::string srcPath = s_dllDir + "\\config.ini";
				std::string dstPath = s_dllDir + "\\" + std::string(newConfigName);
				SaveConfig();
				CopyFileA(srcPath.c_str(), dstPath.c_str(), FALSE);
				s_lastRefresh = 0; // Force refresh
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("Configs are .ini files in the DLL folder.");
		ImGui::TextDisabled("Select a config and click Load to switch.");
	}
	else if (activeTab == TAB_OTHER)
	{
		ImGui::ColorEdit4("Menu Color", (float*)&g_guiColor, ImGuiColorEditFlags_NoInputs);
		if (ImGui::IsItemDeactivatedAfterEdit())
			{
				ApplyClientStyle();
				SaveConfig();
			}
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Config is saved automatically.");
		
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));
		if (ImGui::Button("Unload Cheat", ImVec2(140, 30))) {
			SaveConfig();
			g_running = false;
		}
		ImGui::PopStyleColor(3);
		ImGui::TextDisabled("Сохраняет конфиг и выгружает DLL (END)");
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::SetWindowFontScale(1.1f);
	ImGui::TextUnformatted("END - Unload DLL");
	ImGui::SetWindowFontScale(1.0f);

	// End fade-in animation
	ImGui::PopStyleVar();

	ImGui::EndChild();
	
	// PREVIEW PANEL
	if (activeTab == TAB_ESP) {
		ImGui::SameLine();
		ImGui::BeginChild("##right_preview", ImVec2(actualPreviewW, 0), true, ImGuiWindowFlags_None);
		ImGui::TextDisabled("Preview");
		ImGui::Separator();
		
		ImGui::TextUnformatted("ESP Settings");
		ImGui::Spacing();
		ImVec2 prevSize(180.0f, 170.0f); // 1. УВЕЛИЧИЛИ ВЫСОТУ ДО 170
		ImGui::BeginChild("##esp_preview_inner", prevSize, true, ImGuiWindowFlags_NoScrollbar);
		{
			ImDrawList* pdl = ImGui::GetWindowDrawList();
			ImVec2 wp = ImGui::GetWindowPos();
			ImVec2 ws = ImGui::GetWindowSize();
			
			pdl->AddRectFilled(wp, ImVec2(wp.x+ws.x, wp.y+ws.y), IM_COL32(15,15,25,255)); // Фон
			
			float cx = wp.x + ws.x * 0.5f;
			float cy = wp.y + ws.y * 0.5f + 5.0f; // Чуть подняли центр
			float bw = 40.0f, bh = 85.0f;
			float bl = cx - bw*0.5f, bt = cy - bh*0.5f;

			// 2. РИСУЕМ GLOW (если включены Chams) ПЕРЕД боксами
			if (g_chamsEnabled) {
				ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_chamsColorT.x, g_chamsColorT.y, g_chamsColorT.z, 0.3f));
				// Эффект свечения (несколько полупрозрачных прямоугольников)
				for (int i = 1; i <= 4; i++) {
					pdl->AddRect(ImVec2(bl - i, bt - i), ImVec2(bl + bw + i, bt + bh + i), glowCol, 0.0f, 0, 1.0f);
				}
				// Заливка
				pdl->AddRectFilled(ImVec2(bl, bt), ImVec2(bl + bw, bt + bh), ImGui::ColorConvertFloat4ToU32(ImVec4(g_chamsColorT.x, g_chamsColorT.y, g_chamsColorT.z, 0.15f)));
			}

			// Bounding Box
			if (g_whEnabled) pdl->AddRect(ImVec2(bl,bt), ImVec2(bl+bw,bt+bh), ImGui::ColorConvertFloat4ToU32(g_boxColor), 0.0f, 0, g_boxThickness);
			
			// HP Bar
			if (g_hpBarEnabled) {
				pdl->AddRectFilled(ImVec2(bl-8,bt), ImVec2(bl-4,bt+bh), IM_COL32(0,0,0,180));
				pdl->AddRectFilled(ImVec2(bl-7,bt+bh*0.25f+1), ImVec2(bl-5,bt+bh-1), IM_COL32(80,220,80,255));
			}
			
			// Skeletons
			if (g_bonesEnabled) {
				ImU32 bc = ImGui::ColorConvertFloat4ToU32(g_boneColor);
				pdl->AddLine(ImVec2(cx,bt+5),  ImVec2(cx,bt+45), bc, 1.5f);
				pdl->AddLine(ImVec2(cx,bt+20), ImVec2(bl,bt+40), bc, 1.5f);
				pdl->AddLine(ImVec2(cx,bt+20), ImVec2(bl+bw,bt+40), bc, 1.5f);
				pdl->AddLine(ImVec2(cx,bt+45), ImVec2(bl+5,bt+bh), bc, 1.5f);
				pdl->AddLine(ImVec2(cx,bt+45), ImVec2(bl+bw-5,bt+bh), bc, 1.5f);
				pdl->AddCircleFilled(ImVec2(cx,bt+5), 5.0f, bc);
			}
			
			// Name (3. ПОДНЯЛИ ЧУТЬ ВЫШЕ)
			if (g_nameEspEnabled)
				pdl->AddText(ImVec2(bl - 5.0f, bt - 18.0f), ImGui::ColorConvertFloat4ToU32(g_nameColor), "Player");
				
			// Weapon (4. ОПУСТИЛИ ЧУТЬ НИЖЕ)
			if (g_gunEspEnabled)
				pdl->AddText(ImVec2(bl, bt + bh + 4.0f), ImGui::ColorConvertFloat4ToU32(g_weaponIconColor), "AK-47");
		}
		ImGui::EndChild();
		
		ImGui::EndChild();
	}
	
	ImGui::End();
}


