#pragma once
// Загрузка текстур из ресурсов / файлов в D3D11 SRV.

#include <Windows.h>
#include <d3d11.h>

bool LoadTextureFromFileW(const wchar_t* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
bool LoadTextureFromResource(int resourceId, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
