#pragma once
// Оверлейное окно: WndProc + переключение режима ввода.

#include <Windows.h>

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void SetOverlayInputMode(HWND hWnd, bool interactive);
