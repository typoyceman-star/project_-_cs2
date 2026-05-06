#include "menu.h"
#include "widgets.h"
#include "style.h"
#include "../core/globals.h"
#include "../core/memory.h"
#include "../config/config_io.h"
#include "../config/paths.h"
#include "../features/skinchanger/skinchanger.h"
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
		if (!g_skinWarningShown) {
			ImGui::OpenPopup("##skin_warning");
		}
		
		ImGui::SetNextWindowSize(ImVec2(650.0f, 420.0f));
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal("##skin_warning", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
			ImDrawList* pdl = ImGui::GetWindowDrawList();
			ImVec2 wp = ImGui::GetWindowPos();
			ImVec2 ws = ImGui::GetWindowSize();
			
			// Та же градиентная линия сверху как у основного меню
			ImU32 col1 = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 1.0f));
			ImU32 col2 = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x * 0.5f, g_guiColor.y * 0.3f, 1.0f, 1.0f));
			pdl->AddRectFilledMultiColor(wp, ImVec2(wp.x + ws.x, wp.y + 4.0f), col1, col2, col2, col1);
			
			ImGui::SetCursorPos(ImVec2(18, 20));
			ImGui::TextColored(g_guiColor, "OXRANALOUTABA CLIENT — Skin Changer");
			ImGui::SetCursorPosY(45);
			ImGui::Separator();
			
			ImGui::SetCursorPos(ImVec2(18, 70));
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Предупреждение");
			ImGui::Spacing();
			ImGui::SetCursorPosX(18);
			ImGui::TextWrapped("Скины применяются через FallbackPaintKit — это клиентский метод.");
			ImGui::Spacing();
			ImGui::SetCursorPosX(18);
			ImGui::TextWrapped("UV-развёртка на некоторых скинах может отображаться некорректно. Это ожидаемое поведение данного метода, а не баг.");
			ImGui::Spacing();
			ImGui::SetCursorPosX(18);
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Это сообщение показывается только один раз.");
			
			ImGui::SetCursorPos(ImVec2(ws.x * 0.5f - 60.0f, ws.y - 55.0f));
			ImGui::PushStyleColor(ImGuiCol_Button, g_guiColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(g_guiColor.x + 0.1f, g_guiColor.y + 0.1f, g_guiColor.z + 0.1f, 1.0f));
			if (ImGui::Button("Хорошо", ImVec2(120.0f, 36.0f))) {
				g_skinWarningShown = true;
				SaveConfig();
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleColor(2);
			
			ImGui::EndPopup();
		}
		
		PremiumToggle("Enable Skin Changer", &g_skinChangerEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		
		ImGui::Separator();
		ImGui::Text("Скины (Обновляются моментально!):");
		ImGui::TextDisabled("Кликни по оружию ниже, чтобы развернуть и настроить wear / seed / имя.");
		ImGui::Spacing();

		// Универсальный рендерер блока «один тип оружия».
		// def — C_EconItemView::m_iItemDefinitionIndex (ключ g_skinConfig).
		// names/kits — список вариантов в комбобоксе.
		auto DrawWeaponSkinRow = [&](const char* label, int def,
			const char* const* names, const int* kits, int count)
		{
			WeaponSkinCfg& cfg = g_skinConfig[def];
			
			// Найти текущий индекс по cfg.paintKit (или 0 = Default).
			int curIdx = 0;
			for (int i = 0; i < count; ++i) {
				if (kits[i] == cfg.paintKit) { curIdx = i; break; }
			}

			ImGui::PushID(def);
			if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_SpanAvailWidth)) {
				if (ImGui::Combo("Skin", &curIdx, names, count)) {
					cfg.paintKit = kits[curIdx];
					g_skinUpdateCounter++;
					MarkDirty();
				}
				if (ImGui::SliderFloat("Wear", &cfg.wear, 0.0001f, 1.0f, "%.4f")) {
					g_skinUpdateCounter++;
					MarkDirty();
				}
				if (ImGui::InputInt("Seed", &cfg.seed)) {
					if (cfg.seed < 0) cfg.seed = 0;
					if (cfg.seed > 1000) cfg.seed = 1000;
					g_skinUpdateCounter++;
					MarkDirty();
				}
				if (ImGui::InputText("Name", cfg.customName, IM_ARRAYSIZE(cfg.customName))) {
					g_skinUpdateCounter++;
					MarkDirty();
				}
				ImGui::TreePop();
			} else {
				// При свёрнутом узле справа от заголовка показываем выбранный скин.
				ImGui::SameLine();
				ImGui::TextDisabled("(%s)", names[curIdx]);
			}
			ImGui::PopID();
		};

		// === TEC-9 ===
		static const char* tec9Skins[] = { "Default", "Decimator", "Fuel Injector", "Remote Control", "Isaac", "Toxic", "Avalanche", "Re-Entry", "Brother" };
		static const int tec9PaintKits[] = { 0, 644, 614, 791, 303, 374, 520, 539, 1099 };
		DrawWeaponSkinRow("Tec-9", 30, tec9Skins, tec9PaintKits, IM_ARRAYSIZE(tec9Skins));

		// === USP-S ===
		static const char* uspSkins[] = { "Default", "Printstream", "The Traitor", "Neo-Noir", "Kill Confirmed", "Jawbreaker", "Monster Mashup", "Caiman", "Serum", "Orion", "Whiteout", "Target Acquired", "Ticket to Hell", "Cortex" };
		static const int uspPaintKits[] = { 0, 1142, 1040, 653, 504, 1173, 991, 339, 221, 313, 1065, 1027, 1146, 705 };
		DrawWeaponSkinRow("USP-S", 61, uspSkins, uspPaintKits, IM_ARRAYSIZE(uspSkins));

		// === GLOCK-18 ===
		static const char* glockSkins[] = { "Default", "Twilight Galaxy", "Vogue", "Water Elemental", "Snack Attack", "Gamma Doppler Emerald", "Wasteland Rebel", "Bullet Queen", "Neo-Noir", "Fade", "Moonrise", "Nuclear Garden" };
		static const int glockPaintKits[] = { 0, 437, 963, 353, 1100, 1119, 586, 957, 988, 38, 707, 536 };
		DrawWeaponSkinRow("Glock-18", 4, glockSkins, glockPaintKits, IM_ARRAYSIZE(glockSkins));

		// === AK-47 ===
		static const char* akSkins[] = { "Default", "Nightwish", "Leet Museo", "Legion of Anubis", "Asiimov", "Neon Rider", "The Empress", "Bloodsport", "Neon Revolution", "Fuel Injector", "Aquamarine Revenge", "Wasteland Rebel", "Jaguar", "Vulcan", "Fire Serpent", "Gold Arabesque", "X-Ray", "Wild Lotus", "Ice Coaled", "Phantom Disruptor", "Point Disarray", "Frontside Misty", "Cartel", "Redline", "Case Hardened", "Red Laminate", "Panthera onca", "Hydroponic", "Jet Set" };
		static const int akPaintKits[] = { 0, 1141, 1087, 959, 551, 433, 675, 597, 600, 524, 474, 380, 316, 302, 180, 1026, 1004, 724, 1143, 941, 506, 490, 528, 282, 44, 14, 1018, 456, 340 };
		DrawWeaponSkinRow("AK-47", 7, akSkins, akPaintKits, IM_ARRAYSIZE(akSkins));

		// === AWP ===
		static const char* awpSkins[] = { "Default", "Printstream", "Chromatic Aberration", "Containment Breach", "Wildfire", "Neo-Noir", "Oni Taiji", "Hyper Beast", "Man-o'-war", "Asiimov", "Lightning Strike", "Desert Hydra", "Fade", "The Prince", "Gungnir", "Medusa", "Dragon Lore", "Ice Coaled", "Mortis", "Fever Dream", "Elite Build", "Corticera", "Redline", "Electric Hive", "Graphite", "BOOM", "Silk Tiger" };
		static const int awpPaintKits[] = { 0, 1144, 1120, 887, 917, 803, 662, 475, 395, 279, 51, 1058, 1022, 736, 756, 446, 344, 1143, 691, 640, 525, 181, 259, 227, 212, 174, 1029 };
		DrawWeaponSkinRow("AWP", 9, awpSkins, awpPaintKits, IM_ARRAYSIZE(awpSkins));

		// === FAMAS ===
		static const char* famasSkins[] = { "Default", "Commemoration", "Roll Cage", "Rapid Eye Movement", "Eye of Athena", "Mecha Industries", "Djinn", "Afterimage", "Waters of Nephthys", "Meltdown", "Valence" };
		static const int famasPaintKits[] = { 0, 919, 604, 1127, 723, 587, 429, 154, 1128, 1053, 529 };
		DrawWeaponSkinRow("FAMAS", 10, famasSkins, famasPaintKits, IM_ARRAYSIZE(famasSkins));

		// === GALIL-AR ===
		static const char* galilSkins[] = { "Default", "Chatterbox", "Chromatic Aberration", "Sugar Rush", "Eco", "Cerberus", "Rocket Pop" };
		static const int galilPaintKits[] = { 0, 398, 1144, 661, 428, 379, 478 };
		DrawWeaponSkinRow("Galil-AR", 13, galilSkins, galilPaintKits, IM_ARRAYSIZE(galilSkins));

		// === M4A1-S ===
		static const char* m4a1sSkins[] = { "Default", "Printstream", "Player Two", "Mecha Industries", "Chantico's Fire", "Golden Coil", "Hyper Beast", "Cyrex", "Fade", "Imminent Danger", "Welcome to the Jungle", "Black Lotus", "Nightmare", "Leaded Glass", "Decimator", "Atomic Alloy", "Guardian", "Blue Phosphor", "Control Panel", "Hot Rod", "Master Piece", "Knight" };
		static const int m4a1sPaintKits[] = { 0, 984, 946, 587, 548, 497, 430, 312, 1041, 1073, 1001, 1102, 714, 681, 644, 301, 257, 1017, 792, 445, 321, 326 };
		DrawWeaponSkinRow("M4A1-S", 60, m4a1sSkins, m4a1sPaintKits, IM_ARRAYSIZE(m4a1sSkins));

		// === M4A4 ===
		static const char* m4a4Skins[] = { "Default", "Howl", "In Living Color", "The Emperor", "Neo-Noir", "Buzz Kill", "The Battlestar", "Royal Paladin", "Bullet Rain", "Desert-Strike", "Asiimov", "X-Ray", "The Coalition", "Cyber Security", "Tooth Fairy", "Hellfire", "Desolate Space", "Dragon King", "Poseidon" };
		static const int m4a4PaintKits[] = { 0, 309, 1041, 844, 695, 632, 533, 512, 155, 336, 255, 215, 1063, 985, 971, 664, 588, 400, 449 };
		DrawWeaponSkinRow("M4A4", 16, m4a4Skins, m4a4PaintKits, IM_ARRAYSIZE(m4a4Skins));

		// === SSG-08 ===
		static const char* ssgSkins[] = { "Default", "Dragonfire", "Blood in the Water", "Turbo Peek", "Bloodshot", "Big Iron", "Death Strike" };
		static const int ssgPaintKits[] = { 0, 624, 222, 1101, 899, 503, 1052 };
		DrawWeaponSkinRow("SSG-08", 40, ssgSkins, ssgPaintKits, IM_ARRAYSIZE(ssgSkins));

		// === DESERT EAGLE ===
		static const char* deagleSkins[] = { "Default", "Ocean Drive", "Printstream", "Code Red", "Golden Koi", "Mecha Industries", "Kumicho Dragon", "Conspiracy", "Cobalt Disruption", "Hypnotic", "Fennec Fox" };
		static const int deaglePaintKits[] = { 0, 1090, 984, 711, 185, 587, 527, 351, 231, 61, 1051 };
		DrawWeaponSkinRow("Desert Eagle", 1, deagleSkins, deaglePaintKits, IM_ARRAYSIZE(deagleSkins));

		ImGui::Spacing();
		ImGui::Separator();

		// === KNIFE CHANGER ===
		// Список ножей (def_index из C_EconItemView::m_iItemDefinitionIndex).
		// «Default» = 0 = «не менять модель» (paint kit всё равно применится к стоковому ножу).
		static const char* knifeNames[] = {
			"Default (модель не меняется)",
			"Bayonet", "Flip Knife", "Gut Knife", "Karambit", "M9 Bayonet",
			"Huntsman Knife", "Falchion Knife", "Bowie Knife", "Butterfly Knife",
			"Shadow Daggers", "Paracord Knife", "Survival Knife", "Ursus Knife",
			"Navaja Knife", "Nomad Knife", "Stiletto Knife", "Talon Knife",
			"Skeleton Knife", "Classic Knife", "Kukri Knife"
		};
		static const int knifeDefs[] = {
			0,
			500, 505, 506, 507, 508,
			509, 512, 514, 515,
			516, 517, 518, 519,
			520, 521, 522, 523,
			525, 503, 526
		};
		static_assert(IM_ARRAYSIZE(knifeNames) == IM_ARRAYSIZE(knifeDefs), "knife arrays must match");

		// Универсальные «ножевые» paint kits (в т.ч. редкие — Doppler, Fade, etc.)
		static const char* knifeSkinNames[] = {
			"Default (no skin)", "Vanilla (default model only)",
			"Crimson Web", "Slaughter", "Case Hardened", "Fade", "Forest DDPAT",
			"Stained", "Blue Steel", "Boreal Forest", "Doppler",
			"Damascus Steel", "Ultraviolet", "Marble Fade", "Tiger Tooth",
			"Rust Coat", "Night", "Safari Mesh", "Scorched", "Urban Masked",
			"Black Laminate", "Gamma Doppler"
		};
		static const int knifeSkinKits[] = {
			0, 0,
			12, 41, 44, 38, 5,
			15, 42, 8, 417,
			558, 419, 413, 409,
			421, 17, 9, 34, 35,
			15, 568
		};
		static_assert(IM_ARRAYSIZE(knifeSkinNames) == IM_ARRAYSIZE(knifeSkinKits), "knife skin arrays must match");

		ImGui::Text("Knife Changer:");
		ImGui::Spacing();
		PremiumToggle("Enable Knife Changer", &g_knifeEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();

		if (g_knifeEnabled) {
			// Текущие индексы по сохранённым значениям.
			int knifeModelIdx = 0;
			for (int i = 0; i < IM_ARRAYSIZE(knifeDefs); ++i) {
				if (knifeDefs[i] == g_knifeDefIndex) { knifeModelIdx = i; break; }
			}
			int knifeSkinIdx = 0;
			for (int i = 0; i < IM_ARRAYSIZE(knifeSkinKits); ++i) {
				if (knifeSkinKits[i] == g_knifePaintKit) { knifeSkinIdx = i; break; }
			}
			if (ImGui::Combo("Knife Model", &knifeModelIdx, knifeNames, IM_ARRAYSIZE(knifeNames))) {
				g_knifeDefIndex = knifeDefs[knifeModelIdx];
				g_skinUpdateCounter++;
				MarkDirty();
			}
			if (ImGui::Combo("Knife Skin", &knifeSkinIdx, knifeSkinNames, IM_ARRAYSIZE(knifeSkinNames))) {
				g_knifePaintKit = knifeSkinKits[knifeSkinIdx];
				g_skinUpdateCounter++;
				MarkDirty();
			}
			if (ImGui::SliderFloat("Knife Wear", &g_knifeWear, 0.0001f, 1.0f, "%.4f")) {
				g_skinUpdateCounter++;
				MarkDirty();
			}
			if (ImGui::InputInt("Knife Seed", &g_knifeSeed)) {
				if (g_knifeSeed < 0) g_knifeSeed = 0;
				if (g_knifeSeed > 1000) g_knifeSeed = 1000;
				g_skinUpdateCounter++;
				MarkDirty();
			}
			if (ImGui::InputText("Knife Name", g_knifeName, IM_ARRAYSIZE(g_knifeName))) {
				g_skinUpdateCounter++;
				MarkDirty();
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("Перчатки и агенты пока не поддерживаются — требуют движкового рефреша.");
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
		if (ImGui::Button("Unload Cheat")) {
			// Здесь можно добавить логику выгрузки
		}
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


