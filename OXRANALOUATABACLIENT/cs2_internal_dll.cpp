#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <objbase.h>
#include <cstdint>
#include <cwchar>
#include <vector>
#include <map>
#include <limits>
#include <string>
#include <dwmapi.h>
#include <wincodec.h>
#include <Psapi.h>
#include <cmath>
#include <algorithm>
#include <mmsystem.h>
#include "resource.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dwmapi")
#pragma comment(lib, "dcomp")
#pragma comment(lib, "windowscodecs")
#pragma comment(lib, "Psapi")

#include "MinHook.h"

#define IMGUI_DEFINE_MATH_OPERATORS

#include "output/offsets.hpp"
#include "output/client_dll.hpp"
#include "output/buttons.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// ImGui implementations (this TU is the single compilation unit that contains them).
#pragma warning(push)
#pragma warning(disable: 6011)
#pragma warning(disable: 6387)
#include "imgui-1.92.5/imgui.cpp"
#include "imgui-1.92.5/imgui_draw.cpp"
#include "imgui-1.92.5/imgui_tables.cpp"
#include "imgui-1.92.5/imgui_widgets.cpp"
#include "imgui-1.92.5/backends/imgui_impl_dx11.cpp"
#include "imgui-1.92.5/backends/imgui_impl_win32.cpp"
#pragma warning(pop)

#include "functions/core/types.h"
#include "functions/core/globals.h"
#include "functions/core/memory.h"
#include "functions/core/math.h"
#include "functions/config/config_io.h"
#include "functions/hooks/hooks.h"
#include "functions/render/d3d11.h"
#include "functions/overlay/overlay.h"
#include "functions/gui/style.h"
#include "functions/gui/menu.h"
#include "functions/gui/watermark.h"
#include "functions/gui/keybinds_list.h"
#include "functions/features/aimbot/aimbot.h"
#include "functions/features/triggerbot/triggerbot.h"
#include "functions/features/bhop/bhop.h"
#include "functions/features/skinchanger/skin_changer.hpp"
#include "functions/features/skinchanger/glove_changer.hpp"
#include "functions/features/skinchanger/item_schema.hpp"
#include "main.hpp"
#include "functions/features/fov/fov.h"
#include "functions/features/antiflash/antiflash.h"
#include "functions/features/nosmoke/nosmoke.h"
#include "functions/features/norecoil/norecoil.h"
#include "functions/features/esp/esp.h"
#include "functions/features/bones/bones.h"
#include "functions/features/snaplines/snaplines.h"
#include "functions/features/spectators/spectators.h"

static BOOL wait_for_module(const char* module_name, DWORD timeout_ms) {
	DWORD elapsed = 0;
	while (elapsed < timeout_ms) {
		if (GetModuleHandleA(module_name) != nullptr)
			return TRUE;
		Sleep(50);
		elapsed += 50;
	}
	return FALSE;
}

DWORD WINAPI MainThread(LPVOID)
{
	HRESULT coInitHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const bool coInitOk = (coInitHr == S_OK || coInitHr == S_FALSE);

	timeBeginPeriod(1); // ФИКС: Убираем лок на 64 FPS, повышая точность таймера Windows до 1мс

	Sleep(1500);

	if (!wait_for_module("client.dll", 15000) ||
		!wait_for_module("engine2.dll", 15000) ||
		!wait_for_module("schemasystem.dll", 15000) ||
		!wait_for_module("inputsystem.dll", 15000) ||
		!wait_for_module("filesystem_stdio.dll", 15000)) {
		FreeLibraryAndExitThread(g_hModule, 0);
		return 0;
	}

	Sleep(500);

	MessageBoxA(NULL, "[1] Modules loaded, starting init...", "Debug", MB_OK | MB_TOPMOST);

	InitRuntimeOffsets();

	MessageBoxA(NULL, "[2] RuntimeOffsets done", "Debug", MB_OK | MB_TOPMOST);

	// Инициализация нового скинчейнджера (valve SDK)
	g_modules->m_modules.initialize();
	MessageBoxA(NULL, "[3] g_modules OK", "Debug", MB_OK | MB_TOPMOST);

	g_interfaces->initialize();
	MessageBoxA(NULL, "[4] g_interfaces OK", "Debug", MB_OK | MB_TOPMOST);

	g_item_schema->initialize();
	MessageBoxA(NULL, g_item_schema->is_initialized()
		? "[5] item_schema OK (initialized)"
		: "[5] item_schema SKIPPED (not ready yet, will retry later)", "Debug", MB_OK | MB_TOPMOST);

	g_skin_changer->initialize();
	MessageBoxA(NULL, "[6] skin_changer OK", "Debug", MB_OK | MB_TOPMOST);

	LoadConfig();
	MessageBoxA(NULL, "[7] Config loaded", "Debug", MB_OK | MB_TOPMOST);
	
	InitHooks(); // Инициализация VTable хука для реал-тайм скинов
	MessageBoxA(NULL, "[8] Hooks installed. Init complete!", "Debug", MB_OK | MB_TOPMOST);

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = OverlayWndProc;
	wc.hInstance = g_hModule;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = g_wndClassName;

	ATOM classAtom = RegisterClassExW(&wc);
	if (!classAtom) { FreeLibraryAndExitThread(g_hModule, 0); return 0; }

	int screenW = GetSystemMetrics(SM_CXSCREEN);
	int screenH = GetSystemMetrics(SM_CYSCREEN);

	HWND hwnd = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT,
		g_wndClassName, L"", WS_POPUP, 0, 0, screenW, screenH, nullptr, nullptr, wc.hInstance, nullptr);

	if (!hwnd) { UnregisterClassW(g_wndClassName, wc.hInstance); FreeLibraryAndExitThread(g_hModule, 0); return 0; }
	g_hOverlayWnd = hwnd;
	SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
	
	// Stream Proof: Скрываем оверлей от захвата OBS/Discord (по умолчанию выключено)
	if (g_antiCaptureEnabled) {
		SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
	}
	MARGINS margins = { -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(hwnd, &margins);

	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);

	if (!CreateDeviceD3D(hwnd)) {
		CleanupDeviceD3D(); DestroyWindow(hwnd); UnregisterClassW(g_wndClassName, wc.hInstance); FreeLibraryAndExitThread(g_hModule, 0); return 0;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Шрифты
	{
		// Попытка 1: Ресурсы
		bool fontLoaded = false;
		HRSRC hResource = FindResourceW(g_hModule, MAKEINTRESOURCEW(IDR_FONT_MAIN), RT_RCDATA);
		if (hResource) {
			HGLOBAL hMemory = LoadResource(g_hModule, hResource);
			if (hMemory) {
				DWORD dwSize = SizeofResource(g_hModule, hResource);
				LPVOID lpAddress = LockResource(hMemory);
				if (lpAddress && dwSize > 0) {
					void* fontData = IM_ALLOC(dwSize);
					memcpy(fontData, lpAddress, dwSize);
					ImFont* font = io.Fonts->AddFontFromMemoryTTF(fontData, dwSize, 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
					if (font) { io.FontDefault = font; fontLoaded = true; }
				}
			}
		}
		// Попытка 2: Системные
		if (!fontLoaded) {
			if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic())) fontLoaded = true;
			else if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic())) fontLoaded = true;
		}
	}

	ApplyClientStyle();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	MSG msg{};
	static ULONGLONG s_lastFrameTick = 0;

	// ОСНОВНОЙ ЦИКЛ
	while (g_running)
	{
		// 1. Обработка сообщений Windows (клики, перемещение окна)
		while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
			if (msg.message == WM_QUIT) g_running = false;
		}
		if (!g_running) break;

		// 2. Проверка состояния игры и кнопок
		bool cs2Active = IsCs2Active();
		if (!cs2Active) g_menuOpen = false;

		// Горячие клавиши (работают всегда, если окно CS2 активно)
		if (cs2Active) {
			if (GetAsyncKeyState(VK_F6) & 1) g_bhopEnabled = !g_bhopEnabled;
			if (g_whKey != 0 && (GetAsyncKeyState(g_whKey) & 1)) g_whEnabled = !g_whEnabled;
			if (g_bonesKey != 0 && (GetAsyncKeyState(g_bonesKey) & 1)) g_bonesEnabled = !g_bonesEnabled;
			if (g_triggerToggleKey != 0 && (GetAsyncKeyState(g_triggerToggleKey) & 1)) g_triggerEnabled = !g_triggerEnabled;
			
			// Меню
			if (GetAsyncKeyState(VK_RSHIFT) & 1) g_menuOpen = !g_menuOpen;
			if (g_menuOpen && (GetAsyncKeyState(VK_ESCAPE) & 1)) g_menuOpen = false;
			
			// Выгрузка
			if (GetAsyncKeyState(VK_END) & 1) { g_running = false; break; }
		}

		// 3. Чтение данных из памяти (Game Data)
		uintptr_t client = GetClientBase();
		uintptr_t localPawn = 0;
		uintptr_t localCtrl = 0; // ДОБАВЛЕНО: проверка на главное меню
		if (client) {
			(void)TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn);
			(void)TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerController, localCtrl); // ДОБАВЛЕНО
		}
		uintptr_t localScene = 0;
		int localHp = 0;
		if (localPawn && localCtrl) { // ДОБАВЛЕНО && localCtrl
			(void)TryRead<uintptr_t>(localPawn + g_offsetsRuntime.m_pGameSceneNode, localScene);
			(void)TryRead<int>(localPawn + g_offsetsRuntime.m_iHealth, localHp);
		}
		
		// СТРОГАЯ ПРОВЕРКА: Если localCtrl == 0, мы 100% в главном меню
		bool validLocalPlayer = (client && localPawn && localCtrl && localScene && localHp >= -100 && localHp < 10000);
		static bool s_prevValid = false;
		if (s_prevValid && !validLocalPlayer)
		{
			g_espBoxes.clear();
			g_espTargetsWorld.clear();
			g_boneCache.clear();
			g_spectators.clear(); // Очистка списка зрителей при смерти/смене раунда
		}
		s_prevValid = validLocalPlayer;
		
		// Логика функций (Aim, Bhop, Trigger)
		if (validLocalPlayer && cs2Active) {
			
			// --- ТРЕКИНГ ВЫСТРЕЛОВ ДЛЯ ТОЧНОГО HITSOUND ---
			static int s_lastShots = 0;
			int currentShots = 0;
			if (TryRead<int>(localPawn + g_offsetsRuntime.m_iShotsFired, currentShots)) {
				if (currentShots > s_lastShots) {
					g_lastLocalShotTime = GetTickCount64(); // Запоминаем время выстрела
				}
				s_lastShots = currentShots;
			}
			// ----------------------------------------------
			
			// МГНОВЕННАЯ ДЕТЕКЦИЯ ПОПАДАНИЙ (каждый кадр, без лимитов ESP)
			UpdateHitInfo(client);
			
			RunAimbot(true);
			RunCombat(true);
			RunBhop(true);
			
			// Skin Changer работает через VTable хук (hkFrameStageNotify)
			
			UpdateFovOverride(true);
			UpdateNoRecoilNoSpread(true);
			UpdateAntiFlash(true);
			UpdateNoSmoke(true); // ДОБАВЛЕНО: No Smoke
		} else {
			// Очистка кэша, если мы вышли в меню или мир выгружается
			if (!g_espBoxes.empty()) g_espBoxes.clear();
			if (!g_espTargetsWorld.empty()) g_espTargetsWorld.clear();
			if (!g_boneCache.empty()) g_boneCache.clear();
			if (!g_spectators.empty()) g_spectators.clear(); // Очистка списка зрителей
			g_bombData.found = false;
			g_bombData.pos = { 0,0,0 };
			g_autoFireActive = false;
		}

		// 4. Решение: нужно ли рисовать оверлей?
		bool needRender = g_menuOpen || (validLocalPlayer && cs2Active && (g_whEnabled || g_bonesEnabled || g_aimbotDrawFov || g_bombEspEnabled || g_spectatorListEnabled)); // ИСПРАВЛЕНО

		// Управление видимостью и кликабельностью окна
		// Если меню открыто -> кликабельно. Если нет -> прозрачно для кликов.
		SetOverlayInputMode(hwnd, g_menuOpen);

		
		// Показываем окно, если нужно рисовать, иначе прячем (оптимизация)
		static bool s_prevShow = false;
		if (needRender != s_prevShow) {
			ShowWindow(hwnd, needRender ? SW_SHOWNA : SW_HIDE);
			s_prevShow = needRender;
		}

		if (!needRender) {
			Sleep(16); // Экономим CPU, если ничего не рисуем
			continue;
		}

		// 5. Ограничитель FPS (чтобы не жрать 100% CPU оверлеем)
		ULONGLONG frameNow = GetTickCount64();
		if (s_lastFrameTick != 0 && (frameNow - s_lastFrameTick) < 8) { // ~120 FPS limit
			Sleep(1);
			continue;
		}
		s_lastFrameTick = frameNow;

		// 6. Подготовка данных для отрисовки (ESP)
		// Обновляем данные только если мы в матче
		RECT rc{}; GetClientRect(hwnd, &rc);
		int width = rc.right - rc.left;
		int height = rc.bottom - rc.top;
		float view[16]{}; // Матрица вида

		if (validLocalPlayer && cs2Active) {
			// Читаем ViewMatrix
			__try {
				for (int i = 0; i < 16; ++i)
					(void)TryRead<float>(client + g_offsetsRuntime.dwViewMatrix + i * sizeof(float), view[i]);
			} __except (1) { }

			// Обновляем списки ESP (раз в 33мс для оптимизации)
			static ULONGLONG s_lastEspTick = 0;
			if (frameNow - s_lastEspTick >= 33) {
				// ИСПРАВЛЕНО: Добавлено условие g_damageIndicatorsEnabled
				if (g_whEnabled || g_bonesEnabled || g_bombEspEnabled || g_spectatorListEnabled || g_hitSoundEnabled || g_damageIndicatorsEnabled) UpdateESPInternal(width, height);
				if (g_bonesEnabled) UpdateBonesCache(width, height);
				s_lastEspTick = frameNow;
			}
			
			// Проецируем боксы, если включена ЛЮБАЯ 2D функция
			if (g_whEnabled || g_nameEspEnabled || g_gunEspEnabled || g_hpBarEnabled) {
				ProjectESPBoxes(view, width, height);
			}
		}

		// 7. Рендер ImGui
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		
		// Передаем фокус ввода в ImGui, если меню открыто
		if (g_menuOpen) {
			ImGuiIO& curIO = ImGui::GetIO();
			curIO.MouseDrawCursor = true; // Рисуем курсор ImGui
		} else {
			ImGuiIO& curIO = ImGui::GetIO();
			curIO.MouseDrawCursor = false;
			// ИСПРАВЛЕНИЕ: Принудительно возвращаем фокус в CS2 при закрытии меню
			static bool s_wasMenuOpen = false;
			if (s_wasMenuOpen && !g_menuOpen) {
				// Меню только что закрылось - возвращаем фокус
				HWND cs2Wnd = FindWindowW(nullptr, L"Counter-Strike 2");
				if (cs2Wnd) {
					SetForegroundWindow(cs2Wnd);
					SetActiveWindow(cs2Wnd);
					SetFocus(cs2Wnd);
				}
			}
			s_wasMenuOpen = g_menuOpen;
		}

		ImGui::NewFrame();

		// Рисуем элементы
		if (validLocalPlayer && cs2Active) {
			DrawEspImGui();
			DrawSnaplinesImGui();
			if (g_bonesEnabled) DrawBonesImGui(view, width, height);
			DrawSpectatorListImGui();
		}
		
		DrawKeybindsListImGui();
		DrawWatermarkImGui();
		DrawMenuImGui();

		ImGui::Render();

		// 8. Вывод на экран (DirectX Present)
		if (!g_mainRenderTargetView) CreateRenderTarget();
		if (g_mainRenderTargetView) {
			const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // Прозрачный фон
			g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
			g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}
		g_pSwapChain->Present(0, 0);
	}

	// Выход
	ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
	CleanupDeviceD3D();
	if (g_hOverlayWnd) DestroyWindow(g_hOverlayWnd);
	UnregisterClassW(g_wndClassName, wc.hInstance);
	if (coInitOk) CoUninitialize();
	
	RemoveHooks(); // <--- ОЧЕНЬ ВАЖНО: Снимаем хук перед выгрузкой, иначе игра крашнется
	
	timeEndPeriod(1); // Возвращаем системный таймер в норму
	FreeLibraryAndExitThread(g_hModule, 0);
	return 0;
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		g_hModule = hModule;
		DisableThreadLibraryCalls(hModule);
		g_hMainThread = CreateThread(nullptr, 0, MainThread, nullptr, 0, &g_mainThreadId);
		break;

	case DLL_PROCESS_DETACH:
		// Если DLL выгружается не через FreeLibraryAndExitThread
		if (lpReserved == nullptr)
		{
			g_running = false;
			if (g_hOverlayWnd)
			{
				PostMessageW(g_hOverlayWnd, WM_QUIT, 0, 0);
			}
			// В DllMain нельзя блокироваться (loader lock), и тем более ждать свой же поток.
			// Просто закрываем handle (не влияет на выполнение потока).
			if (g_hMainThread)
			{
				CloseHandle(g_hMainThread);
				g_hMainThread = nullptr;
			}
		}
		break;
	}
	return TRUE;
}
