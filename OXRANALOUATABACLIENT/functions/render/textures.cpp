#include "textures.h"
#include "../core/globals.h"
#include <d3d11.h>
#include <wincodec.h>
#include <objbase.h>

bool LoadTextureFromResource(int resourceId, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
	if (!out_srv || !g_pd3dDevice || !g_pd3dDeviceContext)
		return false;

	*out_srv = nullptr;
	if (out_width) *out_width = 0;
	if (out_height) *out_height = 0;

	// Загружаем ресурс
	HRSRC hResource = FindResourceW(g_hModule, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
	if (!hResource)
		return false;

	HGLOBAL hMemory = LoadResource(g_hModule, hResource);
	if (!hMemory)
		return false;

	DWORD dwSize = SizeofResource(g_hModule, hResource);
	LPVOID lpAddress = LockResource(hMemory);
	if (!lpAddress || dwSize == 0)
		return false;

	// Создаем IStream из памяти
	IStream* stream = nullptr;
	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, dwSize);
	if (!hGlobal)
		return false;

	void* pGlobal = GlobalLock(hGlobal);
	if (!pGlobal)
	{
		GlobalFree(hGlobal);
		return false;
	}

	memcpy(pGlobal, lpAddress, dwSize);
	GlobalUnlock(hGlobal);

	HRESULT hr = CreateStreamOnHGlobal(hGlobal, TRUE, &stream);
	if (FAILED(hr) || !stream)
	{
		GlobalFree(hGlobal);
		return false;
	}

	// Создаем WIC factory
	IWICImagingFactory* factory = nullptr;
	hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if (FAILED(hr) || !factory)
	{
		stream->Release();
		return false;
	}

	// Декодируем из stream
	IWICBitmapDecoder* decoder = nullptr;
	hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
	stream->Release();
	if (FAILED(hr) || !decoder)
	{
		factory->Release();
		return false;
	}

	IWICBitmapFrameDecode* frame = nullptr;
	hr = decoder->GetFrame(0, &frame);
	if (FAILED(hr) || !frame)
	{
		decoder->Release();
		factory->Release();
		return false;
	}

	IWICFormatConverter* converter = nullptr;
	hr = factory->CreateFormatConverter(&converter);
	if (FAILED(hr) || !converter)
	{
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	hr = converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
	if (FAILED(hr))
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	UINT w = 0, h = 0;
	hr = converter->GetSize(&w, &h);
	if (FAILED(hr) || w == 0 || h == 0)
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	std::vector<BYTE> pixels;
	const UINT stride = w * 4;
	const UINT imageSize = stride * h;
	pixels.resize(imageSize);
	hr = converter->CopyPixels(nullptr, stride, imageSize, pixels.data());
	if (FAILED(hr))
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = w;
	texDesc.Height = h;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = pixels.data();
	initData.SysMemPitch = stride;

	ID3D11Texture2D* tex = nullptr;
	hr = g_pd3dDevice->CreateTexture2D(&texDesc, &initData, &tex);
	if (FAILED(hr) || !tex)
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	ID3D11ShaderResourceView* srv = nullptr;
	hr = g_pd3dDevice->CreateShaderResourceView(tex, &srvDesc, &srv);
	tex->Release();
	converter->Release();
	frame->Release();
	decoder->Release();
	factory->Release();

	if (FAILED(hr) || !srv)
		return false;

	*out_srv = srv;
	if (out_width) *out_width = (int)w;
	if (out_height) *out_height = (int)h;
	return true;
}

bool LoadTextureFromFileW(const wchar_t* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
	if (!filename || !out_srv || !g_pd3dDevice || !g_pd3dDeviceContext)
		return false;
	*out_srv = nullptr;
	if (out_width) *out_width = 0;
	if (out_height) *out_height = 0;

	IWICImagingFactory* factory = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if (FAILED(hr) || !factory)
		return false;

	IWICBitmapDecoder* decoder = nullptr;
	hr = factory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
	if (FAILED(hr) || !decoder)
	{
		factory->Release();
		return false;
	}

	IWICBitmapFrameDecode* frame = nullptr;
	hr = decoder->GetFrame(0, &frame);
	if (FAILED(hr) || !frame)
	{
		decoder->Release();
		factory->Release();
		return false;
	}

	IWICFormatConverter* converter = nullptr;
	hr = factory->CreateFormatConverter(&converter);
	if (FAILED(hr) || !converter)
	{
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	hr = converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
	if (FAILED(hr))
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	UINT w = 0, h = 0;
	hr = converter->GetSize(&w, &h);
	if (FAILED(hr) || w == 0 || h == 0)
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	std::vector<BYTE> pixels;
	const UINT stride = w * 4;
	const UINT imageSize = stride * h;
	pixels.resize(imageSize);
	hr = converter->CopyPixels(nullptr, stride, imageSize, pixels.data());
	if (FAILED(hr))
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = w;
	texDesc.Height = h;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = pixels.data();
	initData.SysMemPitch = stride;

	ID3D11Texture2D* tex = nullptr;
	hr = g_pd3dDevice->CreateTexture2D(&texDesc, &initData, &tex);
	if (FAILED(hr) || !tex)
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	ID3D11ShaderResourceView* srv = nullptr;
	hr = g_pd3dDevice->CreateShaderResourceView(tex, &srvDesc, &srv);
	tex->Release();
	converter->Release();
	frame->Release();
	decoder->Release();
	factory->Release();
	if (FAILED(hr) || !srv)
		return false;

	*out_srv = srv;
	if (out_width) *out_width = (int)w;
	if (out_height) *out_height = (int)h;
	return true;
}


