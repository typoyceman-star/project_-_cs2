#include "overlay.h"
#include "../core/globals.h"
#include "../gui/menu.h"
#include "../render/d3d11.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include <dwmapi.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void SetOverlayInputMode(HWND hWnd, bool interactive)
{
	static bool wasInteractive = false;
	if (interactive == wasInteractive) return;
	wasInteractive = interactive;

	LONG exStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
	if (interactive)
	{
		// Меню открыто: окно принимает ввод
		exStyle &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
		SetWindowLongW(hWnd, GWL_EXSTYLE, exStyle);
		SetForegroundWindow(hWnd);
		SetFocus(hWnd);
	}
	else
	{
		// Меню закрыто: окно пропускает клики
		exStyle |= (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
		SetWindowLongW(hWnd, GWL_EXSTYLE, exStyle);
		
		// Принудительно возвращаем фокус игре
		HWND hGame = FindWindowW(L"SDL_app", L"Counter-Strike 2"); // Класс окна CS2
		if (hGame)
		{
			SetForegroundWindow(hGame);
			SetActiveWindow(hGame);
		}
	}
}

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Если меню открыто, сначала даем ImGui обработать сообщение
	if (g_menuOpen)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return 1;
	}

	switch (msg)
	{
	case WM_CREATE:
	{
		MARGINS margin = { -1, -1, -1, -1 };
		DwmExtendFrameIntoClientArea(hWnd, &margin);
		return 0;
	}
	case WM_NCHITTEST:
	{
		// Если меню открыто — это обычное окно (клики работают).
		// Если закрыто — окно прозрачно для кликов (пропускает в игру).
		if (g_menuOpen) {
			// Важно: возвращаем HTCLIENT, чтобы ImGui получал события мыши
			return HTCLIENT;
		}
		return HTTRANSPARENT;
	}
	case WM_MOUSEACTIVATE:
		// При открытом меню разрешаем активацию окна
		return g_menuOpen ? MA_ACTIVATE : MA_NOACTIVATE;

	// УДАЛЕНО: WM_LBUTTONDOWN / UP с SetCapture — это ломало клики!

	case WM_ERASEBKGND:
		return 1;
	case WM_SIZE:
		if (g_pd3dDevice && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX11_InvalidateDeviceObjects();
			CleanupRenderTarget();
			if (g_pSwapChain)
			{
				g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
				CreateRenderTarget();
				ImGui_ImplDX11_CreateDeviceObjects();
				if (g_dcompDevice) g_dcompDevice->Commit();
			}
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

