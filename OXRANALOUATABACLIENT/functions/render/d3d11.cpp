#include "d3d11.h"
#include "../core/globals.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dcomp.h>

bool CreateDeviceD3D(HWND hWnd)
{
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

	HRESULT hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		featureLevelArray,
		2,
		D3D11_SDK_VERSION,
		&g_pd3dDevice,
		&featureLevel,
		&g_pd3dDeviceContext);

	if (hr == DXGI_ERROR_UNSUPPORTED)
	{
		hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			0,
			featureLevelArray,
			2,
			D3D11_SDK_VERSION,
			&g_pd3dDevice,
			&featureLevel,
			&g_pd3dDeviceContext);
	}

	if (FAILED(hr)) return false;

	IDXGIDevice* dxgiDevice = nullptr;
	IDXGIAdapter* adapter = nullptr;
	IDXGIFactory2* factory2 = nullptr;

	hr = g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
	if (FAILED(hr) || !dxgiDevice) return false;

	hr = dxgiDevice->GetAdapter(&adapter);
	if (FAILED(hr) || !adapter) { dxgiDevice->Release(); return false; }

	hr = adapter->GetParent(IID_PPV_ARGS(&factory2));
	if (FAILED(hr) || !factory2) { adapter->Release(); dxgiDevice->Release(); return false; }

	DXGI_SWAP_CHAIN_DESC1 scd{};
	RECT rc{};
	GetClientRect(hWnd, &rc);
	UINT w = static_cast<UINT>((rc.right > rc.left) ? (rc.right - rc.left) : 1);
	UINT h = static_cast<UINT>((rc.bottom > rc.top) ? (rc.bottom - rc.top) : 1);

	scd.Width = w;
	scd.Height = h;
	scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	scd.Stereo = FALSE;
	scd.SampleDesc.Count = 1;
	scd.SampleDesc.Quality = 0;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 2;
	scd.Scaling = DXGI_SCALING_STRETCH;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	scd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
	scd.Flags = 0;

	hr = factory2->CreateSwapChainForComposition(g_pd3dDevice, &scd, nullptr, &g_pSwapChain);
	if (FAILED(hr) || !g_pSwapChain)
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = DCompositionCreateDevice(dxgiDevice, IID_PPV_ARGS(&g_dcompDevice));
	if (FAILED(hr) || !g_dcompDevice)
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = g_dcompDevice->CreateTargetForHwnd(hWnd, TRUE, &g_dcompTarget);
	if (FAILED(hr) || !g_dcompTarget)
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = g_dcompDevice->CreateVisual(&g_dcompVisual);
	if (FAILED(hr) || !g_dcompVisual)
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = g_dcompVisual->SetContent(g_pSwapChain);
	if (FAILED(hr))
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = g_dcompTarget->SetRoot(g_dcompVisual);
	if (FAILED(hr))
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = g_dcompDevice->Commit();

	factory2->Release();
	adapter->Release();
	dxgiDevice->Release();

	if (FAILED(hr)) return false;

	CreateRenderTarget();
	return true;
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer = nullptr;
	if (g_pSwapChain && SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))) && pBackBuffer)
	{
		g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
		pBackBuffer->Release();
	}
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_dcompVisual) { g_dcompVisual->Release(); g_dcompVisual = nullptr; }
	if (g_dcompTarget) { g_dcompTarget->Release(); g_dcompTarget = nullptr; }
	if (g_dcompDevice) { g_dcompDevice->Release(); g_dcompDevice = nullptr; }
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

