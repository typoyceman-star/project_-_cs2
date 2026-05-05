#pragma once
// D3D11 устройство, swap chain и DComp.

#include <Windows.h>

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
